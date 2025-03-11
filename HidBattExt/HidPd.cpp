#include "device.hpp"
#include "HidPd.hpp"
#include <hidclass.h> // for HID_COLLECTION_INFORMATION
#include "CppAllocator.hpp"


static void UpdateBatteryState(BATT_STATE& state, HIDP_REPORT_TYPE reportType, CHAR* report, const HidConfig& hid) {
    USHORT reportLen = 0;
    if (reportType == HidP_Input)
        reportLen = hid.InputReportByteLength;
    else if (reportType == HidP_Feature)
        reportLen = hid.FeatureReportByteLength;
    else
        NT_ASSERTMSG("UpdateBatteryState invalid reportType", false);

    const HidCode code = hid.reports[report[0]]; // ReportID lookup
    if (code == HidCode{ 0, 0 })
        return;

    ULONG value = 0;
    NTSTATUS status = HidP_GetUsageValue(reportType, code.UsagePage, /*default link collection*/0, code.Usage, &value, hid.GetPreparsedData(), report, reportLen);
    if (!NT_SUCCESS(status)) {
        // fails with HIDP_STATUS_USAGE_NOT_FOUND (0xc0110004) for HID_PD_SERIAL (0x02) report with UsagePage=0x84, Usage=0xff
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: HidP_GetUsageValue failed UsagePage=0x%x, Usage=0x%x, (0x%x)"), code.UsagePage, code.Usage, status);
        return;
    }

    // capture shared state
    if (code == CycleCount_Code) {
        auto CycleCountBefore = state.BatteryInfo.CycleCount;

        WdfSpinLockAcquire(state.Lock);
        state.BatteryInfo.CycleCount = value;
        WdfSpinLockRelease(state.Lock);

        if (state.BatteryInfo.CycleCount != CycleCountBefore) {
            DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: Updating HID CycleCount before=%u, after=%u\n", CycleCountBefore, state.BatteryInfo.CycleCount);
        }
    } else if (code == Temperature_Code) {
        auto TempBefore = state.Temperature;

        WdfSpinLockAcquire(state.Lock);
        // convert HID PD unit from (Kelvin) to BATTERY_QUERY_INFORMATION unit (10ths of a degree Kelvin)
        state.Temperature = 10*value;
        WdfSpinLockRelease(state.Lock);

        if (state.Temperature != TempBefore) {
            DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: Updating HID Temperature before=%u, after=%u\n", TempBefore, state.Temperature);
        }
    }
}


/** Blocking IOCTL_HID_GET_FEATURE request with pre-populated report. */
NTSTATUS GetFeatureReport(WDFIOTARGET target, UCHAR reportId) {
    if (!reportId)
        return STATUS_SUCCESS;

    WDFDEVICE device = WdfIoTargetGetDevice(target);
    DEVICE_CONTEXT* context = WdfObjectGet_DEVICE_CONTEXT(device);

    // Battery Temperature query
    RamArray<CHAR> report(context->Hid.FeatureReportByteLength);
    report[0] = reportId;

    WDF_MEMORY_DESCRIPTOR outputDesc = {};
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDesc, report, report.ByteSize());

    NTSTATUS status = WdfIoTargetSendIoctlSynchronously(target, NULL,
        IOCTL_HID_GET_FEATURE,
        NULL, // input
        &outputDesc, // output
        NULL, NULL);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: IOCTL_HID_GET_FEATURE failed 0x%x"), status);
        return status;
    }

    UpdateBatteryState(context->State, HidP_Feature, report, context->Hid);
    return status;
}

NTSTATUS InitializeHidState(_In_ WDFDEVICE Device) {
    DEVICE_CONTEXT* context = WdfObjectGet_DEVICE_CONTEXT(Device);
    WDFIOTARGET_Wrap pdoTarget;
    {
        // Use PDO for HID commands instead of local IO target to avoid 0xc0000061 (STATUS_PRIVILEGE_NOT_HELD) on IOCTL_HID_SET_FEATURE
        NTSTATUS status = WdfIoTargetCreate(Device, WDF_NO_OBJECT_ATTRIBUTES, &pdoTarget);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfIoTargetCreate failed 0x%x"), status);
            return status;
        }

        // open in shared read-write mode
        WDF_IO_TARGET_OPEN_PARAMS openParams = {};
        WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParams, &context->PdoName, FILE_READ_ACCESS | FILE_WRITE_ACCESS);
        // We will let the framework to respond automatically to the pnp state changes of the target by closing and opening the handle.
        openParams.ShareAccess = FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE;

        status = WdfIoTargetOpen(pdoTarget, &openParams);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfIoTargetOpen failed 0x%x"), status);
            return status;
        }
    }

    HID_COLLECTION_INFORMATION collectionInfo = {};
    {
        // populate "collectionInformation"
        WDF_MEMORY_DESCRIPTOR outputDesc = {};
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDesc, &collectionInfo, sizeof(HID_COLLECTION_INFORMATION));

        NTSTATUS status = WdfIoTargetSendIoctlSynchronously(pdoTarget, NULL,
            IOCTL_HID_GET_COLLECTION_INFORMATION,
            NULL, // input
            &outputDesc, // output
            NULL, NULL);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: IOCTL_HID_GET_COLLECTION_INFORMATION failed 0x%x"), status);
            return status;
        }

        DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: ProductID=%x, VendorID=%x, VersionNumber=%u, DescriptorSize=%u\n", collectionInfo.ProductID, collectionInfo.VendorID, collectionInfo.VersionNumber, collectionInfo.DescriptorSize);
    }

    {
        WDF_OBJECT_ATTRIBUTES attr{};
        WDF_OBJECT_ATTRIBUTES_INIT(&attr);
        attr.ParentObject = Device; // auto-delete when "Device" is deleted

        // allocate "preparsedData"
        NTSTATUS status = WdfMemoryCreate(&attr, NonPagedPoolNx, POOL_TAG, collectionInfo.DescriptorSize, &context->Hid.Preparsed, nullptr);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfMemoryCreate failed 0x%x"), status);
            return status;
        }

        WDF_MEMORY_DESCRIPTOR outputDesc = {};
        WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(&outputDesc, context->Hid.Preparsed, NULL);

        // populate "preparsedData"
        status = WdfIoTargetSendIoctlSynchronously(pdoTarget, NULL,
            IOCTL_HID_GET_COLLECTION_DESCRIPTOR, // same as HidD_GetPreparsedData in user-mode
            NULL, // input
            &outputDesc, // output
            NULL, NULL);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: IOCTL_HID_GET_COLLECTION_DESCRIPTOR failed 0x%x"), status);
            return status;
        }
    }

    UCHAR TemperatureReportID = 0;
    UCHAR CycleCountReportID = 0;
    {
        // get capabilities
        HIDP_CAPS caps = {};
        NTSTATUS status = HidP_GetCaps(context->Hid.GetPreparsedData(), &caps);
        if (!NT_SUCCESS(status)) {
            return status;
        }

        context->Hid.InputReportByteLength = caps.InputReportByteLength;
        context->Hid.FeatureReportByteLength = caps.FeatureReportByteLength;

        DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: Usage=%x, UsagePage=%x, InputReportByteLength=%u, FeatureReportByteLength=%u\n", caps.Usage, caps.UsagePage, caps.InputReportByteLength, caps.FeatureReportByteLength);

        // get FEATURE report value caps
        USHORT valueCapsLen = caps.NumberFeatureValueCaps;
        RamArray<HIDP_VALUE_CAPS> valueCaps(valueCapsLen);
        if (!valueCaps) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: HIDP_VALUE_CAPS[%u] allocation failure."), valueCapsLen);
            return status;
        }
        status = HidP_GetValueCaps(HidP_Feature, valueCaps, &valueCapsLen, context->Hid.GetPreparsedData());
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: HidP_GetValueCaps failed 0x%x"), status);
            return status;
        }

        // identify UsagePage & Usage code for all HID reports
        for (USHORT i = 0; i < valueCapsLen; i++) {
            HidCode& code = context->Hid.reports[valueCaps[i].ReportID];
            code.UsagePage = valueCaps[i].UsagePage;
            code.Usage = valueCaps[i].NotRange.Usage;

            if (code == Temperature_Code)
                TemperatureReportID = valueCaps[i].ReportID;
            else if (code == CycleCount_Code)
                CycleCountReportID = valueCaps[i].ReportID;
        }
    }

    {
        // query FEATURE reports
        NTSTATUS status = GetFeatureReport(pdoTarget, TemperatureReportID);
        if (!NT_SUCCESS(status))
            return status;

        status = GetFeatureReport(pdoTarget, CycleCountReportID);
        if (!NT_SUCCESS(status))
            return status;
    }

    // flag HidConfig struct as initialized
    InterlockedIncrement(&context->Hid.Initialized);

    return STATUS_SUCCESS;
}


void ParseReadHidBuffer(WDFDEVICE Device, _In_ WDFREQUEST Request) {
    DEVICE_CONTEXT* context = WdfObjectGet_DEVICE_CONTEXT(Device);

    if (!context->Hid.Initialized) {
        //DebugPrint(DPFLTR_WARNING_LEVEL, "HidBattExt: HidConfig not yet initialized\n");
        return;
    }

    CHAR* report = nullptr;
    size_t Length = 0;
    NTSTATUS status = WdfRequestRetrieveOutputBuffer(Request, 0, (void**)&report, &Length);
    if (!NT_SUCCESS(status) || !report) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfRequestRetrieveOutputBuffer failed 0x%x, report=0x%p"), status, report);
        return;
    }

    if (Length < context->Hid.InputReportByteLength) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: EvtIoReadFilter: Too small output buffer"));
        return;
    }

    UpdateBatteryState(context->State, HidP_Input, report, context->Hid);
}


/** Read request completion routine for parsing the output buffer written by the HW. */
void EvtIoReadHidFilterCompletion(_In_  WDFREQUEST Request, _In_  WDFIOTARGET Target, _In_  WDF_REQUEST_COMPLETION_PARAMS* Params, _In_  WDFCONTEXT Context) {
    UNREFERENCED_PARAMETER(Params); // invalidated by WdfRequestFormatRequestUsingCurrentType
    UNREFERENCED_PARAMETER(Context);

    if (!NT_SUCCESS(WdfRequestGetStatus(Request))) {
        //DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: EvtIoReadHidFilterCompletion status=0x%x"), WdfRequestGetStatus(Request));
        WdfRequestComplete(Request, WdfRequestGetStatus(Request));
        return;
    }

    WDFDEVICE device = WdfIoTargetGetDevice(Target);
    ParseReadHidBuffer(device, Request);

    WdfRequestComplete(Request, STATUS_SUCCESS);
}


_Function_class_(EVT_WDF_IO_QUEUE_IO_READ)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID EvtIoReadHidFilter(_In_ WDFQUEUE Queue, _In_ WDFREQUEST Request, _In_ size_t Length) {
    UNREFERENCED_PARAMETER(Length);
    //DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: EvtIoReadFilter (Length=%Iu)\n", Length);

    WDFDEVICE device = WdfIoQueueGetDevice(Queue);

    // Formating required if specifying a completion routine
    WdfRequestFormatRequestUsingCurrentType(Request);
    // set completion callback
    WdfRequestSetCompletionRoutine(Request, EvtIoReadHidFilterCompletion, nullptr);

    BOOLEAN ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(device), WDF_NO_SEND_OPTIONS);
    if (ret == FALSE) {
        NTSTATUS status = WdfRequestGetStatus(Request);
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfRequestSend failed with status: 0x%x"), status);
        WdfRequestComplete(Request, status);
    }
}

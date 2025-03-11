#include "device.hpp"
#include "HidPd.hpp"
#include <hidclass.h> // for HID_COLLECTION_INFORMATION
#include "CppAllocator.hpp"


static void UpdateSharedState(BatteryState& state, HIDP_REPORT_TYPE reportType, CHAR* report, const HidConfig& hid) {
    USHORT reportLen = 0;
    if (reportType == HidP_Input)
        reportLen = hid.InputReportByteLength;
    else if (reportType == HidP_Feature)
        reportLen = hid.FeatureReportByteLength;
    else
        NT_ASSERTMSG("UpdateSharedState invalid reportType", false);

    CHAR reportId = report[0];

    // capture shared state
    if (hid.CycleCountReportID && (reportId == hid.CycleCountReportID)) {
        ULONG value = 0;
        NTSTATUS status = HidP_GetUsageValue(reportType, CycleCount_UsagePage, /*default link collection*/0, CycleCount_Usage, &value, hid.GetPreparsedData(), report, reportLen);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: HidP_GetUsageValue failed 0x%x"), status);
            return;
        }

        auto CycleCountBefore = state.CycleCount;

        WdfSpinLockAcquire(state.Lock);
        state.CycleCount = value;
        WdfSpinLockRelease(state.Lock);

        if (state.CycleCount != CycleCountBefore) {
            DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: Updating HID CycleCount before=%u, after=%u\n", CycleCountBefore, state.CycleCount);
        }
    } else if (hid.TemperatureReportID && (reportId == hid.TemperatureReportID)) {
        ULONG value = 0;
        NTSTATUS status = HidP_GetUsageValue(reportType, Temperature_UsagePage, /*default link collection*/0, Temperature_Usage, &value, hid.GetPreparsedData(), report, reportLen);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: HidP_GetUsageValue failed 0x%x"), status);
            return;
        }

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
NTSTATUS GetFeatureReport(WDFIOTARGET target, RamArray<CHAR>& report) {
    WDF_MEMORY_DESCRIPTOR outputDesc = {};
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDesc, report, report.ByteSize());

    return WdfIoTargetSendIoctlSynchronously(target, NULL,
        IOCTL_HID_GET_FEATURE,
        NULL, // input
        &outputDesc, // output
        NULL, NULL);
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
        openParams.ShareAccess = FILE_SHARE_WRITE | FILE_SHARE_READ;

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

        // identify ReportID codes for Temperature and CycleCount
        for (USHORT i = 0; i < valueCapsLen; i++) {
            if ((valueCaps[i].UsagePage == Temperature_UsagePage) && (valueCaps[i].NotRange.Usage == Temperature_Usage)) {
                context->Hid.TemperatureReportID = valueCaps[i].ReportID;
                DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: Temperature ReportID is 0x%x\n", valueCaps[i].ReportID);

            }
            if ((valueCaps[i].UsagePage == CycleCount_UsagePage) && (valueCaps[i].NotRange.Usage == CycleCount_Usage)) {
                context->Hid.CycleCountReportID = valueCaps[i].ReportID;
                DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: CycleCount ReportID is 0x%x\n", valueCaps[i].ReportID);
            }
        }
    }

    if (context->Hid.TemperatureReportID) {
        // Battery Temperature query
        RamArray<CHAR> report(context->Hid.FeatureReportByteLength);
        report[0] = context->Hid.TemperatureReportID;

        NTSTATUS status = GetFeatureReport(pdoTarget, report);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: IOCTL_HID_GET_FEATURE failed 0x%x"), status);
            return status;
        }

        UpdateSharedState(context->LowState, HidP_Feature, report, context->Hid);
    }
    if (context->Hid.CycleCountReportID) {
        // Battery CycleCount query
        RamArray<CHAR> report(context->Hid.FeatureReportByteLength);
        report[0] = context->Hid.CycleCountReportID;

        NTSTATUS status = GetFeatureReport(pdoTarget, report);
        if (!NT_SUCCESS(status)) {
            // IOCTL_HID_SET_FEATURE fails with 0xc0000061 (STATUS_PRIVILEGE_NOT_HELD) if using the local IO target (WdfDeviceGetIoTarget)
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: IOCTL_HID_GET_FEATURE failed 0x%x"), status);
            return status;
        }

        UpdateSharedState(context->LowState, HidP_Feature, report, context->Hid);
    }

    // flag HidConfig struct as initialized
    InterlockedIncrement(&context->Hid.Initialized);

    return STATUS_SUCCESS;
}


void ParseReadHidBuffer(WDFDEVICE Device, _In_ WDFREQUEST Request, _In_ size_t Length) {
    DEVICE_CONTEXT* context = WdfObjectGet_DEVICE_CONTEXT(Device);

    if (!context->Hid.Initialized) {
        //DebugPrint(DPFLTR_WARNING_LEVEL, "HidBattExt: HidConfig not yet initialized\n");
        return;
    }

    if (Length != context->Hid.InputReportByteLength) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: EvtIoReadFilter: Incorrect Length"));
        return;
    }

    CHAR* report = nullptr;
    NTSTATUS status = WdfRequestRetrieveOutputBuffer(Request, Length, (void**)&report, NULL);
    if (!NT_SUCCESS(status) || !report) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfRequestRetrieveOutputBuffer failed 0x%x, report=0x%p"), status, report);
        return;
    }

    UpdateSharedState(context->LowState, HidP_Input, report, context->Hid);
}


_Function_class_(EVT_WDF_IO_QUEUE_IO_READ)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID EvtIoReadHidFilter(_In_ WDFQUEUE Queue, _In_ WDFREQUEST Request, _In_ size_t Length) {
    //DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: EvtIoReadFilter (Length=%Iu)\n", Length);

    WDFDEVICE device = WdfIoQueueGetDevice(Queue);

    ParseReadHidBuffer(device, Request, Length);

    // Forward the request down the driver stack
    WDF_REQUEST_SEND_OPTIONS options = {};
    WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

    BOOLEAN ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(device), &options);
    if (ret == FALSE) {
        NTSTATUS status = WdfRequestGetStatus(Request);
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfRequestSend failed with status: 0x%x"), status);
        WdfRequestComplete(Request, status);
    }
}

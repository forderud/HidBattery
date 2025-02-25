#include "driver.h"
#include <hidclass.h> // for HID_COLLECTION_INFORMATION
#include "CppAllocator.hpp"


static void UpdateSharedState(SharedState& state, HidPdReport& report) {
    // capture shared state
    if (report.ReportId == HidPdReport::CycleCount) {
        auto CycleCountBefore = state.CycleCount;

        WdfSpinLockAcquire(state.Lock);
        state.CycleCount = report.Value;
        WdfSpinLockRelease(state.Lock);

        if (state.CycleCount != CycleCountBefore) {
            DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: Updating CycleCount before=%u, after=%u\n", CycleCountBefore, state.CycleCount);
        }
    } else if (report.ReportId == HidPdReport::Temperature) {
        auto TempBefore = state.Temperature;

        WdfSpinLockAcquire(state.Lock);
        state.Temperature = report.Value;
        WdfSpinLockRelease(state.Lock);

        if (state.Temperature != TempBefore) {
            DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: Updating Temperature before=%u, after=%u\n", TempBefore, state.Temperature);
        }
    }
}


NTSTATUS HidPdFeatureRequest(_In_ WDFDEVICE Device) {
    DebugEnter();

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

        //DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: ProductID=%x, VendorID=%x, VersionNumber=%u, DescriptorSize=%u\n", collectionInfo.ProductID, collectionInfo.VendorID, collectionInfo.VersionNumber, collectionInfo.DescriptorSize);
    }

    PHIDP_PREPARSED_DATA_Wrap preparsedData(collectionInfo.DescriptorSize);
    if (!preparsedData) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    {
        // populate "preparsedData"
        WDF_MEMORY_DESCRIPTOR outputDesc = {};
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDesc, static_cast<PHIDP_PREPARSED_DATA>(preparsedData), collectionInfo.DescriptorSize);

        NTSTATUS status = WdfIoTargetSendIoctlSynchronously(pdoTarget, NULL,
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
        NTSTATUS status = HidP_GetCaps(preparsedData, &caps);
        if (!NT_SUCCESS(status)) {
            return status;
        }

        //DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: Usage=%x, UsagePage=%x\n", caps.Usage, caps.UsagePage);

        if (caps.FeatureReportByteLength != sizeof(HidPdReport)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: FeatureReportByteLength mismatch (%u, %Iu)."), caps.FeatureReportByteLength, sizeof(HidPdReport));
            return status;
        }

        // get FEATURE report value caps
        USHORT valueCapsLen = caps.NumberFeatureValueCaps;
        HIDP_VALUE_CAPS* valueCaps = new HIDP_VALUE_CAPS[valueCapsLen];
        status = HidP_GetValueCaps(HidP_Feature, valueCaps, &valueCapsLen, preparsedData);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: HidP_GetValueCaps failed 0x%x"), status);
            return status;
        }

        for (USHORT i = 0; i < valueCapsLen; i++) {
            if ((valueCaps[i].UsagePage == 0x84) && (valueCaps[i].NotRange.Usage == 0x36)) {
                context->TemperatureReportID = valueCaps[i].ReportID;
                DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: Temperature ReportID=0x%x\n", valueCaps[i].ReportID);

            }
            if ((valueCaps[i].UsagePage == 0x85) && (valueCaps[i].NotRange.Usage == 0x6B)) {
                context->CycleCountReportID = valueCaps[i].ReportID;
                DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: CycleCount ReportID=0x%x\n", valueCaps[i].ReportID);
            }
        }

        delete[] valueCaps;
    }

    {
        // Battery Temperature query
        HidPdReport report(HidPdReport::Temperature);

        WDF_MEMORY_DESCRIPTOR outputDesc = {};
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDesc, &report, sizeof(report));

        NTSTATUS status = WdfIoTargetSendIoctlSynchronously(pdoTarget, NULL,
            IOCTL_HID_GET_FEATURE,
            NULL, // input
            &outputDesc, // output
            NULL, NULL);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: IOCTL_HID_GET_FEATURE failed 0x%x"), status);
            return status;
        }

        UpdateSharedState(context->LowState, report);
    }
    {
        // Battery CycleCount query
        HidPdReport report(HidPdReport::CycleCount);

        WDF_MEMORY_DESCRIPTOR outputDesc = {};
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDesc, &report, sizeof(report));

        NTSTATUS status = WdfIoTargetSendIoctlSynchronously(pdoTarget, NULL,
            IOCTL_HID_GET_FEATURE,
            NULL, // input
            &outputDesc, // output
            NULL, NULL);
        if (!NT_SUCCESS(status)) {
            // IOCTL_HID_SET_FEATURE fails with 0xc0000061 (STATUS_PRIVILEGE_NOT_HELD) if using the local IO target (WdfDeviceGetIoTarget)
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: IOCTL_HID_GET_FEATURE failed 0x%x"), status);
            return status;
        }

        UpdateSharedState(context->LowState, report);
    }

    DebugExit();
    return STATUS_SUCCESS;
}


_Function_class_(EVT_WDF_TIMER)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID HidPdFeatureRequestTimer(_In_ WDFTIMER  Timer) {
    DebugEnter();

    WDFDEVICE Device = (WDFDEVICE)WdfTimerGetParentObject(Timer);
    NT_ASSERTMSG("HidPdFeatureRequest Device NULL\n", Device);

    NTSTATUS status = HidPdFeatureRequest(Device);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: HidPdFeatureRequest failure 0x%x"), status);
        return;
    }

    DebugExit();
}

#ifdef HID_IOCTL_FILTER
void EvtIoDeviceControlHidFilterCompletion(_In_  WDFREQUEST Request, _In_  WDFIOTARGET Target, _In_  WDF_REQUEST_COMPLETION_PARAMS* Params, _In_  WDFCONTEXT Context) {
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Context);

    auto* Ioctl = (IoctlBuffers*)Context;

    NTSTATUS status = Params->IoStatus.Status;
    if (!NT_SUCCESS(status)) {
        // status 0xc0000002 (STATUS_NOT_IMPLEMENTED)
        // status 0xc00002b6 (STATUS_DEVICE_REMOVED)
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("ERROR: EvtIoDeviceControlHidFilterCompletion: IOCTL=0x%x, status=0x%x\n"), Ioctl->IoControlCode, status);
        WdfRequestComplete(Request, status);
        return;
    }

    if (Ioctl->IoControlCode != IOCTL_HID_GET_FEATURE) {
        //DebugPrint(DPFLTR_INFO_LEVEL, "EvtIoDeviceControlHidFilterCompletion: Unsupported IOCTL code 0x%x\n", Ioctl->IoControlCode);
        WdfRequestComplete(Request, status);
        return;
    }

    WDFDEVICE Device = WdfIoTargetGetDevice(Target);
    DEVICE_CONTEXT* context = WdfObjectGet_DEVICE_CONTEXT(Device);

    //DebugPrint(DPFLTR_INFO_LEVEL, "EvtIoDeviceControlHidFilterCompletion: IOCTL_HID_GET_FEATURE (OutputBufferLength=%Iu)\n", Ioctl->OutputBufferLength);
    if (Ioctl->OutputBufferLength == sizeof(HidPdReport)) {
        auto* report = (HidPdReport*)Ioctl->OutputBuffer;
        UpdateSharedState(context->LowState, *report);
    }

    WdfRequestComplete(Request, status);
}


_Function_class_(EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID EvtIoDeviceControlHidFilter(
    _In_  WDFQUEUE          Queue,
    _In_  WDFREQUEST        Request,
    _In_  size_t            OutputBufferLength,
    _In_  size_t            InputBufferLength,
    _In_  ULONG             IoControlCode) {
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);

    //DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: EvtIoDeviceControlHidFilter (IoControlCode=0x%x, InputBufferLength=%Iu, OutputBufferLength=%Iu)\n", IoControlCode, InputBufferLength, OutputBufferLength);

    WDFDEVICE Device = WdfIoQueueGetDevice(Queue);
    DEVICE_CONTEXT* context = WdfObjectGet_DEVICE_CONTEXT(Device);

    // update completion context with IOCTL buffer information
    if (IoControlCode == IOCTL_HID_GET_FEATURE)
        context->HidIoctl.Update(IoControlCode, 0, Request);
    else
        context->HidIoctl.Update(IoControlCode, 0, nullptr);

    // Formating required if specifying a completion routine
    WdfRequestFormatRequestUsingCurrentType(Request);
    // set completion callback
    WdfRequestSetCompletionRoutine(Request, EvtIoDeviceControlHidFilterCompletion, &context->HidIoctl);

    // Forward the request down the driver stack
    BOOLEAN ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(Device), WDF_NO_SEND_OPTIONS);
    if (ret == FALSE) {
        NTSTATUS status = WdfRequestGetStatus(Request);
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfRequestSend failed with status: 0x%x"), status);
        WdfRequestComplete(Request, status);
    }
}
#endif

void ParseReadHidBuffer(WDFDEVICE Device, _In_ WDFREQUEST Request, _In_ size_t Length) {
    if (Length != sizeof(HidPdReport)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: EvtIoReadFilter: Incorrect Length"));
        return;
    }

    HidPdReport* packet = nullptr;
    NTSTATUS status = WdfRequestRetrieveOutputBuffer(Request, sizeof(HidPdReport), (void**)&packet, NULL);
    if (!NT_SUCCESS(status) || !packet) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfRequestRetrieveOutputBuffer failed 0x%x, packet=0x%p"), status, packet);
        return;
    }

    DEVICE_CONTEXT* context = WdfObjectGet_DEVICE_CONTEXT(Device);
    UpdateSharedState(context->LowState, *packet);
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

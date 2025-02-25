#include "Battery.hpp"
#include "driver.h"


static void UpdateBatteryInformation(BATTERY_INFORMATION& bi, SharedState& state) {
    auto CycleCountBefore = bi.CycleCount;

    WdfSpinLockAcquire(state.Lock);
    bi.CycleCount = state.CycleCount;
    WdfSpinLockRelease(state.Lock);

    DebugPrint(DPFLTR_INFO_LEVEL, "EvtIoDeviceControlBattFilterCompletion: UpdateBatteryInformation CycleCount before=%u, after=%u\n", CycleCountBefore, bi.CycleCount); CycleCountBefore;
}

static void UpdateBatteryTemperature(ULONG& temp, SharedState& state) {
    auto TempBefore = temp;

    WdfSpinLockAcquire(state.Lock);
    temp = state.Temperature;
    WdfSpinLockRelease(state.Lock);

    DebugPrint(DPFLTR_INFO_LEVEL, "EvtIoDeviceControlBattFilterCompletion: UpdateBatteryTemperature before=%u, after=%u\n", TempBefore, temp); TempBefore;
}


void EvtIoDeviceControlBattFilterCompletion (_In_  WDFREQUEST Request, _In_  WDFIOTARGET Target, _In_  WDF_REQUEST_COMPLETION_PARAMS* Params, _In_  WDFCONTEXT Context) {
    UNREFERENCED_PARAMETER(Target);
    UNREFERENCED_PARAMETER(Context);

    //DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: EvtIoDeviceControlBattFilterCompletion\n");
    auto* Ioctl = (IoctlBuffers*)Context;

    NTSTATUS status = Params->IoStatus.Status;
    if (!NT_SUCCESS(status)) {
        // status 0xc0000002 (STATUS_NOT_IMPLEMENTED)
        // status 0xc00002b6 (STATUS_DEVICE_REMOVED)
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("ERROR: EvtIoDeviceControlBattFilterCompletion: status=0x%x\n"), status);
        WdfRequestComplete(Request, status);
        return;
    }

    if (Ioctl->IoControlCode != IOCTL_BATTERY_QUERY_INFORMATION) {
        DebugPrint(DPFLTR_INFO_LEVEL,"EvtIoDeviceControlBattFilterCompletion: Unsupported IOCTL code 0x%x\n", Ioctl->IoControlCode);
        WdfRequestComplete(Request, status);
        return;
    }

    WDFDEVICE Device = WdfIoTargetGetDevice(Target);
    DEVICE_CONTEXT* context = WdfObjectGet_DEVICE_CONTEXT(Device);

    DebugPrint(DPFLTR_INFO_LEVEL, "EvtIoDeviceControlBattFilterCompletion: IOCTL_BATTERY_QUERY_INFORMATION (InformationLevel=%u, OutputBufferLength=%u)\n", Ioctl->InformationLevel, Ioctl->OutputBufferLength);


    if ((Ioctl->InformationLevel == BatteryInformation) && (Ioctl->OutputBufferLength == sizeof(BATTERY_INFORMATION))) {
        auto* bi = (BATTERY_INFORMATION*)Ioctl->OutputBuffer;
        UpdateBatteryInformation(*bi, *context->Interface.State);
    } else if ((Ioctl->InformationLevel == BatteryTemperature) && (Ioctl->OutputBufferLength == sizeof(ULONG))) {
        auto* temp = (ULONG*)Ioctl->OutputBuffer;
        UpdateBatteryTemperature(*temp, *context->Interface.State);
    }

    WdfRequestComplete(Request, status);
}


_Function_class_(EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID EvtIoDeviceControlBattFilter(
    _In_  WDFQUEUE          Queue,
    _In_  WDFREQUEST        Request,
    _In_  size_t            OutputBufferLength,
    _In_  size_t            InputBufferLength,
    _In_  ULONG             IoControlCode) {
#if 0
    if (IoControlCode == IOCTL_BATTERY_QUERY_INFORMATION)
        DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: EvtIoDeviceControlBattFilter (IoControlCode=IOCTL_BATTERY_QUERY_INFORMATION, InputBufferLength=%Iu, OutputBufferLength=%Iu)\n", InputBufferLength, OutputBufferLength);
    else
        DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: EvtIoDeviceControlBattFilter (IoControlCode=0x%x, InputBufferLength=%Iu, OutputBufferLength=%Iu)\n", IoControlCode, InputBufferLength, OutputBufferLength);
#else
    UNREFERENCED_PARAMETER(OutputBufferLength);
#endif

    WDFDEVICE Device = WdfIoQueueGetDevice(Queue);
    DEVICE_CONTEXT* context = WdfObjectGet_DEVICE_CONTEXT(Device);

    // update completion context with IOCTL buffer information
    if ((IoControlCode == IOCTL_BATTERY_QUERY_INFORMATION) && (InputBufferLength == sizeof(BATTERY_QUERY_INFORMATION))) {
        BATTERY_QUERY_INFORMATION* InputBuffer = nullptr;
        NTSTATUS status = WdfRequestRetrieveInputBuffer(Request, 0, (void**)&InputBuffer, nullptr);
        NT_ASSERTMSG("WdfRequestRetrieveInputBuffer failed", NT_SUCCESS(status)); status;

        context->BattIoctl.Update(IoControlCode, InputBuffer->InformationLevel, Request);
    } else {
        context->BattIoctl.Update(IoControlCode, 0, nullptr);
    }

    // Formating required if specifying a completion routine
    WdfRequestFormatRequestUsingCurrentType(Request);
    // set completion callback
    WdfRequestSetCompletionRoutine(Request, EvtIoDeviceControlBattFilterCompletion, &context->BattIoctl);

    // Forward the request down the driver stack
    BOOLEAN ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(Device), WDF_NO_SEND_OPTIONS);
    if (ret == FALSE) {
        NTSTATUS status = WdfRequestGetStatus(Request);
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfRequestSend failed with status: 0x%x"), status);
        WdfRequestComplete(Request, status);
    }
}

#include "Battery.hpp"
#include "driver.h"


static void UpdateBatteryInformation(BATTERY_INFORMATION& bi, SharedState& state) {
    auto CycleCountBefore = bi.CycleCount;

    auto lock = state.Lock();
    bi.CycleCount = state.CycleCount;
    DebugPrint(DPFLTR_INFO_LEVEL, "EvtIoDeviceControlBattFilterCompletion: UpdateBatteryInformation CycleCount before=%u, after=%u\n", CycleCountBefore, bi.CycleCount);
}

static void UpdateBatteryTemperature(ULONG& temp, SharedState& state) {
    auto TempBefore = temp;

    auto lock = state.Lock();
    temp = state.Temperature;

    DebugPrint(DPFLTR_INFO_LEVEL, "EvtIoDeviceControlBattFilterCompletion: UpdateBatteryTemperature before=%u, after=%u\n", TempBefore, temp);
}


void EvtIoDeviceControlBattFilterCompletion (_In_  WDFREQUEST Request, _In_  WDFIOTARGET Target, _In_  PWDF_REQUEST_COMPLETION_PARAMS Params, _In_  WDFCONTEXT Context) {
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

VOID EvtIoDeviceControlBattFilter(
    _In_  WDFQUEUE          Queue,
    _In_  WDFREQUEST        Request,
    _In_  size_t            OutputBufferLength,
    _In_  size_t            InputBufferLength,
    _In_  ULONG             IoControlCode
)
/*++
Routine Description:
    Callback function for IOCTL_BATTERY_xxx requests.

Arguments:
    Queue - A handle to the queue object that is associated with the I/O request

    Request - A handle to a framework request object.

    OutputBufferLength - The length, in bytes, of the request's output buffer,
            if an output buffer is available.

    InputBufferLength - The length, in bytes, of the request's input buffer, if
            an input buffer is available.

    IoControlCode - The driver or system defined IOCTL associated with the request
--*/
{
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

    if ((IoControlCode == IOCTL_BATTERY_QUERY_INFORMATION) && (InputBufferLength == sizeof(BATTERY_QUERY_INFORMATION))) {
        BATTERY_QUERY_INFORMATION* InputBuffer = nullptr;
        NTSTATUS status = WdfRequestRetrieveInputBuffer(Request, 0, (void**)&InputBuffer, nullptr);
        NT_ASSERTMSG("WdfRequestRetrieveInputBuffer failed", NT_SUCCESS(status));

        context->BattIoctl.Update(IoControlCode, InputBuffer->InformationLevel, Request);
    } else {
        context->BattIoctl.Update(IoControlCode, 0, nullptr);
    }


#if 1
    // Formating required if specifying a completion routine
    WdfRequestFormatRequestUsingCurrentType(Request);
    // set completion callback
    WdfRequestSetCompletionRoutine(Request, EvtIoDeviceControlBattFilterCompletion, &context->BattIoctl);

    // Forward the request down the driver stack
    BOOLEAN ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(Device), WDF_NO_SEND_OPTIONS);
#else
    WDF_REQUEST_SEND_OPTIONS options = {};
    WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);
    BOOLEAN ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(Device), &options);
#endif
    if (ret == FALSE) {
        NTSTATUS status = WdfRequestGetStatus(Request);
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfRequestSend failed with status: 0x%x"), status);
        WdfRequestComplete(Request, status);
    }
}

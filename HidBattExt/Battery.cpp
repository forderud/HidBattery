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
    // only the IoStatus field is valid in the Params argument

    REQUEST_CONTEXT* reqCtx = WdfObjectGet_REQUEST_CONTEXT(Request);

    NTSTATUS status = Params->IoStatus.Status;
    if (!NT_SUCCESS(status)) {
        // Observed error codes:
        //   0xc0000002 (STATUS_NOT_IMPLEMENTED)
        //   0xc0000005 (STATUS_ACCESS_VIOLATION)
        //   0xc0000120 (STATUS_CANCELLED)
        //   0xc00002b6 (STATUS_DEVICE_REMOVED)
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("ERROR: EvtIoDeviceControlBattFilterCompletion: status=0x%x\n"), status);
        WdfRequestComplete(Request, status);
        return;
    }

    if (reqCtx->IoControlCode != IOCTL_BATTERY_QUERY_INFORMATION) {
        switch (reqCtx->IoControlCode) {
        case IOCTL_BATTERY_QUERY_TAG:
        case IOCTL_BATTERY_QUERY_INFORMATION:
        case IOCTL_BATTERY_SET_INFORMATION:
        case IOCTL_BATTERY_QUERY_STATUS:
        case IOCTL_BATTERY_CHARGING_SOURCE_CHANGE:
        case CTL_CODE(FILE_DEVICE_BATTERY,  0x19, METHOD_BUFFERED,  FILE_READ_ACCESS):// TODO: Figure out what 0x294064 is used for
        case CTL_CODE(FILE_DEVICE_KEYBOARD, 0x64, METHOD_OUT_DIRECT,FILE_ANY_ACCESS): // TODO: Figure out what 0x0b0192 is used for
        case CTL_CODE(FILE_DEVICE_KEYBOARD, 0x64, METHOD_NEITHER,   FILE_ANY_ACCESS): // TODO: Figure out what 0x0b0193 is used for
        case CTL_CODE(FILE_DEVICE_KEYBOARD, 0x6a, METHOD_BUFFERED,  FILE_ANY_ACCESS): // TODO: Figure out what 0x0b01a8 is used for
            break; // ignore known codes
        default:
            //DebugPrint(DPFLTR_INFO_LEVEL, "EvtIoDeviceControlBattFilterCompletion: Unknown IOCTL code 0x%x\n", reqCtx->IoControlCode);
            break;
        }
        WdfRequestComplete(Request, status);
        return;
    }

    WDFDEVICE Device = WdfIoTargetGetDevice(Target);
    DEVICE_CONTEXT* context = WdfObjectGet_DEVICE_CONTEXT(Device);

    DebugPrint(DPFLTR_INFO_LEVEL, "EvtIoDeviceControlBattFilterCompletion: IOCTL_BATTERY_QUERY_INFORMATION (InformationLevel=%u, OutputBufferLength=%u)\n", reqCtx->InformationLevel, reqCtx->OutputBufferLength);

    if ((reqCtx->InformationLevel == BatteryInformation) && (reqCtx->OutputBufferLength == sizeof(BATTERY_INFORMATION))) {
        auto* bi = (BATTERY_INFORMATION*)reqCtx->OutputBuffer;
        UpdateBatteryInformation(*bi, *context->Interface.State);
    } else if ((reqCtx->InformationLevel == BatteryTemperature) && (reqCtx->OutputBufferLength == sizeof(ULONG))) {
        auto* temp = (ULONG*)reqCtx->OutputBuffer;
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
    REQUEST_CONTEXT* reqCtx = WdfObjectGet_REQUEST_CONTEXT(Request);

    // update completion context with IOCTL buffer information
    if ((IoControlCode == IOCTL_BATTERY_QUERY_INFORMATION) && (InputBufferLength == sizeof(BATTERY_QUERY_INFORMATION))) {
        BATTERY_QUERY_INFORMATION* InputBuffer = nullptr;
        NTSTATUS status = WdfRequestRetrieveInputBuffer(Request, 0, (void**)&InputBuffer, nullptr);
        NT_ASSERTMSG("WdfRequestRetrieveInputBuffer failed", NT_SUCCESS(status)); status;

        reqCtx->Update(IoControlCode, InputBuffer->InformationLevel, Request);
    } else {
        reqCtx->Update(IoControlCode, 0, nullptr);
    }

    // Formating required if specifying a completion routine
    WdfRequestFormatRequestUsingCurrentType(Request);
    // set completion callback
    WdfRequestSetCompletionRoutine(Request, EvtIoDeviceControlBattFilterCompletion, nullptr);

    // Forward the request down the driver stack
    BOOLEAN ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(Device), WDF_NO_SEND_OPTIONS);
    if (ret == FALSE) {
        NTSTATUS status = WdfRequestGetStatus(Request);
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfRequestSend failed with status: 0x%x"), status);
        WdfRequestComplete(Request, status);
    }
}

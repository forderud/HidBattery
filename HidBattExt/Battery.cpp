#include "Battery.hpp"
#include "driver.h"


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
    //DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: EvtIoDeviceControl (IoControlCode=0x%x, InputBufferLength=%Iu, OutputBufferLength=%Iu)\n", IoControlCode, InputBufferLength, OutputBufferLength);

    WDFDEVICE Device = WdfIoQueueGetDevice(Queue);

    NTSTATUS status = STATUS_SUCCESS; //unhandled
    switch (IoControlCode) {
    case IOCTL_BATTERY_QUERY_INFORMATION:
        DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: IOCTL_BATTERY_QUERY_INFORMATION (InputBufferLength=%Iu, OutputBufferLength=%Iu)\n", InputBufferLength, OutputBufferLength);
        // TODO
        break;
    case IOCTL_BATTERY_QUERY_STATUS:
        DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: IOCTL_BATTERY_QUERY_STATUS (InputBufferLength=%Iu, OutputBufferLength=%Iu)\n", InputBufferLength, OutputBufferLength);
        // TODO
        break;
    }
    // No NT_SUCCESS(status) check here since we don't want to fail blocked calls

    // Forward the request down the driver stack
    WDF_REQUEST_SEND_OPTIONS options = {};
    WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

    BOOLEAN ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(Device), &options);
    if (ret == FALSE) {
        status = WdfRequestGetStatus(Request);
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfRequestSend failed with status: 0x%x"), status);
        WdfRequestComplete(Request, status);
    }
}

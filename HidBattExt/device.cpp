#include "driver.h"
#include <Hidport.h>

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtIoDeviceControlFilter;


NTSTATUS
FireFlyEvtDeviceAdd(
    WDFDRIVER Driver,
    PWDFDEVICE_INIT DeviceInit
    )
/*++
Routine Description:
    EvtDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager. We create and initialize a device object to
    represent to be part of the device stack as a filter.

Arguments:
    Driver - Handle to a framework driver object created in DriverEntry

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.
--*/    
{
    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    // Configure the device as a filter driver
    WdfFdoInitSetFilter(DeviceInit);

    WDF_OBJECT_ATTRIBUTES attributes = {};
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_CONTEXT);

    WDFDEVICE device = 0;
    NTSTATUS status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfDeviceCreate, Error %x\n", status));
        return status;
    }

    status = QueueCreate(device);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Driver Framework always zero initializes an objects context memory
    DEVICE_CONTEXT* deviceContext = WdfObjectGet_DEVICE_CONTEXT(device);

    // Initialize our WMI support
    status = WmiInitialize(device, deviceContext);
    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: Error initializing WMI 0x%x\n", status));
        return status;
    }

    // In order to send ioctls to our PDO, we have open to open it
    // by name so that we have a valid filehandle (fileobject).
    // When we send ioctls using the IoTarget, framework automatically 
    // sets the filobject in the stack location.
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);

    // By parenting it to device, we don't have to worry about
    // deleting explicitly. It will be deleted along witht the device.
    attributes.ParentObject = device;

    WDFMEMORY memory = 0;
    status = WdfDeviceAllocAndQueryProperty(device,
                                    DevicePropertyPhysicalDeviceObjectName,
                                    NonPagedPoolNx,
                                    &attributes,
                                    &memory);

    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfDeviceAllocAndQueryProperty failed 0x%x\n", status));        
        return STATUS_UNSUCCESSFUL;
    }

    {
        // initialize pDeviceContext->PdoName based on memory
        size_t bufferLength = 0;
        deviceContext->PdoName.Buffer = (WCHAR*)WdfMemoryGetBuffer(memory, &bufferLength);
        if (deviceContext->PdoName.Buffer == NULL)
            return STATUS_UNSUCCESSFUL;

        deviceContext->PdoName.MaximumLength = (USHORT)bufferLength;
        deviceContext->PdoName.Length = (USHORT)bufferLength - sizeof(UNICODE_NULL);

        KdPrint(("TailLight: PdoName: %wZ\n", deviceContext->PdoName)); // outputs "\Device\00000083
    }

    return status;
}


NTSTATUS
QueueCreate(
    _In_  WDFDEVICE Device
)
/*++
Routine Description:
    This function creates a default, parallel I/O queue to proces IOCTLs
    from hidclass.sys.

Arguments:
    Device - Handle to a framework device object.
    Queue - Output pointer to a framework I/O queue handle, on success.
--*/
{
    KdPrint(("TailLight: QueueCreate\n"));

    WDF_IO_QUEUE_CONFIG queueConfig = {};
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);
    queueConfig.EvtIoDeviceControl = EvtIoDeviceControlFilter;

    WDFQUEUE queue = 0; // auto-deleted when parent is deleted
    NTSTATUS status = WdfIoQueueCreate(Device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);

    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfIoQueueCreate failed 0x%x\n", status));
        return status;
    }

    KdPrint(("TailLight: QueueCreate completed\n"));

    return status;
}


VOID
EvtIoDeviceControlFilter(
    _In_  WDFQUEUE          Queue,
    _In_  WDFREQUEST        Request,
    _In_  size_t            OutputBufferLength,
    _In_  size_t            InputBufferLength,
    _In_  ULONG             IoControlCode
)
/*++
Routine Description:
    This event callback function is called when the driver receives an

    (KMDF) IOCTL_HID_Xxx code when handlng IRP_MJ_INTERNAL_DEVICE_CONTROL
    (UMDF) IOCTL_HID_Xxx, IOCTL_UMDF_HID_Xxx when handling IRP_MJ_DEVICE_CONTROL

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
    PAGED_CODE();

    UNREFERENCED_PARAMETER(OutputBufferLength);

    KdPrint(("TailLight: EvtIoDeviceControl (IoControlCode=0x%x, InputBufferLength=%u)\n", IoControlCode, InputBufferLength));

    WDFDEVICE device = WdfIoQueueGetDevice(Queue);
    DEVICE_CONTEXT* deviceContext = WdfObjectGet_DEVICE_CONTEXT(device);
    UNREFERENCED_PARAMETER(deviceContext);

    NTSTATUS status = STATUS_SUCCESS; //unhandled
    switch (IoControlCode) {
      case IOCTL_HID_SET_FEATURE:
        status = SetFeatureFilter(Request);
        break;
    }
    // ignore status variable for now

    // Forward the request down the driver stack
    WDFIOTARGET Target = WdfDeviceGetIoTarget(device);

    WDF_REQUEST_SEND_OPTIONS options = {};
    WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

    BOOLEAN ret = WdfRequestSend(Request, Target, &options);
    if (ret == FALSE) {
        status = WdfRequestGetStatus(Request);
        KdPrint(("TailLight: WdfRequestSend failed: 0x%x\n", status));
        WdfRequestComplete(Request, status);
    }
}


NTSTATUS
SetFeatureFilter(
    _In_  WDFREQUEST        Request
)
/*++
Routine Description:
    Handles IOCTL_HID_SET_FEATURE for all the collection.
    For control collection (custom defined collection) it handles
    the user-defined control codes for sideband communication

Arguments:
    QueueContext - The object context associated with the queue

    Request - Pointer to Request Packet.
--*/
{
    WDF_REQUEST_PARAMETERS params = {};
    WDF_REQUEST_PARAMETERS_INIT(&params);
    WdfRequestGetParameters(Request, &params);

    KdPrint(("TailLight: SetFeatureFilter: Type=0x%x (DeviceControl=0xe), InputBufferLength=%u, \n", params.Type, params.Parameters.DeviceIoControl.InputBufferLength));

    if (params.Parameters.DeviceIoControl.InputBufferLength != sizeof(TailLightReport)) {
        KdPrint(("TailLight: SetFeatureFilter: Incorrect InputBufferLength\n"));
        return STATUS_BUFFER_TOO_SMALL;
    }

    TailLightReport* packet = nullptr;
    NTSTATUS status = WdfRequestRetrieveInputBuffer(Request, sizeof(TailLightReport), (void**)&packet, NULL);
    if (!NT_SUCCESS(status) || !packet) {
        KdPrint(("TailLight: WdfRequestRetrieveInputBuffer failed 0x%x, packet=0x%x\n", status, packet));
        return status;
    }

    if (!packet->IsValid()) {
        // If collection ID is not for control collection then handle
        // this request just as you would for a regular collection.
        return STATUS_INVALID_PARAMETER;
    }

    // Enforce safety limits (sets color to RED on failure)
    packet->SafetyCheck();

    return status;
}

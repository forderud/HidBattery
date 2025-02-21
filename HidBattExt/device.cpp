#include "driver.h"
#include <Hidport.h>

EVT_WDF_IO_QUEUE_IO_READ           EvtIoReadHidFilter;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtIoDeviceControlHidFilter;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtIoDeviceControlBattFilter;


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

NTSTATUS EvtSelfManagedIoInit(WDFDEVICE Device) {
    DEVICE_CONTEXT* context = WdfObjectGet_DEVICE_CONTEXT(Device);

    if (context->Mode == LowerFilter) {
        // schedule read of HID FEATURE reports
        WDF_TIMER_CONFIG timerCfg = {};
        WDF_TIMER_CONFIG_INIT(&timerCfg, HidPdFeatureRequestTimer);

        WDF_OBJECT_ATTRIBUTES attribs = {};
        WDF_OBJECT_ATTRIBUTES_INIT(&attribs);
        attribs.ParentObject = Device;
        attribs.ExecutionLevel = WdfExecutionLevelPassive; // required to access HID functions

        WDFTIMER timer = nullptr;
        NTSTATUS status = WdfTimerCreate(&timerCfg, &attribs, &timer);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("WdfTimerCreate failed 0x%x"), status);
            return status;
        }

        BOOLEAN inQueue = WdfTimerStart(timer, 0); // no wait
        NT_ASSERTMSG("HidBattExt: timer already in queue", !inQueue);
        UNREFERENCED_PARAMETER(inQueue);
    }

    return STATUS_SUCCESS;
}


UNICODE_STRING GetTargetPropertyString(WDFIOTARGET target, DEVICE_REGISTRY_PROPERTY DeviceProperty) {
    WDF_OBJECT_ATTRIBUTES attributes = {};
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = target; // auto-delete with I/O target

    WDFMEMORY memory = 0;
    NTSTATUS status = WdfIoTargetAllocAndQueryTargetProperty(target, DeviceProperty, NonPagedPoolNx, &attributes, &memory);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfIoTargetAllocAndQueryTargetProperty with property=0x%x failed 0x%x"), DeviceProperty, status);
        return {};
    }

    // initialize string based on memory
    size_t bufferLength = 0;
    UNICODE_STRING result = {};
    result.Buffer = (WCHAR*)WdfMemoryGetBuffer(memory, &bufferLength);
    if (result.Buffer == NULL)
        return {};

    result.MaximumLength = (USHORT)bufferLength;
    result.Length = (USHORT)bufferLength - sizeof(UNICODE_NULL);
    return result;
}


NTSTATUS EvtDriverDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit)
/*++
Routine Description:
    EvtDriverDeviceAdd is called by the framework in response to AddDevice
    call from the PnP manager. We create and initialize a device object to
    represent to be part of the device stack as a filter.

Arguments:
    Driver - Handle to a framework driver object created in DriverEntry

    DeviceInit - Pointer to a framework-allocated WDFDEVICE_INIT structure.
--*/
{
    UNREFERENCED_PARAMETER(Driver);

    // Configure the device as a filter driver
    WdfFdoInitSetFilter(DeviceInit);

    {
        // register PnP callbacks (must be done before WdfDeviceCreate)
        WDF_PNPPOWER_EVENT_CALLBACKS PnpPowerCallbacks;
        WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&PnpPowerCallbacks);
        PnpPowerCallbacks.EvtDeviceSelfManagedIoInit = EvtSelfManagedIoInit;
        WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &PnpPowerCallbacks);
    }

    WDFDEVICE Device = 0;
    {
        // create device
        WDF_OBJECT_ATTRIBUTES attributes = {};
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_CONTEXT);

        NTSTATUS status = WdfDeviceCreate(&DeviceInit, &attributes, &Device);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfDeviceCreate, Error %x"), status);
            return status;
        }

        DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: PDO(0x%p) FDO(0x%p), Lower(0x%p)\n", WdfDeviceWdmGetPhysicalDevice(Device), WdfDeviceWdmGetDeviceObject(Device), WdfDeviceWdmGetAttachedDevice(Device));
    }

    // Driver Framework always zero initializes an objects context memory
    DEVICE_CONTEXT* deviceContext = WdfObjectGet_DEVICE_CONTEXT(Device);

    {
        if (WdfDeviceWdmGetPhysicalDevice(Device) == WdfDeviceWdmGetAttachedDevice(Device)) {
            DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: Running as Lower filter driver below HidBatt\n");

            deviceContext->Mode = LowerFilter;

            deviceContext->LowState.Initialize(Device);

            NTSTATUS status = deviceContext->Interface.Register(Device, deviceContext->LowState);
            if (!NT_SUCCESS(status)) {
                DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfDeviceAddQueryInterface error %x"), status);
                return status;
            }
        } else {
            DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: Running as Upper filter driver above HidBatt\n");

            deviceContext->Mode = UpperFilter;

            NTSTATUS status = deviceContext->Interface.Lookup(Device);
            if (!NT_SUCCESS(status)) {
                DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfFdoQueryForInterface error %x"), status);
                return status;
            }
        }
    }

    {
        // initialize DEVICE_CONTEXT struct with PdoName
        deviceContext->PdoName = GetTargetPropertyString(WdfDeviceGetIoTarget(Device), DevicePropertyPhysicalDeviceObjectName);
        if (!deviceContext->PdoName.Buffer) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: PdoName query failed"));
            return STATUS_UNSUCCESSFUL;
        }

        DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: PdoName: %wZ\n", deviceContext->PdoName); // outputs "\Device\00000083"
    }

    {
        // create queue for filtering
        WDF_IO_QUEUE_CONFIG queueConfig = {};
        WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel); // don't synchronize
        if (deviceContext->Mode == LowerFilter) {
            // HID Power Device (PD) filtering
            queueConfig.EvtIoRead = EvtIoReadHidFilter; // filter read requests 
            //queueConfig.EvtIoWrite // pass-through write requests
            queueConfig.EvtIoDeviceControl = EvtIoDeviceControlHidFilter; // filter IOCTL requests
        } else if (deviceContext->Mode == UpperFilter) {
            // Battery device filtering
            queueConfig.EvtIoDeviceControl = EvtIoDeviceControlBattFilter; // filter IOCTL requests
        }
        WDFQUEUE queue = 0; // auto-deleted when parent is deleted
        NTSTATUS status = WdfIoQueueCreate(Device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);

        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfIoQueueCreate failed 0x%x"), status);
            return status;
        }
    }

    return STATUS_SUCCESS;
}


VOID EvtIoDeviceControlHidFilter(
    _In_  WDFQUEUE          Queue,
    _In_  WDFREQUEST        Request,
    _In_  size_t            OutputBufferLength,
    _In_  size_t            InputBufferLength,
    _In_  ULONG             IoControlCode
)
/*++
Routine Description:
    Callback function for IOCTL_HID_xxx requests.

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
    UNREFERENCED_PARAMETER(InputBufferLength);

    //DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: EvtIoDeviceControl (IoControlCode=0x%x, InputBufferLength=%Iu, OutputBufferLength=%Iu)\n", IoControlCode, InputBufferLength, OutputBufferLength);

    WDFDEVICE Device = WdfIoQueueGetDevice(Queue);

    NTSTATUS status = STATUS_SUCCESS; //unhandled
    switch (IoControlCode) {
      case IOCTL_HID_GET_FEATURE:
        status = HidGetFeatureFilter(Device, Request, OutputBufferLength);
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


void ParseReadHidBuffer(_In_ WDFREQUEST Request, _In_ size_t Length) {
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

    packet->Print("INPUT");
}

_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID EvtIoReadHidFilter(_In_ WDFQUEUE Queue, _In_ WDFREQUEST Request, _In_ size_t Length)
{
    DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: EvtIoReadFilter (Length=%Iu)\n", Length);

    WDFDEVICE device = WdfIoQueueGetDevice(Queue);

    ParseReadHidBuffer(Request, Length);
 
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

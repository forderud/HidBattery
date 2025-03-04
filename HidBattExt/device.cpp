#include "device.hpp"
#include <Hidport.h>
#include "Battery.hpp"
#include "HidPd.hpp"


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

    {
        // create queue for filtering HID Power Device requests now that HidState have been initialized
        WDF_IO_QUEUE_CONFIG queueConfig = {};
        WDF_IO_QUEUE_CONFIG_INIT(&queueConfig, WdfIoQueueDispatchParallel);
        queueConfig.EvtIoRead = EvtIoReadHidFilter; // filter read requests

        WDFQUEUE queue = 0; // auto-deleted when "Device" is deleted
        status = WdfIoQueueCreate(Device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfIoQueueCreate failed 0x%x"), status);
            return;
        }

        status = WdfDeviceConfigureRequestDispatching(Device, queue, WdfRequestTypeRead);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfDeviceConfigureRequestDispatching(queue, RequestTypeRead) failed 0x%x"), status);
            return;
        }
    }

    DebugExit();
}


_Function_class_(EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS EvtSelfManagedIoInit(WDFDEVICE Device) {
    DEVICE_CONTEXT* context = WdfObjectGet_DEVICE_CONTEXT(Device);

    if (context->Mode == LowerFilter) {
        // schedule read of HID FEATURE reports
        // cannot call HidPdFeatureRequestTimer immediately, since WdfIoTargetOpen of PDO will then fail with 0xc000000e (STATUS_NO_SUCH_DEVICE)
        WDF_TIMER_CONFIG timerCfg = {};
        WDF_TIMER_CONFIG_INIT(&timerCfg, HidPdFeatureRequestTimer);

        WDF_OBJECT_ATTRIBUTES attr = {};
        WDF_OBJECT_ATTRIBUTES_INIT(&attr);
        attr.ParentObject = Device;
        attr.ExecutionLevel = WdfExecutionLevelPassive; // required to access HID functions

        WDFTIMER timer = nullptr;
        NTSTATUS status = WdfTimerCreate(&timerCfg, &attr, &timer);
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


_Function_class_(EVT_WDF_DEVICE_SELF_MANAGED_IO_CLEANUP)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
void EvtSelfManagedIoCleanup(WDFDEVICE Device) {
    UNREFERENCED_PARAMETER(Device);

    DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: Device removed FDO(0x%p)\n", WdfDeviceWdmGetDeviceObject(Device));
}


UNICODE_STRING GetTargetPropertyString(WDFIOTARGET target, DEVICE_REGISTRY_PROPERTY DeviceProperty) {
    WDF_OBJECT_ATTRIBUTES attr = {};
    WDF_OBJECT_ATTRIBUTES_INIT(&attr);
    attr.ParentObject = target; // auto-delete with I/O target

    WDFMEMORY memory = 0;
    NTSTATUS status = WdfIoTargetAllocAndQueryTargetProperty(target, DeviceProperty, NonPagedPoolNx, &attr, &memory);
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


NTSTATUS EvtDriverDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit) {
    UNREFERENCED_PARAMETER(Driver);

    // Configure the device as a filter driver
    WdfFdoInitSetFilter(DeviceInit);

    {
        // register PnP callbacks (must be done before WdfDeviceCreate)
        WDF_PNPPOWER_EVENT_CALLBACKS PnpPowerCallbacks;
        WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&PnpPowerCallbacks);
        PnpPowerCallbacks.EvtDeviceSelfManagedIoInit = EvtSelfManagedIoInit;
        PnpPowerCallbacks.EvtDeviceSelfManagedIoCleanup = EvtSelfManagedIoCleanup;
        WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &PnpPowerCallbacks);
    }

    {
        // reserve context space for request objects
        WDF_OBJECT_ATTRIBUTES attr{};
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr, REQUEST_CONTEXT);

        WdfDeviceInitSetRequestAttributes(DeviceInit, &attr);
    }


    WDFDEVICE Device = 0;
    {
        // create device
        WDF_OBJECT_ATTRIBUTES attr = {};
        WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attr, DEVICE_CONTEXT);

        NTSTATUS status = WdfDeviceCreate(&DeviceInit, &attr, &Device);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfDeviceCreate, Error %x"), status);
            return status;
        }

        DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: Device added PDO(0x%p) FDO(0x%p), Lower(0x%p)\n", WdfDeviceWdmGetPhysicalDevice(Device), WdfDeviceWdmGetDeviceObject(Device), WdfDeviceWdmGetAttachedDevice(Device));
    }

    // Driver Framework always zero initializes an objects context memory
    DEVICE_CONTEXT* deviceContext = WdfObjectGet_DEVICE_CONTEXT(Device);

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

    {
        // initialize DEVICE_CONTEXT struct with PdoName
        deviceContext->PdoName = GetTargetPropertyString(WdfDeviceGetIoTarget(Device), DevicePropertyPhysicalDeviceObjectName);
        if (!deviceContext->PdoName.Buffer) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: PdoName query failed"));
            return STATUS_UNSUCCESSFUL;
        }

        DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: PdoName: %wZ\n", deviceContext->PdoName); // outputs "\Device\00000083"
    }

    if (deviceContext->Mode == LowerFilter) {
        // defer queue creation until after HidState have been initialized
    } else if (deviceContext->Mode == UpperFilter) {
        // create queue for filtering Battery device requests
        WDF_IO_QUEUE_CONFIG queueConfig = {};
        WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);
        queueConfig.EvtIoDeviceControl = EvtIoDeviceControlBattFilter; // filter IOCTL requests

        WDFQUEUE queue = 0; // auto-deleted when "Device" is deleted
        NTSTATUS status = WdfIoQueueCreate(Device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfIoQueueCreate failed 0x%x"), status);
            return status;
        }
    }

    return STATUS_SUCCESS;
}

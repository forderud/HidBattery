#include "Battery.hpp"
#include "device.hpp"
#include <Hidport.h>
#include "HidPd.hpp"


_Function_class_(EVT_WDF_TIMER)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID InitializeHidStateTimer(_In_ WDFTIMER  Timer) {
    DebugEnter();

    WDFDEVICE Device = (WDFDEVICE)WdfTimerGetParentObject(Timer);
    NT_ASSERTMSG("InitializeHidState Device NULL\n", Device);

#if 0
    NTSTATUS status = InitializeHidState(Device);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: InitializeHidState failure 0x%x"), status);
        return;
    }
#endif

    DebugExit();
}

_Use_decl_annotations_
NTSTATUS EvtPrepareHardware(WDFDEVICE Device, WDFCMRESLIST ResourcesRaw, WDFCMRESLIST ResourcesTranslated) {
    UNREFERENCED_PARAMETER(ResourcesRaw);
    UNREFERENCED_PARAMETER(ResourcesTranslated);

    InitializeBatteryState(Device);
    return STATUS_SUCCESS;
}

_Function_class_(EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS EvtSelfManagedIoInit(WDFDEVICE Device) {
#if 0
    NTSTATUS status = InitializeBatteryClass(Device);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: InitializeBattery failed 0x%x"), status);
        return status;
    }
#endif

    {
        // schedule read of HID FEATURE reports
        // cannot call InitializeHidState immediately, since WdfIoTargetOpen of PDO will then fail with 0xc000000e (STATUS_NO_SUCH_DEVICE)
        WDF_TIMER_CONFIG timerCfg = {};
        WDF_TIMER_CONFIG_INIT(&timerCfg, InitializeHidStateTimer);

        WDF_OBJECT_ATTRIBUTES attr = {};
        WDF_OBJECT_ATTRIBUTES_INIT(&attr);
        attr.ParentObject = Device;
        attr.ExecutionLevel = WdfExecutionLevelPassive; // required to access HID functions

        WDFTIMER timer = nullptr;
        NTSTATUS status = WdfTimerCreate(&timerCfg, &attr, &timer);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfTimerCreate failed 0x%x"), status);
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

    UnloadBatteryClass(Device);
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
#if 0
        // register PnP callbacks (must be done before WdfDeviceCreate)
        WDF_PNPPOWER_EVENT_CALLBACKS PnpPowerCallbacks;
        WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&PnpPowerCallbacks);
        PnpPowerCallbacks.EvtDevicePrepareHardware = EvtPrepareHardware;
        PnpPowerCallbacks.EvtDeviceSelfManagedIoInit = EvtSelfManagedIoInit;
        PnpPowerCallbacks.EvtDeviceSelfManagedIoCleanup = EvtSelfManagedIoCleanup;
        WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &PnpPowerCallbacks);
#endif
    }
    
    {
#if 0
        // Register WDM preprocess callbacks for IRP_MJ_DEVICE_CONTROL and
        // IRP_MJ_SYSTEM_CONTROL. The battery class driver needs to handle these IO
        // requests directly.
        NTSTATUS status = WdfDeviceInitAssignWdmIrpPreprocessCallback(
            DeviceInit,
            BattWdmIrpPreprocessDeviceControl,
            IRP_MJ_DEVICE_CONTROL,
            NULL,
            0);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("WdfDeviceInitAssignWdmIrpPreprocessCallback(IRP_MJ_DEVICE_CONTROL) Failed. 0x%x"), status);
            return status;
        }
#endif
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
        // initialize battery fields
        deviceContext->BatteryTag = BATTERY_TAG_INVALID;
        deviceContext->ClassHandle = NULL;

        WDF_OBJECT_ATTRIBUTES LockAttributes{};
        WDF_OBJECT_ATTRIBUTES_INIT(&LockAttributes);
        LockAttributes.ParentObject = Device;

        NTSTATUS status = WdfWaitLockCreate(&LockAttributes, &deviceContext->ClassInitLock);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("WdfWaitLockCreate(ClassInitLock) Failed. Status 0x%x"), status);
            return status;
        }

        WDF_OBJECT_ATTRIBUTES_INIT(&LockAttributes);
        LockAttributes.ParentObject = Device;

        status = WdfSpinLockCreate(&LockAttributes, &deviceContext->State.Lock);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("WdfWaitLockCreate(StateLock) Failed. Status 0x%x"), status);
            return status;
        }
    }

    {
#if 0
        // create queue for filtering HID Power Device requests
        WDF_IO_QUEUE_CONFIG queueConfig = {};
        WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);
        queueConfig.EvtIoRead = EvtIoReadHidFilter; // filter read requests
        queueConfig.EvtIoDeviceControl = EvtIoDeviceControlHidFilter; // filter IOCTL requests

        WDFQUEUE queue = 0; // auto-deleted when "Device" is deleted
        NTSTATUS status = WdfIoQueueCreate(Device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfIoQueueCreate failed 0x%x"), status);
            return status;
        }
#endif
    }

    return STATUS_SUCCESS;
}

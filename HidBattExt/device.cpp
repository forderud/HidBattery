#include "Battery.hpp"
#include "device.hpp"
#include <Hidport.h>
#include "HidPd.hpp"


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
    NTSTATUS status = InitializeBatteryClass(Device);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: InitializeBattery failed 0x%x"), status);
        return status;
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

#if 0
void EvtDeviceFileCreate(IN WDFDEVICE Device, IN WDFREQUEST Request, IN WDFFILEOBJECT FileObject)
/*++
Routine Description:
    The framework calls a driver's EvtDeviceFileCreate callback
    when the framework receives an IRP_MJ_CREATE request.
    The system sends this request when a user application opens the
    device to perform an I/O operation, such as reading or writing to a device.
    This callback is called in the context of the thread
    that created the IRP_MJ_CREATE request.

Arguments:
    Device - Handle to a framework device object.
    FileObject - Pointer to fileobject that represents the open handle.
    CreateParams - Parameters for create
--*/
{
    UNREFERENCED_PARAMETER(Device);

    //DEVICE_CONTEXT* deviceContext = WdfObjectGet_DEVICE_CONTEXT(Device);
#if DBG
    UNICODE_STRING* filename = WdfFileObjectGetFileName(FileObject);
    ULONG processId = WdfFileObjectGetInitiatorProcessId(FileObject);
    ULONG flags = WdfFileObjectGetFlags(FileObject);

    DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: EvtDeviceFileCreate filename=%wZ, processId=%u, flags=%u\n", filename, processId, flags);

    FILE_OBJECT* file = WdfFileObjectWdmGetFileObject(FileObject);
    DebugPrint(DPFLTR_WARNING_LEVEL, "HidBattExt: EvtDeviceFileCreate FsContext=%p\n", file->FsContext);
#endif

    // TODO:
    // * Modify modify the privileges (https://community.osr.com/t/wdfrequestsend-when-formatted-for-read-returns-status-priviledge-not-held-even-though-prior-wdfreque/52554/7)
    // * Pull the PFILE_OBJECT out of the read request you receive and then manually add it to the request after you format it for read (IoGetNextIrpStackLocation(WdfRequestWdmGetIrp(request))->FileObject =<…>;)
    // * Register an EvtDeviceFileCreate callback, if the request is asking for read access, send the request down synchronously using the default target and when it comes back successfully, create a new remote IO target, 
    //   and use WDF_IO_TARGET_OPEN_PARAMS_INIT_EXISTING_DEVICE for the open params, passing WdfDeviceWdmGetAttachedDevice for the existing device, then assign openParams.TargetFileObject to the open request’s PFILE_OBJECT
    //   and then open the target. Then use this new io target when you format and send your own reads (https://community.osr.com/t/status-privilege-not-held-on-a-read-in-a-hid-filter-driver/40616/2)


#if 1
    WDFIOTARGET target = WdfDeviceGetIoTarget(Device);
    WdfRequestFormatRequestUsingCurrentType(Request);

    BOOLEAN res = WdfRequestSend(Request, target, NULL);
    if (res == FALSE) {
        NTSTATUS status = WdfRequestGetStatus(Request);
        WdfRequestComplete(Request, status);

    }
#else
    WdfRequestComplete(Request, STATUS_SUCCESS);
#endif
}
#endif

NTSTATUS EvhHidInterfaceChange(_In_ void* NotificationStruct, _Inout_opt_ void* Context) {
    auto* devNotificationStruct = (DEVICE_INTERFACE_CHANGE_NOTIFICATION*)NotificationStruct;
    auto* deviceContext = (DEVICE_CONTEXT*)Context;

    ASSERTMSG("EvhHidInterfaceChange interface mismatch", IsEqualGUID(devNotificationStruct->InterfaceClassGuid, GUID_DEVINTERFACE_HID));

    if (!IsEqualGUID(devNotificationStruct->Event, GUID_DEVICE_INTERFACE_ARRIVAL))
        return STATUS_SUCCESS; // ignore interface removal and other non-arrival events

    auto device = (WDFDEVICE)WdfObjectContextGetObject(deviceContext);

    {
        // open device to get underlying PDO name
        WDFIOTARGET_Wrap newDev;
        NTSTATUS status = WdfIoTargetCreate(device, WDF_NO_OBJECT_ATTRIBUTES, &newDev);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfIoTargetCreate failed 0x%x"), status);
            return status;
        }

        WDF_IO_TARGET_OPEN_PARAMS openParams = {};
        WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParams, devNotificationStruct->SymbolicLinkName, FILE_READ_ACCESS | FILE_WRITE_ACCESS);
        openParams.ShareAccess = FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE;

        status = WdfIoTargetOpen(newDev, &openParams);
        if (!NT_SUCCESS(status)) {
            //DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfIoTargetOpen failed 0x%x"), status);
            return status;
        }

        UNICODE_STRING devPdo = GetTargetPropertyString(newDev, DevicePropertyPhysicalDeviceObjectName); // owned by newDev

        if (!RtlEqualUnicodeString(&devPdo, &deviceContext->PdoName, /*case insensitive*/FALSE)) {
            //DebugPrint(DPFLTR_WARNING_LEVEL, "HidBattExt: EvhHidInterfaceChange skipping due to SymbolicLinkName %wZ vs. PDO %wZ mismatch\n", &devPdo, &deviceContext->PdoName);
            return STATUS_SUCCESS; // incorrect device
        }

        DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: EvhHidInterfaceChange opening %wZ\n", &devPdo);

#if DBG
        FILE_OBJECT* file = WdfIoTargetWdmGetTargetFileObject(newDev);
        DebugPrint(DPFLTR_WARNING_LEVEL, "HidBattExt: EvhHidInterfaceChange FsContext=%p, FsContext2=%p\n", file->FsContext, file->FsContext2);
#endif

        // unsubscribe from additional PnP events
        IoUnregisterPlugPlayNotificationEx(deviceContext->NotificationHandle);
        deviceContext->NotificationHandle = nullptr;

        InitializeHidState(device, newDev);
    }

    return STATUS_SUCCESS;
}


NTSTATUS EvtDriverDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit) {
    UNREFERENCED_PARAMETER(Driver);

    // Configure the device as a filter driver
    //WdfFdoInitSetFilter(DeviceInit);

    {
        // register PnP callbacks (must be done before WdfDeviceCreate)
        WDF_PNPPOWER_EVENT_CALLBACKS PnpPowerCallbacks;
        WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&PnpPowerCallbacks);
        PnpPowerCallbacks.EvtDevicePrepareHardware = EvtPrepareHardware;
        PnpPowerCallbacks.EvtDeviceSelfManagedIoInit = EvtSelfManagedIoInit;
        PnpPowerCallbacks.EvtDeviceSelfManagedIoCleanup = EvtSelfManagedIoCleanup;
        WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &PnpPowerCallbacks);
    }
    
    {
        // configure handling of device create, close & cleanup requests
        WDF_FILEOBJECT_CONFIG fileConfig{};
        WDF_FILEOBJECT_CONFIG_INIT(&fileConfig, WDF_NO_EVENT_CALLBACK, WDF_NO_EVENT_CALLBACK, WDF_NO_EVENT_CALLBACK); // keep default callbacks
        //WDF_FILEOBJECT_CONFIG_INIT(&fileConfig, EvtDeviceFileCreate, WDF_NO_EVENT_CALLBACK, WDF_NO_EVENT_CALLBACK); // only interested in create, and not close or cleanup
        fileConfig.AutoForwardCleanupClose = WdfTrue; // forward requests down the stack
        fileConfig.FileObjectClass = WdfFileObjectWdfCanUseFsContext2; // cannot use FsContext, since it's reserved by HIDclass

        WdfDeviceInitSetFileObjectConfig(DeviceInit, &fileConfig, WDF_NO_OBJECT_ATTRIBUTES);
    }

    {
#if 1
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
        // create queue for filtering HID Power Device requests
        WDF_IO_QUEUE_CONFIG queueConfig = {};
        WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);
        queueConfig.EvtIoRead = EvtIoReadHidFilter; // filter read requests

        WDFQUEUE queue = 0; // auto-deleted when "Device" is deleted
        NTSTATUS status = WdfIoQueueCreate(Device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfIoQueueCreate failed 0x%x"), status);
            return status;
        }
    }

    // subscribe to PnP events for deferred HID PDO opening
    NTSTATUS status = IoRegisterPlugPlayNotification(EventCategoryDeviceInterfaceChange, PNPNOTIFY_DEVICE_INTERFACE_INCLUDE_EXISTING_INTERFACES, (PVOID)&GUID_DEVINTERFACE_HID,
                                                        WdfDriverWdmGetDriverObject(WdfDeviceGetDriver(Device)), EvhHidInterfaceChange, (PVOID)deviceContext, &deviceContext->NotificationHandle);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: IoRegisterPlugPlayNotification failed: 0x%x"), status);
        return status;
    }

    return STATUS_SUCCESS;
}

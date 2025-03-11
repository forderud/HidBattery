#include "device.hpp"
#include <Hidport.h>
#include "Battery.hpp"
#include "HidPd.hpp"


_Function_class_(EVT_WDF_TIMER)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID InitializeHidStateTimer(_In_ WDFTIMER  Timer) {
    DebugEnter();

    WDFDEVICE Device = (WDFDEVICE)WdfTimerGetParentObject(Timer);
    NT_ASSERTMSG("InitializeHidState Device NULL\n", Device);

    NTSTATUS status = InitializeHidState(Device);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: InitializeHidState failure 0x%x"), status);
        return;
    }

    DebugExit();
}


_Function_class_(EVT_WDF_DEVICE_SELF_MANAGED_IO_INIT)
_IRQL_requires_same_
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS EvtSelfManagedIoInit(WDFDEVICE Device) {
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


void EvtDeviceFileCreate(_In_ WDFDEVICE Device, _In_ WDFREQUEST Request, _In_ WDFFILEOBJECT FileObject) {
    UNREFERENCED_PARAMETER(Device);

    IRP* irp = WdfRequestWdmGetIrp(Request);

    WDF_REQUEST_PARAMETERS params = {};
    WDF_REQUEST_PARAMETERS_INIT(&params);
    WdfRequestGetParameters(Request, &params);

    IO_SECURITY_CONTEXT* SecurityContext = params.Parameters.Create.SecurityContext; UNREFERENCED_PARAMETER(SecurityContext);
    ULONG Options = params.Parameters.Create.Options; UNREFERENCED_PARAMETER(Options);
    USHORT FileAttributes = params.Parameters.Create.FileAttributes; UNREFERENCED_PARAMETER(FileAttributes);
    USHORT ShareAccess = params.Parameters.Create.ShareAccess; UNREFERENCED_PARAMETER(ShareAccess);
    ULONG EaLength = params.Parameters.Create.EaLength; UNREFERENCED_PARAMETER(EaLength);
    
    DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt EvtDeviceFileCreate: FullCreateOptions=0x%x\n", SecurityContext->FullCreateOptions);
    DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt EvtDeviceFileCreate: DesiredAccess=0x%x\n", SecurityContext->DesiredAccess);

    DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt EvtDeviceFileCreate: Options=0x%x\n", Options); // 0x'0100'0040 (SYNCHRONIZE | FILE_NON_DIRECTORY_FILE)
    DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt EvtDeviceFileCreate: FileAttributes=%u\n", FileAttributes); // 0
    DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt EvtDeviceFileCreate: ShareAccess=%u\n", ShareAccess); // 3 (FILE_SHARE_READ | FILE_SHARE_WRITE)
    DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt EvtDeviceFileCreate: EaLength=%u\n", EaLength);

    FILE_OBJECT* fo = WdfFileObjectWdmGetFileObject(FileObject);
    // cannot self-grant read & write access, since the parameters are read-only
    DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt EvtDeviceFileCreate1: ReadAccess=%u, SharedRead=%u\n", fo->ReadAccess, fo->SharedRead);
    DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt EvtDeviceFileCreate1: WriteAccess=%u, SharedWrite=%u\n", fo->WriteAccess, fo->SharedWrite);
    DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt EvtDeviceFileCreate1: FsContext=%u\n", fo->FsContext);

    IO_STACK_LOCATION* ios = IoGetNextIrpStackLocation(irp);
    UNREFERENCED_PARAMETER(ios);
#if 0
    FILE_OBJECT* fo2 = ios->FileObject;
    DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt EvtDeviceFileCreate2: ReadAccess=%u, SharedRead=%u\n", fo2->ReadAccess, fo2->SharedRead);
    DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt EvtDeviceFileCreate2: WriteAccess=%u, SharedWrite=%u\n", fo2->WriteAccess, fo2->SharedWrite);
    DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt EvtDeviceFileCreate2: FsContext=%u\n", fo2->FsContext);
#endif

    // Forward the request down the driver stack
    BOOLEAN ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(Device), WDF_NO_SEND_OPTIONS);
    if (ret == FALSE) {
        NTSTATUS status = WdfRequestGetStatus(Request);
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfRequestSend failed with status: 0x%x"), status);
        WdfRequestComplete(Request, status);
    }
}


NTSTATUS EvtDriverDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit) {
    UNREFERENCED_PARAMETER(Driver);

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

    {
        WDF_FILEOBJECT_CONFIG config = {};
        WDF_FILEOBJECT_CONFIG_INIT(&config, EvtDeviceFileCreate, nullptr, nullptr);
        WdfDeviceInitSetFileObjectConfig(DeviceInit, &config, WDF_NO_OBJECT_ATTRIBUTES);
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
        deviceContext->LowState.Initialize(Device);
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

    return STATUS_SUCCESS;
}

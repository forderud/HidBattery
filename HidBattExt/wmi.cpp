#include "FireFly.h"


// Register our GUID and Datablock generated from the Firefly.mof file.
NTSTATUS
WmiInitialize(
    WDFDEVICE       Device,
    DEVICE_CONTEXT* DeviceContext
    )
{
    DECLARE_CONST_UNICODE_STRING(mofRsrcName, MOFRESOURCENAME);

    UNREFERENCED_PARAMETER(DeviceContext);

    PAGED_CODE();

    NTSTATUS status = WdfDeviceAssignMofResourceName(Device, &mofRsrcName);
    if (!NT_SUCCESS(status)) {
        KdPrint(("FireFly: Error in WdfDeviceAssignMofResourceName %x\n", status));
        return status;
    }

    WDF_WMI_PROVIDER_CONFIG providerConfig = {};
    WDF_WMI_PROVIDER_CONFIG_INIT(&providerConfig, &FireflyDeviceInformation_GUID);
    providerConfig.MinInstanceBufferSize = sizeof(FireflyDeviceInformation);

    WDF_WMI_INSTANCE_CONFIG instanceConfig = {};
    WDF_WMI_INSTANCE_CONFIG_INIT_PROVIDER_CONFIG(&instanceConfig, &providerConfig);
    instanceConfig.Register = TRUE;
    instanceConfig.EvtWmiInstanceQueryInstance = EvtWmiInstanceQueryInstance;
    instanceConfig.EvtWmiInstanceSetInstance = EvtWmiInstanceSetInstance;
    instanceConfig.EvtWmiInstanceSetItem = EvtWmiInstanceSetItem;

    WDF_OBJECT_ATTRIBUTES woa = {};
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&woa, FireflyDeviceInformation);

    // No need to store the WDFWMIINSTANCE in the device context because it is
    // passed back in the WMI instance callbacks and is not referenced outside
    // of those callbacks.
    WDFWMIINSTANCE WmiInstance = 0;
    status = WdfWmiInstanceCreate(Device, &instanceConfig, &woa, &WmiInstance);

    if (NT_SUCCESS(status)) {
        FireflyDeviceInformation* info = WdfObjectGet_FireflyDeviceInformation(WmiInstance);
        info->TailLight = 0x000000; // black
    }

    return status;
}

NTSTATUS
EvtWmiInstanceQueryInstance(
    IN  WDFWMIINSTANCE WmiInstance,
    IN  ULONG OutBufferSize,
    IN  PVOID OutBuffer,
    OUT PULONG BufferUsed
    )
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(OutBufferSize);

    KdPrint(("FireFly: WMI QueryInstance\n"));

    FireflyDeviceInformation* pInfo = WdfObjectGet_FireflyDeviceInformation(WmiInstance);

    // Our mininum buffer size has been checked by the Framework
    // and failed automatically if too small.
    *BufferUsed = sizeof(*pInfo);

    RtlCopyMemory(/*dst*/OutBuffer, /*src*/pInfo, sizeof(*pInfo));

    KdPrint(("FireFly: WMI QueryInstance completed\n"));
    return STATUS_SUCCESS;
}

NTSTATUS
EvtWmiInstanceSetInstance(
    IN  WDFWMIINSTANCE WmiInstance,
    IN  ULONG InBufferSize,
    IN  PVOID InBuffer
    )
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(InBufferSize);

    KdPrint(("FireFly: WMI SetInstance\n"));

    FireflyDeviceInformation* pInfo = WdfObjectGet_FireflyDeviceInformation(WmiInstance);

    // Our mininum buffer size has been checked by the Framework
    // and failed automatically if too small.
    ULONG length = sizeof(*pInfo);

    RtlMoveMemory(/*dst*/pInfo, /*src*/InBuffer, length);

    // Tell the HID device about the new tail light state
    NTSTATUS status = FireflySetFeature(
        WdfWmiInstanceGetDevice(WmiInstance),
        pInfo->TailLight
        );

    KdPrint(("FireFly: WMI SetInstance completed\n"));
    return status;
}

NTSTATUS
EvtWmiInstanceSetItem(
    IN  WDFWMIINSTANCE WmiInstance,
    IN  ULONG DataItemId,
    IN  ULONG InBufferSize,
    IN  PVOID InBuffer
    )
{
    PAGED_CODE();

    KdPrint(("FireFly: WMI SetItem\n"));

    if (DataItemId != 1)
        return STATUS_INVALID_DEVICE_REQUEST;

    if (InBufferSize < FireflyDeviceInformation_TailLight_SIZE)
        return STATUS_BUFFER_TOO_SMALL;

    FireflyDeviceInformation* pInfo = WdfObjectGet_FireflyDeviceInformation(WmiInstance);
    pInfo->TailLight = ((FireflyDeviceInformation*)InBuffer)->TailLight;

    // Tell the HID device about the new tail light state
    NTSTATUS status = FireflySetFeature(
        WdfWmiInstanceGetDevice(WmiInstance),
        pInfo->TailLight
        );

    KdPrint(("FireFly: WMI SetItem completed\n"));
    return status;
}

#include "driver.h"


// Register our GUID and Datablock generated from the TailLight.mof file.
NTSTATUS WmiInitialize(_In_ WDFDEVICE Device)
{
    DECLARE_CONST_UNICODE_STRING(mofRsrcName, MOFRESOURCENAME);

    NTSTATUS status = WdfDeviceAssignMofResourceName(Device, &mofRsrcName);
    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: Error in WdfDeviceAssignMofResourceName %x\n", status));
        return status;
    }

    WDF_WMI_PROVIDER_CONFIG providerConfig = {};
    WDF_WMI_PROVIDER_CONFIG_INIT(&providerConfig, &TailLightDeviceInformation_GUID);
    providerConfig.MinInstanceBufferSize = sizeof(TailLightDeviceInformation);

    WDF_WMI_INSTANCE_CONFIG instanceConfig = {};
    WDF_WMI_INSTANCE_CONFIG_INIT_PROVIDER_CONFIG(&instanceConfig, &providerConfig);
    instanceConfig.Register = TRUE;
    instanceConfig.EvtWmiInstanceQueryInstance = EvtWmiInstanceQueryInstance;
    instanceConfig.EvtWmiInstanceSetInstance = EvtWmiInstanceSetInstance;
    instanceConfig.EvtWmiInstanceSetItem = EvtWmiInstanceSetItem;

    WDF_OBJECT_ATTRIBUTES woa = {};
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&woa, TailLightDeviceInformation);

    // No need to store the WDFWMIINSTANCE in the device context because it is
    // passed back in the WMI instance callbacks and is not referenced outside
    // of those callbacks.
    WDFWMIINSTANCE WmiInstance = 0;
    status = WdfWmiInstanceCreate(Device, &instanceConfig, &woa, &WmiInstance);

    if (NT_SUCCESS(status)) {
        TailLightDeviceInformation* info = WdfObjectGet_TailLightDeviceInformation(WmiInstance);
        info->TailLight = 0x000000; // black
    }

    return status;
}

NTSTATUS EvtWmiInstanceQueryInstance(
    _In_  WDFWMIINSTANCE WmiInstance,
    _In_  ULONG OutBufferSize,
    _Out_writes_bytes_to_(OutBufferSize, *BufferUsed)  PVOID OutBuffer,
    _Out_ PULONG BufferUsed
    )
{
    UNREFERENCED_PARAMETER(OutBufferSize);

    KdPrint(("TailLight: WMI QueryInstance\n"));

    WDFDEVICE Device = WdfWmiInstanceGetDevice(WmiInstance);
    DEVICE_CONTEXT* deviceContext = WdfObjectGet_DEVICE_CONTEXT(Device);

    TailLightDeviceInformation* pInfo = WdfObjectGet_TailLightDeviceInformation(WmiInstance);
    // update WMI state with last written color
    pInfo->TailLight = deviceContext->TailLight;

    // Our mininum buffer size has been checked by the Framework
    // and failed automatically if too small.
    *BufferUsed = sizeof(*pInfo);

    RtlCopyMemory(/*dst*/OutBuffer, /*src*/pInfo, sizeof(*pInfo));

    KdPrint(("TailLight: WMI QueryInstance completed\n"));
    return STATUS_SUCCESS;
}

NTSTATUS EvtWmiInstanceSetInstance(
    _In_  WDFWMIINSTANCE WmiInstance,
    _In_  ULONG InBufferSize,
    _In_reads_bytes_(InBufferSize)  PVOID InBuffer
    )
{
    UNREFERENCED_PARAMETER(InBufferSize);

    KdPrint(("TailLight: WMI SetInstance\n"));

    TailLightDeviceInformation* pInfo = WdfObjectGet_TailLightDeviceInformation(WmiInstance);

    // Our mininum buffer size has been checked by the Framework
    // and failed automatically if too small.
    ULONG length = sizeof(*pInfo);

    RtlMoveMemory(/*dst*/pInfo, /*src*/InBuffer, length);

    // Tell the HID device about the new tail-light state
    NTSTATUS status = SetFeatureColor(
        WdfWmiInstanceGetDevice(WmiInstance),
        pInfo->TailLight
        );

    KdPrint(("TailLight: WMI SetInstance completed\n"));
    return status;
}

NTSTATUS EvtWmiInstanceSetItem(
    _In_  WDFWMIINSTANCE WmiInstance,
    _In_  ULONG DataItemId,
    _In_  ULONG InBufferSize,
    _In_reads_bytes_(InBufferSize)  PVOID InBuffer
    )
{
    KdPrint(("TailLight: WMI SetItem\n"));

    if (DataItemId != TailLightDeviceInformation_TailLight_ID)
        return STATUS_INVALID_DEVICE_REQUEST;

    if (InBufferSize < TailLightDeviceInformation_TailLight_SIZE)
        return STATUS_BUFFER_TOO_SMALL;

    TailLightDeviceInformation* pInfo = WdfObjectGet_TailLightDeviceInformation(WmiInstance);
    pInfo->TailLight = ((TailLightDeviceInformation*)InBuffer)->TailLight;

    // Tell the HID device about the new tail-light state
    NTSTATUS status = SetFeatureColor(
        WdfWmiInstanceGetDevice(WmiInstance),
        pInfo->TailLight
        );

    KdPrint(("TailLight: WMI SetItem completed\n"));
    return status;
}

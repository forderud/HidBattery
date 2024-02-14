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

    WDFWMIINSTANCE WmiInstance = 0;
    status = WdfWmiInstanceCreate(Device, &instanceConfig, &woa, &WmiInstance);
    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfWmiInstanceCreate error %x\n", status));
        return status;
    }

    DEVICE_CONTEXT* deviceContext = WdfObjectGet_DEVICE_CONTEXT(Device);
    deviceContext->WmiInstance = WmiInstance;

    return status;
}

NTSTATUS EvtWmiInstanceQueryInstance(
    _In_  WDFWMIINSTANCE WmiInstance,
    _In_  ULONG OutBufferSize,
    _Out_writes_bytes_to_(OutBufferSize, *BufferUsed)  PVOID OutBuffer,
    _Out_ PULONG BufferUsed
    )
{
    UNREFERENCED_PARAMETER(OutBufferSize); // // mininum buffer size already checked by WDF

    KdPrint(("TailLight: WMI QueryInstance\n"));

    TailLightDeviceInformation* pInfo = WdfObjectGet_TailLightDeviceInformation(WmiInstance);
    RtlCopyMemory(/*dst*/OutBuffer, /*src*/pInfo, sizeof(*pInfo));
    *BufferUsed = sizeof(*pInfo);

    KdPrint(("TailLight: WMI QueryInstance completed\n"));
    return STATUS_SUCCESS;
}

NTSTATUS EvtWmiInstanceSetInstance(
    _In_  WDFWMIINSTANCE WmiInstance,
    _In_  ULONG InBufferSize,
    _In_reads_bytes_(InBufferSize)  PVOID InBuffer
    )
{
    UNREFERENCED_PARAMETER(InBufferSize); // mininum buffer size already checked by WDF

    KdPrint(("TailLight: WMI SetInstance\n"));

    TailLightDeviceInformation* pInfo = WdfObjectGet_TailLightDeviceInformation(WmiInstance);
    RtlCopyMemory(/*dst*/pInfo, /*src*/InBuffer, sizeof(*pInfo));

    // Don't update deviceContext->TailLight directly here.
    // Instead, call SetFeatureColor that will internally update deviceContext->TailLight after filtering.
    NTSTATUS status = SetFeatureColor(WdfWmiInstanceGetDevice(WmiInstance), pInfo->TailLight);

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

    TailLightDeviceInformation* pInfo = WdfObjectGet_TailLightDeviceInformation(WmiInstance);
    NTSTATUS status = STATUS_SUCCESS;

    if (DataItemId == TailLightDeviceInformation_TailLight_ID) {
        if (InBufferSize < TailLightDeviceInformation_TailLight_SIZE)
            return STATUS_BUFFER_TOO_SMALL;

        pInfo->TailLight = *(ULONG*)InBuffer;

        // Don't update deviceContext->TailLight directly here.
        // Instead, call SetFeatureColor that will internally update deviceContext->TailLight after filtering.
        status = SetFeatureColor(WdfWmiInstanceGetDevice(WmiInstance), pInfo->TailLight);
    } else {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    KdPrint(("TailLight: WMI SetItem completed\n"));
    return status;
}

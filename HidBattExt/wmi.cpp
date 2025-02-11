#include "driver.h"
#include <stdlib.h>


static NTSTATUS EvtWmiInstanceExecuteMethod(
    _In_    WDFWMIINSTANCE WmiInstance,
    _In_    ULONG MethodId,
    _In_    ULONG InBufferSize,
    _In_    ULONG OutBufferSize,
    _Inout_ PVOID Buffer,
    _Out_   PULONG BufferUsed
) {
    UNREFERENCED_PARAMETER(WmiInstance);
    UNREFERENCED_PARAMETER(MethodId);
    UNREFERENCED_PARAMETER(InBufferSize);
    UNREFERENCED_PARAMETER(Buffer);
    UNREFERENCED_PARAMETER(BufferUsed);

    if (OutBufferSize < SelfTest_OUT_SIZE)
        return STATUS_BUFFER_TOO_SMALL;

    return STATUS_WMI_ITEMID_NOT_FOUND;
}


// Register our GUID and Datablock generated from the TailLight.mof file.
NTSTATUS WmiInitialize(_In_ WDFDEVICE Device)
{
    DECLARE_CONST_UNICODE_STRING(mofRsrcName, MOFRESOURCENAME);

    NTSTATUS status = WdfDeviceAssignMofResourceName(Device, &mofRsrcName);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: Error in WdfDeviceAssignMofResourceName %x"), status);
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
    instanceConfig.EvtWmiInstanceExecuteMethod = EvtWmiInstanceExecuteMethod;

    WDF_OBJECT_ATTRIBUTES woa = {};
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&woa, TailLightDeviceInformation);

    WDFWMIINSTANCE WmiInstance = 0;
    status = WdfWmiInstanceCreate(Device, &instanceConfig, &woa, &WmiInstance);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("TailLight: WdfWmiInstanceCreate error %x"), status);
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
    UNREFERENCED_PARAMETER(OutBufferSize); // mininum buffer size already checked by WDF

    DebugEnter();

    TailLightDeviceInformation* pInfo = WdfObjectGet_TailLightDeviceInformation(WmiInstance);
    RtlCopyMemory(/*dst*/OutBuffer, /*src*/pInfo, sizeof(*pInfo));
    *BufferUsed = sizeof(*pInfo);

    DebugExit();
    return STATUS_SUCCESS;
}

NTSTATUS EvtWmiInstanceSetInstance(
    _In_  WDFWMIINSTANCE WmiInstance,
    _In_  ULONG InBufferSize,
    _In_reads_bytes_(InBufferSize)  PVOID InBuffer
    )
{
    UNREFERENCED_PARAMETER(InBufferSize); // mininum buffer size already checked by WDF

    DebugEnter();

    TailLightDeviceInformation* pInfo = WdfObjectGet_TailLightDeviceInformation(WmiInstance);
    RtlCopyMemory(/*dst*/pInfo, /*src*/InBuffer, sizeof(*pInfo));

    // call SetFeatureColor to trigger tail-light update
    NTSTATUS status = SetFeatureColor(WdfWmiInstanceGetDevice(WmiInstance), pInfo->TailLight);

    DebugExitStatus(status);
    return status;
}

NTSTATUS EvtWmiInstanceSetItem(
    _In_  WDFWMIINSTANCE WmiInstance,
    _In_  ULONG DataItemId,
    _In_  ULONG InBufferSize,
    _In_reads_bytes_(InBufferSize)  PVOID InBuffer
    )
{
    DebugEnter();

    TailLightDeviceInformation* pInfo = WdfObjectGet_TailLightDeviceInformation(WmiInstance);
    NTSTATUS status = STATUS_SUCCESS;

    if (DataItemId == TailLightDeviceInformation_TailLight_ID) {
        if (InBufferSize < TailLightDeviceInformation_TailLight_SIZE)
            return STATUS_BUFFER_TOO_SMALL;

        pInfo->TailLight = *(ULONG*)InBuffer;

        // call SetFeatureColor to trigger tail-light update
        status = SetFeatureColor(WdfWmiInstanceGetDevice(WmiInstance), pInfo->TailLight);
    } else {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    DebugExitStatus(status);
    return status;
}

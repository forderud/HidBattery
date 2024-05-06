#include "driver.h"
#include <stdlib.h>


/** Fake self-test that gradually dims the tail-light color. */
VOID SelfTestTimerProc (_In_ WDFTIMER timer) {
    WDFDEVICE device = (WDFDEVICE)WdfTimerGetParentObject(timer);
    DEVICE_CONTEXT* deviceContext = WdfObjectGet_DEVICE_CONTEXT(device);
    TailLightDeviceInformation* pInfo = WdfObjectGet_TailLightDeviceInformation(deviceContext->WmiInstance);

    {
        // gradually dim color
        BYTE* rgb = reinterpret_cast<BYTE*>(&pInfo->TailLight);
        rgb[0] = max(rgb[0] - 16, 0);
        rgb[1] = max(rgb[1] - 16, 0);
        rgb[2] = max(rgb[2] - 16, 0);
    }

    auto* stCtx = WdfObjectGet_SELF_TEST_CONTEXT(timer);
    NTSTATUS status = SetFeatureColor(device, pInfo->TailLight);
    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: %s: failed 0x%x\n", __func__, status));
        stCtx->Result = status;
        LONG wasSignaled = KeSetEvent(&deviceContext->SelfTestCompleted, IO_MOUSE_INCREMENT, FALSE);
        NT_ASSERTMSG("TailLight: SelfTest method set to signaled while already signaled.\n", wasSignaled == 0);
        return; // abort self-test
    }

    if (!stCtx->Advance()) {
        KdPrint(("TailLight: Self-test completed\n"));
        stCtx->Result = STATUS_SUCCESS;
        LONG wasSignaled = KeSetEvent(&deviceContext->SelfTestCompleted, IO_MOUSE_INCREMENT, FALSE);
        NT_ASSERTMSG("TailLight: SelfTest method set to signaled while already signaled.\n", wasSignaled == 0);
        return;
    }

    // enqueue the next callback
    BOOLEAN inQueue = WdfTimerStart(timer, WDF_REL_TIMEOUT_IN_MS(SELF_TEST_CONTEXT::DURATION_MS));
    NT_ASSERTMSG("TailLight: Previous active timer overwritten", !inQueue);
}

static NTSTATUS EvtWmiInstanceExecuteMethod(
    _In_    WDFWMIINSTANCE WmiInstance,
    _In_    ULONG MethodId,
    _In_    ULONG InBufferSize,
    _In_    ULONG OutBufferSize,
    _Inout_ PVOID Buffer,
    _Out_   PULONG BufferUsed
) {
    UNREFERENCED_PARAMETER(InBufferSize);

    if (OutBufferSize < SelfTest_OUT_SIZE)
        return STATUS_BUFFER_TOO_SMALL;

    WDFDEVICE device = WdfWmiInstanceGetDevice(WmiInstance);

    switch (MethodId) {
    case SelfTest:
        {
            DEVICE_CONTEXT* devCtx = WdfObjectGet_DEVICE_CONTEXT(device);
            SELF_TEST_CONTEXT* stCtx = WdfObjectGet_SELF_TEST_CONTEXT(devCtx->SelfTestTimer);
            if (stCtx->IsBusy())
                return STATUS_DEVICE_BUSY; // self-test already in progress

            KdPrint(("TailLight: Starting self-test\n"));
            {
                TailLightDeviceInformation* pInfo = WdfObjectGet_TailLightDeviceInformation(WmiInstance);
                pInfo->TailLight = 0x00D0D0D0; // start color
            }
            stCtx->Start();
            SelfTestTimerProc(devCtx->SelfTestTimer);
            
            {
                unsigned int SLACK = 500;
                LARGE_INTEGER timeout = {};
                timeout.QuadPart = WDF_REL_TIMEOUT_IN_MS(SELF_TEST_CONTEXT::STEP_COUNT*SELF_TEST_CONTEXT::DURATION_MS + SLACK);
                NTSTATUS waitStatus = KeWaitForSingleObject(&devCtx->SelfTestCompleted, Executive, KernelMode, FALSE, &timeout);
                switch (waitStatus) {
                case STATUS_ALERTED:
                case STATUS_USER_APC:
                    NT_ASSERTMSG("TailLight: SelfTestCompleted KEVENT was interrupted.\n", FALSE);
                    return STATUS_INTERNAL_ERROR;
                }

                // WMI method output arguments
                auto* out = (SelfTest_OUT*)Buffer;
                out->result = stCtx->Result;
                *BufferUsed = sizeof(SelfTest_OUT);
                return STATUS_SUCCESS;
            }
        }
    default:
        break;
    }

    return STATUS_WMI_ITEMID_NOT_FOUND;
}


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
    instanceConfig.EvtWmiInstanceExecuteMethod = EvtWmiInstanceExecuteMethod;

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

    {
        // Initialize self-test timer
        WDF_TIMER_CONFIG timerCfg = {};
        WDF_TIMER_CONFIG_INIT(&timerCfg, SelfTestTimerProc);

        WDF_OBJECT_ATTRIBUTES attribs = {};
        WDF_OBJECT_ATTRIBUTES_INIT(&attribs);
        WDF_OBJECT_ATTRIBUTES_SET_CONTEXT_TYPE(&attribs, SELF_TEST_CONTEXT);
        attribs.ExecutionLevel = WdfExecutionLevelPassive; // required to access HID functions
        attribs.ParentObject = Device;

        status = WdfTimerCreate(&timerCfg, &attribs, &deviceContext->SelfTestTimer);
        if (!NT_SUCCESS(status)) {
            KdPrint(("TailLight: %s: WdfTimerCreate failed 0x%x\n", __func__, status));
            return status;
        }
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
    UNREFERENCED_PARAMETER(OutBufferSize); // mininum buffer size already checked by WDF

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

    // call SetFeatureColor to trigger tail-light update
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

        // call SetFeatureColor to trigger tail-light update
        status = SetFeatureColor(WdfWmiInstanceGetDevice(WmiInstance), pInfo->TailLight);
    } else {
        return STATUS_INVALID_DEVICE_REQUEST;
    }

    KdPrint(("TailLight: WMI SetItem completed\n"));
    return status;
}

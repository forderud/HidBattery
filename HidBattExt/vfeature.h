#pragma once
#include "TailLight.h"


NTSTATUS SetFeatureColor (
    _In_  WDFDEVICE Device,
    _In_  ULONG     Color
    );

NTSTATUS SetFeatureFilter(
    _In_ WDFDEVICE  Device,
    _In_ WDFREQUEST Request,
    _In_ size_t     InputBufferLength
);

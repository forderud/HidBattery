#pragma once

/** Driver-specific struct for storing instance-specific data. */
struct DEVICE_CONTEXT {
    UNICODE_STRING PdoName;
    WDFWMIINSTANCE WmiInstance;
};
WDF_DECLARE_CONTEXT_TYPE(DEVICE_CONTEXT)

WDF_DECLARE_CONTEXT_TYPE(TailLightDeviceInformation)

EVT_WDF_DEVICE_CONTEXT_CLEANUP EvtDeviceContextCleanup;

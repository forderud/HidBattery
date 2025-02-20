#pragma once

enum FilterMode {
    UpperFilter, // above HidBatt: Filters battery IOCTL communication
    LowerFilter, // below HidBatt: Filters HID Power Device communication
};

/** Driver-specific struct for storing instance-specific data. */
struct DEVICE_CONTEXT {
    FilterMode     Mode;
    UNICODE_STRING PdoName;
    WDFWMIINSTANCE WmiInstance;
    WDFTIMER       SelfTestTimer;
    KEVENT         SelfTestCompleted;
};
WDF_DECLARE_CONTEXT_TYPE(DEVICE_CONTEXT)

WDF_DECLARE_CONTEXT_TYPE(TailLightDeviceInformation)

EVT_WDF_DEVICE_CONTEXT_CLEANUP EvtDeviceContextCleanup;

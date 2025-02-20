#pragma once

enum FilterMode {
    UpperFilter, // above HidBatt: Filters battery IOCTL communication
    LowerFilter, // below HidBatt: Filters HID Power Device communication
};

/** State to share between Upper and Lower filter driver instances. */
struct SharedState {
    ULONG CycleCount; // BATTERY_INFORMATION::CycleCount value
    ULONG Temperature; // IOCTL_BATTERY_QUERY_INFORMATION BatteryTemperature value
};

/** Driver-specific struct for storing instance-specific data. */
struct DEVICE_CONTEXT {
    FilterMode     Mode;
    UNICODE_STRING PdoName;
    SharedState    State;
};
WDF_DECLARE_CONTEXT_TYPE(DEVICE_CONTEXT)

EVT_WDF_DEVICE_CONTEXT_CLEANUP EvtDeviceContextCleanup;

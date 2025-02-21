#pragma once

enum FilterMode {
    UpperFilter, // above HidBatt: Filters battery IOCTL communication
    LowerFilter, // below HidBatt: Filters HID Power Device communication
};

/** State to share between Upper and Lower filter driver instances. */
struct SharedState {
    ULONG CycleCount;  // BATTERY_INFORMATION::CycleCount value
    ULONG Temperature; // IOCTL_BATTERY_QUERY_INFORMATION BatteryTemperature value
};

DEFINE_GUID(GUID_HIDBATTEXT_SHARED_STATE, 0x2f52277a, 0x88f8, 0x44f3, 0x87, 0xec, 0x48, 0xb2, 0xe9, 0x51, 0x84, 0x58);

/** State to share between Upper and Lower filter driver instances. */
struct HidBattExtIf : public INTERFACE {
    SharedState* State = nullptr; // non-owning ptr.

    /** Register this object so that it can be looked up by other driver instances. */
    NTSTATUS Register (WDFDEVICE device, SharedState& state) {
        // initialize INTERFACE header
        Size = sizeof(HidBattExtIf);
        Version = 1;
        Context = device;
        // Let the framework handle reference counting
        InterfaceReference = WdfDeviceInterfaceReferenceNoOp;
        InterfaceDereference = WdfDeviceInterfaceDereferenceNoOp;

        // initialize shared state ptr.
        State = &state;

        // register device interface, so that other driver instances can detect it
        WDF_QUERY_INTERFACE_CONFIG  cfg{};
        WDF_QUERY_INTERFACE_CONFIG_INIT(&cfg, this, &GUID_HIDBATTEXT_SHARED_STATE, NULL);
        return WdfDeviceAddQueryInterface(device, &cfg);
    }

    /** Lookup driver state from other driver instances. WILL OVERWRITE all fields in this object. */
    NTSTATUS Lookup(WDFDEVICE device) {
        // We will provide an example on how to get a bus-specific direct
        // call interface from a bus driver.
        return WdfFdoQueryForInterface(device, &GUID_HIDBATTEXT_SHARED_STATE, this, sizeof(HidBattExtIf), 1, NULL);
    }
};

/** Driver-specific struct for storing instance-specific data. */
struct DEVICE_CONTEXT {
    FilterMode     Mode;
    UNICODE_STRING PdoName;
    SharedState    State;
    HidBattExtIf   Interface;
};
WDF_DECLARE_CONTEXT_TYPE(DEVICE_CONTEXT)

EVT_WDF_DEVICE_CONTEXT_CLEANUP EvtDeviceContextCleanup;

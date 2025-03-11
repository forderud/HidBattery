#pragma once
#include "driver.hpp"
#include "HidPd.hpp"


enum class FilterMode {
    Upper, // above HidBatt: Filter battery IOCTL communication
    Lower, // below HidBatt: Filter HID Power Device communication
};

struct HidCode {
    USAGE UsagePage = 0;
    USAGE Usage = 0;

    bool operator ==(HidCode other) const {
        return (other.UsagePage == UsagePage) && (other.Usage == Usage);
    }
};

/** HID-related configuration for usage by the Lower filter driver instance.
    Members sorted in initialization order. */
struct HidConfig {
    WDFMEMORY Preparsed = 0; // preparsed HID report descriptor (~3kB)

    PHIDP_PREPARSED_DATA GetPreparsedData() const {
        NT_ASSERTMSG("WDFMEMORY Preparsed null", Preparsed);
        return (PHIDP_PREPARSED_DATA)WdfMemoryGetBuffer(Preparsed, nullptr);
    }

    USHORT InputReportByteLength = 0;
    USHORT FeatureReportByteLength = 0;

    UCHAR TemperatureReportID = 0;
    UCHAR CycleCountReportID = 0;

    LONG Initialized = 0;
};

/** HID Power Device report UsagePage and Usage codes from https://www.usb.org/sites/default/files/pdcv11.pdf */
static constexpr HidCode Temperature_Code = { 0x84, 0x36 }; // from 4.1 Power Device Page (x84) Table 2.
static constexpr HidCode CycleCount_Code = { 0x85, 0x6B }; // from 4.2 Battery System Page (x85) Table 3.


/** Battery parameters shared between Upper and Lower filter driver instances. */
class BatteryState {
public:
    WDFSPINLOCK Lock = 0;  // to protext member access

    ULONG CycleCount = 0;  // BATTERY_INFORMATION::CycleCount value
    ULONG Temperature = 0; // IOCTL_BATTERY_QUERY_INFORMATION BatteryTemperature value

    void Initialize(WDFDEVICE device) {
        WDF_OBJECT_ATTRIBUTES attr{};
        WDF_OBJECT_ATTRIBUTES_INIT(&attr);
        attr.ParentObject = device; // auto-deleted when "device" is deleted

        NTSTATUS status = WdfSpinLockCreate(&attr, &Lock);
        NT_ASSERTMSG("WdfSpinLockCreate failed.\n", status == STATUS_SUCCESS); status;
    }
};

DEFINE_GUID(GUID_HIDBATTEXT_SHARED_STATE, 0x2f52277a, 0x88f8, 0x44f3, 0x87, 0xec, 0x48, 0xb2, 0xe9, 0x51, 0x84, 0x58);

/** State to share between Upper and Lower filter driver instances. */
struct HidBattExtIf : public INTERFACE {
    BatteryState* State = nullptr; // non-owning ptr.

    HidBattExtIf() {
        // clear all INTERFACE members
        Size = 0;
        Version = 0;
        Context = 0;
        InterfaceReference = nullptr;
        InterfaceDereference = nullptr;
    }

    /** Register this object so that it can be looked up by other driver instances. */
    NTSTATUS Register (WDFDEVICE device, BatteryState& state) {
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

    /** Lookup state from other driver instance in same driver stack. WILL OVERWRITE all fields in this object. */
    NTSTATUS Lookup(WDFDEVICE device) {
        return WdfFdoQueryForInterface(device, &GUID_HIDBATTEXT_SHARED_STATE, this, sizeof(HidBattExtIf), 1, NULL);
    }
};

/** Driver-specific struct for storing instance-specific data. */
struct DEVICE_CONTEXT {
    FilterMode     Mode;      // upper or lower driver instance
    UNICODE_STRING PdoName;
    HidConfig      Hid;       // for lower filter usage
    BatteryState   LowState;  // lower filter instance state (not directly accessible from upper filter)
    HidBattExtIf   Interface; // for communication between driver instances
};
WDF_DECLARE_CONTEXT_TYPE(DEVICE_CONTEXT)


/** IOCTL buffer object to allow completion routines to access request information. */
struct REQUEST_CONTEXT {
    ULONG IoControlCode = 0;
    ULONG InformationLevel = 0; // contains BATTERY_QUERY_INFORMATION::InformationLevel in IOCTL_BATTERY_QUERY_INFORMATION requests

    void Set(ULONG ioctl, ULONG infoLevel) {
        IoControlCode = ioctl;
        InformationLevel = infoLevel;
    }
};
WDF_DECLARE_CONTEXT_TYPE(REQUEST_CONTEXT)

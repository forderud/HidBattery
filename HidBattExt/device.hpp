#pragma once
#include "driver.hpp"
#include <hidpddi.h> // for PHIDP_PREPARSED_DATA


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

    LONG Initialized = 0; // struct initialized (atomic value)
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

/** Driver-specific struct for storing instance-specific data. */
struct DEVICE_CONTEXT {
    UNICODE_STRING PdoName;
    HidConfig      Hid;       // for lower filter usage
    BatteryState   LowState;  // lower filter instance state (not directly accessible from upper filter)
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

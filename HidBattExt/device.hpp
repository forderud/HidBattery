#pragma once
#include "driver.hpp"
#include <hidpddi.h> // for PHIDP_PREPARSED_DATA
#include "Battery.hpp"


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

    HidCode reports[256] = {}; // ReportID lookup

    LONG Initialized = 0; // struct initialized (atomic value)
};

/** HID Power Device report UsagePage and Usage codes from https://www.usb.org/sites/default/files/pdcv11.pdf */
static constexpr HidCode Temperature_Code = { 0x84, 0x36 }; // from 4.1 Power Device Page (x84) Table 2.
static constexpr HidCode CycleCount_Code = { 0x85, 0x6B }; // from 4.2 Battery System Page (x85) Table 3.
static constexpr HidCode RemainingCapacity_Code = { 0x85, 0x66 };
static constexpr HidCode DesignCapacity_Code = { 0x85, 0x83 };
static constexpr HidCode FullCapacity_Code = { 0x85, 0x67 };
static constexpr HidCode Voltage_Code = { 0x84, 0x30 };
static constexpr HidCode RunTimeToEmpty_Code = { 0x85, 0x68 };
static constexpr HidCode ManufacturerDate_Code = {0x85, 0x85};

/** Driver-specific struct for storing instance-specific data. */
struct DEVICE_CONTEXT {
    UNICODE_STRING PdoName;
    HidConfig      Hid;       // for lower filter usage
    void*          NotificationHandle; // opaque value to identify PnP notification registration

    // Battery class registration
    void*              ClassHandle;
    WDFWAITLOCK        ClassInitLock;
    WMILIB_CONTEXT     WmiLibContext;

    // Battery state
    ULONG              BatteryTag;
    BATT_STATE         State;
};
WDF_DECLARE_CONTEXT_TYPE(DEVICE_CONTEXT)

#pragma once

/** HID Power Device report (https://www.usb.org/sites/default/files/pdcv11.pdf). */
#pragma pack(push, 1) // no padding
struct HidPdReport {
    HidPdReport(UCHAR type) {
        ReportId = type;
    }

    UCHAR  ReportId = 0;
    USHORT Value = 0;

    // UsagePage and Usage codes from https://www.usb.org/sites/default/files/pdcv11.pdf
    static const UCHAR s_Temperature_UsagePage = 0x84; // from 4.1 Power Device Page (x84) Table 2.
    static const UCHAR s_Temperature_Usage = 0x36;
    static const UCHAR s_CycleCount_UsagePage = 0x85; // from 4.2 Battery System Page (x85) Table 3.
    static const UCHAR s_CycleCount_Usage = 0x6B; 
};
#pragma pack(pop) // restore default settings
static_assert(sizeof(HidPdReport) == 3);

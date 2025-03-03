#pragma once

/** HID Power Device report (https://www.usb.org/sites/default/files/pdcv11.pdf). */
#pragma pack(push, 1) // no padding
struct HidPdReport {
    HidPdReport(UCHAR type) {
        ReportId = type;
    }

    UCHAR  ReportId = 0;
    USHORT Value = 0;
};
#pragma pack(pop) // restore default settings
static_assert(sizeof(HidPdReport) == 3);

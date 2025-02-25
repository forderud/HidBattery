#pragma once

/** HID Power Device report (https://www.usb.org/sites/default/files/pdcv11.pdf). */
#pragma pack(push, 1) // no padding
struct HidPdReport {
    HidPdReport() = default;

    HidPdReport(UCHAR type) {
        ReportId = type;
    }

#ifdef _KERNEL_MODE
    void Print(const char* prefix) const {
        DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: %s ReportID=0x%x, Value=%u\n", prefix, ReportId, Value); prefix;
    }
#else
    void Print(const char* prefix) const {
        printf("%s Report=%s, Value=%u\n", prefix, TypeStr((ReportType)ReportId), Value);
    }
#endif

    //report ID of the collection to which the control request is sent
    UCHAR  ReportId = 0;
    USHORT Value = 0;
};
#pragma pack(pop) // restore default settings
static_assert(sizeof(HidPdReport) == 3);

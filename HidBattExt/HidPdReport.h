#pragma once

/** HID Power Device report (https://www.usb.org/sites/default/files/pdcv11.pdf). */
#pragma pack(push, 1) // no padding
struct HidPdReport {
    /** ReportID values taken from https://github.com/forderud/HidBattery/blob/master/src/HIDPowerDevice.h */
    enum ReportType : UCHAR {
        Zero = 0, // dummy report
        ManufactureDate = 0x09,
        Temperature = 0x0A,
        Voltage = 0x0B,
        RemainingCapacity = 0x0c,
        CycleCount = 0x14,
    };

    static const char* TypeStr(ReportType type) {
        switch (type) {
        case Zero: return "0";
        case ManufactureDate: return "ManufactureDate";
        case Temperature: return "Temperature";
        case Voltage: return "Voltage";
        case RemainingCapacity: return "RemainingCapacity";
        case CycleCount: return "CycleCount";
        default:
            return "Unknown";
        }
    }

    HidPdReport() = default;

    HidPdReport(ReportType type) {
        ReportId = type;
    }

#ifdef _KERNEL_MODE
    bool IsValid() const {
        return true;
    }

    void Print(const char* prefix) const {
        DebugPrint(DPFLTR_INFO_LEVEL, "TailLight: %s Report=%s, Value=%u\n", prefix, TypeStr((ReportType)ReportId), Value);
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

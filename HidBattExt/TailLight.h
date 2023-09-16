#pragma once

/** Tail-light feature report as observed in USBPcap/Wireshark. */
struct TailLightReport {
    TailLightReport() {
    }

    void SetColor(ULONG Color) {
        Red = (Color) & 0xFF; // red;
        Green = (Color >> 8) & 0xFF; // green
        Blue = (Color >> 16) & 0xFF; // blue
    }

    ULONG GetColor() const {
        return (Blue << 16) | (Green << 8) | Red;
    }

#ifdef _KERNEL_MODE
    bool IsValid() const {
        if (ReportId != 36) {// 0x24
            KdPrint(("TailLight: TailLightReport: Unsupported report id %d\n", ReportId));
            return false;
        }

        if ((Unknown1 != 0xB2) || (Unknown2 != 0x03)) {
            KdPrint(("TailLight: TailLightReport: Unknown control Code 0x%x 0x%x\n", Unknown1, Unknown2));
            return false;
        }

        return true;
    }
#endif

#ifdef _KERNEL_MODE
    bool SafetyCheck() {
        // RGB check
        unsigned int color_sum = Red + Green + Blue;
        if (color_sum > 2 * 256) {
            KdPrint(("TailLight: Color saturation %u exceeded 512 threshold. Reseting color to RED to signal error\n", color_sum));
            Red = 255;
            Green = 0;
            Blue = 0;
            return false;
        }

        return true;
    }
#endif

    //report ID of the collection to which the control request is sent
    UCHAR    ReportId = 36; // (0x24)

    // control codes (user-defined)
    UCHAR   Unknown1 = 0xB2; // magic value
    UCHAR   Unknown2 = 0x03; // magic value

    UCHAR   Red = 0;
    UCHAR   Green = 0;
    UCHAR   Blue = 0;

    UCHAR  padding[67] = {};
};
static_assert(sizeof(TailLightReport) == 73);

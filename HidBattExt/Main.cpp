#include "HID.hpp"


bool UpdateTailColor(HANDLE hid_dev, HIDP_CAPS caps, COLORREF color) {
#if 0
    printf("Device capabilities:\n");
    printf("  Usage=0x%04X, UsagePage=0x%04X\n", caps.Usage, caps.UsagePage);
    printf("  InputReportByteLength=%u, OutputReportByteLength=%u, FeatureReportByteLength=%u, NumberLinkCollectionNodes=%u\n", caps.InputReportByteLength, caps.OutputReportByteLength, caps.FeatureReportByteLength, caps.NumberLinkCollectionNodes);
    printf("  NumberInputButtonCaps=%u, NumberInputValueCaps=%u, NumberInputDataIndices=%u\n", caps.NumberInputButtonCaps, caps.NumberInputValueCaps, caps.NumberInputDataIndices);
    printf("  NumberOutputButtonCaps=%u, NumberOutputValueCaps=%u, NumberOutputDataIndices=%u\n", caps.NumberOutputButtonCaps, caps.NumberOutputValueCaps, caps.NumberOutputDataIndices);
    printf("  NumberFeatureButtonCaps=%u, NumberFeatureValueCaps=%u, NumberFeatureDataIndices=%u\n", caps.NumberFeatureButtonCaps, caps.NumberFeatureValueCaps, caps.NumberFeatureDataIndices);
#endif

    if (caps.FeatureReportByteLength != 73)
        return false;
    
    // increase size by 1 for report ID header
    std::vector<BYTE> FeatureReport(caps.FeatureReportByteLength + 1, (BYTE)0);

    // Set feature report values (as observed in USBPcap/Wireshark)
    FeatureReport[0] = 0x24; // Report ID 36
    FeatureReport[1] = 0xB2; // magic value
    FeatureReport[2] = 0x03; // magic value
    // tail-light color
    FeatureReport[3] = (color     )  & 0xFF; // red
    FeatureReport[4] = (color >> 8)  & 0xFF; // green
    FeatureReport[5] = (color >> 16) & 0xFF; // blue

    BOOLEAN ok = HidD_SetFeature(hid_dev, FeatureReport.data(), (ULONG)FeatureReport.size());
    if (!ok) {
        DWORD err = GetLastError();
        printf("ERROR: HidD_SetFeature failure (err %d).\n", err);
        assert(ok);
        return false;
    }

    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printf("IntelliMouse tail-light shifter.\n");
        printf("Usage: \"TailLight.exe <red> <green> <blue>\" (example: \"TailLight.exe 0 0 255\").\n");
        return -1;
    }

    BYTE red = atoi(argv[1]);
    BYTE green = atoi(argv[2]);
    BYTE blue = atoi(argv[3]);

    HID::Query mouse;
    mouse.VendorID = 0x045E;  // Microsoft
    mouse.ProductID = 0x082A; // Pro IntelliMouse
    mouse.Usage = 0x0212;     // magic value

    auto matches = HID::FindDevices(mouse);
    for (HID::Match& match : matches) {
        wprintf(L"Updating %s\n", match.name.c_str());
        bool ok = UpdateTailColor(match.dev.Get(), match.caps, RGB(red, green, blue));
        if (ok)
            printf("SUCCESS: Tail-light color updated.\n");
    }
}

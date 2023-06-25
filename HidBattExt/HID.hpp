#pragma once
#include <windows.h>
#include <cfgmgr32.h> // for CM_Get_Device_Interface_List
#include <hidsdi.h>
#include <wrl/wrappers/corewrappers.h>

#include <cassert>
#include <string>
#include <vector>

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "mincore.lib")


/** Human Interface Devices (HID) device search class. */
class HID {
public:
    // RAII wrapper of file HANDLE objects
    using FileHandle = Microsoft::WRL::Wrappers::FileHandle;

    struct Query {
        USHORT VendorID = 0;
        USHORT ProductID = 0;
        USHORT Usage = 0;
        USHORT UsagePage = 0;
    };

    struct Match {
        std::wstring name;
        FileHandle dev;
        HIDP_CAPS caps = {};
    };

    static std::vector<Match> FindDevices (const Query& query) {
        GUID hidguid = {};
        HidD_GetHidGuid(&hidguid);

        ULONG deviceInterfaceListLength = 0;
        CONFIGRET cr = CM_Get_Device_Interface_List_SizeW(&deviceInterfaceListLength, &hidguid, NULL, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
        assert(cr == CR_SUCCESS);

        std::wstring deviceInterfaceList(deviceInterfaceListLength, L'\0');
        cr = CM_Get_Device_Interface_ListW(&hidguid, NULL, const_cast<wchar_t*>(deviceInterfaceList.data()), deviceInterfaceListLength, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
        assert(cr == CR_SUCCESS);

        printf("Searching for matching HID devices...\n");

        std::vector<Match> results;
        for (const wchar_t * currentInterface = deviceInterfaceList.c_str(); *currentInterface; currentInterface += wcslen(currentInterface) + 1) {
            FileHandle hid_dev(CreateFileW(currentInterface,
                GENERIC_READ | GENERIC_WRITE,
                FILE_SHARE_READ | FILE_SHARE_WRITE,
                NULL,
                OPEN_EXISTING,
                0,
                NULL));
            if (!hid_dev.IsValid()) {
                DWORD err = GetLastError();
                //assert(err == ERROR_ACCESS_DENIED); // (5) observed for already used devices
                //printf("WARNING: CreateFile failed: (err %d) for %ls\n", err, currentInterface);
                continue;
            }

            HIDD_ATTRIBUTES attr = {};
            HidD_GetAttributes(hid_dev.Get(), &attr);

            if (query.VendorID && (query.VendorID != attr.VendorID))
                continue;
            if (query.ProductID && (query.ProductID != attr.ProductID))
                continue;

            // found a match
            PHIDP_PREPARSED_DATA reportDesc = {}; // report descriptor for top-level collection
            HidD_GetPreparsedData(hid_dev.Get(), &reportDesc);

            HIDP_CAPS caps = {};
            HidP_GetCaps(reportDesc, &caps);

            if (query.Usage && (query.Usage != caps.Usage))
                continue;
            if (query.UsagePage && (query.UsagePage != caps.UsagePage))
                continue;

            printf("  Found matching device with VendorID=%x, ProductID=%x\n", attr.VendorID, attr.ProductID);
            
#if 0
            wchar_t man_buffer[128] = L"<unknown>";
            HidD_GetManufacturerString(hid_dev.Get(), man_buffer, (ULONG)std::size(man_buffer)); // ignore errors
            printf("  Manufacturer: %ws\n", man_buffer);

            wchar_t prod_buffer[128] = L"<unknown>";
            HidD_GetProductString(hid_dev.Get(), prod_buffer, (ULONG)std::size(prod_buffer)); // ignore erorrs
            printf("  Product: %ws\n", prod_buffer);
#endif

            HidD_FreePreparsedData(reportDesc);

            results.push_back({currentInterface, std::move(hid_dev), caps});
        }

        return results;
    }
};

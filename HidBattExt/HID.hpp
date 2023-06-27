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


/** RAII wrapper of PHIDP_PREPARSED_DATA. */
class PreparsedData {
public:
    PreparsedData(HANDLE hid_dev) {
        HidD_GetPreparsedData(hid_dev, &report);
    }
    ~PreparsedData() {
        if (report) {
            HidD_FreePreparsedData(report);
            report = nullptr;
        }
    }

    PreparsedData(PreparsedData&& obj) {
        std::swap(report, obj.report);
    }

    operator PHIDP_PREPARSED_DATA() const {
        return report;
    }

private:
    PHIDP_PREPARSED_DATA report = nullptr; // report descriptor for top-level collection
};

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
        PreparsedData report;
        HIDP_CAPS caps = {};
    };

    static std::vector<Match> FindDevices (const Query& query, bool verbose = false) {
        GUID hidguid = {};
        HidD_GetHidGuid(&hidguid);

        const ULONG searchScope = CM_GET_DEVICE_INTERFACE_LIST_PRESENT; // only currently 'live' device interfaces

        ULONG deviceInterfaceListLength = 0;
        CONFIGRET cr = CM_Get_Device_Interface_List_SizeW(&deviceInterfaceListLength, &hidguid, NULL, searchScope);
        assert(cr == CR_SUCCESS);

        std::wstring deviceInterfaceList(deviceInterfaceListLength, L'\0');
        cr = CM_Get_Device_Interface_ListW(&hidguid, NULL, const_cast<wchar_t*>(deviceInterfaceList.data()), deviceInterfaceListLength, searchScope);
        assert(cr == CR_SUCCESS);

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
                //assert(err != ERROR_ACCESS_DENIED); // (5) observed for already used devices
                //assert(err != ERROR_SHARING_VIOLATION); // (32)
                if (verbose)
                    printf("WARNING: CreateFile failed: (err %d) for %ls\n", err, currentInterface);

                continue;
            }

            HIDD_ATTRIBUTES attr = {};
            HidD_GetAttributes(hid_dev.Get(), &attr);

            PreparsedData reportDesc(hid_dev.Get());

            HIDP_CAPS caps = {};
            HidP_GetCaps(reportDesc, &caps);

            if (verbose)
                printf("Device %ls (VendorID=%x, ProductID=%x, Usage=%x, UsagePage=%x)\n", currentInterface, attr.VendorID, attr.ProductID, caps.Usage, caps.UsagePage);

            if (query.VendorID && (query.VendorID != attr.VendorID))
                continue;
            if (query.ProductID && (query.ProductID != attr.ProductID))
                continue;

            if (query.Usage && (query.Usage != caps.Usage))
                continue;
            if (query.UsagePage && (query.UsagePage != caps.UsagePage))
                continue;

            if (verbose)
                printf("  Found matching device with VendorID=%x, ProductID=%x\n", attr.VendorID, attr.ProductID);
#if 0
            wchar_t man_buffer[128] = L"<unknown>";
            HidD_GetManufacturerString(hid_dev.Get(), man_buffer, (ULONG)std::size(man_buffer)); // ignore errors
            printf("  Manufacturer: %ws\n", man_buffer);

            wchar_t prod_buffer[128] = L"<unknown>";
            HidD_GetProductString(hid_dev.Get(), prod_buffer, (ULONG)std::size(prod_buffer)); // ignore erorrs
            printf("  Product: %ws\n", prod_buffer);
#endif
            results.push_back({currentInterface, std::move(hid_dev), std::move(reportDesc), caps});
        }

        return results;
    }
};

#pragma once
#include <ntddk.h>
#include <wdf.h>
#include <Wmilib.h>
extern "C" {
#include <Batclass.h>
}


struct BATT_STATE {
    WDFWAITLOCK              Lock; // protects state changes

    BATTERY_MANUFACTURE_DATE ManufactureDate;
    BATTERY_INFORMATION      BatteryInfo;
    BATTERY_STATUS           BatteryStatus;
    ULONG                    GranularityCount;
    BATTERY_REPORTING_SCALE  GranularityScale[4];
    ULONG                    EstimatedTime;
    ULONG                    Temperature;
    WCHAR                    DeviceName[MAX_BATTERY_STRING_SIZE];
    WCHAR                    ManufacturerName[MAX_BATTERY_STRING_SIZE];
    WCHAR                    SerialNumber[MAX_BATTERY_STRING_SIZE];
    WCHAR                    UniqueId[MAX_BATTERY_STRING_SIZE];
};


NTSTATUS InitializeBatteryClass(_In_ WDFDEVICE Device);
NTSTATUS UnloadBatteryClass(_In_ WDFDEVICE Device);

_IRQL_requires_same_
void InitializeBatteryState(_In_ WDFDEVICE Device);

EVT_WDFDEVICE_WDM_IRP_PREPROCESS BattWdmIrpPreprocessDeviceControl;
EVT_WDFDEVICE_WDM_IRP_PREPROCESS BattWdmIrpPreprocessSystemControl;

#pragma once
#include <ntddk.h>
#include <wdf.h>
extern "C" {
#include <Batclass.h>
}
#include <Wmilib.h>
#include "device.hpp"


struct BATT_STATE {
    BATTERY_MANUFACTURE_DATE        ManufactureDate;
    BATTERY_INFORMATION             BatteryInfo;
    BATTERY_STATUS                  BatteryStatus;
    ULONG                           GranularityCount;
    BATTERY_REPORTING_SCALE         GranularityScale[4];
    ULONG                           EstimatedTime;
    ULONG                           Temperature;
    WCHAR                           DeviceName[MAX_BATTERY_STRING_SIZE];
    WCHAR                           ManufacturerName[MAX_BATTERY_STRING_SIZE];
    WCHAR                           SerialNumber[MAX_BATTERY_STRING_SIZE];
    WCHAR                           UniqueId[MAX_BATTERY_STRING_SIZE];
};

struct BATT_FDO_DATA {
    // Battery class registration
    void* ClassHandle;
    WDFWAITLOCK                     ClassInitLock;
    WMILIB_CONTEXT                  WmiLibContext;

    // Battery state
    WDFWAITLOCK                     StateLock;
    ULONG                           BatteryTag;
    BATT_STATE                   State;
};

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(BATT_FDO_DATA, GetDeviceExtension);


NTSTATUS InitializeBattery(_In_ WDFDEVICE Device);

_IRQL_requires_same_
void InitializeBatteryState(_In_ WDFDEVICE Device);

EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL BattIoDeviceControl;

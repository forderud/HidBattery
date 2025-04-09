#include "device.hpp"
#include "HidPd.hpp"
#include <hidclass.h> // for HID_COLLECTION_INFORMATION
#include "CppAllocator.hpp"


/** AmpSec=mWh*360/centiVolt */
ULONG Compute_mWh(ULONG ampereSec, ULONG mV) {
    return (ampereSec * mV)/3600;
}

static ULONG SetClearMask(ULONG prev, ULONG mask, bool setMask) {
    if (setMask)
        return prev | mask; // set mask bits
    else
        return prev & ~mask; // clear mask bits
}

static void UpdateBatteryState(BATT_STATE& state, HIDP_REPORT_TYPE reportType, CHAR* report, const HidConfig& hid) {
    USHORT reportLen = 0;
    if (reportType == HidP_Input)
        reportLen = hid.InputReportByteLength;
    else if (reportType == HidP_Feature)
        reportLen = hid.FeatureReportByteLength;
    else
        NT_ASSERTMSG("UpdateBatteryState invalid reportType", false);

    const HidCode code = hid.reports[report[0]]; // ReportID lookup
    if (code == HidCode{ 0, 0 })
        return;

    ULONG value = 0;

    if (code.button) {
        USAGE usagesOn[16] = {};
        ULONG usagesOn_count = 16;

        // parse button presses
        NTSTATUS status = HidP_GetUsages(reportType, code.UsagePage, /*default link collection*/0, usagesOn, &usagesOn_count, hid.GetPreparsedData(), report, reportLen);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: HidP_GetUsages failed UsagePage=0x%x, (0x%x)"), code.UsagePage, status);
            return;
        }

        if (code.UsagePage == 0x84) {
            // Power Device Page (x84)
            bool shutdownimminent = false;

            for (size_t i = 0; i < usagesOn_count; i++) {
                USAGE usage = usagesOn[i];

                if (usage == ShutdownImminent_Code.Usage)
                    shutdownimminent = true;
            }

            WdfSpinLockAcquire(state.Lock);
            state.BatteryStatus.PowerState = SetClearMask(state.BatteryStatus.PowerState, BATTERY_CRITICAL, shutdownimminent);
            WdfSpinLockRelease(state.Lock);
        } else if (code.UsagePage == 0x85) {
            // Battery System Page (x85)
            bool charging = false;
            bool discharging = false;
            bool acpresent = false;

            for (size_t i = 0; i < usagesOn_count; i++) {
                USAGE usage = usagesOn[i];

                if (usage == Charging_Code.Usage)
                    charging = true;
                else if (usage == Discharging_Code.Usage)
                    discharging = true;
                else if (usage == ACPresent_Code.Usage)
                    acpresent = true;
            }

            WdfSpinLockAcquire(state.Lock);
            state.BatteryStatus.PowerState = SetClearMask(state.BatteryStatus.PowerState, BATTERY_CHARGING, charging);
            state.BatteryStatus.PowerState = SetClearMask(state.BatteryStatus.PowerState, BATTERY_DISCHARGING, discharging);
            state.BatteryStatus.PowerState = SetClearMask(state.BatteryStatus.PowerState, BATTERY_POWER_ON_LINE, acpresent);
            WdfSpinLockRelease(state.Lock);
        }
    } else {
        // parse parameter values
        NTSTATUS status = HidP_GetUsageValue(reportType, code.UsagePage, /*default link collection*/0, code.Usage, &value, hid.GetPreparsedData(), report, reportLen);
        if (!NT_SUCCESS(status)) {
            // fails with HIDP_STATUS_USAGE_NOT_FOUND (0xc0110004) for HID_PD_SERIAL (0x02) report with UsagePage=0x84, Usage=0xff
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: HidP_GetUsageValue failed UsagePage=0x%x, Usage=0x%x, (0x%x)"), code.UsagePage, code.Usage, status);
            return;
        }
    }

    // capture shared state
    if (code == Temperature_Code) {
         auto TempBefore = state.Temperature;

         WdfSpinLockAcquire(state.Lock);
         // convert HID PD unit from (Kelvin) to BATTERY_QUERY_INFORMATION unit (10ths of a degree Kelvin)
         state.Temperature = 10 * value;
         WdfSpinLockRelease(state.Lock);

         if (state.Temperature != TempBefore) {
             DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: Updating HID Temperature before=%u, after=%u\n", TempBefore, state.Temperature);
         }
     } else if (code == CycleCount_Code) {
        auto CycleCountBefore = state.BatteryInfo.CycleCount;

        WdfSpinLockAcquire(state.Lock);
        state.BatteryInfo.CycleCount = value;
        WdfSpinLockRelease(state.Lock);

        if (state.BatteryInfo.CycleCount != CycleCountBefore) {
            DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: Updating HID CycleCount before=%u, after=%u\n", CycleCountBefore, state.BatteryInfo.CycleCount);
        }
    } else if (code == Voltage_Code) {
        auto VoltageBefore = state.BatteryStatus.Voltage;

        WdfSpinLockAcquire(state.Lock);
        state.BatteryStatus.Voltage = 10*value; // centiVolt to millivolts 
        WdfSpinLockRelease(state.Lock);

        if (state.BatteryStatus.Voltage != VoltageBefore) {
            DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: Updating HID Voltage before=%u, after=%u\n", VoltageBefore, state.BatteryStatus.Voltage);
        }
    } else if (code == RemainingCapacity_Code) {
        auto CapBefore = state.BatteryStatus.Capacity;

        WdfSpinLockAcquire(state.Lock);
        state.BatteryStatus.Capacity = Compute_mWh(value, state.BatteryStatus.Voltage); // AmpSec to mWh conversion
        WdfSpinLockRelease(state.Lock);

        if (state.BatteryStatus.Capacity != CapBefore) {
            DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: Updating HID RemainingCapacity before=%u, after=%u\n", CapBefore, state.BatteryStatus.Capacity);
        }
    } else if (code == DesignCapacity_Code) {
        auto DesignCapBefore = state.BatteryInfo.DesignedCapacity;

        WdfSpinLockAcquire(state.Lock);
        state.BatteryInfo.DesignedCapacity = Compute_mWh(value, state.BatteryStatus.Voltage); // AmpSec to mWh conversion
        WdfSpinLockRelease(state.Lock);

        if (state.BatteryInfo.DesignedCapacity != DesignCapBefore) {
            DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: Updating HID DesignedCapacity before=%u, after=%u\n", DesignCapBefore, state.BatteryInfo.DesignedCapacity);
        }
    } else if (code == FullCapacity_Code) {
        auto FullCapBefore = state.BatteryInfo.FullChargedCapacity;

        WdfSpinLockAcquire(state.Lock);
        state.BatteryInfo.FullChargedCapacity = Compute_mWh(value, state.BatteryStatus.Voltage); // AmpSec to mWh conversion
        WdfSpinLockRelease(state.Lock);

        if (state.BatteryInfo.FullChargedCapacity != FullCapBefore) {
            DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: Updating HID FullChargedCapacity before=%u, after=%u\n", FullCapBefore, state.BatteryInfo.FullChargedCapacity);
        }
    } else if (code == RunTimeToEmpty_Code) {
        auto timeBefore = state.EstimatedTime;

        WdfSpinLockAcquire(state.Lock);
        state.EstimatedTime = value;
        WdfSpinLockRelease(state.Lock);

        if (state.EstimatedTime != timeBefore) {
            DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: Updating HID RunTimeToEmpty before=%u, after=%u\n", timeBefore, state.EstimatedTime);
        }
    } else if (code == ManufacturerDate_Code) {
        WdfSpinLockAcquire(state.Lock);
        // value = (year - 1980) * 512 + month * 32 + day; // from 4.2.6 Battery Settings in "Universal Serial Bus Usage Tables for HID Power Devices"
        state.ManufactureDate.Day = value % 32;
        state.ManufactureDate.Month = (value >> 5) % 16;
        state.ManufactureDate.Year = (USHORT)(1980 + (value >> 9));
        WdfSpinLockRelease(state.Lock);

        DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: Updating HID ManufactureDate\n");
    }
}


/** Blocking IOCTL_HID_GET_FEATURE request with pre-populated report. */
NTSTATUS GetFeatureReport(WDFIOTARGET target, UCHAR reportId) {
    if (!reportId)
        return STATUS_SUCCESS;

    WDFDEVICE device = WdfIoTargetGetDevice(target);
    DEVICE_CONTEXT* context = WdfObjectGet_DEVICE_CONTEXT(device);

    // Battery Temperature query
    RamArray<CHAR> report(context->Hid.FeatureReportByteLength);
    report[0] = reportId;

    WDF_MEMORY_DESCRIPTOR outputDesc = {};
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDesc, report, report.ByteSize());

    NTSTATUS status = WdfIoTargetSendIoctlSynchronously(target, NULL,
        IOCTL_HID_GET_FEATURE,
        NULL, // input
        &outputDesc, // output
        NULL, NULL);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: IOCTL_HID_GET_FEATURE failed 0x%x"), status);
        return status;
    }

    UpdateBatteryState(context->State, HidP_Feature, report, context->Hid);
    return status;
}

template <size_t BUF_SIZE>
NTSTATUS GetStringFromReport(WDFIOTARGET target, UCHAR reportId, wchar_t (&buffer)[BUF_SIZE]) {
    if (!reportId)
        return STATUS_SUCCESS;

    WDFDEVICE device = WdfIoTargetGetDevice(target);
    DEVICE_CONTEXT* context = WdfObjectGet_DEVICE_CONTEXT(device);

    ULONG strIdx = 0;
    {
        // string index query
        RamArray<CHAR> report(context->Hid.FeatureReportByteLength);
        report[0] = reportId;

        WDF_MEMORY_DESCRIPTOR outputDesc = {};
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDesc, report, report.ByteSize());

        NTSTATUS status = WdfIoTargetSendIoctlSynchronously(target, NULL,
            IOCTL_HID_GET_FEATURE,
            NULL, // input
            &outputDesc, // output
            NULL, NULL);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: IOCTL_HID_GET_FEATURE failed 0x%x"), status);
            return status;
        }
        // string report index
        strIdx = report[1];
    }
    {
        // battery chemistry query
        WDF_MEMORY_DESCRIPTOR inputDesc = {};
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputDesc, &strIdx, sizeof(strIdx));

        WDF_MEMORY_DESCRIPTOR outputDesc = {};
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDesc, buffer, sizeof(buffer));

        NTSTATUS status = WdfIoTargetSendIoctlSynchronously(target, NULL,
            IOCTL_HID_GET_INDEXED_STRING,
            &inputDesc, // input
            &outputDesc, // output
            NULL, NULL);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: IOCTL_HID_GET_INDEXED_STRING failed 0x%x"), status);
            return status;
        }
    }

    return STATUS_SUCCESS;
}

template <size_t BUF_SIZE>
NTSTATUS GetStandardStringReport(WDFIOTARGET target, ULONG ioctl, wchar_t (&buffer)[BUF_SIZE]) {
    WDF_MEMORY_DESCRIPTOR outputDesc = {};
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDesc, buffer, sizeof(buffer));

    NTSTATUS status = WdfIoTargetSendIoctlSynchronously(target, NULL,
        ioctl,
        NULL, // input
        &outputDesc, // output
        NULL, NULL);
    if (!NT_SUCCESS(status)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: IOCTL=0x%x failed 0x%x"), ioctl, status);
        return status;
    }

    return STATUS_SUCCESS;
}

NTSTATUS InitializeHidState(_In_ WDFDEVICE Device) {
    DEVICE_CONTEXT* context = WdfObjectGet_DEVICE_CONTEXT(Device);
    WDFIOTARGET_Wrap pdoTarget;
    {
        // Use PDO for HID commands instead of local IO target to avoid 0xc0000061 (STATUS_PRIVILEGE_NOT_HELD) on IOCTL_HID_SET_FEATURE
        NTSTATUS status = WdfIoTargetCreate(Device, WDF_NO_OBJECT_ATTRIBUTES, &pdoTarget);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfIoTargetCreate failed 0x%x"), status);
            return status;
        }

        // open in shared read-write mode
        WDF_IO_TARGET_OPEN_PARAMS openParams = {};
        WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParams, &context->PdoName, FILE_READ_ACCESS | FILE_WRITE_ACCESS);
        // We will let the framework to respond automatically to the pnp state changes of the target by closing and opening the handle.
        openParams.ShareAccess = FILE_SHARE_WRITE | FILE_SHARE_READ;

        status = WdfIoTargetOpen(pdoTarget, &openParams);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfIoTargetOpen failed 0x%x"), status);
            return status;
        }
    }

    HID_COLLECTION_INFORMATION collectionInfo = {};
    {
        // populate "collectionInformation"
        WDF_MEMORY_DESCRIPTOR outputDesc = {};
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDesc, &collectionInfo, sizeof(HID_COLLECTION_INFORMATION));

        NTSTATUS status = WdfIoTargetSendIoctlSynchronously(pdoTarget, NULL,
            IOCTL_HID_GET_COLLECTION_INFORMATION,
            NULL, // input
            &outputDesc, // output
            NULL, NULL);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: IOCTL_HID_GET_COLLECTION_INFORMATION failed 0x%x"), status);
            return status;
        }

        DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: ProductID=%x, VendorID=%x, VersionNumber=%u, DescriptorSize=%u\n", collectionInfo.ProductID, collectionInfo.VendorID, collectionInfo.VersionNumber, collectionInfo.DescriptorSize);
    }

    {
        WDF_OBJECT_ATTRIBUTES attr{};
        WDF_OBJECT_ATTRIBUTES_INIT(&attr);
        attr.ParentObject = Device; // auto-delete when "Device" is deleted

        // allocate "preparsedData"
        NTSTATUS status = WdfMemoryCreate(&attr, NonPagedPoolNx, POOL_TAG, collectionInfo.DescriptorSize, &context->Hid.Preparsed, nullptr);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfMemoryCreate failed 0x%x"), status);
            return status;
        }

        WDF_MEMORY_DESCRIPTOR outputDesc = {};
        WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(&outputDesc, context->Hid.Preparsed, NULL);

        // populate "preparsedData"
        status = WdfIoTargetSendIoctlSynchronously(pdoTarget, NULL,
            IOCTL_HID_GET_COLLECTION_DESCRIPTOR, // same as HidD_GetPreparsedData in user-mode
            NULL, // input
            &outputDesc, // output
            NULL, NULL);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: IOCTL_HID_GET_COLLECTION_DESCRIPTOR failed 0x%x"), status);
            return status;
        }
    }

    // get capabilities
    HIDP_CAPS caps = {};
    NTSTATUS status = HidP_GetCaps(context->Hid.GetPreparsedData(), &caps);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    UCHAR TemperatureReportID = 0;
    UCHAR CycleCountReportID = 0;
    UCHAR VoltageID = 0;
    UCHAR RemainingCapacityID = 0;
    UCHAR DesignCapacityID = 0;
    UCHAR FullCapacityID = 0;
    UCHAR RunTimeToEmptyID = 0;
    UCHAR ManufacturerDateID = 0;
    UCHAR ChemistryID = 0;
    UCHAR ManufacturerID = 0;
    UCHAR SerialNumberID = 0;
    {
        context->Hid.InputReportByteLength = caps.InputReportByteLength;
        context->Hid.FeatureReportByteLength = caps.FeatureReportByteLength;

        DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: Usage=%x, UsagePage=%x, InputReportByteLength=%u, FeatureReportByteLength=%u\n", caps.Usage, caps.UsagePage, caps.InputReportByteLength, caps.FeatureReportByteLength);

        // get FEATURE report value caps
        USHORT valueCapsLen = caps.NumberFeatureValueCaps;
        RamArray<HIDP_VALUE_CAPS> valueCaps(valueCapsLen);
        if (!valueCaps) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: HIDP_VALUE_CAPS[%u] allocation failure."), valueCapsLen);
            return status;
        }
        status = HidP_GetValueCaps(HidP_Feature, valueCaps, &valueCapsLen, context->Hid.GetPreparsedData());
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: HidP_GetValueCaps failed 0x%x"), status);
            return status;
        }

        // identify UsagePage & Usage code for all HID reports
        for (USHORT i = 0; i < valueCapsLen; i++) {
            HidCode& code = context->Hid.reports[valueCaps[i].ReportID];
            code.UsagePage = valueCaps[i].UsagePage;
            code.Usage = valueCaps[i].NotRange.Usage;
            code.button = false;

            if (code == Temperature_Code)
                TemperatureReportID = valueCaps[i].ReportID;
            else if (code == CycleCount_Code)
                CycleCountReportID = valueCaps[i].ReportID;
            else if (code == Voltage_Code)
                VoltageID = valueCaps[i].ReportID;
            else if (code == RemainingCapacity_Code)
                RemainingCapacityID = valueCaps[i].ReportID;
            else if (code == DesignCapacity_Code)
                DesignCapacityID = valueCaps[i].ReportID;
            else if (code == FullCapacity_Code)
                FullCapacityID = valueCaps[i].ReportID;
            else if (code == RunTimeToEmpty_Code)
                RunTimeToEmptyID = valueCaps[i].ReportID;
            else if (code == ManufacturerDate_Code)
                ManufacturerDateID = valueCaps[i].ReportID;
            else if (code == Chemistry_Code)
                ChemistryID = valueCaps[i].ReportID;
            else if (code == Manufacturer_Code)
                ManufacturerID  = valueCaps[i].ReportID;
            else if (code == SerialNumber_Code)
                SerialNumberID = valueCaps[i].ReportID;
        }
    }

    {
        // get FEATURE report button caps
        USHORT buttonCapsLen = caps.NumberFeatureButtonCaps;
        RamArray<HIDP_BUTTON_CAPS> buttonCaps(buttonCapsLen);
        if (!buttonCaps) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: HIDP_BUTTON_CAPS[%u] allocation failure."), buttonCapsLen);
            return status;
        }
        status = HidP_GetButtonCaps(HidP_Feature, buttonCaps, &buttonCapsLen, context->Hid.GetPreparsedData());
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: HidP_GetButtonCaps failed 0x%x"), status);
            return status;
        }

        // identify UsagePage & Usage code for all HID reports
        for (USHORT i = 0; i < buttonCapsLen; i++) {
            HidCode& code = context->Hid.reports[buttonCaps[i].ReportID];
            code.UsagePage = buttonCaps[i].UsagePage;
            code.Usage = buttonCaps[i].NotRange.Usage;
            code.button = true;
        }
    }

    {
        // query FEATURE reports
        status = GetFeatureReport(pdoTarget, TemperatureReportID);
        if (!NT_SUCCESS(status))
            return status;

        status = GetFeatureReport(pdoTarget, CycleCountReportID);
        if (!NT_SUCCESS(status))
            return status;

        status = GetFeatureReport(pdoTarget, VoltageID); // voltage must be retrieved BEFORE capacity reports
        if (!NT_SUCCESS(status))
            return status;

        status = GetFeatureReport(pdoTarget, RemainingCapacityID);
        if (!NT_SUCCESS(status))
            return status;

        status = GetFeatureReport(pdoTarget, DesignCapacityID);
        if (!NT_SUCCESS(status))
            return status;

        status = GetFeatureReport(pdoTarget, FullCapacityID);
        if (!NT_SUCCESS(status))
            return status;

        status = GetFeatureReport(pdoTarget, RunTimeToEmptyID);
        if (!NT_SUCCESS(status))
            return status;

        status = GetFeatureReport(pdoTarget, ManufacturerDateID);
        if (!NT_SUCCESS(status))
            return status;

        {
            // query battery chemistry
            wchar_t strBuf[128] = {}; // max USB length is 126 wchar's
            status = GetStringFromReport(pdoTarget, ChemistryID, strBuf);
            if (!NT_SUCCESS(status))
                return status;

            // convert wchar_t string to fixed-length ASCII
            context->State.BatteryInfo.Chemistry[0] = (UCHAR)strBuf[0];
            context->State.BatteryInfo.Chemistry[1] = (UCHAR)strBuf[1];
            context->State.BatteryInfo.Chemistry[2] = (UCHAR)strBuf[2];
            context->State.BatteryInfo.Chemistry[3] = (UCHAR)strBuf[3];
        }
        {
            // query device name
            status = GetStandardStringReport(pdoTarget, IOCTL_HID_GET_PRODUCT_STRING, context->State.DeviceName);
            if (!NT_SUCCESS(status))
                return status;
        }
        {
            // query manufacturer
            status = GetStringFromReport(pdoTarget, ManufacturerID, context->State.ManufacturerName);
            if (!NT_SUCCESS(status))
                return status;
        }
        {
            // query serial number
            // IOCTL_HID_GET_SERIALNUMBER_STRING cannot be used since it only returns "HIDAP"
            status = GetStringFromReport(pdoTarget, SerialNumberID, context->State.SerialNumber);
            if (!NT_SUCCESS(status))
                return status;
        }
        {
            // TODO: query unique ID
            // IOCTL_HID_GET_MANUFACTURER_STRING returns "Arduino LLC"
            status = GetStandardStringReport(pdoTarget, IOCTL_HID_GET_MANUFACTURER_STRING, context->State.UniqueId);
            if (!NT_SUCCESS(status))
                return status;
        }
    }

    // flag HidConfig struct as initialized
    InterlockedIncrement(&context->Hid.Initialized);

    return STATUS_SUCCESS;
}


void ParseReadHidBuffer(WDFDEVICE Device, _In_ WDFREQUEST Request) {
    DEVICE_CONTEXT* context = WdfObjectGet_DEVICE_CONTEXT(Device);

    if (!context->Hid.Initialized) {
        //DebugPrint(DPFLTR_WARNING_LEVEL, "HidBattExt: HidConfig not yet initialized\n");
        return;
    }

    CHAR* report = nullptr;
    size_t Length = 0;
    NTSTATUS status = WdfRequestRetrieveOutputBuffer(Request, 0, (void**)&report, &Length);
    if (!NT_SUCCESS(status) || !report) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfRequestRetrieveOutputBuffer failed 0x%x, report=0x%p"), status, report);
        return;
    }

    if (Length < context->Hid.InputReportByteLength) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: EvtIoReadFilter: Too small output buffer"));
        return;
    }

    UpdateBatteryState(context->State, HidP_Input, report, context->Hid);
}


/** Read request completion routine for parsing the output buffer written by the HW. */
void EvtIoReadHidFilterCompletion(_In_  WDFREQUEST Request, _In_  WDFIOTARGET Target, _In_  WDF_REQUEST_COMPLETION_PARAMS* Params, _In_  WDFCONTEXT Context) {
    UNREFERENCED_PARAMETER(Params); // invalidated by WdfRequestFormatRequestUsingCurrentType
    UNREFERENCED_PARAMETER(Context);

    if (!NT_SUCCESS(WdfRequestGetStatus(Request))) {
        //DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: EvtIoReadHidFilterCompletion status=0x%x"), WdfRequestGetStatus(Request));
        WdfRequestComplete(Request, WdfRequestGetStatus(Request));
        return;
    }

    WDFDEVICE device = WdfIoTargetGetDevice(Target);
    ParseReadHidBuffer(device, Request);

    WdfRequestComplete(Request, STATUS_SUCCESS);
}


_Function_class_(EVT_WDF_IO_QUEUE_IO_READ)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID EvtIoReadHidFilter(_In_ WDFQUEUE Queue, _In_ WDFREQUEST Request, _In_ size_t Length) {
    UNREFERENCED_PARAMETER(Length);
    DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: EvtIoReadHidFilter (Length=%Iu)\n", Length);

    WDFDEVICE device = WdfIoQueueGetDevice(Queue);

    // Formating required if specifying a completion routine
    WdfRequestFormatRequestUsingCurrentType(Request);
    // set completion callback
    WdfRequestSetCompletionRoutine(Request, EvtIoReadHidFilterCompletion, nullptr);

    BOOLEAN ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(device), WDF_NO_SEND_OPTIONS);
    if (ret == FALSE) {
        NTSTATUS status = WdfRequestGetStatus(Request);
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfRequestSend failed with status: 0x%x"), status);
        WdfRequestComplete(Request, status);
    }
}


_Function_class_(EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID EvtIoDeviceControlHidFilter(
    _In_  WDFQUEUE          Queue,
    _In_  WDFREQUEST        Request,
    _In_  size_t            OutputBufferLength,
    _In_  size_t            InputBufferLength,
    _In_  ULONG             IoControlCode) {
#if 0
    DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: EvtIoDeviceControlHidFilter (IoControlCode=0x%x, InputBufferLength=%Iu, OutputBufferLength=%Iu)\n", IoControlCode, InputBufferLength, OutputBufferLength);
#endif
    UNREFERENCED_PARAMETER(OutputBufferLength);
    UNREFERENCED_PARAMETER(InputBufferLength);
    UNREFERENCED_PARAMETER(IoControlCode);

    WDFDEVICE Device = WdfIoQueueGetDevice(Queue);

    // Forward the request down the driver stack
    WDF_REQUEST_SEND_OPTIONS options{};
    WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_SEND_AND_FORGET);

    BOOLEAN ret = WdfRequestSend(Request, WdfDeviceGetIoTarget(Device), &options);
    if (ret == FALSE) {
        NTSTATUS status = WdfRequestGetStatus(Request);
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfRequestSend failed with status: 0x%x"), status);
        WdfRequestComplete(Request, status);
    }
}

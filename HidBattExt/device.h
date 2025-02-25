#pragma once

enum FilterMode {
    UpperFilter, // above HidBatt: Filters battery IOCTL communication
    LowerFilter, // below HidBatt: Filters HID Power Device communication
};

/** State to share between Upper and Lower filter driver instances. */
class SharedState {
public:
    WDFSPINLOCK Lock = 0;  // to protext member access

    ULONG CycleCount;  // BATTERY_INFORMATION::CycleCount value
    ULONG Temperature; // IOCTL_BATTERY_QUERY_INFORMATION BatteryTemperature value

    void Initialize(WDFDEVICE device) {
        WDF_OBJECT_ATTRIBUTES attribs{};
        WDF_OBJECT_ATTRIBUTES_INIT(&attribs);
        attribs.ParentObject = device;

        NTSTATUS status = WdfSpinLockCreate(&attribs, &Lock);
        NT_ASSERTMSG("WdfSpinLockCreate failed.\n", status == STATUS_SUCCESS); status;
    }
};

DEFINE_GUID(GUID_HIDBATTEXT_SHARED_STATE, 0x2f52277a, 0x88f8, 0x44f3, 0x87, 0xec, 0x48, 0xb2, 0xe9, 0x51, 0x84, 0x58);

/** State to share between Upper and Lower filter driver instances. */
struct HidBattExtIf : public INTERFACE {
    SharedState* State = nullptr; // non-owning ptr.

    HidBattExtIf() {
        // clear all INTERFACE members
        Size = 0;
        Version = 0;
        Context = 0;
        InterfaceReference = nullptr;
        InterfaceDereference = nullptr;
    }

    /** Register this object so that it can be looked up by other driver instances. */
    NTSTATUS Register (WDFDEVICE device, SharedState& state) {
        // initialize INTERFACE header
        Size = sizeof(HidBattExtIf);
        Version = 1;
        Context = device;
        // Let the framework handle reference counting
        InterfaceReference = WdfDeviceInterfaceReferenceNoOp;
        InterfaceDereference = WdfDeviceInterfaceDereferenceNoOp;

        // initialize shared state ptr.
        State = &state;

        // register device interface, so that other driver instances can detect it
        WDF_QUERY_INTERFACE_CONFIG  cfg{};
        WDF_QUERY_INTERFACE_CONFIG_INIT(&cfg, this, &GUID_HIDBATTEXT_SHARED_STATE, NULL);
        return WdfDeviceAddQueryInterface(device, &cfg);
    }

    /** Lookup driver state from other driver instances. WILL OVERWRITE all fields in this object. */
    NTSTATUS Lookup(WDFDEVICE device) {
        return WdfFdoQueryForInterface(device, &GUID_HIDBATTEXT_SHARED_STATE, this, sizeof(HidBattExtIf), 1, NULL);
    }
};

/** IOCTL buffer object to allow completion routines to access request bufffers.
     Used because the "WDF_REQUEST_COMPLETION_PARAMS* Params" argument appear to contain garbage data. */
struct IoctlBuffers {
    ULONG IoControlCode = 0;
    
    ULONG InformationLevel = 0; // contains BATTERY_QUERY_INFORMATION::InformationLevel in IOCTL_BATTERY_QUERY_INFORMATION requests

    size_t OutputBufferLength = 0;
    void * OutputBuffer = nullptr;

    void Update(ULONG ioctl, ULONG infoLevel, WDFREQUEST Request) {
        IoControlCode = ioctl;

        InformationLevel = infoLevel;

        OutputBufferLength = 0;
        OutputBuffer = nullptr;
        if (Request) {
            NTSTATUS status = WdfRequestRetrieveOutputBuffer(Request, 0, (void**)&OutputBuffer, &OutputBufferLength);
            NT_ASSERTMSG("WdfRequestRetrieveOutputBuffer failed", NT_SUCCESS(status)); status;
        }
    }
};

/** Driver-specific struct for storing instance-specific data. */
struct DEVICE_CONTEXT {
    FilterMode     Mode;      // upper or lower driver instance
    UNICODE_STRING PdoName;
    UCHAR          TemperatureReportID;
    UCHAR          CycleCountReportID;
    SharedState    LowState;  // lower filter instance state (not accessible from upper filter)
    HidBattExtIf   Interface; // for communication between driver instances
    IoctlBuffers   BattIoctl; // for Battery request completion routine
};
WDF_DECLARE_CONTEXT_TYPE(DEVICE_CONTEXT)

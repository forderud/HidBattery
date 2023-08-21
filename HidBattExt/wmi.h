// Where they are described.
#define MOFRESOURCENAME L"TailLightWMI"

// Initialize the TailLight drivers WMI support
NTSTATUS WmiInitialize(
    WDFDEVICE       Device,
    DEVICE_CONTEXT* DeviceContext
    );

EVT_WDF_WMI_INSTANCE_QUERY_INSTANCE EvtWmiInstanceQueryInstance;

EVT_WDF_WMI_INSTANCE_SET_INSTANCE EvtWmiInstanceSetInstance;

EVT_WDF_WMI_INSTANCE_SET_ITEM EvtWmiInstanceSetItem;

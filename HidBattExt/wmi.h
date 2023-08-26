// Where they are described.
#define MOFRESOURCENAME L"TailLightWMI"

// Initialize WMI provider
extern "C"
NTSTATUS WmiInitialize(_In_ WDFDEVICE Device);

extern "C"
EVT_WDF_WMI_INSTANCE_QUERY_INSTANCE EvtWmiInstanceQueryInstance;

extern "C"
EVT_WDF_WMI_INSTANCE_SET_INSTANCE EvtWmiInstanceSetInstance;

extern "C"
EVT_WDF_WMI_INSTANCE_SET_ITEM EvtWmiInstanceSetItem;

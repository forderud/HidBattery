// Where they are described.
#define MOFRESOURCENAME L"TailLightWMI"

/** Self-test context that counts down until completion. */
struct SELF_TEST_CONTEXT {
    ULONG State = 0;

    bool IsBusy() const {
        return (State != 0);
    }

    void Start() {
        State = 16;
    }

    bool Advance() {
        State -= 1;
        return (State != 0);
    }
};

WDF_DECLARE_CONTEXT_TYPE(SELF_TEST_CONTEXT);

// Initialize WMI provider
NTSTATUS WmiInitialize(_In_ WDFDEVICE Device);

EVT_WDF_WMI_INSTANCE_QUERY_INSTANCE EvtWmiInstanceQueryInstance;

EVT_WDF_WMI_INSTANCE_SET_INSTANCE EvtWmiInstanceSetInstance;

EVT_WDF_WMI_INSTANCE_SET_ITEM EvtWmiInstanceSetItem;

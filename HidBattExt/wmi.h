// Where they are described.
#define MOFRESOURCENAME L"TailLightWMI"

/** Self-test context that counts down until completion. */
struct SELF_TEST_CONTEXT {
    NTSTATUS Result = STATUS_UNSUCCESSFUL;
    ULONG    State = 0;

    bool IsBusy() const {
        return (State != 0);
    }

    void Start(ULONG& TailLight) {
        TailLight = 0x000000FF; // red start color

        State = STEP_COUNT;
    }

    bool Advance(ULONG& TailLight) {
        // cross-fade from red to green
        BYTE* rgb = reinterpret_cast<BYTE*>(&TailLight);
        rgb[0] = max(rgb[0] - 16, 0);   // red
        rgb[1] = min(rgb[1] + 16, 255); // green
        rgb[2]; // blue

        State -= 1;
        return (State != 0);
    }

    static const unsigned int STEP_COUNT = 16;  // how many steps to go through
    static const unsigned int DURATION_MS = 200; // duration per step
};

WDF_DECLARE_CONTEXT_TYPE(SELF_TEST_CONTEXT);

// Initialize WMI provider
NTSTATUS WmiInitialize(_In_ WDFDEVICE Device);

EVT_WDF_WMI_INSTANCE_QUERY_INSTANCE EvtWmiInstanceQueryInstance;

EVT_WDF_WMI_INSTANCE_SET_INSTANCE EvtWmiInstanceSetInstance;

EVT_WDF_WMI_INSTANCE_SET_ITEM EvtWmiInstanceSetItem;

#include <ntddk.h>
#include <wdf.h>
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>
#include <initguid.h>
#include <wdmguid.h>

// Generated WMI class definitions (from TailLight.mof)
#include "TailLightmof.h"

#include "device.h"
#include "wmi.h"
#include "vfeature.h"


/** Memory allocation tag name (for debugging leaks). */
static constexpr ULONG POOL_TAG = 'ffly';

extern "C"
DRIVER_INITIALIZE         DriverEntry;

EVT_WDF_DRIVER_UNLOAD     EvtDriverUnload;

EVT_WDF_DRIVER_DEVICE_ADD EvtDriverDeviceAdd;

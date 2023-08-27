#include <ntddk.h>
#include <wdf.h>
#define NTSTRSAFE_LIB
#include <ntstrsafe.h>
#include <initguid.h>
#include <wdmguid.h>

// Our drivers generated include from TailLight.mof
// See makefile.inc for wmi commands
#include "TailLightmof.h"

// Our drivers modules includes
#include "device.h"
#include "wmi.h"
#include "vfeature.h"


/** Memory allocation tag name (for debugging leaks). */
static constexpr ULONG TAG_NAME = 'ffly';

// WDFDRIVER Object Events
extern "C"
DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD EvtDeviceAdd;

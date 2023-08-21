#include "driver.h"

// Main driver entry, initialize the framework, register
// driver event handlers
NTSTATUS DriverEntry(
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
{
    KdPrint(("TailLight: DriverEntry - WDF version built on %s %s\n", __DATE__, __TIME__));

    WDF_DRIVER_CONFIG params = {};
    WDF_DRIVER_CONFIG_INIT(/*out*/&params, FireFlyEvtDeviceAdd);

    // Create the framework WDFDRIVER object, with the handle to it returned in Driver.
    NTSTATUS status = WdfDriverCreate(DriverObject, 
                             RegistryPath, 
                             WDF_NO_OBJECT_ATTRIBUTES, 
                             &params, 
                             WDF_NO_HANDLE); // [out]
    if (!NT_SUCCESS(status)) {
        // Framework will automatically cleanup on error Status return
        KdPrint(("TailLight: Error Creating WDFDRIVER 0x%x\n", status));
    }

    return status;
}

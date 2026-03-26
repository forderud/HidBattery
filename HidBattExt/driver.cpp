#include "driver.hpp"

/** Driver entry point.
    Initialize the framework and register driver event handlers. */
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT  DriverObject, _In_ PUNICODE_STRING RegistryPath ) {
    DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: DriverEntry\n");

    {
        // Win11 build version that made HidBattExt redundant
        OSVERSIONINFOEXW ver{};
        ver.dwOSVersionInfoSize = sizeof(ver);
        ver.dwMajorVersion = 10;
        ver.dwMinorVersion = 0;
        ver.dwBuildNumber = 29550;

        ULONGLONG ConditionMask = 0;
        VER_SET_CONDITION(ConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
        VER_SET_CONDITION(ConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);
        VER_SET_CONDITION(ConditionMask, VER_BUILDNUMBER, VER_GREATER_EQUAL);

        NTSTATUS status = RtlVerifyVersionInfo(&ver, VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER, ConditionMask);

        if (NT_SUCCESS(status)) {
            // Windows 11 build 29550 or newer
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: Driver no longer needed on Windows %d.%d.%d\n"), ver.dwMajorVersion, ver.dwMinorVersion, ver.dwBuildNumber);
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: Please uninstall this driver.\n"));
            return STATUS_NOT_SUPPORTED;
        }
    }

    WDF_DRIVER_CONFIG params = {};
    WDF_DRIVER_CONFIG_INIT(/*out*/&params, EvtDriverDeviceAdd);
    params.DriverPoolTag = POOL_TAG;
    params.EvtDriverUnload = EvtDriverUnload;

    // Create the framework WDFDRIVER object, with the handle to it returned in Driver.
    NTSTATUS status = WdfDriverCreate(DriverObject, RegistryPath, WDF_NO_OBJECT_ATTRIBUTES, &params, WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        // Framework will automatically cleanup on error Status return
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: Error Creating WDFDRIVER 0x%x"), status);
    }

    return status;
}


/** Driver unload callback.
    Used to perform operations that must take place before the driver is unloaded.  */
VOID EvtDriverUnload(_In_ WDFDRIVER Driver) {
    UNREFERENCED_PARAMETER(Driver);
    DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: DriverUnload.\n");
}

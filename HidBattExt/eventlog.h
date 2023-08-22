#pragma once
#include <ntddk.h>
#include <wdf.h>
#include "messages.h" // driver-specific MessageId values


/** Write to the Windows Event Viewer "System" log.
    InsertionStr1 is a null-terminated string. */
void WriteToSystemLog(WDFDEVICE Device, NTSTATUS MessageId, WCHAR* InsertionStr1);

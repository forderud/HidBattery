#pragma once
#include <ntddk.h>
#include <wdf.h>
#include "messages.h" // driver-specific MessageId values


/** Write to the Windows Event Viewer "System" log. */
void WriteToSystemLog(WDFDEVICE Device, NTSTATUS MessageId);

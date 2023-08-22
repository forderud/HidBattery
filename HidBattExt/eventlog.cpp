#include "eventlog.h"


void WriteToSystemLog(WDFDEVICE Device, NTSTATUS MessageId) {
    // placeholder for future data
    ULONG* DumpData = nullptr;
    USHORT DumpLen = 0;

    size_t total_size = sizeof(IO_ERROR_LOG_PACKET) + DumpLen;
    if (total_size > ERROR_LOG_MAXIMUM_SIZE) {
        // overflow check
        KdPrint(("FireFly: IoAllocateErrorLogEntry too long message.\n"));
        return;
    }

    // Log an informational event with the caller name
    DEVICE_OBJECT* dev_object = WdfDeviceWdmGetDeviceObject(Device);
    auto* entry = (IO_ERROR_LOG_PACKET*)IoAllocateErrorLogEntry(dev_object, static_cast<UCHAR>(total_size));
    if (!entry) {
        KdPrint(("FireFly: IoAllocateErrorLogEntry allocation failure.\n"));
        return;
    }

    entry->MajorFunctionCode = IRP_MJ_DEVICE_CONTROL;
    entry->RetryCount = 0;
    entry->DumpDataSize = DumpLen;
    entry->NumberOfStrings = 0; // number of insertion strings
    entry->StringOffset = 0; // insertion string offsets
    entry->EventCategory = 0; // TBD
    entry->ErrorCode = MessageId;
    //entry->UniqueErrorValue = 0; // driver-specific code (optional)
    entry->FinalStatus = STATUS_SUCCESS; // user-space error code
    //entry->SequenceNumber = 0; // IRP sequence (optional)
    //entry->IoControlCode = IoControlCode; (optional)
    //entry->DeviceOffset.QuadPart = 0; // offset in device where error occured (optional)

    if (DumpLen)
        RtlCopyMemory(/*dst*/entry->DumpData, /*src*/DumpData, DumpLen);

    // Write to windows system log.
    // The function will take over ownership of the object.
    IoWriteErrorLogEntry(entry);
}

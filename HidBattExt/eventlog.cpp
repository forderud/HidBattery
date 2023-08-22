#include "eventlog.h"


constexpr USHORT IO_ERROR_LOG_PACKET_size() {
    return sizeof(IO_ERROR_LOG_PACKET); // -8;
}


void WriteToSystemLog(WDFDEVICE Device, NTSTATUS MessageId, WCHAR* InsertionStr1) {
    // placeholder for future data
    ULONG* DumpData = nullptr;
    USHORT DumpDataLen = 0; // in bytes

    // determine length of insertion string
    UCHAR InsertionStr1Len = 0;
    if (InsertionStr1)
        InsertionStr1Len = sizeof(WCHAR)*((UCHAR)wcslen(InsertionStr1)+1); // in bytes

    size_t total_size = IO_ERROR_LOG_PACKET_size() + DumpDataLen + InsertionStr1Len;
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

    entry->MajorFunctionCode = 0; // (optional)
    entry->RetryCount = 0;
    entry->DumpDataSize = DumpDataLen;
    entry->NumberOfStrings = InsertionStr1Len ? 1 : 0;
    entry->StringOffset = IO_ERROR_LOG_PACKET_size() + DumpDataLen; // insertion string offsets
    entry->EventCategory = 0;    // TBD
    entry->ErrorCode = MessageId;
    entry->UniqueErrorValue = 0; // driver-specific code (optional)
    entry->FinalStatus = 0;      // user-space error code (optional)
    entry->SequenceNumber = 0;   // IRP sequence (optional)
    entry->IoControlCode = 0;    // (optional)
    entry->DeviceOffset.QuadPart = 0; // offset in device where error occured (optional)

    if (DumpDataLen)
        RtlCopyMemory(/*dst*/entry->DumpData, /*src*/DumpData, DumpDataLen);

    if (InsertionStr1Len)
        RtlCopyMemory(/*dst*/(BYTE*)entry + entry->StringOffset, /*src*/InsertionStr1, InsertionStr1Len);

    // Write to windows system log.
    // The function will take over ownership of the object.
    IoWriteErrorLogEntry(entry);
}

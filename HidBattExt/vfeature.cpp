#include "driver.h"
#include <hidpddi.h>
#include <hidclass.h>
#include "eventlog.h"



/** RAII wrapper of PHIDP_PREPARSED_DATA. */
class PHIDP_PREPARSED_DATA_Wrap {
public:
    PHIDP_PREPARSED_DATA_Wrap(PHIDP_PREPARSED_DATA ptr) {
        m_ptr = ptr;
    }
    ~PHIDP_PREPARSED_DATA_Wrap() {
        if (m_ptr) {
            ExFreePool(m_ptr);
            m_ptr = nullptr;
        }
    }

    operator PHIDP_PREPARSED_DATA () const {
        return m_ptr;
    }

private:
    PHIDP_PREPARSED_DATA m_ptr = nullptr;
};

/** RAII wrapper of WDFIOTARGET. */
class WDFIOTARGET_Wrap {
public:
    WDFIOTARGET_Wrap() {
    }
    ~WDFIOTARGET_Wrap() {
        if (m_obj != NULL) {
            WdfObjectDelete(m_obj);
        }
    }

    operator WDFIOTARGET () const {
        return m_obj;
    }
    WDFIOTARGET* operator & () {
        return &m_obj;
    }

private:
    WDFIOTARGET m_obj = NULL;
};


NTSTATUS SetFeatureColor (
    IN  WDFDEVICE Device,
    IN  ULONG     Color
    )
/*++
    This routine sets the HID feature by sending HID ioctls to our device.
    These IOCTLs will be handled by HIDUSB and converted into USB requests
    and send to the device.
--*/
{
    PAGED_CODE();

    KdPrint(("TailLight: SetFeatureColor\n"));

    // Preinit for error.
    WDFIOTARGET_Wrap hidTarget;
    NTSTATUS status = WdfIoTargetCreate(Device, WDF_NO_OBJECT_ATTRIBUTES, &hidTarget);
    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfIoTargetCreate failed 0x%x\n", status));
        return status;
    }

    // open in write-only mode
    DEVICE_CONTEXT* deviceContext = WdfObjectGet_DEVICE_CONTEXT(Device);
    WDF_IO_TARGET_OPEN_PARAMS openParams = {0};
    WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParams, &deviceContext->PdoName, FILE_WRITE_ACCESS);

    // We will let the framework to respond automatically to the pnp
    // state changes of the target by closing and opening the handle.
    openParams.ShareAccess = FILE_SHARE_WRITE | FILE_SHARE_READ;

    status = WdfIoTargetOpen(hidTarget, &openParams);
    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfIoTargetOpen failed 0x%x\n", status));
        return status;
    }
    

    WDF_MEMORY_DESCRIPTOR      outputDescriptor = {};
    HID_COLLECTION_INFORMATION collectionInformation = {};
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDescriptor, // out (mapped to collectionInformation)
                                      (PVOID) &collectionInformation,
                                      sizeof(HID_COLLECTION_INFORMATION));

    // Now get the collection information for this device
    status = WdfIoTargetSendIoctlSynchronously(hidTarget,
                                  NULL,
                                  IOCTL_HID_GET_COLLECTION_INFORMATION,
                                  NULL,
                                  &outputDescriptor,
                                  NULL,
                                  NULL);

    KdPrint(("TailLight: ProductID=%x, VendorID=%x, VersionNumber=%u, DescriptorSize=%u\n", collectionInformation.ProductID, collectionInformation.VendorID, collectionInformation.VersionNumber, collectionInformation.DescriptorSize));

    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfIoTargetSendIoctlSynchronously1 failed 0x%x\n", status));
        return status;
    }

    PHIDP_PREPARSED_DATA_Wrap preparsedData((PHIDP_PREPARSED_DATA)ExAllocatePool2(POOL_FLAG_NON_PAGED, collectionInformation.DescriptorSize, 'ffly'));
    if (!preparsedData) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        return status;
    }

    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDescriptor, // out (mapped to preparsedData)
                                      static_cast<PHIDP_PREPARSED_DATA>(preparsedData),
                                      collectionInformation.DescriptorSize);

    status = WdfIoTargetSendIoctlSynchronously(hidTarget,
                                  NULL,
                                  IOCTL_HID_GET_COLLECTION_DESCRIPTOR, // same as HidD_GetPreparsedData in user-mode
                                  NULL,
                                  &outputDescriptor,
                                  NULL,
                                  NULL);

    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfIoTargetSendIoctlSynchronously2 failed 0x%x\n", status));
        return status;
    }

    // Now get the capabilities.
    HIDP_CAPS caps = {};
    RtlZeroMemory(&caps, sizeof(HIDP_CAPS));

    status = HidP_GetCaps(preparsedData, &caps);
    if (!NT_SUCCESS(status)) {
        return status;
    }

    //KdPrint(("TailLight: Usage=%x, UsagePage=%x\n", caps.Usage, caps.UsagePage));

    if (caps.FeatureReportByteLength != sizeof(TailLightReport)) {
        KdPrint(("TailLight: FeatureReportByteLength mismatch (%u, %u).\n", caps.FeatureReportByteLength, sizeof(TailLightReport)));
        return status;
    }


    // Start with a zeroed report. If we are disabling the feature, this might
    // be all we need to do.
    status = STATUS_SUCCESS;

    HIDP_VALUE_CAPS valueCaps = {};
    USHORT ValueCapsLength = caps.NumberFeatureValueCaps;
    status = HidP_GetValueCaps(HidP_Feature, &valueCaps, &ValueCapsLength, preparsedData);
    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: HidP_GetValueCaps failed 0x%x\n", status));
        return status;
    }

    // Create a report to send to the device.
    TailLightReport report(valueCaps.ReportID, Color);

    WDF_MEMORY_DESCRIPTOR inputDescriptor = {};
    WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&inputDescriptor,
                                      &report,
                                      sizeof(report));
    status = WdfIoTargetSendIoctlSynchronously(hidTarget,
                                  NULL,
                                  IOCTL_HID_SET_FEATURE,
                                  &inputDescriptor,
                                  NULL,
                                  NULL,
                                  NULL);
    if (!NT_SUCCESS(status)) {
        KdPrint(("TailLight: WdfIoTargetSendIoctlSynchronously3 failed 0x%x\n", status)); 
        return status;
    }

    return status;
}


NTSTATUS SetFeatureFilter(
    _In_  WDFDEVICE  Device,
    _In_  WDFREQUEST Request
)
/*++
Routine Description:
    Handles IOCTL_HID_SET_FEATURE for all the collection.
    For control collection (custom defined collection) it handles
    the user-defined control codes for sideband communication

Arguments:
    QueueContext - The object context associated with the queue

    Request - Pointer to Request Packet.
--*/
{
    WDF_REQUEST_PARAMETERS params = {};
    WDF_REQUEST_PARAMETERS_INIT(&params);
    WdfRequestGetParameters(Request, &params);

    KdPrint(("TailLight: SetFeatureFilter: Type=0x%x (DeviceControl=0xe), InputBufferLength=%u, \n", params.Type, params.Parameters.DeviceIoControl.InputBufferLength));

    if (params.Parameters.DeviceIoControl.InputBufferLength != sizeof(TailLightReport)) {
        KdPrint(("TailLight: SetFeatureFilter: Incorrect InputBufferLength\n"));
        return STATUS_BUFFER_TOO_SMALL;
    }

    TailLightReport* packet = nullptr;
    NTSTATUS status = WdfRequestRetrieveInputBuffer(Request, sizeof(TailLightReport), (void**)&packet, NULL);
    if (!NT_SUCCESS(status) || !packet) {
        KdPrint(("TailLight: WdfRequestRetrieveInputBuffer failed 0x%x, packet=0x%x\n", status, packet));
        return status;
    }

    if (!packet->IsValid()) {
        // If collection ID is not for control collection then handle
        // this request just as you would for a regular collection.
        return STATUS_INVALID_PARAMETER;
    }

    // capture color before safety adjustments
    UCHAR r = packet->Red;
    UCHAR g = packet->Green;
    UCHAR b = packet->Blue;
    // Enforce safety limits (sets color to RED on failure)
    if (!packet->SafetyCheck()) {
        // log safety violation to Windows Event Viewer "System" log
        WCHAR color_requested[16] = {};
        swprintf(color_requested, L"%u,%u,%u", r, g, b);

        WCHAR color_adjusted[16] = {};
        swprintf(color_adjusted, L"%u,%u,%u", packet->Red, packet->Green, packet->Blue);


        WriteToSystemLog(Device, TailLight_SAFETY, color_requested, color_adjusted);
        return STATUS_CONTENT_BLOCKED;
    }

    return status;
}

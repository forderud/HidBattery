#include "driver.h"
#include <hidpddi.h> // for PHIDP_PREPARSED_DATA
#include <hidclass.h> // for HID_COLLECTION_INFORMATION
#include "CppAllocator.hpp"


/** RAII wrapper of PHIDP_PREPARSED_DATA. */
class PHIDP_PREPARSED_DATA_Wrap {
public:
    PHIDP_PREPARSED_DATA_Wrap(size_t size) {
        m_ptr = new BYTE[size];
    }
    ~PHIDP_PREPARSED_DATA_Wrap() {
        if (m_ptr) {
            delete[] m_ptr;
            m_ptr = nullptr;
        }
    }

    operator PHIDP_PREPARSED_DATA () const {
        return (PHIDP_PREPARSED_DATA)m_ptr;
    }

private:
    BYTE* m_ptr = nullptr;
};


NTSTATUS HidPdFeatureRequest(_In_ WDFDEVICE Device)
{
    DebugEnter();

    DEVICE_CONTEXT* context = WdfObjectGet_DEVICE_CONTEXT(Device);
    WDFIOTARGET_Wrap pdoTarget;
    {
        // Use PDO for HID commands instead of local IO target to avoid 0xc0000061 (STATUS_PRIVILEGE_NOT_HELD) on IOCTL_HID_SET_FEATURE
        NTSTATUS status = WdfIoTargetCreate(Device, WDF_NO_OBJECT_ATTRIBUTES, &pdoTarget);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfIoTargetCreate failed 0x%x"), status);
            return status;
        }

        // open in shared read-write mode
        WDF_IO_TARGET_OPEN_PARAMS openParams = {};
        WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(&openParams, &context->PdoName, FILE_READ_ACCESS | FILE_WRITE_ACCESS);
        // We will let the framework to respond automatically to the pnp state changes of the target by closing and opening the handle.
        openParams.ShareAccess = FILE_SHARE_WRITE | FILE_SHARE_READ;

        status = WdfIoTargetOpen(pdoTarget, &openParams);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfIoTargetOpen failed 0x%x"), status);
            return status;
        }
    }

    HID_COLLECTION_INFORMATION collectionInfo = {};
    {
        // populate "collectionInformation"
        WDF_MEMORY_DESCRIPTOR outputDesc = {};
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDesc, &collectionInfo, sizeof(HID_COLLECTION_INFORMATION));

        NTSTATUS status = WdfIoTargetSendIoctlSynchronously(pdoTarget, NULL,
            IOCTL_HID_GET_COLLECTION_INFORMATION,
            NULL, // input
            &outputDesc, // output
            NULL, NULL);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: IOCTL_HID_GET_COLLECTION_INFORMATION failed 0x%x"), status);
            return status;
        }

        //DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: ProductID=%x, VendorID=%x, VersionNumber=%u, DescriptorSize=%u\n", collectionInfo.ProductID, collectionInfo.VendorID, collectionInfo.VersionNumber, collectionInfo.DescriptorSize);
    }

    PHIDP_PREPARSED_DATA_Wrap preparsedData(collectionInfo.DescriptorSize);
    if (!preparsedData) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    {
        // populate "preparsedData"
        WDF_MEMORY_DESCRIPTOR outputDesc = {};
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDesc, static_cast<PHIDP_PREPARSED_DATA>(preparsedData), collectionInfo.DescriptorSize);

        NTSTATUS status = WdfIoTargetSendIoctlSynchronously(pdoTarget, NULL,
            IOCTL_HID_GET_COLLECTION_DESCRIPTOR, // same as HidD_GetPreparsedData in user-mode
            NULL, // input
            &outputDesc, // output
            NULL, NULL);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: IOCTL_HID_GET_COLLECTION_DESCRIPTOR failed 0x%x"), status);
            return status;
        }
    }

    {
        // get capabilities
        HIDP_CAPS caps = {};
        NTSTATUS status = HidP_GetCaps(preparsedData, &caps);
        if (!NT_SUCCESS(status)) {
            return status;
        }

        //DebugPrint(DPFLTR_INFO_LEVEL, "HidBattExt: Usage=%x, UsagePage=%x\n", caps.Usage, caps.UsagePage);

        if (caps.FeatureReportByteLength != sizeof(HidPdReport)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: FeatureReportByteLength mismatch (%u, %Iu)."), caps.FeatureReportByteLength, sizeof(HidPdReport));
            return status;
        }
    }

    {
        // Get HidPdReport from device.
        HidPdReport report(HidPdReport::Temperature);

        WDF_MEMORY_DESCRIPTOR outputDesc = {};
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDesc, &report, sizeof(report));

        NTSTATUS status = WdfIoTargetSendIoctlSynchronously(pdoTarget, NULL,
            IOCTL_HID_GET_FEATURE,
            NULL, // input
            &outputDesc, // output
            NULL, NULL);
        if (!NT_SUCCESS(status)) {
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: IOCTL_HID_GET_FEATURE failed 0x%x"), status);
            return status;
        }

        context->State.Temperature = report.Temperature;
        report.Print("IOCTL_HID_GET_FEATURE");
    }
    {
        // Get HidPdReport from device.
        HidPdReport report(HidPdReport::CycleCount);

        WDF_MEMORY_DESCRIPTOR outputDesc = {};
        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputDesc, &report, sizeof(report));

        NTSTATUS status = WdfIoTargetSendIoctlSynchronously(pdoTarget, NULL,
            IOCTL_HID_GET_FEATURE,
            NULL, // input
            &outputDesc, // output
            NULL, NULL);
        if (!NT_SUCCESS(status)) {
            // IOCTL_HID_SET_FEATURE fails with 0xc0000061 (STATUS_PRIVILEGE_NOT_HELD) if using the local IO target (WdfDeviceGetIoTarget)
            DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: IOCTL_HID_GET_FEATURE failed 0x%x"), status);
            return status;
        }

        context->State.Temperature = report.CycleCount;
        report.Print("IOCTL_HID_GET_FEATURE");
    }

    DebugExit();
    return STATUS_SUCCESS;
}


NTSTATUS HidGetFeatureFilter(
    _In_ WDFDEVICE  Device,
    _In_ WDFREQUEST Request,
    _In_ size_t     OutputBufferLength
)
{
    DebugEnter();
    UNREFERENCED_PARAMETER(Device);

    if (OutputBufferLength < sizeof(HidPdReport)) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: HidGetFeatureFilter: Too small OutputBufferLength"));
        return STATUS_BUFFER_TOO_SMALL;
    }

    HidPdReport* packet = nullptr;
    NTSTATUS status = WdfRequestRetrieveOutputBuffer(Request, sizeof(HidPdReport), (void**)&packet, NULL);
    if (!NT_SUCCESS(status) || !packet) {
        DebugPrint(DPFLTR_ERROR_LEVEL, DML_ERR("HidBattExt: WdfRequestRetrieveOutputBuffer failed 0x%x, packet=0x%p"), status, packet);
        return status;
    }

    DEVICE_CONTEXT* context = WdfObjectGet_DEVICE_CONTEXT(Device);
    NT_ASSERTMSG("HidGetFeatureFilter context NULL\n", context);

    // capture shared state
    if (packet->ReportId == HidPdReport::CycleCount)
        context->State.CycleCount = packet->CycleCount;
    else if (packet->ReportId == HidPdReport::Temperature)
        context->State.Temperature = packet->Temperature;

    // capture color before safety adjustments
    packet->Print("HidGetFeatureFilter");

    DebugExitStatus(status);
    return status;
}

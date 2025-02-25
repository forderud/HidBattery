#pragma once
#include "HidPdReport.h"
#include <hidpddi.h> // for PHIDP_PREPARSED_DATA


/** RAII wrapper of WDFIOTARGET. */
class WDFIOTARGET_Wrap {
public:
    WDFIOTARGET_Wrap() {
    }
    ~WDFIOTARGET_Wrap() {
        if (m_obj != NULL) {
            WdfObjectDelete(m_obj);
            m_obj = NULL;
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


_Function_class_(EVT_WDF_TIMER)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID HidPdFeatureRequestTimer(_In_ WDFTIMER  Timer);


EVT_WDF_IO_QUEUE_IO_READ           EvtIoReadHidFilter;
#ifdef HID_IOCTL_FILTER
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtIoDeviceControlHidFilter;
#endif

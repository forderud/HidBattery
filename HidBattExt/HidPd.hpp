#pragma once
#include "HidPdReport.h"


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


VOID HidPdFeatureRequestTimer(_In_ WDFTIMER  Timer);


EVT_WDF_IO_QUEUE_IO_READ           EvtIoReadHidFilter;
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL EvtIoDeviceControlHidFilter;

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


_Function_class_(EVT_WDF_TIMER)
_IRQL_requires_same_
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID HidPdFeatureRequestTimer(_In_ WDFTIMER  Timer);


EVT_WDF_IO_QUEUE_IO_READ           EvtIoReadHidFilter;

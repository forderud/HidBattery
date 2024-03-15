#pragma once
#include "TailLight.h"


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


NTSTATUS SetFeatureColor (
    _In_  WDFDEVICE Device,
    _In_  ULONG     Color
    );

NTSTATUS SetFeatureFilter(
    _In_ WDFDEVICE  Device,
    _In_ WDFREQUEST Request,
    _In_ size_t     InputBufferLength
);

#pragma once

/* This codes assumes that POOL_TAG have already been defined if building in kernel-mode. */

#ifdef _KERNEL_MODE

/** Templatized RAII array class for physical RAM allocations that won't be paged out (non-paged). */
template<class T>
class RamArray {
public:
    RamArray(ULONG size) : m_size(size) {
        // allocate in non-paged pool (will always reside in RAM)
        m_ptr = (T*)ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(T)*size, POOL_TAG);
    }

    ~RamArray() {
        if (m_ptr) {
            ExFreePoolWithTag(m_ptr, POOL_TAG);
            m_ptr = nullptr;
        }
    }

    operator T* () {
        return m_ptr;
    }

    ULONG ByteSize() const {
        return sizeof(T)*m_size;
    }

private:
    ULONG m_size = 0;
    T*    m_ptr = nullptr;
};

#endif // _KERNEL_MODE

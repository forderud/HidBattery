#pragma once

/* This codes assumes that POOL_TAG have already been defined if building in kernel-mode. */

#ifdef _KERNEL_MODE
void* operator new (size_t size) noexcept {
    // allocate in non-paged pool (will always reside in RAM)
    return ExAllocatePool2(POOL_FLAG_NON_PAGED, size, POOL_TAG);
}

_Ret_writes_maybenull_(size)
void* operator new[] (size_t size) noexcept {
    // allocate in non-paged pool (will always reside in RAM)
    return ExAllocatePool2(POOL_FLAG_NON_PAGED, size, POOL_TAG);
}

void operator delete (void *ptr, size_t /*size*/) noexcept {
    return ExFreePoolWithTag(ptr, POOL_TAG);
}

void operator delete[] (void* ptr) noexcept {
    return ExFreePoolWithTag(ptr, POOL_TAG);
}

#endif // _KERNEL_MODE

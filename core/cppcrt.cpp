#include <stddef.h>

// C++ global constructors
typedef void (*constructor_t)();
extern "C" constructor_t _init_array_start[];
extern "C" constructor_t _init_array_end[];

// Call global constructors
extern "C" void init_cppcrt() {
    const size_t count = _init_array_end - _init_array_start;
    
    for (size_t i = 0; i < count; i++) {
        _init_array_start[i]();
    }
}

// Override new/delete operators

void* operator new(size_t size) {
    // Currently, just a placeholder - will be hooked to kernel heap
    // in a future implementation
    return nullptr;
}

void* operator new[](size_t size) {
    return operator new(size);
}

void operator delete(void* p) {
    // Currently, just a placeholder - will be hooked to kernel heap
    // in a future implementation
}

void operator delete[](void* p) {
    operator delete(p);
}

// These versions are for when the size is known
void operator delete(void* p, size_t size) {
    operator delete(p);
}

void operator delete[](void* p, size_t size) {
    operator delete(p);
}

// Pure virtual function handler
extern "C" void __cxa_pure_virtual() {
    // Just hang the system if a pure virtual is called
    for(;;);
}

// C++ exception handling stubs
extern "C" void __cxa_atexit(void (*f)(void *), void *p, void *d) {
    // Currently, just a stub
}

extern "C" int __cxa_guard_acquire(unsigned long long *guard_object) {
    // If the low byte is 0, we need to initialize
    return !((*guard_object) & 1);
}

extern "C" void __cxa_guard_release(unsigned long long *guard_object) {
    // Set the low byte to 1 to indicate initialization is done
    *guard_object |= 1;
}

extern "C" void __cxa_guard_abort(unsigned long long *guard_object) {
    // Nothing to do in the current implementation
} 
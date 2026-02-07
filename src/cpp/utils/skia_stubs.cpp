#include <stdint.h>

#include "include/core/SkTypeface.h"
#include "include/private/base/SkAPI.h"
#include "include/private/base/SkSemaphore.h"

// SkSemaphore
SkSemaphore::~SkSemaphore() {}
void SkSemaphore::osSignal(int n) {}
void SkSemaphore::osWait() {}

// Thread ID
int64_t SkGetThreadID() { return 0; }

// SkSL
namespace SkSL {
class DebugTracePriv {
   public:
    virtual ~DebugTracePriv() {}
};
}  // namespace SkSL
SkSL::DebugTracePriv *force_vtable_sksl = nullptr;

// SJLJ
extern "C" {
int saveSetjmp(void *env, int label, void *stackPtr, int value) { return 0; }
int testSetjmp(void *env, int label, int value) { return 0; }
}

// -----------------------------------------------------------------------------
// SkOTUtils Fix
// The error shows the expected signature is (i32, i32) -> void (likely a
// return-by-value-pointer) We define it as a weak symbol to satisfy the linker
// if the real one is missing.
// -----------------------------------------------------------------------------
extern "C" {
// This matches the mangled name from the error exactly
// Signature: void MakeForFamilyNames(const SkTypeface&, LocalizedStrings**)
void _ZN9SkOTUtils26LocalizedStrings_NameTable18MakeForFamilyNamesERK10SkTypeface(
    void *result_ptr, const void *typeface) __attribute__((weak));

void _ZN9SkOTUtils26LocalizedStrings_NameTable18MakeForFamilyNamesERK10SkTypeface(
    void *result_ptr, const void *typeface) {
    // Just set the output pointer to null
    if (result_ptr) {
        *(void **)result_ptr = nullptr;
    }
}
}
#include <stdint.h>
#include "include/private/base/SkSemaphore.h"

// SkSemaphore
// These are needed because SkSemaphore.cpp is explicitly excluded from the build
// to avoid OS-specific dependencies.
SkSemaphore::~SkSemaphore() {}
void SkSemaphore::osSignal(int n) {}
void SkSemaphore::osWait() {}

// Thread ID
// Needed because SkThread implementation is excluded for single-threaded WASM.
int64_t SkGetThreadID() { return 0; }

// SkSL Debug Stub
// Satisfies vtable requirements for DebugTracePriv which is used in some core headers.
namespace SkSL {
class DebugTracePriv {
   public:
    virtual ~DebugTracePriv() {}
};
}  // namespace SkSL
SkSL::DebugTracePriv *force_vtable_sksl = nullptr;

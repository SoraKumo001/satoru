#include <stdint.h>

// Thread ID
uint32_t SkGetThreadID() { return 0; }

// SkSL stubs
namespace SkSL {
    class DebugTracePriv {
    public:
        virtual ~DebugTracePriv() {}
    };
}
// Ensure symbols exist
SkSL::DebugTracePriv* force_vtable_sksl = nullptr;

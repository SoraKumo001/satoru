#include <stdint.h>

#include "include/private/base/SkSemaphore.h"

// SkSemaphore
SkSemaphore::~SkSemaphore() {}
void SkSemaphore::osSignal(int n) {}
void SkSemaphore::osWait() {}

// Thread ID
int64_t SkGetThreadID() { return 0; }

// -----------------------------------------------------------------------------
// Skia Stubs for Aggressive Optimization (-Oz)
// Must match the EXACT signatures expected by the linker to avoid wasm-opt crash.
// -----------------------------------------------------------------------------

extern "C" {
// (i32, i32) -> i32
int _ZNK15SkRuntimeEffect9findChildENSt3__217basic_string_viewIcNS0_11char_traitsIcEEEE(void* a,
                                                                                        void* b) {
    return 0;
}
int _ZNK15SkRuntimeEffect11findUniformENSt3__217basic_string_viewIcNS0_11char_traitsIcEEEE(
    void* a, void* b) {
    return 0;
}

// (i32, f32) -> i32
int _ZN11GrPathUtils15cubicPointCountEPK7SkPointf(void* a, float b) { return 0; }
int _ZN11GrPathUtils19quadraticPointCountEPK7SkPointf(void* a, float b) { return 0; }

// (i32, i32, i32, f32, i32, i32) -> i32
int _ZN11GrPathUtils23generateQuadraticPointsERK7SkPointS2_S2_fPPS0_j(void* a, void* b, void* c,
                                                                      float d, void* e, int f) {
    return 0;
}

// (i32, i32, i32, i32, f32, i32, i32) -> i32
int _ZN11GrPathUtils19generateCubicPointsERK7SkPointS2_S2_S2_fPPS0_j(void* a, void* b, void* c,
                                                                     void* d, float e, void* f,
                                                                     int g) {
    return 0;
}

// (i32) -> i32
int _ZNK13GrStyledShape15unstyledKeySizeEv(void* a) { return 0; }
int _ZN6SkMeshD1Ev(void* a) { return 0; }
int _ZNK15SkRuntimeEffect7Uniform11sizeInBytesEv(void* a) { return 0; }
int _ZNK15SkRuntimeEffect6sourceEv(void* a) { return 0; }
int _ZNK15SkRuntimeEffect11uniformSizeEv(void* a) { return 0; }

// Others
void _ZN15SkRuntimeEffect13MakeForShaderE8SkStringRKNS_7OptionsE() {}
void _ZN15SkRuntimeEffect14MakeForBlenderE8SkStringRKNS_7OptionsE() {}
void _ZN15SkRuntimeEffect18MakeForColorFilterE8SkStringRKNS_7OptionsE() {}
void _ZN15SkRuntimeEffect20RegisterFlattenablesEv() {}
void _ZNK15SkRuntimeEffect10makeShaderE5sk_spIK6SkDataE6SkSpanIKNS_8ChildPtrEEPK8SkMatrix() {}
void _ZNK15SkRuntimeEffect11makeBlenderE5sk_spIK6SkDataE6SkSpanIKNS_8ChildPtrEE() {}
void _ZNK15SkRuntimeEffect15makeColorFilterE5sk_spIK6SkDataE() {}
void _ZNK22SkRuntimeEffectBuilder10makeShaderEPK8SkMatrix() {}
void _Z25SkMakeCachedRuntimeEffectPFN15SkRuntimeEffect6ResultE8SkStringRKNS_7OptionsEES1_() {}
void _ZN6SkMeshC1ERKS_() {}
void _ZN13GrStyledShape8simplifyEv() {}
void _ZNK13GrStyledShape16writeUnstyledKeyEPj() {}

// SkOTUtils
void _ZN9SkOTUtils26LocalizedStrings_NameTable18MakeForFamilyNamesERK10SkTypeface(
    void* result_ptr, const void* typeface) {
    if (result_ptr) {
        *(void**)result_ptr = nullptr;
    }
}
}

namespace SkSL {
class DebugTracePriv {
   public:
    virtual ~DebugTracePriv() {}
};
}  // namespace SkSL
SkSL::DebugTracePriv* force_vtable_sksl = nullptr;

#include "pdf_merger.h"
#include <stdexcept>

namespace satoru {
    std::vector<uint8_t> merge_pdf_binaries(const std::vector<const uint8_t*>& data_ptrs, 
                                           const std::vector<size_t>& sizes) {
        // PDF merging is currently disabled in WASM build due to QPDF dependency complexity.
        // Return an empty vector or the first PDF if available as a placeholder.
        if (data_ptrs.empty()) return {};
        
        // Just return the first PDF for now (not a real merge, but avoids crash)
        return std::vector<uint8_t>(data_ptrs[0], data_ptrs[0] + sizes[0]);
    }
}

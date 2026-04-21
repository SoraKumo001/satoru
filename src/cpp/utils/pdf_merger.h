#ifndef SATORU_PDF_MERGER_H
#define SATORU_PDF_MERGER_H

#include <cstdint>
#include <memory>
#include <vector>

namespace satoru {
/**
 * Merges multiple PDF binaries into a single PDF binary using QPDF.
 */
std::vector<uint8_t> merge_pdf_binaries(const std::vector<const uint8_t*>& data_ptrs,
                                        const std::vector<size_t>& sizes);
}  // namespace satoru

#endif

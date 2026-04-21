#include "pdf_merger.h"

#include <qpdf/BufferInputSource.hh>
#include <qpdf/QPDF.hh>
#include <qpdf/QPDFPageDocumentHelper.hh>
#include <qpdf/QPDFWriter.hh>

namespace satoru {

std::vector<uint8_t> merge_pdf_binaries(const std::vector<const uint8_t*>& data_ptrs,
                                        const std::vector<size_t>& sizes) {
    if (data_ptrs.empty()) return {};
    if (data_ptrs.size() == 1) {
        return std::vector<uint8_t>(data_ptrs[0], data_ptrs[0] + sizes[0]);
    }

    try {
        QPDF merged_qpdf;
        merged_qpdf.emptyPDF();
        QPDFPageDocumentHelper merged_helper(merged_qpdf);

        for (size_t i = 0; i < data_ptrs.size(); ++i) {
            // QPDF Buffer doesn't own memory by default if created this way
            Buffer* buf = new Buffer(const_cast<unsigned char*>(data_ptrs[i]), sizes[i]);
            auto input_source = std::shared_ptr<InputSource>(new BufferInputSource(
                "pdf_part_" + std::to_string(i), buf,
                true  // own_memory = true, so it will delete buf
                ));

            QPDF part_qpdf;
            part_qpdf.processInputSource(input_source);
            QPDFPageDocumentHelper part_helper(part_qpdf);

            std::vector<QPDFPageObjectHelper> pages = part_helper.getAllPages();
            for (auto& page : pages) {
                merged_helper.addPage(page, false);
            }
        }

        QPDFWriter writer(merged_qpdf);
        writer.setOutputMemory();
        writer.write();

        Buffer* buffer = writer.getBuffer();
        const uint8_t* buf_data = reinterpret_cast<const uint8_t*>(buffer->getBuffer());
        std::vector<uint8_t> result(buf_data, buf_data + buffer->getSize());
        delete buffer;
        return result;

    } catch (const std::exception& e) {
        // In a real production app, we might want to log this error via SATORU_LOG_ERROR
        return {};
    }
}

}  // namespace satoru

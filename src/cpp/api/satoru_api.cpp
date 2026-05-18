#include "satoru_api.h"

#include <emscripten.h>
#include <stdio.h>

#include <chrono>
#include <cstdarg>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <vector>

#include "core/container_skia.h"
#include "core/master_css.h"
#include "core/resource_manager.h"
#include "core/satoru_context.h"
#include "renderers/pdf_renderer.h"
#include "renderers/png_renderer.h"
#include "renderers/svg_renderer.h"
#include "renderers/webp_renderer.h"
#include "utils/pdf_merger.h"

// --- Logging ---
static LogLevel g_log_level = LogLevel::None;

EM_JS(void, satoru_log_js, (int level, const char* message), {
    if (Module.onLog) {
        try {
            Module.onLog(level, UTF8ToString(message));
        } catch (e) {
            // Silently ignore log errors to prevent Wasm crash
        }
    }
});

void satoru_log(LogLevel level, const char* message) {
    if (level <= g_log_level) {
        satoru_log_js((int)level, message);
    }
}

void satoru_log_printf(LogLevel level, const char* format, ...) {
    if (level <= g_log_level) {
        va_list args;
        va_start(args, format);

        va_list args_copy;
        va_copy(args_copy, args);
        int size = vsnprintf(nullptr, 0, format, args_copy);
        va_end(args_copy);

        if (size >= 0) {
            std::vector<char> buffer(size + 1);
            vsnprintf(buffer.data(), buffer.size(), format, args);
            // Ensure any invalid UTF-8 at the end (from truncation elsewhere) is handled
            // although here we are not truncating.
            satoru_log_js((int)level, buffer.data());
        }

        va_end(args);
    }
}

// --- Helpers ---
namespace {
std::string json_escape(const std::string& s) {
    std::string result;
    result.reserve(s.size() + 16);
    for (unsigned char c : s) {
        if (c == '"')
            result += "\\\"";
        else if (c == '\\')
            result += "\\\\";
        else if (c <= 0x1f) {
            char buf[7];
            snprintf(buf, sizeof(buf), "\\u%04x", c);
            result += buf;
        } else {
            result += (char)c;
        }
    }
    return result;
}

typedef sk_sp<SkData> SkDataPtr;

template <typename F>
const uint8_t* render_and_store(SatoruInstance* inst, F render_func,
                                void (SatoruContext::*setter)(sk_sp<SkData>), int& out_size) {
    auto data = render_func();
    if (!data) {
        out_size = 0;
        return nullptr;
    }
    out_size = (int)data->size();
    const uint8_t* bytes = data->bytes();
    (inst->context.*setter)(std::move(data));
    return bytes;
}

std::string codepoints_to_utf8(std::vector<char32_t>& cps) {
    std::sort(cps.begin(), cps.end());
    cps.erase(std::unique(cps.begin(), cps.end()), cps.end());
    std::string res;
    res.reserve(cps.size() * 3);
    for (char32_t cp : cps) {
        if (cp <= 0x7F) {
            res += (char)cp;
        } else if (cp <= 0x7FF) {
            res += (char)(0xC0 | (cp >> 6));
            res += (char)(0x80 | (cp & 0x3F));
        } else if (cp <= 0xFFFF) {
            res += (char)(0xE0 | (cp >> 12));
            res += (char)(0x80 | ((cp >> 6) & 0x3F));
            res += (char)(0x80 | (cp & 0x3F));
        } else if (cp <= 0x10FFFF) {
            res += (char)(0xF0 | (cp >> 18));
            res += (char)(0x80 | ((cp >> 12) & 0x3F));
            res += (char)(0x80 | ((cp >> 6) & 0x3F));
            res += (char)(0x80 | (cp & 0x3F));
        }
    }
    return res;
}
}  // namespace

// --- SatoruInstance Implementation ---

SatoruInstance::SatoruInstance() : resourceManager(context) {
    context.init();
    cached_full_master_css = std::string(litehtml::master_css) + "\n" + satoru_master_css +
                             "\nbr { display: -litehtml-br !important; }\n";
}

SatoruInstance::~SatoruInstance() {}

const std::string& SatoruInstance::get_full_master_css() const { return cached_full_master_css; }

void SatoruInstance::init_document(const char* html, int width, int height) {
    int initial_height = (height > 0) ? height : 3000;
    render_container = std::make_unique<container_skia>(width, initial_height, nullptr, context,
                                                        &resourceManager, false);

    std::string master_css = get_full_master_css() + "\nbr { display: -litehtml-br !important; }\n";
    std::string user_css = context.getExtraCss();
    doc = litehtml::document::createFromString(html, render_container.get(), master_css.c_str(),
                                               user_css.c_str());
    if (render_container) {
        render_container->set_document(doc.get());
    }
}

void SatoruInstance::layout_document(int width) {
    if (doc && width != last_width) {
        doc->render(width);
        last_width = width;
        if (render_container) {
            render_container->set_height(doc->height());
        }
    }
}

static void scan_image_sizes(litehtml::element::ptr el, SatoruContext& context) {
    if (!el) return;
    const char* tag = el->get_tagName();
    if (tag && (strcmp(tag, "img") == 0 || strcmp(tag, "image") == 0)) {
        const char* src = el->get_attr("src");
        if (!src) src = el->get_attr("href");
        if (!src) src = el->get_attr("xlink:href");

        const char* w_str = el->get_attr("width");
        const char* h_str = el->get_attr("height");
        if (src && w_str && h_str) {
            int w = atoi(w_str);
            int h_val = atoi(h_str);
            if (w > 0 && h_val > 0) {
                int curr_w, curr_h;
                if (!context.get_image_size(src, curr_w, curr_h) || (curr_w == 0 && curr_h == 0)) {
                    context.loadImage(src, nullptr, w, h_val);
                }
            }
        }
    }
    for (auto& child : el->children()) {
        scan_image_sizes(child, context);
    }
}

void SatoruInstance::collect_resources(const std::string& html, int width, int height,
                                       int mediaType) {
    auto elapsed_ms = [](const auto& start, const auto& end) {
        return std::chrono::duration<double, std::milli>(end - start).count();
    };
    profile_scan_font_faces_ms = 0.0;
    profile_create_document_ms = 0.0;
    profile_render_layout_ms = 0.0;
    profile_scan_image_sizes_ms = 0.0;
    profile_font_requests_ms = 0.0;
    profile_requested_font_count = 0;
    profile_font_url_count = 0;
    profile_font_requests_with_urls_count = 0;
    profile_provider_font_request_count = 0;
    profile_mapped_font_url_request_count = 0;
    profile_font_character_count = 0;
    profile_measured_font_character_count = 0;
    profile_rebuild_count = 0;
    profile_rebuild_initial_count = 0;
    profile_rebuild_html_count = 0;
    profile_rebuild_css_count = 0;
    profile_rebuild_font_count = 0;
    profile_rebuild_media_count = 0;
    profile_layout_count = 0;
    profile_layout_size_count = 0;
    profile_layout_relayout_count = 0;
    context.layoutProfile.enabled = collect_profile_enabled;
    context.layoutProfile.reset();

    try {
        litehtml::media_type mt =
            (mediaType == 1) ? litehtml::media_type_print : litehtml::media_type_screen;
        uint64_t cssVersion = context.getCssVersion();
        bool rebuild_initial = !doc;
        bool rebuild_html = html != last_parsed_html;
        bool rebuild_css = cssVersion != last_css_version;
        bool rebuild_font = context.getFontVersion() != last_font_version;
        bool rebuild_media = mt != (litehtml::media_type)last_media_type;
        if (rebuild_initial || rebuild_html || rebuild_css || rebuild_font || rebuild_media) {
            bool is_first_pass = (doc == nullptr);
            if (collect_profile_enabled) {
                profile_rebuild_count++;
                if (rebuild_initial) profile_rebuild_initial_count++;
                if (rebuild_html) profile_rebuild_html_count++;
                if (rebuild_css) profile_rebuild_css_count++;
                if (rebuild_font) profile_rebuild_font_count++;
                if (rebuild_media) profile_rebuild_media_count++;
            }

            doc.reset();  // Destroy doc first so it doesn't use the old container!
            last_parsed_html = html;
            last_extra_css_size = context.getExtraCss().size();
            last_css_version = cssVersion;
            last_font_version = context.getFontVersion();
            last_image_version = context.getImageVersion();
            last_width = -1;  // Force re-layout
            last_media_type = (int)mt;
            image_sizes_scanned = false;
            int initial_height = (height > 0) ? height : 3000;
            render_container = std::make_unique<container_skia>(
                width, initial_height, nullptr, context, &resourceManager, false, mt);

            if (html != last_font_face_scan_html) {
                auto scan_start = std::chrono::high_resolution_clock::time_point{};
                if (collect_profile_enabled) scan_start = std::chrono::high_resolution_clock::now();
                context.fontManager.scanFontFaces(html.c_str());
                if (collect_profile_enabled) {
                    auto scan_end = std::chrono::high_resolution_clock::now();
                    profile_scan_font_faces_ms += elapsed_ms(scan_start, scan_end);
                }
                last_font_face_scan_html = html;
            }

            auto create_start = std::chrono::high_resolution_clock::time_point{};
            if (collect_profile_enabled) create_start = std::chrono::high_resolution_clock::now();
            doc = litehtml::document::createFromString(html.c_str(), render_container.get(),
                                                       get_full_master_css().c_str(),
                                                       context.getExtraCss().c_str());
            if (collect_profile_enabled) {
                auto create_end = std::chrono::high_resolution_clock::now();
                profile_create_document_ms += elapsed_ms(create_start, create_end);
            }
            if (render_container) {
                render_container->set_document(doc.get());
            }

            if (is_first_pass && !image_sizes_scanned) {
                // For the first pass, scan images WITHOUT full render to start loading them early
                auto scan_images_start = std::chrono::high_resolution_clock::time_point{};
                if (collect_profile_enabled)
                    scan_images_start = std::chrono::high_resolution_clock::now();
                scan_image_sizes(doc->root(), context);
                if (collect_profile_enabled) {
                    auto scan_images_end = std::chrono::high_resolution_clock::now();
                    profile_scan_image_sizes_ms += elapsed_ms(scan_images_start, scan_images_end);
                }
                image_sizes_scanned = true;
            }
        }

        if (doc) {
            if (width != last_width || height != last_height || context.needsRelayout) {
                if (collect_profile_enabled) {
                    profile_layout_count++;
                    if (width != last_width || height != last_height) profile_layout_size_count++;
                    if (context.needsRelayout) profile_layout_relayout_count++;
                }
                auto render_start = std::chrono::high_resolution_clock::time_point{};
                if (collect_profile_enabled)
                    render_start = std::chrono::high_resolution_clock::now();
                doc->render(width);
                if (collect_profile_enabled) {
                    auto render_end = std::chrono::high_resolution_clock::now();
                    profile_render_layout_ms += elapsed_ms(render_start, render_end);
                }
                last_width = width;
                last_height = height;
                context.needsRelayout = false;
                if (render_container) {
                    render_container->set_height(doc->height());
                }
            }
            if (!image_sizes_scanned) {
                auto scan_images_start = std::chrono::high_resolution_clock::time_point{};
                if (collect_profile_enabled)
                    scan_images_start = std::chrono::high_resolution_clock::now();
                scan_image_sizes(doc->root(), context);
                if (collect_profile_enabled) {
                    auto scan_images_end = std::chrono::high_resolution_clock::now();
                    profile_scan_image_sizes_ms += elapsed_ms(scan_images_start, scan_images_end);
                }
                image_sizes_scanned = true;
            }
        }
    } catch (const std::exception& e) {
        satoru_log_printf(LogLevel::Error, "Exception in collect_resources: %s", e.what());
        throw;
    } catch (...) {
        satoru_log_printf(LogLevel::Error, "Unknown exception in collect_resources");
        throw;
    }

    auto font_requests_start = std::chrono::high_resolution_clock::time_point{};
    if (collect_profile_enabled) font_requests_start = std::chrono::high_resolution_clock::now();
    const auto& usedCodepoints = render_container->get_used_codepoints();
    auto requestedAttribs = render_container->get_requested_font_attributes();

    // Ensure sans-serif is always requested as a fallback if not already present
    bool hasSansSerif = false;
    for (const auto& req : requestedAttribs) {
        if (req.family == "sans-serif") {
            hasSansSerif = true;
            break;
        }
    }
    if (!hasSansSerif) {
        font_request req;
        req.family = "sans-serif";
        req.weight = 400;
        req.slant = SkFontStyle::kUpright_Slant;
        requestedAttribs.insert(req);
    }

    if (collect_profile_enabled) profile_requested_font_count = (int)requestedAttribs.size();
    for (const auto& req : requestedAttribs) {
        std::string charactersStr;
        std::vector<char32_t> usedFontCharacters;
        render_container->collect_used_font_characters(req, usedFontCharacters);
        const std::set<char32_t>* measuredFontCodepoints =
            usedFontCharacters.empty() ? render_container->get_measured_font_codepoints(req)
                                       : nullptr;
        if (!usedFontCharacters.empty()) {
            charactersStr = codepoints_to_utf8(usedFontCharacters);
        }
        if (collect_profile_enabled) {
            profile_font_character_count += (int)usedFontCharacters.size();
            if (measuredFontCodepoints) {
                profile_measured_font_character_count += (int)measuredFontCodepoints->size();
            }
        }

        std::set<char32_t> usedFontCodepoints;
        const std::set<char32_t>* fontUrlCodepoints = &usedCodepoints;
        if (!usedFontCharacters.empty()) {
            usedFontCodepoints.insert(usedFontCharacters.begin(), usedFontCharacters.end());
            fontUrlCodepoints = &usedFontCodepoints;
        } else if (measuredFontCodepoints) {
            fontUrlCodepoints = measuredFontCodepoints;
        }
        std::vector<std::string> urls =
            context.fontManager.getFontUrls(req.family, req.weight, req.slant, fontUrlCodepoints);
        if (collect_profile_enabled) {
            profile_font_url_count += (int)urls.size();
            if (!urls.empty()) profile_font_requests_with_urls_count++;
        }

        if (urls.empty()) {
            auto loaded = context.fontManager.matchFonts(req.family, req.weight, req.slant);
            bool needRequest = loaded.empty();

            if (!loaded.empty()) {
                // If the loaded font has a significantly different weight, we might still want to
                // request a specific weight from the font map.
                int loadedWeight = loaded[0]->fontStyle().weight();
                int loadedSlant = loaded[0]->fontStyle().slant();
                if (std::abs(loadedWeight - req.weight) > 100 || loadedSlant != req.slant) {
                    needRequest = true;
                }
            }

            if (needRequest) {
                const auto& fontMap = context.getFontMap();
                auto it = fontMap.find(req.family);
                if (it != fontMap.end()) {
                    const std::string& mapped = it->second;
                    if (mapped.substr(0, 7) == "http://" || mapped.substr(0, 8) == "https://") {
                        if (collect_profile_enabled) profile_mapped_font_url_request_count++;
                        resourceManager.request(mapped, req.family, ResourceType::Font, false,
                                                charactersStr);
                    } else {
                        // Treat as font family name for Google Fonts
                        std::string providerUrl =
                            "provider:google-fonts?family=" + mapped +
                            "&weight=" + std::to_string(req.weight) +
                            "&italic=" + (req.slant == SkFontStyle::kItalic_Slant ? "1" : "0");

                        if (!charactersStr.empty()) {
                            providerUrl += "&text=" + charactersStr;
                        }

                        resourceManager.request(providerUrl, req.family, ResourceType::Font, false,
                                                charactersStr);
                        if (collect_profile_enabled) profile_provider_font_request_count++;
                    }
                }
            }
        } else {
            for (const auto& url : urls) {
                resourceManager.request(url, req.family, ResourceType::Font, false, charactersStr);
            }
        }
    }
    if (collect_profile_enabled) {
        auto font_requests_end = std::chrono::high_resolution_clock::now();
        profile_font_requests_ms += elapsed_ms(font_requests_start, font_requests_end);
    }
}

std::string SatoruInstance::get_collect_profile_json() const {
    std::ostringstream ss;
    ss << "{\"cppScanFontFaces\":" << profile_scan_font_faces_ms
       << ",\"cppCreateDocument\":" << profile_create_document_ms
       << ",\"cppRenderLayout\":" << profile_render_layout_ms
       << ",\"cppScanImageSizes\":" << profile_scan_image_sizes_ms
       << ",\"cppFontRequests\":" << profile_font_requests_ms
       << ",\"cppRequestedFontCount\":" << profile_requested_font_count
       << ",\"cppFontUrlCount\":" << profile_font_url_count
       << ",\"cppFontRequestsWithUrlsCount\":" << profile_font_requests_with_urls_count
       << ",\"cppProviderFontRequestCount\":" << profile_provider_font_request_count
       << ",\"cppMappedFontUrlRequestCount\":" << profile_mapped_font_url_request_count
       << ",\"cppFontCharacterCount\":" << profile_font_character_count
       << ",\"cppMeasuredFontCharacterCount\":" << profile_measured_font_character_count
       << ",\"cppDocumentRebuildCount\":" << profile_rebuild_count
       << ",\"cppDocumentRebuildInitialCount\":" << profile_rebuild_initial_count
       << ",\"cppDocumentRebuildHtmlCount\":" << profile_rebuild_html_count
       << ",\"cppDocumentRebuildCssCount\":" << profile_rebuild_css_count
       << ",\"cppDocumentRebuildFontCount\":" << profile_rebuild_font_count
       << ",\"cppDocumentRebuildMediaCount\":" << profile_rebuild_media_count
       << ",\"cppLayoutCount\":" << profile_layout_count
       << ",\"cppLayoutSizeCount\":" << profile_layout_size_count
       << ",\"cppLayoutRelayoutCount\":" << profile_layout_relayout_count
       << ",\"cppCssVersion\":" << context.getCssVersion()
       << ",\"cppUserCssVersion\":" << context.getUserCssVersion()
       << ",\"cppExternalCssVersion\":" << context.getExternalCssVersion()
       << ",\"cppFontResourceCssVersion\":" << context.getFontResourceCssVersion()
       << ",\"cppFontAliasCssVersion\":" << context.getFontAliasCssVersion()
       << ",\"cppGeneratedFontFaceCssVersion\":" << context.getGeneratedFontFaceCssVersion()
       << ",\"cppFontResourceNamedLoadCount\":" << context.getFontResourceNamedLoadCount()
       << ",\"cppFontResourceFallbackLoadCount\":" << context.getFontResourceFallbackLoadCount()
       << ",\"cppFontResourceRegisteredCount\":" << context.getFontResourceRegisteredCount()
       << ",\"cppFontResourceDuplicateLoadCount\":" << context.getFontResourceDuplicateLoadCount()
       << ",\"cppGeneratedFontFaceAttemptCount\":" << context.getGeneratedFontFaceAttemptCount()
       << ",\"cppGeneratedFontFaceAddedCount\":" << context.getGeneratedFontFaceAddedCount()
       << ",\"cppGeneratedFontFaceDuplicateCount\":" << context.getGeneratedFontFaceDuplicateCount()
       << ",\"cppCreateFont\":" << context.layoutProfile.container_create_font_ms
       << ",\"cppTextWidth\":" << context.layoutProfile.container_text_width_ms
       << ",\"cppSplitText\":" << context.layoutProfile.container_split_text_ms
       << ",\"cppBidiLevel\":" << context.layoutProfile.container_bidi_ms
       << ",\"cppTextMeasure\":" << context.layoutProfile.text_measure_ms
       << ",\"cppTextAnalyze\":" << context.layoutProfile.text_analyze_ms
       << ",\"cppTextShape\":" << context.layoutProfile.text_shape_ms
       << ",\"cppTextShapePrepared\":" << context.layoutProfile.text_shape_prepared_ms
       << ",\"cppCreateFontCount\":" << context.layoutProfile.container_create_font_count
       << ",\"cppTextWidthCount\":" << context.layoutProfile.container_text_width_count
       << ",\"cppTextWidthUniqueCount\":" << context.layoutProfile.container_text_width_unique_count
       << ",\"cppTextWidthDuplicateCount\":"
       << context.layoutProfile.container_text_width_duplicate_count
       << ",\"cppSplitTextCount\":" << context.layoutProfile.container_split_text_count
       << ",\"cppBidiLevelCount\":" << context.layoutProfile.container_bidi_count
       << ",\"cppTextMeasureCount\":" << context.layoutProfile.text_measure_count
       << ",\"cppTextMeasureCacheableCount\":" << context.layoutProfile.text_measure_cacheable_count
       << ",\"cppTextMeasureCacheHitCount\":" << context.layoutProfile.text_measure_cache_hit_count
       << ",\"cppTextAnalyzeCount\":" << context.layoutProfile.text_analyze_count
       << ",\"cppTextShapeCount\":" << context.layoutProfile.text_shape_count
       << ",\"cppTextShapePreparedCount\":" << context.layoutProfile.text_shape_prepared_count
       << "}";
    return ss.str();
}

void SatoruInstance::add_resource(const std::string& url, ResourceType type,
                                  const std::vector<uint8_t>& data) {
    resourceManager.add(url.c_str(), data.data(), (int)data.size(), type);
}

void SatoruInstance::scan_css(const std::string& css) {
    if (context.addCss(css.c_str(), CssChangeKind::UserScan)) {
        context.fontManager.scanFontFaces(css.c_str());
    }
}

void SatoruInstance::load_font(const std::string& name, const std::vector<uint8_t>& data) {
    context.load_font(name.c_str(), data.data(), (int)data.size());
}

void SatoruInstance::load_image(const std::string& name, const std::string& data_url, int width,
                                int height) {
    context.load_image(name.c_str(), data_url.c_str(), width, height);
}

void SatoruInstance::load_image_pixels(const std::string& name, int width, int height,
                                       const std::vector<uint8_t>& pixels,
                                       const std::string& data_url) {
    context.loadImageFromPixels(name.c_str(), width, height, pixels.data(), data_url.c_str());
}

std::string SatoruInstance::get_pending_resources_json() {
    auto requests = resourceManager.getPendingRequests();
    if (requests.empty()) return "";

    std::stringstream ss;
    ss << "[";
    for (size_t i = 0; i < requests.size(); ++i) {
        const auto& req = requests[i];
        std::string typeStr = "font";
        if (req.type == ResourceType::Image)
            typeStr = "image";
        else if (req.type == ResourceType::Css)
            typeStr = "css";

        ss << "{\"url\":\"" << json_escape(req.url) << "\",\"name\":\"" << json_escape(req.name)
           << "\",\"characters\":\"" << json_escape(req.characters) << "\",\"type\":\"" << typeStr
           << "\",\"redraw_on_ready\":" << (req.redraw_on_ready ? "true" : "false") << "}";
        if (i < requests.size() - 1) ss << ",";
    }
    ss << "]";
    return ss.str();
}

const uint8_t* SatoruInstance::get_pending_resources_binary(int& out_size) {
    auto requests = resourceManager.getPendingRequests();
    pending_resources_buffer.clear();
    if (requests.empty()) {
        out_size = 0;
        return nullptr;
    }

    pending_resources_buffer.reserve(requests.size() * 128);

    auto write_u32 = [&](uint32_t val) {
        pending_resources_buffer.push_back((uint8_t)(val & 0xFF));
        pending_resources_buffer.push_back((uint8_t)((val >> 8) & 0xFF));
        pending_resources_buffer.push_back((uint8_t)((val >> 16) & 0xFF));
        pending_resources_buffer.push_back((uint8_t)((val >> 24) & 0xFF));
    };

    auto write_str = [&](const std::string& s) {
        write_u32((uint32_t)s.size());
        for (char c : s) pending_resources_buffer.push_back((uint8_t)c);
    };

    write_u32((uint32_t)requests.size());
    for (const auto& req : requests) {
        pending_resources_buffer.push_back((uint8_t)req.type);
        pending_resources_buffer.push_back(req.redraw_on_ready ? 1 : 0);
        write_str(req.url);
        write_str(req.name);
        write_str(req.characters);
    }

    out_size = (int)pending_resources_buffer.size();
    return pending_resources_buffer.data();
}

// --- API Functions (Wrappers) ---

SatoruInstance* api_create_instance() { return new SatoruInstance(); }

void api_destroy_instance(SatoruInstance* inst) { delete inst; }

std::string api_html_to_svg(SatoruInstance* inst, const char* html, int width, int height,
                            const RenderOptions& options) {
    return renderHtmlToSvg(html, width, height, inst->context, inst->get_full_master_css().c_str(),
                           inst->context.getExtraCss().c_str(), options);
}

const uint8_t* api_html_to_png(SatoruInstance* inst, const char* html, int width, int height,
                               const RenderOptions& options, int& out_size) {
    return render_and_store(
        inst,
        [&]() {
            return renderHtmlToPng(html, width, height, inst->context,
                                   inst->get_full_master_css().c_str(),
                                   inst->context.getExtraCss().c_str(), options);
        },
        &SatoruContext::set_last_png, out_size);
}

const uint8_t* api_html_to_webp(SatoruInstance* inst, const char* html, int width, int height,
                                const RenderOptions& options, int& out_size) {
    return render_and_store(
        inst,
        [&]() {
            return renderHtmlToWebp(html, width, height, inst->context,
                                    inst->get_full_master_css().c_str(),
                                    inst->context.getExtraCss().c_str(), options);
        },
        &SatoruContext::set_last_webp, out_size);
}

const uint8_t* api_html_to_pdf(SatoruInstance* inst, const char* html, int width, int height,
                               const RenderOptions& options, int& out_size) {
    return render_and_store(
        inst,
        [&]() {
            std::vector<std::string> htmls = {html};
            return renderHtmlsToPdf(htmls, width, height, inst->context,
                                    inst->get_full_master_css().c_str(),
                                    inst->context.getExtraCss().c_str(), options);
        },
        &SatoruContext::set_last_pdf, out_size);
}

const uint8_t* api_htmls_to_pdf(SatoruInstance* inst, const std::vector<std::string>& htmls,
                                int width, int height, const RenderOptions& options,
                                int& out_size) {
    return render_and_store(
        inst,
        [&]() {
            return renderHtmlsToPdf(htmls, width, height, inst->context,
                                    inst->get_full_master_css().c_str(),
                                    inst->context.getExtraCss().c_str(), options);
        },
        &SatoruContext::set_last_pdf, out_size);
}

const uint8_t* api_render(SatoruInstance* inst, const std::vector<std::string>& htmls, int width,
                          int height, RenderFormat format, const RenderOptions& options,
                          int& out_size) {
    if (htmls.empty()) {
        out_size = 0;
        return nullptr;
    }

    switch (format) {
        case RenderFormat::SVG: {
            std::string svg = api_html_to_svg(inst, htmls[0].c_str(), width, height, options);
            auto data = SkData::MakeWithCopy(svg.c_str(), svg.length());
            inst->context.set_last_svg(std::move(data));
            out_size = (int)inst->context.get_last_svg()->size();
            return inst->context.get_last_svg()->bytes();
        }
        case RenderFormat::PNG:
            return api_html_to_png(inst, htmls[0].c_str(), width, height, options, out_size);
        case RenderFormat::WebP:
            return api_html_to_webp(inst, htmls[0].c_str(), width, height, options, out_size);
        case RenderFormat::PDF:
            return api_htmls_to_pdf(inst, htmls, width, height, options, out_size);
    }
    out_size = 0;
    return nullptr;
}

int api_get_last_png_size(SatoruInstance* inst) { return (int)inst->context.get_last_png_size(); }
int api_get_last_webp_size(SatoruInstance* inst) { return (int)inst->context.get_last_webp_size(); }
int api_get_last_pdf_size(SatoruInstance* inst) { return (int)inst->context.get_last_pdf_size(); }
int api_get_last_svg_size(SatoruInstance* inst) { return (int)inst->context.get_last_svg_size(); }

void api_collect_resources(SatoruInstance* inst, const std::string& html, int width, int height,
                           int mediaType) {
    inst->collect_resources(html, width, height, mediaType);
}

std::string api_get_collect_profile(SatoruInstance* inst) {
    return inst ? inst->get_collect_profile_json() : "{}";
}

void api_set_collect_profile_enabled(SatoruInstance* inst, bool enabled) {
    if (inst) inst->set_collect_profile_enabled(enabled);
}

void api_add_resource(SatoruInstance* inst, const std::string& url, int type,
                      const std::vector<uint8_t>& data) {
    inst->add_resource(url, (ResourceType)type, data);
}

void api_scan_css(SatoruInstance* inst, const std::string& css) { inst->scan_css(css); }

void api_load_font(SatoruInstance* inst, const std::string& name,
                   const std::vector<uint8_t>& data) {
    inst->load_font(name, data);
}

void api_load_fallback_font(SatoruInstance* inst, const std::vector<uint8_t>& data) {
    inst->context.loadFont("__fallback__", data.data(), (int)data.size());
    auto tfs =
        inst->context.fontManager.matchFonts("__fallback__", 400, SkFontStyle::kUpright_Slant);
    if (!tfs.empty()) {
        inst->context.fontManager.addFallbackTypeface(tfs[0]);
        inst->context.markFontChanged();
    }
}

void api_load_image(SatoruInstance* inst, const std::string& name, const std::string& data_url,
                    int width, int height) {
    inst->load_image(name, data_url, width, height);
}

void api_set_font_map(SatoruInstance* inst, const std::map<std::string, std::string>& fontMap) {
    inst->context.setFontMap(fontMap);
}

void api_set_log_level(int level) { g_log_level = (LogLevel)level; }

std::string api_get_pending_resources(SatoruInstance* inst) {
    return inst->get_pending_resources_json();
}

const uint8_t* api_get_pending_resources_binary(SatoruInstance* inst, int& out_size) {
    return inst->get_pending_resources_binary(out_size);
}

void api_init_document(SatoruInstance* inst, const char* html, int width, int height) {
    inst->init_document(html, width, height);
}

void api_layout_document(SatoruInstance* inst, int width) { inst->layout_document(width); }

const uint8_t* api_render_from_state(SatoruInstance* inst, int width, int height,
                                     RenderFormat format, const RenderOptions& options,
                                     int& out_size) {
    if (!inst->doc) {
        out_size = 0;
        return nullptr;
    }

    switch (format) {
        case RenderFormat::SVG: {
            std::string svg = renderDocumentToSvg(inst, width, height, options);
            auto data = SkData::MakeWithCopy(svg.c_str(), svg.length());
            inst->context.set_last_svg(std::move(data));
            out_size = (int)inst->context.get_last_svg()->size();
            return inst->context.get_last_svg()->bytes();
        }
        case RenderFormat::PNG:
            return render_and_store(
                inst, [&]() { return renderDocumentToPng(inst, width, height, options); },
                &SatoruContext::set_last_png, out_size);
        case RenderFormat::WebP:
            return render_and_store(
                inst, [&]() { return renderDocumentToWebp(inst, width, height, options); },
                &SatoruContext::set_last_webp, out_size);
        case RenderFormat::PDF:
            return render_and_store(
                inst, [&]() { return renderDocumentToPdf(inst, width, height, options); },
                &SatoruContext::set_last_pdf, out_size);
    }
    out_size = 0;
    return nullptr;
}

const uint8_t* api_merge_pdfs(SatoruInstance* inst, const std::vector<sk_sp<SkData>>& pdfs,
                              int& out_size) {
    if (pdfs.empty()) {
        out_size = 0;
        return nullptr;
    }

    std::vector<const uint8_t*> data_ptrs;
    std::vector<size_t> sizes;
    for (auto& pdf : pdfs) {
        if (pdf) {
            data_ptrs.push_back(reinterpret_cast<const uint8_t*>(pdf->data()));
            sizes.push_back(pdf->size());
        }
    }

    auto merged = satoru::merge_pdf_binaries(data_ptrs, sizes);
    if (merged.empty()) {
        out_size = 0;
        return nullptr;
    }

    auto merged_data = SkData::MakeWithCopy(merged.data(), merged.size());
    inst->context.set_last_pdf(merged_data);
    out_size = (int)merged_data->size();
    return reinterpret_cast<const uint8_t*>(merged_data->data());
}

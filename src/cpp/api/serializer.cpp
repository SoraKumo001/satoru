#include "serializer.h"
#include <litehtml/render_item.h>

static void serialize_recursive(const std::shared_ptr<litehtml::render_item>& item, std::vector<float>& out) {
    if (!item) return;

    out.push_back((float)item->pos().x);
    out.push_back((float)item->pos().y);
    out.push_back((float)item->pos().width);
    out.push_back((float)item->pos().height);

    out.push_back((float)item->get_margins().left);
    out.push_back((float)item->get_margins().right);
    out.push_back((float)item->get_margins().top);
    out.push_back((float)item->get_margins().bottom);

    out.push_back((float)item->get_paddings().left);
    out.push_back((float)item->get_paddings().right);
    out.push_back((float)item->get_paddings().top);
    out.push_back((float)item->get_paddings().bottom);

    out.push_back((float)item->get_borders().left);
    out.push_back((float)item->get_borders().right);
    out.push_back((float)item->get_borders().top);
    out.push_back((float)item->get_borders().bottom);

    out.push_back(item->skip() ? 1.0f : 0.0f);

    for (const auto& child : item->children()) {
        serialize_recursive(child, out);
    }
}

std::vector<float> Serializer::serialize_layout(const std::shared_ptr<litehtml::render_item>& root) {
    std::vector<float> out;
    if (root) {
        // Reserve some space to avoid reallocations, assumption based on tree depth
        out.reserve(1024); 
        serialize_recursive(root, out);
    }
    return out;
}

static bool deserialize_recursive(const std::shared_ptr<litehtml::render_item>& item, const std::vector<float>& data, size_t& offset) {
    if (!item) return true;

    // We expect 17 floats per item
    if (offset + 17 > data.size()) return false;

    item->pos().x = (litehtml::pixel_t)data[offset++];
    item->pos().y = (litehtml::pixel_t)data[offset++];
    item->pos().width = (litehtml::pixel_t)data[offset++];
    item->pos().height = (litehtml::pixel_t)data[offset++];

    item->get_margins().left = (litehtml::pixel_t)data[offset++];
    item->get_margins().right = (litehtml::pixel_t)data[offset++];
    item->get_margins().top = (litehtml::pixel_t)data[offset++];
    item->get_margins().bottom = (litehtml::pixel_t)data[offset++];

    item->get_paddings().left = (litehtml::pixel_t)data[offset++];
    item->get_paddings().right = (litehtml::pixel_t)data[offset++];
    item->get_paddings().top = (litehtml::pixel_t)data[offset++];
    item->get_paddings().bottom = (litehtml::pixel_t)data[offset++];

    item->get_borders().left = (litehtml::pixel_t)data[offset++];
    item->get_borders().right = (litehtml::pixel_t)data[offset++];
    item->get_borders().top = (litehtml::pixel_t)data[offset++];
    item->get_borders().bottom = (litehtml::pixel_t)data[offset++];

    item->skip(data[offset++] != 0.0f);

    for (const auto& child : item->children()) {
        if (!deserialize_recursive(child, data, offset)) return false;
    }
    return true;
}

bool Serializer::deserialize_layout(const std::shared_ptr<litehtml::render_item>& root, const std::vector<float>& data) {
    size_t offset = 0;
    if (root) {
        return deserialize_recursive(root, data, offset);
    }
    return true;
}

void Serializer::rebuild_stacking_contexts(const std::shared_ptr<litehtml::render_item>& root) {
    if (root) {
        root->fetch_positioned();
        root->sort_positioned();
    }
}

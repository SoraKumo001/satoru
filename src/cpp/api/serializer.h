#ifndef SERIALIZER_H
#define SERIALIZER_H

#include <litehtml/render_item.h>
#include <vector>

class Serializer {
public:
    static std::vector<float> serialize_layout(const std::shared_ptr<litehtml::render_item>& root);
    static bool deserialize_layout(const std::shared_ptr<litehtml::render_item>& root, const std::vector<float>& data);
    static void rebuild_stacking_contexts(const std::shared_ptr<litehtml::render_item>& root);
};

#endif // SERIALIZER_H

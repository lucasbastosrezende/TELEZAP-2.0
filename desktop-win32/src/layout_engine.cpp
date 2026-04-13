#include "layout_engine.h"

namespace tocachat {

namespace {

float MainSize(const D2D1_RECT_F& rect, LayoutDirection direction) noexcept {
    return direction == LayoutDirection::Row ? WidthOf(rect) : HeightOf(rect);
}

float CrossSize(const D2D1_RECT_F& rect, LayoutDirection direction) noexcept {
    return direction == LayoutDirection::Row ? HeightOf(rect) : WidthOf(rect);
}

D2D1_RECT_F MakeChildRect(const D2D1_RECT_F& inner,
                          LayoutDirection direction,
                          float main_offset,
                          float main_size,
                          float cross_offset,
                          float cross_size) noexcept {
    if (direction == LayoutDirection::Row) {
        return D2D1::RectF(
            inner.left + main_offset,
            inner.top + cross_offset,
            inner.left + main_offset + main_size,
            inner.top + cross_offset + cross_size);
    }

    return D2D1::RectF(
        inner.left + cross_offset,
        inner.top + main_offset,
        inner.left + cross_offset + cross_size,
        inner.top + main_offset + main_size);
}

}  // namespace

D2D1_RECT_F LayoutEngine::Inset(const D2D1_RECT_F& rect, const EdgeInsets& insets) noexcept {
    return D2D1::RectF(
        rect.left + insets.left,
        rect.top + insets.top,
        rect.right - insets.right,
        rect.bottom - insets.bottom);
}

std::vector<D2D1_RECT_F> LayoutEngine::Flex(const D2D1_RECT_F& container,
                                            const FlexOptions& options,
                                            const std::vector<FlexItem>& items) {
    std::vector<D2D1_RECT_F> result;
    result.reserve(items.size());

    if (items.empty()) {
        return result;
    }

    const D2D1_RECT_F inner = Inset(container, options.padding);
    const float inner_main = MainSize(inner, options.direction);
    const float inner_cross = CrossSize(inner, options.direction);
    float used_main = options.gap * static_cast<float>(items.size() > 1 ? items.size() - 1 : 0);
    float total_grow = 0.0f;

    for (const FlexItem& item : items) {
        used_main += item.basis;
        total_grow += item.grow;
    }

    const float remaining = std::max(0.0f, inner_main - used_main);
    float gap = options.gap;
    float cursor = 0.0f;

    if (options.justify == JustifyContent::Center) {
        cursor = remaining * 0.5f;
    } else if (options.justify == JustifyContent::End) {
        cursor = remaining;
    } else if (options.justify == JustifyContent::SpaceBetween && items.size() > 1) {
        gap = options.gap + (remaining / static_cast<float>(items.size() - 1));
    }

    for (size_t index = 0; index < items.size(); ++index) {
        const FlexItem& item = items[index];
        float main_extent = item.basis;
        if (total_grow > 0.0f && item.grow > 0.0f) {
            main_extent += remaining * (item.grow / total_grow);
        }

        float cross_extent = item.cross_size > 0.0f ? std::min(item.cross_size, inner_cross) : inner_cross;
        float cross_offset = 0.0f;

        switch (options.align) {
        case AlignItems::Center:
            cross_offset = (inner_cross - cross_extent) * 0.5f;
            break;
        case AlignItems::End:
            cross_offset = inner_cross - cross_extent;
            break;
        case AlignItems::Stretch:
            cross_extent = inner_cross;
            break;
        case AlignItems::Start:
        default:
            break;
        }

        result.push_back(MakeChildRect(inner, options.direction, cursor, main_extent, cross_offset, cross_extent));
        cursor += main_extent;
        if (index + 1 < items.size()) {
            cursor += gap;
        }
    }

    return result;
}

}  // namespace tocachat

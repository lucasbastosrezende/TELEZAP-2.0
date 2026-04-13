#pragma once

#include "common.h"

namespace tocachat {

enum class LayoutDirection {
    Row,
    Column
};

enum class AlignItems {
    Start,
    Center,
    End,
    Stretch
};

enum class JustifyContent {
    Start,
    Center,
    End,
    SpaceBetween
};

struct EdgeInsets {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

struct FlexItem {
    float basis = 0.0f;
    float grow = 0.0f;
    float cross_size = -1.0f;
};

struct FlexOptions {
    LayoutDirection direction = LayoutDirection::Row;
    AlignItems align = AlignItems::Stretch;
    JustifyContent justify = JustifyContent::Start;
    float gap = 0.0f;
    EdgeInsets padding;
};

class LayoutEngine {
public:
    static std::vector<D2D1_RECT_F> Flex(const D2D1_RECT_F& container,
                                         const FlexOptions& options,
                                         const std::vector<FlexItem>& items);

    static D2D1_RECT_F Inset(const D2D1_RECT_F& rect, const EdgeInsets& insets) noexcept;
};

}  // namespace tocachat

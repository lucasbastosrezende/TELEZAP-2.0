#pragma once

#include "common.h"

namespace tocachat {

struct AppTheme {
    D2D1_COLOR_F bg_primary;
    D2D1_COLOR_F bg_secondary;
    D2D1_COLOR_F bg_tertiary;
    D2D1_COLOR_F bg_panel;
    D2D1_COLOR_F bg_input;
    D2D1_COLOR_F accent;
    D2D1_COLOR_F accent_hover;
    D2D1_COLOR_F text_primary;
    D2D1_COLOR_F text_secondary;
    D2D1_COLOR_F text_muted;
    D2D1_COLOR_F border;
    D2D1_COLOR_F success;
    D2D1_COLOR_F danger;
    D2D1_COLOR_F bubble_me;
    D2D1_COLOR_F bubble_other;
    D2D1_COLOR_F overlay;
    D2D1_COLOR_F shadow;
    float radius_card = 8.0f;
    float radius_bubble = 20.0f;
    float radius_pill = 999.0f;

    static const AppTheme& Default() noexcept;
};

}  // namespace tocachat

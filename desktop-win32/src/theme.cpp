#include "theme.h"

namespace tocachat {

const AppTheme& AppTheme::Default() noexcept {
    static const AppTheme theme{
        ColorFromHex(0x0f0f1e),
        ColorFromHex(0x1a1a2e),
        ColorFromHex(0x252540),
        ColorFromHex(0x17172a, 0.94f),
        ColorFromHex(0x1d1d32, 0.98f),
        ColorFromHex(0x6c5ce7),
        ColorFromHex(0x7d6ff0),
        ColorFromHex(0xffffff),
        ColorFromHex(0xa0a0b0),
        ColorFromHex(0x737386),
        ColorFromHex(0x2d2d44),
        ColorFromHex(0x2ecc71),
        ColorFromHex(0xe74c3c),
        ColorFromHex(0x6c5ce7, 0.92f),
        ColorFromHex(0x23233a, 0.96f),
        ColorFromHex(0x0b0b14, 0.76f),
        ColorFromHex(0x000000, 0.30f),
        8.0f,
        20.0f,
        999.0f
    };

    return theme;
}

}  // namespace tocachat

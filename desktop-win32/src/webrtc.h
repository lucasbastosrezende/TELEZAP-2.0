#pragma once

#include "common.h"

namespace tocachat {

class WebRtcController {
public:
    bool Initialize() noexcept { return true; }
    void Shutdown() noexcept {}
};

}  // namespace tocachat

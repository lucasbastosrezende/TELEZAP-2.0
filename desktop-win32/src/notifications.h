#pragma once

#include "common.h"

namespace tocachat {

class NotificationCenter {
public:
    bool Initialize(HWND hwnd) noexcept;
    void Shutdown() noexcept;

private:
    HWND owner_ = nullptr;
};

}  // namespace tocachat

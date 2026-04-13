#include "notifications.h"

namespace tocachat {

bool NotificationCenter::Initialize(HWND hwnd) noexcept {
    owner_ = hwnd;
    return owner_ != nullptr;
}

void NotificationCenter::Shutdown() noexcept {
    owner_ = nullptr;
}

}  // namespace tocachat

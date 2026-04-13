#pragma once

#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <KnownFolders.h>
#include <objbase.h>
#include <shellscalingapi.h>
#include <shlobj.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tocachat {

using Microsoft::WRL::ComPtr;

class ScopedCoInit {
public:
    explicit ScopedCoInit(DWORD flags) noexcept
        : hr_(::CoInitializeEx(nullptr, flags)) {}

    ~ScopedCoInit() {
        if (SUCCEEDED(hr_)) {
            ::CoUninitialize();
        }
    }

    HRESULT status() const noexcept { return hr_; }

private:
    HRESULT hr_;
};

inline float ClampFloat(float value, float minimum, float maximum) noexcept {
    return std::max(minimum, std::min(value, maximum));
}

inline float WidthOf(const D2D1_RECT_F& rect) noexcept {
    return rect.right - rect.left;
}

inline float HeightOf(const D2D1_RECT_F& rect) noexcept {
    return rect.bottom - rect.top;
}

inline D2D1_COLOR_F ColorFromHex(unsigned int rgb, float alpha = 1.0f) noexcept {
    const float red = static_cast<float>((rgb >> 16) & 0xFF) / 255.0f;
    const float green = static_cast<float>((rgb >> 8) & 0xFF) / 255.0f;
    const float blue = static_cast<float>(rgb & 0xFF) / 255.0f;
    return D2D1::ColorF(red, green, blue, alpha);
}

inline D2D1_RECT_F InsetRect(const D2D1_RECT_F& rect, float padding) noexcept {
    return D2D1::RectF(
        rect.left + padding,
        rect.top + padding,
        rect.right - padding,
        rect.bottom - padding);
}

inline bool PointInRect(const D2D1_RECT_F& rect, float x, float y) noexcept {
    return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
}

inline UINT GetWindowDpiOrDefault(HWND hwnd) noexcept {
    return hwnd != nullptr ? ::GetDpiForWindow(hwnd) : USER_DEFAULT_SCREEN_DPI;
}

inline std::wstring MaskPassword(const std::wstring& value) {
    return std::wstring(value.size(), L'*');
}

inline std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }

    const int size = ::MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(size), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

inline std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }

    const int size = ::WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>(size), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

inline std::wstring TrimCopy(const std::wstring& value) {
    const auto begin = value.find_first_not_of(L" \t\r\n");
    if (begin == std::wstring::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(L" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

inline std::wstring JoinStrings(const std::vector<std::wstring>& items, const wchar_t* separator) {
    std::wostringstream builder;
    for (size_t index = 0; index < items.size(); ++index) {
        if (index != 0) {
            builder << separator;
        }
        builder << items[index];
    }
    return builder.str();
}

inline std::wstring GetKnownFolderPathString(REFKNOWNFOLDERID folder_id) {
    PWSTR raw_path = nullptr;
    if (FAILED(::SHGetKnownFolderPath(folder_id, KF_FLAG_CREATE, nullptr, &raw_path))) {
        return {};
    }

    std::wstring result(raw_path);
    ::CoTaskMemFree(raw_path);
    return result;
}

}  // namespace tocachat

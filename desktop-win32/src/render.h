#pragma once

#include "theme.h"
#include "ui_components.h"

namespace tocachat {

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool Initialize(HWND hwnd);
    void Resize(UINT width, UINT height);
    void DiscardDeviceResources();
    bool Render(const AppState& state, const ShellUiLayout& layout, const AuthViewState& auth_view);

private:
    bool CreateDeviceIndependentResources();
    bool CreateDeviceResources();
    bool CreateTextResources();
    void DrawAuthScreen(const AppState& state, const LoginUiLayout& layout, const AuthViewState& auth_view);
    void DrawChatShell(const AppState& state, const ChatUiLayout& layout);
    void DrawCard(const D2D1_RECT_F& rect, D2D1_COLOR_F color, float radius);
    void DrawLabel(const std::wstring& text,
                   IDWriteTextFormat* format,
                   const D2D1_RECT_F& rect,
                   D2D1_COLOR_F color,
                   DWRITE_TEXT_ALIGNMENT text_alignment = DWRITE_TEXT_ALIGNMENT_LEADING,
                   DWRITE_PARAGRAPH_ALIGNMENT paragraph_alignment = DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    void DrawAvatar(const D2D1_RECT_F& rect, const std::wstring& title, D2D1_COLOR_F fill);

    HWND hwnd_ = nullptr;
    AppTheme theme_;
    ComPtr<ID2D1Factory> d2d_factory_;
    ComPtr<IDWriteFactory> dwrite_factory_;
    ComPtr<IWICImagingFactory> wic_factory_;
    ComPtr<ID2D1HwndRenderTarget> render_target_;
    ComPtr<ID2D1SolidColorBrush> brush_;
    ComPtr<IDWriteTextFormat> title_format_;
    ComPtr<IDWriteTextFormat> body_format_;
    ComPtr<IDWriteTextFormat> small_format_;
    ComPtr<IDWriteTextFormat> button_format_;
    ComPtr<IDWriteTextFormat> hero_format_;
};

}  // namespace tocachat

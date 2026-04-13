#include "render.h"

namespace tocachat {

namespace {

std::wstring TimeFragment(const std::wstring& iso_datetime) {
    return iso_datetime.size() >= 16 ? iso_datetime.substr(11, 5) : std::wstring(L"--:--");
}

}  // namespace

Renderer::Renderer()
    : theme_(AppTheme::Default()) {}

Renderer::~Renderer() {
    DiscardDeviceResources();
}

bool Renderer::Initialize(HWND hwnd) {
    hwnd_ = hwnd;
    return CreateDeviceIndependentResources() && CreateDeviceResources() && CreateTextResources();
}

bool Renderer::CreateDeviceIndependentResources() {
    const HRESULT d2d_result = D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        d2d_factory_.ReleaseAndGetAddressOf());
    if (FAILED(d2d_result)) {
        return false;
    }

    const HRESULT dwrite_result = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(dwrite_factory_.ReleaseAndGetAddressOf()));
    if (FAILED(dwrite_result)) {
        return false;
    }

    const HRESULT wic_result = CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(wic_factory_.ReleaseAndGetAddressOf()));
    return SUCCEEDED(wic_result);
}

bool Renderer::CreateDeviceResources() {
    if (render_target_ != nullptr) {
        return true;
    }

    RECT client_rect{};
    ::GetClientRect(hwnd_, &client_rect);
    const D2D1_SIZE_U size = D2D1::SizeU(
        static_cast<UINT>(std::max(1L, client_rect.right - client_rect.left)),
        static_cast<UINT>(std::max(1L, client_rect.bottom - client_rect.top)));

    const HRESULT target_result = d2d_factory_->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(hwnd_, size),
        render_target_.ReleaseAndGetAddressOf());
    if (FAILED(target_result)) {
        return false;
    }

    render_target_->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    render_target_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

    const HRESULT brush_result = render_target_->CreateSolidColorBrush(
        theme_.text_primary,
        brush_.ReleaseAndGetAddressOf());
    return SUCCEEDED(brush_result);
}

bool Renderer::CreateTextResources() {
    if (hero_format_ != nullptr) {
        return true;
    }

    auto create_format = [this](const wchar_t* family,
                                FLOAT size,
                                DWRITE_FONT_WEIGHT weight,
                                ComPtr<IDWriteTextFormat>& out_format) -> bool {
        const HRESULT result = dwrite_factory_->CreateTextFormat(
            family,
            nullptr,
            weight,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            size,
            L"pt-BR",
            out_format.ReleaseAndGetAddressOf());
        return SUCCEEDED(result);
    };

    return create_format(L"Segoe UI", 32.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, hero_format_) &&
           create_format(L"Segoe UI", 18.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, title_format_) &&
           create_format(L"Segoe UI", 14.0f, DWRITE_FONT_WEIGHT_NORMAL, body_format_) &&
           create_format(L"Segoe UI", 12.0f, DWRITE_FONT_WEIGHT_NORMAL, small_format_) &&
           create_format(L"Segoe UI", 14.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, button_format_);
}

void Renderer::Resize(UINT width, UINT height) {
    if (render_target_ != nullptr) {
        render_target_->Resize(D2D1::SizeU(std::max<UINT>(1, width), std::max<UINT>(1, height)));
    }
}

void Renderer::DiscardDeviceResources() {
    brush_.Reset();
    render_target_.Reset();
}

void Renderer::DrawCard(const D2D1_RECT_F& rect, D2D1_COLOR_F color, float radius) {
    brush_->SetColor(color);
    render_target_->FillRoundedRect(D2D1::RoundedRect(rect, radius, radius), brush_.Get());
}

void Renderer::DrawLabel(const std::wstring& text,
                         IDWriteTextFormat* format,
                         const D2D1_RECT_F& rect,
                         D2D1_COLOR_F color,
                         DWRITE_TEXT_ALIGNMENT text_alignment,
                         DWRITE_PARAGRAPH_ALIGNMENT paragraph_alignment) {
    if (format == nullptr || text.empty()) {
        return;
    }

    format->SetTextAlignment(text_alignment);
    format->SetParagraphAlignment(paragraph_alignment);
    brush_->SetColor(color);
    render_target_->DrawTextW(
        text.c_str(),
        static_cast<UINT32>(text.size()),
        format,
        rect,
        brush_.Get(),
        D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
}

void Renderer::DrawAvatar(const D2D1_RECT_F& rect, const std::wstring& title, D2D1_COLOR_F fill) {
    brush_->SetColor(fill);
    const D2D1_ELLIPSE ellipse = D2D1::Ellipse(
        D2D1::Point2F((rect.left + rect.right) * 0.5f, (rect.top + rect.bottom) * 0.5f),
        WidthOf(rect) * 0.5f,
        HeightOf(rect) * 0.5f);
    render_target_->FillEllipse(ellipse, brush_.Get());

    const wchar_t initial = title.empty() ? L'T' : title[0];
    const std::wstring text(1, initial);
    DrawLabel(text, button_format_.Get(), rect, theme_.text_primary, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
}

void Renderer::DrawAuthScreen(const AppState& state, const LoginUiLayout& layout, const AuthViewState& auth_view) {
    render_target_->Clear(theme_.bg_primary);

    brush_->SetColor(ColorFromHex(0x6c5ce7, 0.18f));
    render_target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(160.0f, 120.0f), 220.0f, 160.0f), brush_.Get());
    brush_->SetColor(ColorFromHex(0x2d2d44, 0.20f));
    render_target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(state.window_width() - 140.0f, state.window_height() - 120.0f), 220.0f, 180.0f), brush_.Get());

    DrawCard(layout.card, theme_.bg_panel, 18.0f);

    DrawLabel(L"TocaChat", hero_format_.Get(), layout.header_block, theme_.text_primary);
    DrawLabel(L"Cliente desktop nativo com shell inicial em Win32 + Direct2D",
              body_format_.Get(),
              D2D1::RectF(layout.header_block.left, layout.header_block.top + 42.0f, layout.header_block.right, layout.header_block.bottom),
              theme_.text_secondary);

    const D2D1_COLOR_F user_border = auth_view.focus == AuthFocusField::Username ? theme_.accent : theme_.border;
    const D2D1_COLOR_F pass_border = auth_view.focus == AuthFocusField::Password ? theme_.accent : theme_.border;

    DrawCard(layout.username_rect, theme_.bg_input, 10.0f);
    brush_->SetColor(user_border);
    render_target_->DrawRoundedRect(D2D1::RoundedRect(layout.username_rect, 10.0f, 10.0f), brush_.Get(), 1.5f);
    DrawLabel(L"Usuário", small_format_.Get(),
              D2D1::RectF(layout.username_rect.left + 14.0f, layout.username_rect.top + 6.0f, layout.username_rect.right - 14.0f, layout.username_rect.bottom),
              theme_.text_muted);
    DrawLabel(auth_view.username_text.empty() ? L"seu_usuario" : auth_view.username_text,
              body_format_.Get(),
              D2D1::RectF(layout.username_rect.left + 14.0f, layout.username_rect.top + 18.0f, layout.username_rect.right - 14.0f, layout.username_rect.bottom - 4.0f),
              auth_view.username_text.empty() ? theme_.text_secondary : theme_.text_primary);

    DrawCard(layout.password_rect, theme_.bg_input, 10.0f);
    brush_->SetColor(pass_border);
    render_target_->DrawRoundedRect(D2D1::RoundedRect(layout.password_rect, 10.0f, 10.0f), brush_.Get(), 1.5f);
    DrawLabel(L"Senha", small_format_.Get(),
              D2D1::RectF(layout.password_rect.left + 14.0f, layout.password_rect.top + 6.0f, layout.password_rect.right - 14.0f, layout.password_rect.bottom),
              theme_.text_muted);
    DrawLabel(auth_view.password_text.empty() ? L"mínimo 4 caracteres" : MaskPassword(auth_view.password_text),
              body_format_.Get(),
              D2D1::RectF(layout.password_rect.left + 14.0f, layout.password_rect.top + 18.0f, layout.password_rect.right - 14.0f, layout.password_rect.bottom - 4.0f),
              auth_view.password_text.empty() ? theme_.text_secondary : theme_.text_primary);

    DrawCard(layout.primary_button_rect, theme_.accent, 10.0f);
    DrawLabel(L"Entrar no shell", button_format_.Get(), layout.primary_button_rect, theme_.text_primary, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    DrawLabel(L"Criar conta local", button_format_.Get(), layout.secondary_button_rect, theme_.accent, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    const std::wstring helper = state.auth_status().empty() ? auth_view.helper_text : state.auth_status();
    DrawLabel(helper.empty() ? L"Tab alterna entre campos. Enter faz login local nesta primeira etapa." : helper,
              small_format_.Get(),
              layout.hint_rect,
              theme_.text_secondary);
}

void Renderer::DrawChatShell(const AppState& state, const ChatUiLayout& layout) {
    render_target_->Clear(theme_.bg_primary);

    DrawCard(layout.sidebar, theme_.bg_panel, 18.0f);
    DrawCard(layout.main_panel, ColorFromHex(0x111123, 0.95f), 18.0f);

    DrawLabel(L"Conversas", title_format_.Get(), layout.sidebar_header, theme_.text_primary);
    DrawCard(layout.logout_button, ColorFromHex(0x22223b, 0.95f), 12.0f);
    DrawLabel(L"Sair", small_format_.Get(), layout.logout_button, theme_.text_secondary, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    const auto& conversations = state.conversations();
    for (const ConversationRowLayout& row : layout.conversation_rows) {
        const Conversa& conversa = conversations[row.index];
        DrawCard(row.rect, row.selected ? ColorFromHex(0x232348, 0.95f) : ColorFromHex(0x1b1b30, 0.60f), 12.0f);

        const D2D1_RECT_F avatar_rect = D2D1::RectF(row.rect.left + 12.0f, row.rect.top + 12.0f, row.rect.left + 52.0f, row.rect.top + 52.0f);
        DrawAvatar(avatar_rect, conversa.nome, row.selected ? theme_.accent : ColorFromHex(0x39405a));

        DrawLabel(conversa.nome,
                  button_format_.Get(),
                  D2D1::RectF(avatar_rect.right + 12.0f, row.rect.top + 10.0f, row.rect.right - 78.0f, row.rect.top + 32.0f),
                  theme_.text_primary);

        const std::wstring preview = conversa.ultima_mensagem ? conversa.ultima_mensagem->conteudo : L"Nenhuma mensagem";
        DrawLabel(preview,
                  small_format_.Get(),
                  D2D1::RectF(avatar_rect.right + 12.0f, row.rect.top + 34.0f, row.rect.right - 66.0f, row.rect.bottom - 10.0f),
                  theme_.text_secondary);

        DrawLabel(conversa.ultima_mensagem ? TimeFragment(conversa.ultima_mensagem->criado_em) : L"--:--",
                  small_format_.Get(),
                  D2D1::RectF(row.rect.right - 60.0f, row.rect.top + 10.0f, row.rect.right - 12.0f, row.rect.top + 26.0f),
                  theme_.text_muted,
                  DWRITE_TEXT_ALIGNMENT_TRAILING);
    }

    const Conversa* conversa = state.selected_conversation();
    if (conversa == nullptr) {
        DrawLabel(L"Nenhuma conversa disponível",
                  title_format_.Get(),
                  layout.main_panel,
                  theme_.text_primary,
                  DWRITE_TEXT_ALIGNMENT_CENTER,
                  DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        return;
    }

    DrawLabel(conversa->nome, title_format_.Get(), layout.chat_header, theme_.text_primary);

    const std::wstring subtitle = conversa->tipo == L"grupo"
        ? L"Grupo ativo no shell Win32"
        : L"Conversa direta";
    DrawLabel(subtitle,
              small_format_.Get(),
              D2D1::RectF(layout.chat_header.left, layout.chat_header.top + 28.0f, layout.chat_header.right - 120.0f, layout.chat_header.bottom),
              theme_.text_secondary);

    if (!state.chat_status().empty()) {
        DrawLabel(state.chat_status(),
                  small_format_.Get(),
                  D2D1::RectF(layout.chat_header.left + 220.0f, layout.chat_header.top + 8.0f, layout.chat_header.right - 44.0f, layout.chat_header.bottom),
                  state.busy() ? theme_.accent : theme_.text_muted,
                  DWRITE_TEXT_ALIGNMENT_TRAILING);
    }

    brush_->SetColor(theme_.accent);
    render_target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(layout.chat_header.right - 26.0f, layout.chat_header.top + 24.0f), 7.0f, 7.0f), brush_.Get());

    const std::vector<Mensagem>& messages = state.selected_messages();
    for (const MessageBubbleLayout& bubble : layout.message_rows) {
        const Mensagem& mensagem = messages[bubble.index];
        DrawCard(bubble.rect, bubble.own_message ? theme_.bubble_me : theme_.bubble_other, theme_.radius_bubble);

        const D2D1_RECT_F text_rect = InsetRect(bubble.rect, 14.0f);
        DrawLabel(mensagem.conteudo, body_format_.Get(), text_rect, theme_.text_primary);
        DrawLabel(TimeFragment(mensagem.criado_em),
                  small_format_.Get(),
                  D2D1::RectF(text_rect.left, bubble.rect.bottom - 22.0f, text_rect.right, bubble.rect.bottom - 8.0f),
                  ColorFromHex(0xffffff, 0.65f),
                  bubble.own_message ? DWRITE_TEXT_ALIGNMENT_TRAILING : DWRITE_TEXT_ALIGNMENT_LEADING);
    }

    DrawCard(layout.composer, ColorFromHex(0x17172c, 0.96f), 16.0f);
    DrawCard(layout.composer_input, theme_.bg_input, 14.0f);
    DrawLabel(state.composer_text().empty() ? L"Digite sua mensagem aqui..." : state.composer_text(),
              body_format_.Get(),
              InsetRect(layout.composer_input, 14.0f),
              state.composer_text().empty() ? theme_.text_secondary : theme_.text_primary,
              DWRITE_TEXT_ALIGNMENT_LEADING,
              DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    DrawCard(layout.send_button, theme_.accent, 14.0f);
    DrawLabel(L">", button_format_.Get(), layout.send_button, theme_.text_primary, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
}

bool Renderer::Render(const AppState& state, const ShellUiLayout& layout, const AuthViewState& auth_view) {
    if (!CreateDeviceResources()) {
        return false;
    }

    render_target_->BeginDraw();
    render_target_->SetTransform(D2D1::Matrix3x2F::Identity());

    if (state.screen() == AppScreen::Auth) {
        DrawAuthScreen(state, layout.login, auth_view);
    } else {
        DrawChatShell(state, layout.chat);
    }

    const HRESULT end_result = render_target_->EndDraw();
    if (end_result == D2DERR_RECREATE_TARGET) {
        DiscardDeviceResources();
        return false;
    }

    return SUCCEEDED(end_result);
}

}  // namespace tocachat

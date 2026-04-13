#include "window.h"

namespace tocachat {

namespace {

constexpr wchar_t kWindowClassName[] = L"TocaChatDesktopWindow";
constexpr UINT_PTR kSyncTimerId = 1001;
constexpr UINT kSocketEventMessage = WM_APP + 101;
constexpr UINT kSocketStatusMessage = WM_APP + 102;

struct PendingSocketEvent {
    SocketEvent event;
};

struct PendingSocketStatus {
    bool connected = false;
    std::wstring message;
};

}  // namespace

MainWindow::MainWindow(HINSTANCE instance) noexcept
    : instance_(instance),
      auth_controller_(network_),
      chat_controller_(state_, network_, database_) {
    auth_view_.helper_text = L"Shell local criado para acelerar a base Win32 antes da integração HTTP/Socket.IO.";
    socketio_.SetEventCallback([this](const SocketEvent& event) {
        auto* pending = new PendingSocketEvent{event};
        ::PostMessageW(hwnd_, kSocketEventMessage, 0, reinterpret_cast<LPARAM>(pending));
    });
    socketio_.SetStatusCallback([this](bool connected, const std::wstring& message) {
        auto* pending = new PendingSocketStatus{connected, message};
        ::PostMessageW(hwnd_, kSocketStatusMessage, 0, reinterpret_cast<LPARAM>(pending));
    });
}

bool MainWindow::RegisterWindowClass() {
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = &MainWindow::WindowProc;
    window_class.hInstance = instance_;
    window_class.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    window_class.lpszClassName = kWindowClassName;
    return ::RegisterClassExW(&window_class) != 0;
}

bool MainWindow::Create() {
    if (!RegisterWindowClass()) {
        const DWORD error = ::GetLastError();
        if (error != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }

    hwnd_ = ::CreateWindowExW(
        0,
        kWindowClassName,
        L"TocaChat Desktop",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1360,
        860,
        nullptr,
        nullptr,
        instance_,
        this);

    return hwnd_ != nullptr;
}

int MainWindow::Run() {
    MSG message{};
    while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

void MainWindow::RefreshLayout() {
    layout_ = UiComponents::Build(state_);
}

void MainWindow::Invalidate() noexcept {
    if (hwnd_ != nullptr) {
        ::InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void MainWindow::CycleAuthFocus() {
    if (auth_view_.focus == AuthFocusField::Username) {
        auth_view_.focus = AuthFocusField::Password;
    } else {
        auth_view_.focus = AuthFocusField::Username;
    }
}

void MainWindow::OnAuthChar(wchar_t ch) {
    if (ch < 32) {
        return;
    }

    std::wstring* target = nullptr;
    if (auth_view_.focus == AuthFocusField::Username) {
        target = &auth_view_.username_text;
    } else if (auth_view_.focus == AuthFocusField::Password) {
        target = &auth_view_.password_text;
    }

    if (target != nullptr && target->size() < 64) {
        target->push_back(ch);
        Invalidate();
    }
}

void MainWindow::OnAuthBackspace() {
    std::wstring* target = nullptr;
    if (auth_view_.focus == AuthFocusField::Username) {
        target = &auth_view_.username_text;
    } else if (auth_view_.focus == AuthFocusField::Password) {
        target = &auth_view_.password_text;
    }

    if (target != nullptr && !target->empty()) {
        target->pop_back();
        Invalidate();
    }
}

void MainWindow::SubmitAuth(bool creating_account) {
    const AuthCredentials credentials{auth_view_.username_text, auth_view_.password_text};
    state_.SetBusy(true);
    const AuthResult result = auth_controller_.Submit(state_, credentials, creating_account);
    state_.SetBusy(false);
    state_.SetAuthStatus(result.message);

    if (result.ok) {
        auth_view_.helper_text = result.message;
        auth_view_.password_text.clear();
        BeginChatSession();
    }

    Invalidate();
}

void MainWindow::Logout() {
    const AuthResult result = auth_controller_.Logout(state_);
    state_.SetAuthStatus(result.message);
    auth_view_.password_text.clear();
    ::KillTimer(hwnd_, kSyncTimerId);
    socketio_.Stop();
    RefreshLayout();
    Invalidate();
}

void MainWindow::OnLeftButtonDown(float x, float y) {
    if (state_.screen() == AppScreen::Auth) {
        if (PointInRect(layout_.login.username_rect, x, y)) {
            auth_view_.focus = AuthFocusField::Username;
        } else if (PointInRect(layout_.login.password_rect, x, y)) {
            auth_view_.focus = AuthFocusField::Password;
        } else if (PointInRect(layout_.login.primary_button_rect, x, y)) {
            SubmitAuth(false);
            return;
        } else if (PointInRect(layout_.login.secondary_button_rect, x, y)) {
            SubmitAuth(true);
            return;
        }

        Invalidate();
        return;
    }

    if (PointInRect(layout_.chat.logout_button, x, y)) {
        Logout();
        return;
    }

    if (PointInRect(layout_.chat.send_button, x, y)) {
        SendCurrentMessage();
        return;
    }

    const ConversationRowLayout* row = UiComponents::HitTestConversation(layout_.chat, x, y);
    if (row != nullptr) {
        chat_controller_.OpenConversation(row->index);
        const AuthResult load_result = chat_controller_.LoadSelectedConversationMessages();
        state_.SetChatStatus(load_result.message);
        const int conversation_id = state_.selected_conversation_id();
        if (conversation_id != 0) {
            socketio_.EmitEvent(L"join_conv", Json{{"conversa_id", conversation_id}});
        }
        RefreshLayout();
        Invalidate();
    }
}

void MainWindow::OnSize(UINT width, UINT height) {
    state_.SetWindowSize(static_cast<float>(width), static_cast<float>(height));
    renderer_.Resize(width, height);
    RefreshLayout();
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    MainWindow* window = nullptr;

    if (message == WM_NCCREATE) {
        const CREATESTRUCTW* create_struct = reinterpret_cast<CREATESTRUCTW*>(lparam);
        window = static_cast<MainWindow*>(create_struct->lpCreateParams);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        window->hwnd_ = hwnd;
    } else {
        window = reinterpret_cast<MainWindow*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (window != nullptr) {
        return window->HandleMessage(message, wparam, lparam);
    }

    return ::DefWindowProcW(hwnd, message, wparam, lparam);
}

void MainWindow::OnChatChar(wchar_t ch) {
    if (ch < 32) {
        return;
    }

    std::wstring text = state_.composer_text();
    if (text.size() >= 1024) {
        return;
    }
    text.push_back(ch);
    state_.SetComposerText(std::move(text));
    Invalidate();
}

void MainWindow::OnChatBackspace() {
    std::wstring text = state_.composer_text();
    if (!text.empty()) {
        text.pop_back();
        state_.SetComposerText(std::move(text));
        Invalidate();
    }
}

void MainWindow::SendCurrentMessage() {
    state_.SetBusy(true);
    const AuthResult result = chat_controller_.SendCurrentMessage();
    state_.SetBusy(false);
    state_.SetChatStatus(result.message);
    RefreshLayout();
    Invalidate();
}

void MainWindow::BeginChatSession() {
    state_.SetBusy(true);
    AuthResult conversations = chat_controller_.LoadConversations();
    if (conversations.ok && state_.selected_conversation_id() != 0) {
        conversations = chat_controller_.LoadSelectedConversationMessages();
    }
    state_.SetBusy(false);
    state_.SetChatStatus(conversations.message);
    ::SetTimer(hwnd_, kSyncTimerId, 2500, nullptr);
    socketio_.Configure(network_.base_url(), network_.cookie_header());
    socketio_.Start();
    RefreshLayout();
}

void MainWindow::TryResumePersistedSession() {
    if (!network_.LoadPersistedSession()) {
        return;
    }

    state_.SetBusy(true);
    const AuthResult result = auth_controller_.TryResume(state_);
    state_.SetBusy(false);
    if (!result.ok) {
        network_.ClearPersistedSession();
        state_.SetAuthStatus(result.message);
        RefreshLayout();
        return;
    }

    BeginChatSession();
    state_.SetChatStatus(result.message);
}

void MainWindow::SyncConversation() {
    if (state_.screen() != AppScreen::Chat || state_.busy()) {
        return;
    }

    const ChatSyncResult result = chat_controller_.SyncSelectedConversation();
    if (!result.ok) {
        state_.SetChatStatus(result.error_message);
    } else if (!result.new_messages.empty()) {
        state_.SetChatStatus(L"Sincronizado agora.");
    }
    RefreshLayout();
    Invalidate();
}

LRESULT MainWindow::HandleMessage(UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE: {
        RECT client_rect{};
        ::GetClientRect(hwnd_, &client_rect);
        state_.SetWindowSize(static_cast<float>(client_rect.right - client_rect.left),
                             static_cast<float>(client_rect.bottom - client_rect.top));
        dpi_ = GetWindowDpiOrDefault(hwnd_);
        if (!renderer_.Initialize(hwnd_)) {
            return -1;
        }
        database_.OpenDefault();
        notifications_.Initialize(hwnd_);
        RefreshLayout();
        TryResumePersistedSession();
        return 0;
    }

    case WM_SIZE:
        OnSize(LOWORD(lparam), HIWORD(lparam));
        return 0;

    case WM_DPICHANGED: {
        dpi_ = HIWORD(wparam);
        RECT* suggested = reinterpret_cast<RECT*>(lparam);
        ::SetWindowPos(hwnd_,
                       nullptr,
                       suggested->left,
                       suggested->top,
                       suggested->right - suggested->left,
                       suggested->bottom - suggested->top,
                       SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    case WM_LBUTTONDOWN:
        ::SetFocus(hwnd_);
        OnLeftButtonDown(static_cast<float>(GET_X_LPARAM(lparam)), static_cast<float>(GET_Y_LPARAM(lparam)));
        return 0;

    case WM_CHAR:
        if (state_.screen() == AppScreen::Auth) {
            if (wparam == VK_RETURN) {
                SubmitAuth(false);
            } else if (wparam == VK_BACK) {
                OnAuthBackspace();
            } else {
                OnAuthChar(static_cast<wchar_t>(wparam));
            }
            return 0;
        } else if (state_.screen() == AppScreen::Chat) {
            if (wparam == VK_RETURN) {
                SendCurrentMessage();
            } else if (wparam == VK_BACK) {
                OnChatBackspace();
            } else {
                OnChatChar(static_cast<wchar_t>(wparam));
            }
            return 0;
        }
        break;

    case WM_KEYDOWN:
        if (state_.screen() == AppScreen::Auth) {
            if (wparam == VK_TAB) {
                CycleAuthFocus();
                Invalidate();
                return 0;
            }
        } else if (state_.screen() == AppScreen::Chat && wparam == VK_ESCAPE) {
            Logout();
            return 0;
        }
        break;

    case WM_TIMER:
        if (wparam == kSyncTimerId) {
            SyncConversation();
            return 0;
        }
        break;

    case kSocketEventMessage: {
        std::unique_ptr<PendingSocketEvent> pending(reinterpret_cast<PendingSocketEvent*>(lparam));
        if (!pending) {
            return 0;
        }

        if (pending->event.name == L"new_message" && pending->event.payload.is_object()) {
            const Mensagem message = ChatController::ParseMensagem(pending->event.payload);
            state_.AppendMessages(message.conversa_id, {message});
            chat_controller_.LoadConversations();
            if (message.conversa_id != state_.selected_conversation_id()) {
                state_.SetChatStatus(L"Nova mensagem recebida em outra conversa.");
            }
            RefreshLayout();
            Invalidate();
        } else if (pending->event.name == L"message_deleted" && pending->event.payload.is_object()) {
            const int deleted_id = pending->event.payload.value("id", 0);
            const int conversation_id = state_.selected_conversation_id();
            if (deleted_id != 0 && conversation_id != 0) {
                state_.RemoveMessage(conversation_id, deleted_id);
                state_.SetChatStatus(L"Mensagem removida em tempo real.");
                RefreshLayout();
                Invalidate();
            }
        } else if (pending->event.name == L"message_reaction_updated" && pending->event.payload.is_object()) {
            const int message_id = pending->event.payload.value("mensagem_id", 0);
            const int conversation_id = pending->event.payload.value("conversa_id", state_.selected_conversation_id());
            if (message_id != 0 && conversation_id != 0) {
                std::map<std::wstring, std::vector<int>> reactions;
                if (pending->event.payload.contains("reacoes") && pending->event.payload["reacoes"].is_array()) {
                    const Mensagem temp = ChatController::ParseMensagem(Json{
                        {"id", message_id},
                        {"conversa_id", conversation_id},
                        {"reacoes", pending->event.payload["reacoes"]}
                    });
                    reactions = temp.reacoes;
                }
                state_.UpdateMessageReactions(conversation_id, message_id, reactions);
                state_.SetChatStatus(L"Reações atualizadas.");
                Invalidate();
            }
        } else if (pending->event.name == L"pinned_update") {
            SyncConversation();
        }
        return 0;
    }

    case kSocketStatusMessage: {
        std::unique_ptr<PendingSocketStatus> pending(reinterpret_cast<PendingSocketStatus*>(lparam));
        if (!pending) {
            return 0;
        }
        state_.SetChatStatus(pending->message);
        state_.SetSocketConnected(pending->connected);
        if (pending->connected) {
            if (const Usuario* user = state_.current_user(); user != nullptr) {
                socketio_.EmitEvent(L"join", Json{{"user_id", user->id}, {"conversa_id", state_.selected_conversation_id()}});
            }
            if (state_.selected_conversation_id() != 0) {
                socketio_.EmitEvent(L"join_conv", Json{{"conversa_id", state_.selected_conversation_id()}});
            }
        }
        Invalidate();
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT paint{};
        ::BeginPaint(hwnd_, &paint);
        renderer_.Render(state_, layout_, auth_view_);
        ::EndPaint(hwnd_, &paint);
        return 0;
    }

    case WM_DESTROY:
        notifications_.Shutdown();
        ::KillTimer(hwnd_, kSyncTimerId);
        socketio_.Stop();
        database_.Close();
        ::PostQuitMessage(0);
        return 0;
    }

    return ::DefWindowProcW(hwnd_, message, wparam, lparam);
}

}  // namespace tocachat

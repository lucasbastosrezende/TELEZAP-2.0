#pragma once

#include "auth.h"
#include "chat.h"
#include "notifications.h"
#include "render.h"
#include "socketio.h"

namespace tocachat {

class MainWindow {
public:
    explicit MainWindow(HINSTANCE instance) noexcept;

    bool Create();
    int Run();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);

    bool RegisterWindowClass();
    void RefreshLayout();
    void Invalidate() noexcept;
    void SubmitAuth(bool creating_account);
    void Logout();
    void CycleAuthFocus();
    void OnAuthChar(wchar_t ch);
    void OnAuthBackspace();
    void OnChatChar(wchar_t ch);
    void OnChatBackspace();
    void SendCurrentMessage();
    void BeginChatSession();
    void TryResumePersistedSession();
    void SyncConversation();
    void OnLeftButtonDown(float x, float y);
    void OnSize(UINT width, UINT height);

    HINSTANCE instance_ = nullptr;
    HWND hwnd_ = nullptr;
    UINT dpi_ = USER_DEFAULT_SCREEN_DPI;
    Renderer renderer_;
    AppState state_;
    NetworkClient network_;
    DatabaseCache database_;
    AuthController auth_controller_;
    ChatController chat_controller_;
    NotificationCenter notifications_;
    SocketIoClient socketio_;
    ShellUiLayout layout_;
    AuthViewState auth_view_;
};

}  // namespace tocachat

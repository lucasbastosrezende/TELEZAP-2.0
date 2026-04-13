#pragma once

#include "models.h"

namespace tocachat {

enum class AppScreen {
    Auth,
    Chat,
    Call
};

class AppState {
public:
    AppState();

    void SetWindowSize(float width, float height) noexcept;
    void EnterChatShell(const std::wstring& username);
    void EnterAuthenticatedChat(const Usuario& user);
    void ReturnToAuth();
    void SelectConversation(size_t index) noexcept;
    void SetAuthStatus(const std::wstring& message);
    void SetChatStatus(const std::wstring& message);
    void SetCurrentUser(const Usuario& user);
    void SetConversations(std::vector<Conversa> conversations);
    void SetMessagesForConversation(int conversation_id, std::vector<Mensagem> messages);
    void AppendMessages(int conversation_id, const std::vector<Mensagem>& messages);
    void RemoveMessage(int conversation_id, int message_id);
    void UpdateMessageReactions(int conversation_id, int message_id, const std::map<std::wstring, std::vector<int>>& reactions);
    void SetComposerText(std::wstring text);
    void ClearComposer();
    void SetBusy(bool busy) noexcept;
    void SetSocketConnected(bool value) noexcept;

    AppScreen screen() const noexcept { return screen_; }
    float window_width() const noexcept { return window_width_; }
    float window_height() const noexcept { return window_height_; }
    const Usuario* current_user() const noexcept;
    const std::vector<Conversa>& conversations() const noexcept { return conversations_; }
    const Conversa* selected_conversation() const noexcept;
    const std::vector<Mensagem>& selected_messages() const noexcept;
    size_t selected_conversation_index() const noexcept { return selected_conversation_index_; }
    const std::wstring& auth_status() const noexcept { return auth_status_; }
    const std::wstring& chat_status() const noexcept { return chat_status_; }
    const CallState& call_state() const noexcept { return call_state_; }
    const std::wstring& composer_text() const noexcept { return composer_text_; }
    bool busy() const noexcept { return busy_; }
    bool socket_connected() const noexcept { return socket_connected_; }
    int selected_conversation_id() const noexcept;
    int last_message_id_for_selected_conversation() const noexcept;

private:
    void SeedShellData(const std::wstring& username);
    void NormalizeSelectedConversation() noexcept;

    AppScreen screen_ = AppScreen::Auth;
    float window_width_ = 1280.0f;
    float window_height_ = 800.0f;
    std::optional<Usuario> current_user_;
    std::vector<Conversa> conversations_;
    std::unordered_map<int, std::vector<Mensagem>> messages_by_conversation_;
    std::vector<Mensagem> empty_messages_;
    size_t selected_conversation_index_ = 0;
    std::wstring auth_status_;
    std::wstring chat_status_;
    std::wstring composer_text_;
    bool busy_ = false;
    bool socket_connected_ = false;
    CallState call_state_;
};

}  // namespace tocachat

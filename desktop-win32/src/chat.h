#pragma once

#include "app_state.h"
#include "auth.h"
#include "database.h"
#include "network.h"

namespace tocachat {

struct ChatSyncResult {
    bool ok = false;
    std::wstring error_message;
    std::vector<Conversa> conversations;
    std::vector<Mensagem> new_messages;
    std::vector<int> deleted_message_ids;
};

class ChatController {
public:
    ChatController(AppState& state, NetworkClient& network, DatabaseCache& database) noexcept
        : state_(state), network_(network), database_(database) {}

    static Conversa ParseConversa(const Json& json_body);
    static Mensagem ParseMensagem(const Json& json_body);

    void OpenConversation(size_t index) noexcept;
    AuthResult LoadConversations();
    AuthResult LoadSelectedConversationMessages();
    AuthResult SendCurrentMessage();
    ChatSyncResult SyncSelectedConversation();

private:
    AppState& state_;
    NetworkClient& network_;
    DatabaseCache& database_;
};

}  // namespace tocachat

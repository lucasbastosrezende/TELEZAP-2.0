#include "chat.h"

namespace tocachat {

namespace {

std::wstring JsonString(const Json& json_body, const char* key) {
    if (!json_body.is_object() || !json_body.contains(key) || !json_body[key].is_string()) {
        return {};
    }
    return Utf8ToWide(json_body[key].get<std::string>());
}

std::map<std::wstring, std::vector<int>> ParseReactionsMap(const Json& json_body) {
    std::map<std::wstring, std::vector<int>> reactions;
    if (!json_body.is_array()) {
        return reactions;
    }

    for (const Json& reaction_json : json_body) {
        if (!reaction_json.is_object() || !reaction_json.contains("emoji") || !reaction_json["emoji"].is_string()) {
            continue;
        }

        const std::wstring emoji = Utf8ToWide(reaction_json["emoji"].get<std::string>());
        const int total = reaction_json.value("total", 0);
        std::vector<int> users;
        users.resize(static_cast<size_t>(std::max(0, total)), 0);
        reactions[emoji] = std::move(users);
    }

    return reactions;
}

}  // namespace

Conversa ChatController::ParseConversa(const Json& json_body) {
    Conversa conversation;
    conversation.id = json_body.value("id", 0);
    conversation.tipo = JsonString(json_body, "tipo");
    conversation.nome = JsonString(json_body, "display_nome");
    if (conversation.nome.empty()) {
        conversation.nome = JsonString(json_body, "nome");
    }
    conversation.foto_url = JsonString(json_body, "display_foto");
    if (conversation.foto_url.empty()) {
        conversation.foto_url = JsonString(json_body, "foto");
    }
    conversation.wallpaper_url = JsonString(json_body, "wallpaper");
    if (json_body.contains("pinned_message_id") && !json_body["pinned_message_id"].is_null()) {
        conversation.pinned_message_id = json_body["pinned_message_id"].get<int>();
    }

    if (json_body.contains("ultima_msg") && json_body["ultima_msg"].is_string()) {
        Mensagem last_message;
        last_message.conversa_id = conversation.id;
        last_message.conteudo = Utf8ToWide(json_body["ultima_msg"].get<std::string>());
        last_message.criado_em = JsonString(json_body, "ultima_msg_em");
        conversation.ultima_mensagem = last_message;
    }

    if (json_body.contains("membros") && json_body["membros"].is_array()) {
        for (const Json& member_json : json_body["membros"]) {
            Usuario member;
            member.id = member_json.value("id", 0);
            member.username = JsonString(member_json, "username");
            member.nome = JsonString(member_json, "nome");
            member.bio = JsonString(member_json, "bio");
            member.foto_url = JsonString(member_json, "foto");
            member.wallpaper_url = JsonString(member_json, "wallpaper");
            if (member.nome.empty()) {
                member.nome = member.username;
            }
            conversation.membros.push_back(member);
        }
    }

    return conversation;
}

Mensagem ChatController::ParseMensagem(const Json& json_body) {
    Mensagem message;
    message.id = json_body.value("id", 0);
    message.conversa_id = json_body.value("conversa_id", 0);
    message.remetente_id = json_body.value("usuario_id", 0);
    if (message.remetente_id == 0) {
        message.remetente_id = json_body.value("remetente_id", 0);
    }
    message.conteudo = JsonString(json_body, "conteudo");
    message.media_url = JsonString(json_body, "media_url");
    message.criado_em = JsonString(json_body, "criado_em");

    if (json_body.contains("reply_to_id") && !json_body["reply_to_id"].is_null()) {
        message.reply_to_id = json_body["reply_to_id"].get<int>();
    }

    if (json_body.contains("subtopico_id") && !json_body["subtopico_id"].is_null()) {
        message.subtopico_id = json_body["subtopico_id"].get<int>();
    }

    if (json_body.contains("reacoes")) {
        message.reacoes = ParseReactionsMap(json_body["reacoes"]);
    }

    return message;
}

void ChatController::OpenConversation(size_t index) noexcept {
    state_.SelectConversation(index);
}

AuthResult ChatController::LoadConversations() {
    const ApiResult api = network_.GetJson(L"/api/conversas");
    if (!api.ok || !api.json_body.is_array()) {
        std::vector<Conversa> cached = database_.LoadConversations();
        if (!cached.empty()) {
            state_.SetConversations(std::move(cached));
            return {true, L"Conversas carregadas do cache local."};
        }
        return {false, api.error_message.empty() ? L"Falha ao carregar conversas." : api.error_message};
    }

    std::vector<Conversa> conversations;
    for (const Json& conversation_json : api.json_body) {
        conversations.push_back(ParseConversa(conversation_json));
    }

    database_.SaveConversations(conversations);
    state_.SetConversations(std::move(conversations));
    return {true, L"Conversas carregadas do backend."};
}

AuthResult ChatController::LoadSelectedConversationMessages() {
    const int conversation_id = state_.selected_conversation_id();
    if (conversation_id == 0) {
        return {false, L"Nenhuma conversa selecionada."};
    }

    const std::wstring path = L"/api/conversas/" + std::to_wstring(conversation_id) + L"/mensagens";
    const ApiResult api = network_.GetJson(path);
    if (!api.ok || !api.json_body.is_array()) {
        std::vector<Mensagem> cached = database_.LoadMessages(conversation_id);
        if (!cached.empty()) {
            state_.SetMessagesForConversation(conversation_id, std::move(cached));
            return {true, L"Mensagens carregadas do cache local."};
        }
        return {false, api.error_message.empty() ? L"Falha ao carregar mensagens." : api.error_message};
    }

    std::vector<Mensagem> messages;
    for (const Json& message_json : api.json_body) {
        messages.push_back(ParseMensagem(message_json));
    }

    database_.SaveMessages(conversation_id, messages);
    state_.SetMessagesForConversation(conversation_id, std::move(messages));
    return {true, L"Mensagens carregadas."};
}

AuthResult ChatController::SendCurrentMessage() {
    const int conversation_id = state_.selected_conversation_id();
    const std::wstring content = TrimCopy(state_.composer_text());
    if (conversation_id == 0) {
        return {false, L"Selecione uma conversa antes de enviar."};
    }
    if (content.empty()) {
        return {false, L"Digite uma mensagem antes de enviar."};
    }

    Json body = {
        {"conteudo", WideToUtf8(content)}
    };
    const ApiResult api = network_.PostJson(L"/api/conversas/" + std::to_wstring(conversation_id) + L"/mensagens", body);
    if (!api.ok || !api.json_body.is_object()) {
        return {false, api.error_message.empty() ? L"Falha ao enviar mensagem." : api.error_message};
    }

    const Mensagem message = ParseMensagem(api.json_body);
    state_.AppendMessages(conversation_id, {message});
    database_.SaveMessages(conversation_id, state_.selected_messages());
    state_.ClearComposer();
    return {true, L"Mensagem enviada."};
}

ChatSyncResult ChatController::SyncSelectedConversation() {
    ChatSyncResult result{};
    const int conversation_id = state_.selected_conversation_id();
    if (conversation_id == 0) {
        result.ok = true;
        return result;
    }

    std::wstring path = L"/api/chat/sync?conversa_id=" + std::to_wstring(conversation_id) +
                        L"&after_id=" + std::to_wstring(state_.last_message_id_for_selected_conversation());

    const ApiResult api = network_.GetJson(path);
    if (!api.ok || !api.json_body.is_object()) {
        result.error_message = api.error_message.empty() ? L"Falha ao sincronizar conversa." : api.error_message;
        return result;
    }

    if (api.json_body.contains("conversas") && api.json_body["conversas"].is_array()) {
        for (const Json& conversation_json : api.json_body["conversas"]) {
            result.conversations.push_back(ParseConversa(conversation_json));
        }
    }

    if (api.json_body.contains("mensagens") && api.json_body["mensagens"].is_array()) {
        for (const Json& message_json : api.json_body["mensagens"]) {
            result.new_messages.push_back(ParseMensagem(message_json));
        }
    }

    if (api.json_body.contains("deleted_ids") && api.json_body["deleted_ids"].is_array()) {
        for (const Json& deleted_id : api.json_body["deleted_ids"]) {
            if (deleted_id.is_number_integer()) {
                result.deleted_message_ids.push_back(deleted_id.get<int>());
            }
        }
    }

    if (!result.conversations.empty()) {
        database_.SaveConversations(result.conversations);
        state_.SetConversations(result.conversations);
    }
    if (!result.new_messages.empty()) {
        state_.AppendMessages(conversation_id, result.new_messages);
        database_.SaveMessages(conversation_id, state_.selected_messages());
    }
    for (int deleted_id : result.deleted_message_ids) {
        state_.RemoveMessage(conversation_id, deleted_id);
    }
    if (!result.deleted_message_ids.empty()) {
        database_.SaveMessages(conversation_id, state_.selected_messages());
    }

    result.ok = true;
    return result;
}

}  // namespace tocachat

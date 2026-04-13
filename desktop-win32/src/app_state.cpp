#include "app_state.h"

namespace tocachat {

namespace {

Mensagem MakeMessage(int id,
                     int conversa_id,
                     int remetente_id,
                     const wchar_t* texto,
                     const wchar_t* criado_em) {
    Mensagem mensagem;
    mensagem.id = id;
    mensagem.conversa_id = conversa_id;
    mensagem.remetente_id = remetente_id;
    mensagem.conteudo = texto;
    mensagem.criado_em = criado_em;
    return mensagem;
}

}  // namespace

AppState::AppState() = default;

void AppState::SetWindowSize(float width, float height) noexcept {
    window_width_ = width;
    window_height_ = height;
}

void AppState::EnterChatShell(const std::wstring& username) {
    auth_status_.clear();
    SeedShellData(username);
    screen_ = AppScreen::Chat;
    chat_status_ = L"Shell local ativo. Agora vamos substituir os dados mockados pelo backend real.";
}

void AppState::EnterAuthenticatedChat(const Usuario& user) {
    conversations_.clear();
    messages_by_conversation_.clear();
    selected_conversation_index_ = 0;
    composer_text_.clear();
    SetCurrentUser(user);
    screen_ = AppScreen::Chat;
    auth_status_.clear();
}

void AppState::ReturnToAuth() {
    screen_ = AppScreen::Auth;
    auth_status_ = L"Esqueleto local ativo. Na próxima etapa a autenticação virá do backend Flask.";
    chat_status_.clear();
    composer_text_.clear();
    busy_ = false;
    current_user_.reset();
    conversations_.clear();
    messages_by_conversation_.clear();
    selected_conversation_index_ = 0;
}

void AppState::SelectConversation(size_t index) noexcept {
    if (index < conversations_.size()) {
        selected_conversation_index_ = index;
    }
}

void AppState::SetAuthStatus(const std::wstring& message) {
    auth_status_ = message;
}

void AppState::SetChatStatus(const std::wstring& message) {
    chat_status_ = message;
}

void AppState::SetCurrentUser(const Usuario& user) {
    current_user_ = user;
}

void AppState::SetConversations(std::vector<Conversa> conversations) {
    const int previous_id = selected_conversation_id();
    conversations_ = std::move(conversations);
    if (previous_id != 0) {
        for (size_t index = 0; index < conversations_.size(); ++index) {
            if (conversations_[index].id == previous_id) {
                selected_conversation_index_ = index;
                return;
            }
        }
    }
    NormalizeSelectedConversation();
}

void AppState::SetMessagesForConversation(int conversation_id, std::vector<Mensagem> messages) {
    messages_by_conversation_[conversation_id] = std::move(messages);
}

void AppState::AppendMessages(int conversation_id, const std::vector<Mensagem>& messages) {
    auto& target = messages_by_conversation_[conversation_id];
    for (const Mensagem& message : messages) {
        const auto existing = std::find_if(target.begin(), target.end(), [&message](const Mensagem& item) {
            return item.id == message.id;
        });
        if (existing == target.end()) {
            target.push_back(message);
        }
    }
}

void AppState::RemoveMessage(int conversation_id, int message_id) {
    auto found = messages_by_conversation_.find(conversation_id);
    if (found == messages_by_conversation_.end()) {
        return;
    }

    auto& messages = found->second;
    messages.erase(
        std::remove_if(messages.begin(), messages.end(), [message_id](const Mensagem& item) {
            return item.id == message_id;
        }),
        messages.end());
}

void AppState::UpdateMessageReactions(int conversation_id, int message_id, const std::map<std::wstring, std::vector<int>>& reactions) {
    auto found = messages_by_conversation_.find(conversation_id);
    if (found == messages_by_conversation_.end()) {
        return;
    }

    for (Mensagem& message : found->second) {
        if (message.id == message_id) {
            message.reacoes = reactions;
            return;
        }
    }
}

void AppState::SetComposerText(std::wstring text) {
    composer_text_ = std::move(text);
}

void AppState::ClearComposer() {
    composer_text_.clear();
}

void AppState::SetBusy(bool busy) noexcept {
    busy_ = busy;
}

const Usuario* AppState::current_user() const noexcept {
    return current_user_ ? &current_user_.value() : nullptr;
}

const Conversa* AppState::selected_conversation() const noexcept {
    if (selected_conversation_index_ >= conversations_.size()) {
        return nullptr;
    }
    return &conversations_[selected_conversation_index_];
}

const std::vector<Mensagem>& AppState::selected_messages() const noexcept {
    const Conversa* conversa = selected_conversation();
    if (conversa == nullptr) {
        return empty_messages_;
    }

    const auto found = messages_by_conversation_.find(conversa->id);
    if (found == messages_by_conversation_.end()) {
        return empty_messages_;
    }

    return found->second;
}

int AppState::selected_conversation_id() const noexcept {
    const Conversa* conversation = selected_conversation();
    return conversation != nullptr ? conversation->id : 0;
}

int AppState::last_message_id_for_selected_conversation() const noexcept {
    const std::vector<Mensagem>& messages = selected_messages();
    return messages.empty() ? 0 : messages.back().id;
}

void AppState::NormalizeSelectedConversation() noexcept {
    if (conversations_.empty()) {
        selected_conversation_index_ = 0;
        return;
    }

    if (selected_conversation_index_ >= conversations_.size()) {
        selected_conversation_index_ = 0;
    }
}

void AppState::SeedShellData(const std::wstring& username) {
    Usuario eu;
    eu.id = 1;
    eu.username = username;
    eu.nome = username.empty() ? L"Lucas" : username;
    eu.bio = L"Construindo o cliente nativo do TocaChat";
    current_user_ = eu;

    Usuario ana{2, L"ana.clara", L"Ana Clara", L"Vamos fechar o P0 hoje"};
    Usuario renan{3, L"renan.dev", L"Renan", L"Socket.IO + WinHTTP"};
    Usuario beatriz{4, L"bia.estudos", L"Beatriz", L"Agenda e simulados"};

    conversations_.clear();
    messages_by_conversation_.clear();

    Conversa dm_ana;
    dm_ana.id = 101;
    dm_ana.tipo = L"direto";
    dm_ana.nome = L"Ana Clara";
    dm_ana.membros = {eu, ana};

    std::vector<Mensagem> mensagens_ana;
    mensagens_ana.push_back(MakeMessage(1, 101, 2, L"Fechei o layout do chat no web. Quer alinhar a versão Win32?", L"2026-04-13T10:11:00"));
    mensagens_ana.push_back(MakeMessage(2, 101, 1, L"Sim. Vou começar pelo shell nativo com Direct2D e listas virtualizadas.", L"2026-04-13T10:12:00"));
    mensagens_ana.push_back(MakeMessage(3, 101, 2, L"Perfeito. Depois conectamos no backend existente sem quebrar o protocolo.", L"2026-04-13T10:14:00"));
    dm_ana.ultima_mensagem = mensagens_ana.back();
    messages_by_conversation_[dm_ana.id] = mensagens_ana;

    Conversa grupo_core;
    grupo_core.id = 202;
    grupo_core.tipo = L"grupo";
    grupo_core.nome = L"Equipe TocaChat";
    grupo_core.membros = {eu, ana, renan, beatriz};
    grupo_core.subtopicos = {
        Subtopico{1, L"Geral", L"Roadmap e decisões técnicas", L"#6c5ce7", 0, std::nullopt},
        Subtopico{2, L"Desktop", L"Cliente Win32", L"#2ecc71", 1, std::nullopt}
    };

    std::vector<Mensagem> mensagens_core;
    mensagens_core.push_back(MakeMessage(10, 202, 3, L"Backend pronto para /api/conversas e sync incremental.", L"2026-04-13T09:45:00"));
    mensagens_core.push_back(MakeMessage(11, 202, 4, L"O módulo acadêmico continua no backlog do desktop.", L"2026-04-13T09:52:00"));
    mensagens_core.push_back(MakeMessage(12, 202, 1, L"Nesta etapa vamos fechar solução, tema, janela, login shell e painel principal.", L"2026-04-13T10:03:00"));
    grupo_core.ultima_mensagem = mensagens_core.back();
    messages_by_conversation_[grupo_core.id] = mensagens_core;

    Conversa grupo_estudos;
    grupo_estudos.id = 303;
    grupo_estudos.tipo = L"grupo";
    grupo_estudos.nome = L"Sessão de Estudos";
    grupo_estudos.membros = {eu, beatriz};
    grupo_estudos.subtopicos = {
        Subtopico{3, L"Simulados", L"Histórico e métricas", L"#e67e22", 0, std::nullopt}
    };

    std::vector<Mensagem> mensagens_estudos;
    mensagens_estudos.push_back(MakeMessage(20, 303, 4, L"Quero o dashboard acadêmico no desktop com o mesmo tema escuro.", L"2026-04-12T21:03:00"));
    mensagens_estudos.push_back(MakeMessage(21, 303, 1, L"Vamos manter a base do cliente enxuta e acoplar o módulo acadêmico depois do chat core.", L"2026-04-12T21:08:00"));
    grupo_estudos.ultima_mensagem = mensagens_estudos.back();
    messages_by_conversation_[grupo_estudos.id] = mensagens_estudos;

    conversations_.push_back(dm_ana);
    conversations_.push_back(grupo_core);
    conversations_.push_back(grupo_estudos);
    selected_conversation_index_ = 0;
    call_state_ = {};
    composer_text_.clear();
    busy_ = false;
}

}  // namespace tocachat

#pragma once

#include "common.h"

namespace tocachat {

struct Usuario {
    int id = 0;
    std::wstring username;
    std::wstring nome;
    std::wstring bio;
    std::wstring foto_url;
    std::wstring wallpaper_url;
};

struct Subtopico {
    int id = 0;
    std::wstring nome;
    std::wstring descricao;
    std::wstring cor;
    int ordem = 0;
    std::optional<int> pinned_message_id;
};

struct Mensagem {
    int id = 0;
    int conversa_id = 0;
    int remetente_id = 0;
    std::wstring conteudo;
    std::wstring media_url;
    std::optional<int> reply_to_id;
    std::optional<int> subtopico_id;
    std::wstring criado_em;
    std::optional<std::wstring> excluido_em;
    std::map<std::wstring, std::vector<int>> reacoes;
};

struct Conversa {
    int id = 0;
    std::wstring tipo;
    std::wstring nome;
    std::wstring foto_url;
    std::wstring wallpaper_url;
    std::optional<int> pinned_message_id;
    std::optional<Mensagem> ultima_mensagem;
    std::vector<Usuario> membros;
    std::vector<Subtopico> subtopicos;
};

struct CallState {
    enum class Status {
        Idle,
        Ringing,
        InCall
    };

    Status status = Status::Idle;
    int conversa_id = 0;
    std::vector<int> participantes;
    bool camera_on = false;
    bool mic_on = true;
};

}  // namespace tocachat

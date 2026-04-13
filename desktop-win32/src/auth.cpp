#include "auth.h"

namespace tocachat {

namespace {

std::wstring JsonStringOrDefault(const Json& json_body, const char* key) {
    if (!json_body.is_object() || !json_body.contains(key) || !json_body[key].is_string()) {
        return {};
    }
    return Utf8ToWide(json_body[key].get<std::string>());
}

}  // namespace

std::optional<Usuario> AuthController::ParseUsuario(const Json& json_body) {
    if (!json_body.is_object() || !json_body.contains("id")) {
        return std::nullopt;
    }

    Usuario user;
    user.id = json_body.value("id", 0);
    user.username = JsonStringOrDefault(json_body, "username");
    user.nome = JsonStringOrDefault(json_body, "nome");
    user.bio = JsonStringOrDefault(json_body, "bio");
    user.foto_url = JsonStringOrDefault(json_body, "foto");
    user.wallpaper_url = JsonStringOrDefault(json_body, "wallpaper");
    if (user.nome.empty()) {
        user.nome = user.username;
    }
    return user.id != 0 ? std::optional<Usuario>(user) : std::nullopt;
}

AuthResult AuthController::Submit(AppState& state,
                                  const AuthCredentials& credentials,
                                  bool creating_account) const {
    if (credentials.login.empty() || credentials.senha.empty()) {
        return {false, L"Preencha usuário e senha para continuar."};
    }

    Json body = {
        {creating_account ? "username" : "login", WideToUtf8(credentials.login)},
        {"senha", WideToUtf8(credentials.senha)}
    };

    const ApiResult api = network_.PostJson(creating_account ? L"/api/registro" : L"/api/login", body);
    if (!api.ok) {
        return {false, api.error_message.empty() ? L"Falha ao autenticar no backend." : api.error_message};
    }

    const std::optional<Usuario> user = ParseUsuario(api.json_body);
    if (!user.has_value()) {
        return {false, L"Resposta de autenticação inválida."};
    }

    state.EnterAuthenticatedChat(user.value());
    return {
        true,
        creating_account
            ? L"Conta criada com sucesso no backend."
            : L"Sessão autenticada com sucesso."
    };
}

AuthResult AuthController::TryResume(AppState& state) const {
    const ApiResult api = network_.GetJson(L"/api/me");
    if (!api.ok) {
        return {false, L"Sessão salva inválida ou expirada."};
    }

    const std::optional<Usuario> user = ParseUsuario(api.json_body);
    if (!user.has_value()) {
        return {false, L"Não foi possível restaurar a sessão salva."};
    }

    state.EnterAuthenticatedChat(user.value());
    return {true, L"Sessão restaurada a partir do cookie salvo."};
}

AuthResult AuthController::Logout(AppState& state) const {
    const ApiResult api = network_.PostJson(L"/api/logout", Json::object());
    network_.ClearPersistedSession();
    state.ReturnToAuth();

    if (!api.ok) {
        return {false, api.error_message.empty() ? L"Sessão local encerrada." : api.error_message};
    }
    return {true, L"Sessão encerrada."};
}

}  // namespace tocachat

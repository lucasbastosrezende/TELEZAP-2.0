#pragma once

#include "app_state.h"
#include "network.h"

namespace tocachat {

struct AuthCredentials {
    std::wstring login;
    std::wstring senha;
};

struct AuthResult {
    bool ok = false;
    std::wstring message;
};

class AuthController {
public:
    explicit AuthController(NetworkClient& network) noexcept
        : network_(network) {}

    AuthResult Submit(AppState& state, const AuthCredentials& credentials, bool creating_account) const;
    AuthResult TryResume(AppState& state) const;
    AuthResult Logout(AppState& state) const;

private:
    static std::optional<Usuario> ParseUsuario(const Json& json_body);

    NetworkClient& network_;
};

}  // namespace tocachat

#pragma once

#include "common.h"
#include "json\\json.hpp"

namespace tocachat {

using Json = nlohmann::json;

struct ApiResult {
    bool ok = false;
    DWORD status_code = 0;
    Json json_body;
    std::wstring error_message;
    std::string raw_body;
};

class NetworkClient {
public:
    NetworkClient();
    ~NetworkClient();

    bool Initialize();
    void Shutdown();

    void SetBaseUrl(const std::wstring& base_url);
    const std::wstring& base_url() const noexcept { return base_url_; }
    std::wstring cookie_header() const;

    bool LoadPersistedSession();
    void ClearPersistedSession();
    bool HasPersistedSession() const noexcept;

    ApiResult GetJson(const std::wstring& path);
    ApiResult PostJson(const std::wstring& path, const Json& body);

private:
    struct ParsedBaseUrl {
        std::wstring host;
        INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
        bool secure = true;
        std::wstring base_path;
    };

    ApiResult SendJsonRequest(const std::wstring& method,
                              const std::wstring& path,
                              const std::string* utf8_body);
    bool ParseBaseUrl();
    bool EnsureSessionHandle();
    bool UpdateCookiesFromResponse(HINTERNET request);
    std::wstring BuildCookieHeader() const;
    std::wstring BuildRequestPath(const std::wstring& path) const;
    std::wstring SessionFilePath() const;
    bool PersistCookies() const;

    HINTERNET session_handle_ = nullptr;
    std::wstring base_url_ = L"https://tocachat.duckdns.org:8080";
    ParsedBaseUrl parsed_url_;
    std::map<std::wstring, std::wstring> cookies_;
};

}  // namespace tocachat

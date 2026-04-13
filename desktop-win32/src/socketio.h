#pragma once

#include "common.h"
#include "network.h"

namespace tocachat {

struct SocketEvent {
    std::wstring name;
    Json payload;
};

class SocketIoClient {
public:
    using EventCallback = std::function<void(const SocketEvent&)>;
    using StatusCallback = std::function<void(bool connected, const std::wstring& message)>;

    SocketIoClient();
    ~SocketIoClient();

    void Configure(const std::wstring& base_url, const std::wstring& cookie_header);
    void SetEventCallback(EventCallback callback);
    void SetStatusCallback(StatusCallback callback);

    bool Start();
    void Stop();
    bool EmitEvent(const std::wstring& event_name, const Json& payload);

    bool is_connected() const noexcept { return connected_.load(); }

private:
    struct ParsedBaseUrl {
        std::wstring host;
        INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
        bool secure = true;
        std::wstring base_path;
    };

    void WorkerLoop();
    bool OpenSession();
    bool ParseBaseUrl();
    bool Handshake();
    bool OpenNamespace();
    bool PollOnce();
    bool SendEnginePacket(const std::string& packet);
    bool SendHttpRequest(const std::wstring& method, const std::wstring& path, const std::string* body, std::string& response_body);
    void ProcessPayload(const std::string& payload);
    void ProcessPacket(const std::string& packet);
    void NotifyStatus(bool connected, const std::wstring& message);
    std::wstring SocketPathBase() const;

    std::wstring base_url_ = L"https://tocachat.duckdns.org:8080";
    std::wstring cookie_header_;
    ParsedBaseUrl parsed_url_;
    HINTERNET session_handle_ = nullptr;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};
    std::thread worker_;
    std::mutex emit_mutex_;
    std::wstring sid_;
    EventCallback event_callback_;
    StatusCallback status_callback_;
};

}  // namespace tocachat

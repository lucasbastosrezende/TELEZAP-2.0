#include "socketio.h"

namespace tocachat {

namespace {

class HttpHandle {
public:
    explicit HttpHandle(HINTERNET handle = nullptr) noexcept
        : handle_(handle) {}

    ~HttpHandle() {
        if (handle_ != nullptr) {
            ::WinHttpCloseHandle(handle_);
        }
    }

    HttpHandle(const HttpHandle&) = delete;
    HttpHandle& operator=(const HttpHandle&) = delete;

    HINTERNET get() const noexcept { return handle_; }

private:
    HINTERNET handle_;
};

std::wstring TimeToken() {
    return std::to_wstring(static_cast<long long>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count()));
}

std::vector<std::string> SplitEnginePayload(const std::string& payload) {
    std::vector<std::string> packets;
    size_t start = 0;
    while (start <= payload.size()) {
        size_t end = payload.find('\x1e', start);
        if (end == std::string::npos) {
            if (start < payload.size()) {
                packets.push_back(payload.substr(start));
            }
            break;
        }
        packets.push_back(payload.substr(start, end - start));
        start = end + 1;
    }
    return packets;
}

}  // namespace

SocketIoClient::SocketIoClient() = default;

SocketIoClient::~SocketIoClient() {
    Stop();
}

void SocketIoClient::Configure(const std::wstring& base_url, const std::wstring& cookie_header) {
    base_url_ = base_url;
    cookie_header_ = cookie_header;
    ParseBaseUrl();
}

void SocketIoClient::SetEventCallback(EventCallback callback) {
    event_callback_ = std::move(callback);
}

void SocketIoClient::SetStatusCallback(StatusCallback callback) {
    status_callback_ = std::move(callback);
}

bool SocketIoClient::Start() {
    Stop();
    if (!ParseBaseUrl()) {
        NotifyStatus(false, L"URL do Socket.IO inválida.");
        return false;
    }

    running_ = true;
    worker_ = std::thread(&SocketIoClient::WorkerLoop, this);
    return true;
}

void SocketIoClient::Stop() {
    running_ = false;
    connected_ = false;
    sid_.clear();
    if (worker_.joinable()) {
        worker_.join();
    }
    if (session_handle_ != nullptr) {
        ::WinHttpCloseHandle(session_handle_);
        session_handle_ = nullptr;
    }
}

bool SocketIoClient::OpenSession() {
    if (session_handle_ != nullptr) {
        return true;
    }
    session_handle_ = ::WinHttpOpen(
        L"TocaChatDesktopSocket/0.1",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (session_handle_ == nullptr) {
        return false;
    }
    // Resolve/connect/send: 5s cada. Receive: 35s.
    // Engine.IO long-poll: o servidor segura o GET aberto por ~25s (pingInterval padrão).
    // Com 8s o cliente cronometrava antes do servidor responder, causando reconexão constante.
    ::WinHttpSetTimeouts(session_handle_, 5000, 5000, 5000, 35000);
    return true;
}

bool SocketIoClient::ParseBaseUrl() {
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);

    std::vector<wchar_t> buffer(base_url_.begin(), base_url_.end());
    buffer.push_back(L'\0');
    if (!::WinHttpCrackUrl(buffer.data(), 0, 0, &parts)) {
        return false;
    }

    parsed_url_.host.assign(parts.lpszHostName, parts.dwHostNameLength);
    parsed_url_.port = parts.nPort;
    parsed_url_.secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
    parsed_url_.base_path.assign(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parsed_url_.base_path.empty()) {
        parsed_url_.base_path = L"/";
    }
    if (parsed_url_.base_path.back() == L'/') {
        parsed_url_.base_path.pop_back();
    }
    return !parsed_url_.host.empty();
}

std::wstring SocketIoClient::SocketPathBase() const {
    if (parsed_url_.base_path.empty() || parsed_url_.base_path == L"/") {
        return L"/socket.io/";
    }
    return parsed_url_.base_path + L"/socket.io/";
}

void SocketIoClient::NotifyStatus(bool connected, const std::wstring& message) {
    connected_ = connected;
    if (status_callback_) {
        status_callback_(connected, message);
    }
}

bool SocketIoClient::SendHttpRequest(const std::wstring& method,
                                     const std::wstring& path,
                                     const std::string* body,
                                     std::string& response_body) {
    if (!OpenSession()) {
        return false;
    }

    HttpHandle connection(::WinHttpConnect(session_handle_, parsed_url_.host.c_str(), parsed_url_.port, 0));
    if (connection.get() == nullptr) {
        return false;
    }

    const DWORD flags = parsed_url_.secure ? WINHTTP_FLAG_SECURE : 0;
    HttpHandle request(::WinHttpOpenRequest(connection.get(), method.c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (request.get() == nullptr) {
        return false;
    }

    std::wstring headers = L"Accept: */*\r\n";
    if (!cookie_header_.empty()) {
        headers += L"Cookie: " + cookie_header_ + L"\r\n";
    }
    if (body != nullptr) {
        headers += L"Content-Type: text/plain;charset=UTF-8\r\n";
    }

    if (!::WinHttpAddRequestHeaders(request.get(), headers.c_str(), static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD)) {
        return false;
    }

    LPVOID optional = WINHTTP_NO_REQUEST_DATA;
    DWORD optional_size = 0;
    if (body != nullptr && !body->empty()) {
        optional = const_cast<char*>(body->data());
        optional_size = static_cast<DWORD>(body->size());
    }

    if (!::WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, optional, optional_size, optional_size, 0)) {
        return false;
    }
    if (!::WinHttpReceiveResponse(request.get(), nullptr)) {
        return false;
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    if (!::WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size, WINHTTP_NO_HEADER_INDEX)) {
        return false;
    }
    if (status_code < 200 || status_code >= 300) {
        return false;
    }

    response_body.clear();
    DWORD available = 0;
    do {
        if (!::WinHttpQueryDataAvailable(request.get(), &available)) {
            return false;
        }
        if (available == 0) {
            break;
        }
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!::WinHttpReadData(request.get(), chunk.data(), available, &read)) {
            return false;
        }
        chunk.resize(read);
        response_body += chunk;
    } while (available > 0);

    return true;
}

bool SocketIoClient::Handshake() {
    const std::wstring path = SocketPathBase() + L"?EIO=4&transport=polling&t=" + TimeToken();
    std::string response;
    if (!SendHttpRequest(L"GET", path, nullptr, response)) {
        return false;
    }

    ProcessPayload(response);
    return !sid_.empty();
}

bool SocketIoClient::OpenNamespace() {
    return SendEnginePacket("40");
}

bool SocketIoClient::SendEnginePacket(const std::string& packet) {
    if (sid_.empty()) {
        return false;
    }

    const std::wstring path = SocketPathBase() + L"?EIO=4&transport=polling&sid=" + sid_ + L"&t=" + TimeToken();
    std::string response;
    std::lock_guard<std::mutex> lock(emit_mutex_);
    return SendHttpRequest(L"POST", path, &packet, response);
}

bool SocketIoClient::EmitEvent(const std::wstring& event_name, const Json& payload) {
    Json array = Json::array();
    array.push_back(WideToUtf8(event_name));
    array.push_back(payload);
    return SendEnginePacket("42" + array.dump());
}

void SocketIoClient::ProcessPacket(const std::string& packet) {
    if (packet.empty()) {
        return;
    }

    // ── Engine.IO packet types ─────────────────────────────────────────────
    // '0' open: servidor envia SID e configurações de ping/timeout
    if (packet[0] == '0') {
        try {
            Json open = Json::parse(packet.substr(1));
            if (open.contains("sid") && open["sid"].is_string()) {
                sid_ = Utf8ToWide(open["sid"].get<std::string>());
                NotifyStatus(true, L"Socket.IO conectado por polling.");
            }
        } catch (...) {
        }
        return;
    }

    // '1' close: servidor pediu encerramento da sessão
    if (packet[0] == '1') {
        connected_ = false;
        sid_.clear();
        return;
    }

    // '2' ping: servidor envia ping, cliente responde com '3' pong
    if (packet == "2") {
        SendEnginePacket("3");
        return;
    }

    // '6' noop: keepalive sem conteúdo, ignorar silenciosamente
    if (packet[0] == '6') {
        return;
    }

    // ── Socket.IO packet types (prefixados com EIO type '4') ──────────────
    // '40' namespace connect: Socket.IO confirmou entrada no namespace "/"
    if (packet.rfind("40", 0) == 0) {
        NotifyStatus(true, L"Namespace Socket.IO pronto.");
        return;
    }

    // '41' namespace disconnect: servidor expulsou do namespace, reconectar
    if (packet.rfind("41", 0) == 0) {
        connected_ = false;
        sid_.clear();
        return;
    }

    // '42' event: pacote de evento Socket.IO (formato ["nome", payload])
    if (packet.rfind("42", 0) == 0) {
        try {
            Json payload = Json::parse(packet.substr(2));
            if (payload.is_array() && payload.size() >= 2 && payload[0].is_string()) {
                SocketEvent event;
                event.name = Utf8ToWide(payload[0].get<std::string>());
                event.payload = payload[1];
                if (event_callback_) {
                    event_callback_(event);
                }
            }
        } catch (...) {
        }
        return;
    }
}

void SocketIoClient::ProcessPayload(const std::string& payload) {
    for (const std::string& packet : SplitEnginePayload(payload)) {
        ProcessPacket(packet);
    }
}

bool SocketIoClient::PollOnce() {
    if (sid_.empty()) {
        return false;
    }

    const std::wstring path = SocketPathBase() + L"?EIO=4&transport=polling&sid=" + sid_ + L"&t=" + TimeToken();
    std::string response;
    if (!SendHttpRequest(L"GET", path, nullptr, response)) {
        return false;
    }
    if (!response.empty()) {
        ProcessPayload(response);
    } else {
        // Resposta vazia: servidor respondeu antes do timeout do long-poll (incomum).
        // Pausa curta para não criar spin-loop caso o servidor fique respondendo rápido.
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
    // Retorna false se ProcessPayload limpou sid_ (ex.: pacote close '1' ou '41')
    return !sid_.empty();
}

void SocketIoClient::WorkerLoop() {
    // Backoff exponencial para reconexões: começa em 2s, dobra até o cap de 30s.
    // Reseta para 2s a cada vez que o polling entra em estado conectado com sucesso.
    uint32_t retry_ms = 2000;

    while (running_) {
        sid_.clear();

        if (!Handshake() || !OpenNamespace()) {
            const std::wstring delay_label = std::to_wstring(retry_ms / 1000);
            NotifyStatus(false, L"Falha ao conectar Socket.IO. Retry em " + delay_label + L"s.");
            // Sleep em fatias de 200 ms para responder rapidamente ao Stop()
            for (uint32_t elapsed = 0; elapsed < retry_ms && running_; elapsed += 200) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            retry_ms = std::min(retry_ms * 2, static_cast<uint32_t>(30000));
            continue;
        }

        retry_ms = 2000;  // Reconectou com sucesso: reseta o backoff

        while (running_) {
            if (!PollOnce()) {
                // Falha de poll: pode ser timeout HTTP, close pelo servidor, ou sid_ limpo por '1'/'41'
                NotifyStatus(false, L"Socket.IO desconectado. Reconectando...");
                connected_ = false;
                sid_.clear();
                break;
            }
        }
    }
}

}  // namespace tocachat

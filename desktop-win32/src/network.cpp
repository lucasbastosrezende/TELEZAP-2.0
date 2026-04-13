#include "network.h"

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
    operator HINTERNET() const noexcept { return handle_; }

private:
    HINTERNET handle_;
};

std::wstring FormatWindowsErrorMessage(DWORD error_code) {
    LPWSTR raw = nullptr;
    const DWORD size = ::FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&raw),
        0,
        nullptr);

    if (size == 0 || raw == nullptr) {
        return L"Erro Win32 " + std::to_wstring(error_code);
    }

    std::wstring result(raw, size);
    ::LocalFree(raw);
    return TrimCopy(result);
}

bool StartsWithInsensitive(const std::wstring& value, const wchar_t* prefix) {
    const size_t prefix_length = wcslen(prefix);
    if (value.size() < prefix_length) {
        return false;
    }

    return _wcsnicmp(value.c_str(), prefix, prefix_length) == 0;
}

bool ReadAllBytes(const std::wstring& path, std::vector<BYTE>& bytes) {
    HANDLE file = ::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    LARGE_INTEGER size{};
    const bool ok = ::GetFileSizeEx(file, &size) == TRUE && size.QuadPart >= 0;
    if (!ok || size.QuadPart > static_cast<LONGLONG>(16 * 1024)) {
        ::CloseHandle(file);
        return false;
    }

    bytes.resize(static_cast<size_t>(size.QuadPart));
    DWORD bytes_read = 0;
    const bool read_ok = bytes.empty() || ::ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &bytes_read, nullptr) == TRUE;
    ::CloseHandle(file);
    return read_ok && bytes_read == bytes.size();
}

bool WriteAllBytes(const std::wstring& path, const BYTE* data, DWORD size) {
    HANDLE file = ::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD bytes_written = 0;
    const bool ok = ::WriteFile(file, data, size, &bytes_written, nullptr) == TRUE && bytes_written == size;
    ::CloseHandle(file);
    return ok;
}

}  // namespace

NetworkClient::NetworkClient() = default;

NetworkClient::~NetworkClient() {
    Shutdown();
}

bool NetworkClient::Initialize() {
    return ParseBaseUrl() && EnsureSessionHandle();
}

void NetworkClient::Shutdown() {
    if (session_handle_ != nullptr) {
        ::WinHttpCloseHandle(session_handle_);
        session_handle_ = nullptr;
    }
}

void NetworkClient::SetBaseUrl(const std::wstring& base_url) {
    base_url_ = base_url;
    ParseBaseUrl();
}

std::wstring NetworkClient::cookie_header() const {
    return BuildCookieHeader();
}

bool NetworkClient::ParseBaseUrl() {
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);

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

bool NetworkClient::EnsureSessionHandle() {
    if (session_handle_ != nullptr) {
        return true;
    }

    session_handle_ = ::WinHttpOpen(
        L"TocaChatDesktop/0.1",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (session_handle_ == nullptr) {
        return false;
    }

    ::WinHttpSetTimeouts(session_handle_, 5000, 5000, 5000, 8000);
    return true;
}

std::wstring NetworkClient::BuildCookieHeader() const {
    std::vector<std::wstring> parts;
    for (const auto& [name, value] : cookies_) {
        if (!name.empty() && !value.empty()) {
            parts.push_back(name + L"=" + value);
        }
    }
    return JoinStrings(parts, L"; ");
}

std::wstring NetworkClient::BuildRequestPath(const std::wstring& path) const {
    if (path.empty()) {
        return parsed_url_.base_path.empty() ? L"/" : parsed_url_.base_path;
    }

    if (parsed_url_.base_path.empty() || parsed_url_.base_path == L"/") {
        return path.front() == L'/' ? path : L"/" + path;
    }

    if (path.front() == L'/') {
        return parsed_url_.base_path + path;
    }
    return parsed_url_.base_path + L"/" + path;
}

std::wstring NetworkClient::SessionFilePath() const {
    const std::wstring base = GetKnownFolderPathString(FOLDERID_RoamingAppData);
    if (base.empty()) {
        return {};
    }

    const std::wstring directory = base + L"\\TocaChatDesktop";
    ::CreateDirectoryW(directory.c_str(), nullptr);
    return directory + L"\\session.dat";
}

bool NetworkClient::PersistCookies() const {
    const std::wstring path = SessionFilePath();
    if (path.empty()) {
        return false;
    }

    const std::wstring cookie_header = BuildCookieHeader();
    if (cookie_header.empty()) {
        ::DeleteFileW(path.c_str());
        return true;
    }

    const std::string utf8 = WideToUtf8(cookie_header);
    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(utf8.data()));
    input.cbData = static_cast<DWORD>(utf8.size());

    DATA_BLOB output{};
    const BOOL protect_ok = ::CryptProtectData(&input, L"TocaChat Session", nullptr, nullptr, nullptr, 0, &output);
    if (!protect_ok) {
        return false;
    }

    const bool write_ok = WriteAllBytes(path, output.pbData, output.cbData);
    ::LocalFree(output.pbData);
    return write_ok;
}

bool NetworkClient::LoadPersistedSession() {
    cookies_.clear();
    const std::wstring path = SessionFilePath();
    if (path.empty()) {
        return false;
    }

    std::vector<BYTE> bytes;
    if (!ReadAllBytes(path, bytes) || bytes.empty()) {
        return false;
    }

    DATA_BLOB input{};
    input.pbData = bytes.data();
    input.cbData = static_cast<DWORD>(bytes.size());

    DATA_BLOB output{};
    if (!::CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0, &output)) {
        return false;
    }

    const std::string utf8(reinterpret_cast<char*>(output.pbData), output.cbData);
    ::LocalFree(output.pbData);

    const std::wstring cookie_header = Utf8ToWide(utf8);
    size_t begin = 0;
    while (begin < cookie_header.size()) {
        size_t end = cookie_header.find(L';', begin);
        std::wstring token = TrimCopy(cookie_header.substr(begin, end == std::wstring::npos ? std::wstring::npos : end - begin));
        if (!token.empty()) {
            const size_t separator = token.find(L'=');
            if (separator != std::wstring::npos) {
                cookies_[TrimCopy(token.substr(0, separator))] = token.substr(separator + 1);
            }
        }
        if (end == std::wstring::npos) {
            break;
        }
        begin = end + 1;
    }

    return !cookies_.empty();
}

void NetworkClient::ClearPersistedSession() {
    cookies_.clear();
    const std::wstring path = SessionFilePath();
    if (!path.empty()) {
        ::DeleteFileW(path.c_str());
    }
}

bool NetworkClient::HasPersistedSession() const noexcept {
    return !cookies_.empty();
}

bool NetworkClient::UpdateCookiesFromResponse(HINTERNET request) {
    DWORD size = 0;
    ::WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, WINHTTP_NO_OUTPUT_BUFFER, &size, WINHTTP_NO_HEADER_INDEX);
    if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0) {
        return false;
    }

    std::wstring headers(size / sizeof(wchar_t), L'\0');
    if (!::WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, headers.data(), &size, WINHTTP_NO_HEADER_INDEX)) {
        return false;
    }

    bool changed = false;
    size_t start = 0;
    while (start < headers.size()) {
        size_t end = headers.find(L"\r\n", start);
        const std::wstring line = headers.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
        if (StartsWithInsensitive(line, L"Set-Cookie:")) {
            const std::wstring remainder = TrimCopy(line.substr(11));
            const size_t semicolon = remainder.find(L';');
            const std::wstring pair = semicolon == std::wstring::npos ? remainder : remainder.substr(0, semicolon);
            const size_t equals = pair.find(L'=');
            if (equals != std::wstring::npos) {
                const std::wstring name = TrimCopy(pair.substr(0, equals));
                const std::wstring value = pair.substr(equals + 1);
                if (!name.empty()) {
                    if (value.empty()) {
                        cookies_.erase(name);
                    } else {
                        cookies_[name] = value;
                    }
                    changed = true;
                }
            }
        }

        if (end == std::wstring::npos) {
            break;
        }
        start = end + 2;
    }

    if (changed) {
        PersistCookies();
    }
    return changed;
}

ApiResult NetworkClient::GetJson(const std::wstring& path) {
    return SendJsonRequest(L"GET", path, nullptr);
}

ApiResult NetworkClient::PostJson(const std::wstring& path, const Json& body) {
    const std::string utf8_body = body.dump();
    return SendJsonRequest(L"POST", path, &utf8_body);
}

ApiResult NetworkClient::SendJsonRequest(const std::wstring& method,
                                         const std::wstring& path,
                                         const std::string* utf8_body) {
    ApiResult result{};
    if (!Initialize()) {
        result.error_message = L"Falha ao inicializar WinHTTP.";
        return result;
    }

    HttpHandle connection(::WinHttpConnect(session_handle_, parsed_url_.host.c_str(), parsed_url_.port, 0));
    if (connection.get() == nullptr) {
        result.error_message = FormatWindowsErrorMessage(::GetLastError());
        return result;
    }

    const std::wstring request_path = BuildRequestPath(path);
    const DWORD flags = parsed_url_.secure ? WINHTTP_FLAG_SECURE : 0;
    HttpHandle request(::WinHttpOpenRequest(connection.get(), method.c_str(), request_path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (request.get() == nullptr) {
        result.error_message = FormatWindowsErrorMessage(::GetLastError());
        return result;
    }

    std::wstring headers = L"Accept: application/json\r\n";
    const std::wstring cookie_header = BuildCookieHeader();
    if (!cookie_header.empty()) {
        headers += L"Cookie: " + cookie_header + L"\r\n";
    }
    if (utf8_body != nullptr) {
        headers += L"Content-Type: application/json\r\n";
    }

    if (!headers.empty() && !::WinHttpAddRequestHeaders(request.get(), headers.c_str(), static_cast<DWORD>(-1), WINHTTP_ADDREQ_FLAG_ADD)) {
        result.error_message = FormatWindowsErrorMessage(::GetLastError());
        return result;
    }

    LPVOID optional = WINHTTP_NO_REQUEST_DATA;
    DWORD optional_size = 0;
    if (utf8_body != nullptr && !utf8_body->empty()) {
        optional = const_cast<char*>(utf8_body->data());
        optional_size = static_cast<DWORD>(utf8_body->size());
    }

    if (!::WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, optional, optional_size, optional_size, 0)) {
        result.error_message = FormatWindowsErrorMessage(::GetLastError());
        return result;
    }

    if (!::WinHttpReceiveResponse(request.get(), nullptr)) {
        result.error_message = FormatWindowsErrorMessage(::GetLastError());
        return result;
    }

    UpdateCookiesFromResponse(request.get());

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    if (::WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size, WINHTTP_NO_HEADER_INDEX)) {
        result.status_code = status_code;
    }

    std::string response_body;
    DWORD available = 0;
    do {
        available = 0;
        if (!::WinHttpQueryDataAvailable(request.get(), &available)) {
            result.error_message = FormatWindowsErrorMessage(::GetLastError());
            return result;
        }

        if (available == 0) {
            break;
        }

        std::string chunk(available, '\0');
        DWORD bytes_read = 0;
        if (!::WinHttpReadData(request.get(), chunk.data(), available, &bytes_read)) {
            result.error_message = FormatWindowsErrorMessage(::GetLastError());
            return result;
        }
        chunk.resize(bytes_read);
        response_body += chunk;
    } while (available > 0);

    result.raw_body = response_body;

    if (!response_body.empty()) {
        try {
            result.json_body = Json::parse(response_body);
        } catch (...) {
            if (status_code >= 200 && status_code < 300) {
                result.error_message = L"Servidor respondeu com JSON inválido.";
                return result;
            }
        }
    }

    if (status_code >= 200 && status_code < 300) {
        result.ok = true;
        return result;
    }

    if (result.json_body.is_object() && result.json_body.contains("erro") && result.json_body["erro"].is_string()) {
        result.error_message = Utf8ToWide(result.json_body["erro"].get<std::string>());
    } else if (!response_body.empty()) {
        result.error_message = Utf8ToWide(response_body);
    } else {
        result.error_message = L"Erro HTTP " + std::to_wstring(status_code);
    }

    return result;
}

}  // namespace tocachat

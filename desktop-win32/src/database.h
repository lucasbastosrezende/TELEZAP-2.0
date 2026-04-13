#pragma once

#include "models.h"
#include "sqlite3.h"

namespace tocachat {

class DatabaseCache {
public:
    DatabaseCache();
    ~DatabaseCache();

    bool OpenDefault();
    void Close() noexcept;
    bool is_open() const noexcept { return db_ != nullptr; }

    void SaveConversations(const std::vector<Conversa>& conversations);
    std::vector<Conversa> LoadConversations();

    void SaveMessages(int conversation_id, const std::vector<Mensagem>& messages);
    std::vector<Mensagem> LoadMessages(int conversation_id);

private:
    bool EnsureSchema();
    std::wstring DatabasePath() const;
    static std::string Narrow(const std::wstring& value);
    static std::wstring Wide(const unsigned char* value);

    sqlite3* db_ = nullptr;
};

}  // namespace tocachat

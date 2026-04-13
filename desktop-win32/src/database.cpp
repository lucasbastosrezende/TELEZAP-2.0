#include "database.h"
#include "common.h"

namespace tocachat {

DatabaseCache::DatabaseCache() = default;

DatabaseCache::~DatabaseCache() {
    Close();
}

bool DatabaseCache::OpenDefault() {
    Close();

    const std::string path = Narrow(DatabasePath());
    if (path.empty()) {
        return false;
    }

    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        Close();
        return false;
    }

    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
    return EnsureSchema();
}

void DatabaseCache::Close() noexcept {
    if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool DatabaseCache::EnsureSchema() {
    static const char* schema = R"SQL(
CREATE TABLE IF NOT EXISTS cached_conversations (
    id INTEGER PRIMARY KEY,
    tipo TEXT,
    nome TEXT,
    foto_url TEXT,
    wallpaper_url TEXT,
    pinned_message_id INTEGER,
    ultima_msg TEXT,
    ultima_msg_em TEXT
);
CREATE TABLE IF NOT EXISTS cached_messages (
    id INTEGER PRIMARY KEY,
    conversation_id INTEGER NOT NULL,
    sender_id INTEGER NOT NULL,
    content TEXT,
    media_url TEXT,
    reply_to_id INTEGER,
    subtopico_id INTEGER,
    created_at TEXT
);
CREATE INDEX IF NOT EXISTS idx_cached_messages_conversation_id ON cached_messages(conversation_id, id);
)SQL";

    return sqlite3_exec(db_, schema, nullptr, nullptr, nullptr) == SQLITE_OK;
}

std::wstring DatabaseCache::DatabasePath() const {
    const std::wstring base = GetKnownFolderPathString(FOLDERID_RoamingAppData);
    if (base.empty()) {
        return {};
    }

    const std::wstring directory = base + L"\\TocaChatDesktop";
    ::CreateDirectoryW(directory.c_str(), nullptr);
    return directory + L"\\cache.db";
}

std::string DatabaseCache::Narrow(const std::wstring& value) {
    return WideToUtf8(value);
}

std::wstring DatabaseCache::Wide(const unsigned char* value) {
    if (value == nullptr) {
        return {};
    }
    return Utf8ToWide(reinterpret_cast<const char*>(value));
}

void DatabaseCache::SaveConversations(const std::vector<Conversa>& conversations) {
    if (db_ == nullptr) {
        return;
    }

    sqlite3_exec(db_, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "DELETE FROM cached_conversations;", nullptr, nullptr, nullptr);

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT OR REPLACE INTO cached_conversations "
        "(id, tipo, nome, foto_url, wallpaper_url, pinned_message_id, ultima_msg, ultima_msg_em) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return;
    }

    for (const Conversa& conversation : conversations) {
        sqlite3_bind_int(stmt, 1, conversation.id);
        sqlite3_bind_text(stmt, 2, Narrow(conversation.tipo).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, Narrow(conversation.nome).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, Narrow(conversation.foto_url).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, Narrow(conversation.wallpaper_url).c_str(), -1, SQLITE_TRANSIENT);
        if (conversation.pinned_message_id.has_value()) {
            sqlite3_bind_int(stmt, 6, conversation.pinned_message_id.value());
        } else {
            sqlite3_bind_null(stmt, 6);
        }
        const std::wstring last_text = conversation.ultima_mensagem ? conversation.ultima_mensagem->conteudo : L"";
        const std::wstring last_time = conversation.ultima_mensagem ? conversation.ultima_mensagem->criado_em : L"";
        sqlite3_bind_text(stmt, 7, Narrow(last_text).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 8, Narrow(last_time).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
}

std::vector<Conversa> DatabaseCache::LoadConversations() {
    std::vector<Conversa> conversations;
    if (db_ == nullptr) {
        return conversations;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id, tipo, nome, foto_url, wallpaper_url, pinned_message_id, ultima_msg, ultima_msg_em "
        "FROM cached_conversations ORDER BY COALESCE(ultima_msg_em, '') DESC;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return conversations;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Conversa conversation;
        conversation.id = sqlite3_column_int(stmt, 0);
        conversation.tipo = Wide(sqlite3_column_text(stmt, 1));
        conversation.nome = Wide(sqlite3_column_text(stmt, 2));
        conversation.foto_url = Wide(sqlite3_column_text(stmt, 3));
        conversation.wallpaper_url = Wide(sqlite3_column_text(stmt, 4));
        if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
            conversation.pinned_message_id = sqlite3_column_int(stmt, 5);
        }

        const std::wstring last_message_text = Wide(sqlite3_column_text(stmt, 6));
        if (!last_message_text.empty()) {
            Mensagem last_message;
            last_message.conversa_id = conversation.id;
            last_message.conteudo = last_message_text;
            last_message.criado_em = Wide(sqlite3_column_text(stmt, 7));
            conversation.ultima_mensagem = last_message;
        }

        conversations.push_back(std::move(conversation));
    }

    sqlite3_finalize(stmt);
    return conversations;
}

void DatabaseCache::SaveMessages(int conversation_id, const std::vector<Mensagem>& messages) {
    if (db_ == nullptr) {
        return;
    }

    sqlite3_exec(db_, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr);
    sqlite3_stmt* clear_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "DELETE FROM cached_messages WHERE conversation_id = ?;", -1, &clear_stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(clear_stmt, 1, conversation_id);
        sqlite3_step(clear_stmt);
    }
    sqlite3_finalize(clear_stmt);

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT OR REPLACE INTO cached_messages "
        "(id, conversation_id, sender_id, content, media_url, reply_to_id, subtopico_id, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return;
    }

    for (const Mensagem& message : messages) {
        sqlite3_bind_int(stmt, 1, message.id);
        sqlite3_bind_int(stmt, 2, conversation_id);
        sqlite3_bind_int(stmt, 3, message.remetente_id);
        sqlite3_bind_text(stmt, 4, Narrow(message.conteudo).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, Narrow(message.media_url).c_str(), -1, SQLITE_TRANSIENT);
        if (message.reply_to_id.has_value()) {
            sqlite3_bind_int(stmt, 6, message.reply_to_id.value());
        } else {
            sqlite3_bind_null(stmt, 6);
        }
        if (message.subtopico_id.has_value()) {
            sqlite3_bind_int(stmt, 7, message.subtopico_id.value());
        } else {
            sqlite3_bind_null(stmt, 7);
        }
        sqlite3_bind_text(stmt, 8, Narrow(message.criado_em).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
}

std::vector<Mensagem> DatabaseCache::LoadMessages(int conversation_id) {
    std::vector<Mensagem> messages;
    if (db_ == nullptr) {
        return messages;
    }

    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT id, conversation_id, sender_id, content, media_url, reply_to_id, subtopico_id, created_at "
        "FROM cached_messages WHERE conversation_id = ? ORDER BY id ASC;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return messages;
    }

    sqlite3_bind_int(stmt, 1, conversation_id);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Mensagem message;
        message.id = sqlite3_column_int(stmt, 0);
        message.conversa_id = sqlite3_column_int(stmt, 1);
        message.remetente_id = sqlite3_column_int(stmt, 2);
        message.conteudo = Wide(sqlite3_column_text(stmt, 3));
        message.media_url = Wide(sqlite3_column_text(stmt, 4));
        if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
            message.reply_to_id = sqlite3_column_int(stmt, 5);
        }
        if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
            message.subtopico_id = sqlite3_column_int(stmt, 6);
        }
        message.criado_em = Wide(sqlite3_column_text(stmt, 7));
        messages.push_back(std::move(message));
    }

    sqlite3_finalize(stmt);
    return messages;
}

}  // namespace tocachat

# Prompt de continuação — TocaChat Desktop Win32

## Contexto geral

Estou desenvolvendo um cliente desktop nativo Windows para o **TocaChat**, um sistema de chat em tempo real. O backend já existe (Flask + SocketIO + SQLite, rodando em `https://tocachat.duckdns.org:8080`) e não deve ser modificado.

O cliente desktop está sendo construído em **C++17 / MSVC 2022 / Win32 puro**, sem frameworks externos pesados. Meta: executável ~5 MB, ~2 MB RAM idle.

---

## Workspace

```
C:\Projetos\Telezap\TELEZAP-2.0\desktop-win32\
```

---

## Stack técnica

- **UI**: Win32 API + Direct2D + DirectWrite (sem MFC, sem Qt, sem Electron)
- **Rede**: WinHTTP (REST + long-polling Socket.IO)
- **TLS**: Schannel nativo (via WinHTTP)
- **JSON**: `third_party/json/json.hpp` (nlohmann/json, header-only)
- **Cache local**: SQLite amalgamation (`third_party/sqlite/sqlite-amalgamation-3460000/`)
- **Autenticação**: sessão Flask persistida via DPAPI (`CryptProtectData`) em `%AppData%\TocaChatDesktop\session.dat`
- **Flags de compilação**: `/MT /O2 /GR- /EHs-c- /LTCG /W4 WIN32_LEAN_AND_MEAN`
- **C++17** (structured bindings, `std::optional`, `std::filesystem` opcional)

---

## Estrutura de arquivos

```
desktop-win32/
├── TocaChat.sln
├── TocaChat.vcxproj
├── docs/
│   ├── build-win32.md
│   └── codex-prompt.md         ← este arquivo
├── manifests/TocaChat.manifest
├── resources/resources.rc
└── src/
    ├── common.h          ← includes Win32/D2D/DWrite/WIC, utilitários inline (Utf8ToWide, ColorFromHex, etc.)
    ├── main.cpp          ← WinMain, CoInit, criação de MainWindow
    ├── models.h          ← structs: Usuario, Mensagem, Conversa, CallState, Subtopico
    ├── theme.h/.cpp      ← AppTheme::Default() — cores do tema escuro
    ├── layout_engine.h/.cpp  ← mini flex-engine (Row/Column, gap, grow, padding)
    ├── ui_components.h/.cpp  ← BuildChatLayout, BuildLoginLayout, HitTestConversation
    ├── app_state.h/.cpp  ← estado global imutável da UI (conversas, mensagens, screen, flags)
    ├── render.h/.cpp     ← Direct2D: DrawAuthScreen, DrawChatShell, DrawCard, DrawLabel, DrawAvatar
    ├── window.h/.cpp     ← WndProc, message loop, integração de todos os módulos
    ├── auth.h/.cpp       ← AuthController: login, registro, TryResume, logout
    ├── network.h/.cpp    ← NetworkClient: WinHTTP GET/POST JSON, cookie jar, DPAPI session persist
    ├── chat.h/.cpp       ← ChatController: parse, LoadConversations, LoadMessages, SendMessage, SyncSelectedConversation
    ├── socketio.h/.cpp   ← SocketIoClient: Engine.IO polling, emit, reconnect com backoff
    ├── database.h/.cpp   ← DatabaseCache: SQLite, cached_conversations, cached_messages
    ├── media.h/.cpp      ← STUB (futuro: WIC decode + upload)
    ├── webrtc.h/.cpp     ← STUB (futuro: libdatachannel)
    ├── call_ui.h/.cpp    ← STUB (futuro: UI de chamadas)
    └── notifications.h/.cpp  ← STUB (futuro: Windows Toast)
```

---

## Estado atual do código (o que já funciona)

### Autenticação (`auth.cpp`)
- Login/registro real via `/api/login` e `/api/registro`
- Retomada de sessão com `/api/me` ao iniciar
- Sessão persistida via DPAPI

### Chat (`chat.cpp`)
- Parse completo de `Conversa` (id, tipo, nome, foto, wallpaper, pinned_message_id, ultima_mensagem, membros)
- Parse completo de `Mensagem` (id, conversa_id, remetente_id, conteudo, media_url, reply_to_id, subtopico_id, reacoes)
- `LoadConversations()` → GET `/api/conversas`, fallback para SQLite
- `LoadSelectedConversationMessages()` → GET `/api/conversas/{id}/mensagens`, fallback para SQLite
- `SendCurrentMessage()` → POST `/api/conversas/{id}/mensagens`
- `SyncSelectedConversation()` → GET `/api/chat/sync?conversa_id=X&after_id=Y`, trata `new_messages`, `conversations` e `deleted_ids`

### Socket.IO (`socketio.cpp`)
- Engine.IO polling (EIO=4), handshake, namespace open `40`
- Ping/pong (`2` → `3`)
- Pacotes `0` (open), `1` (close), `2` (ping), `6` (noop), `40` (ns connect), `41` (ns disconnect), `42` (event)
- Backoff exponencial na reconexão: 2s → 4s → 8s → ... → 30s max
- Timeout de receive: **35 segundos** (corrigido de 8s que causava reconexão em loop)
- Sleep 150ms em poll vazio (anti-spin)
- `PollOnce()` retorna false quando `sid_` é limpo por close/41

### Eventos Socket.IO tratados em `window.cpp`
- `new_message` → `state_.AppendMessages()` + reload conversas
- `message_deleted` → `state_.RemoveMessage()`
- `message_reaction_updated` → `state_.UpdateMessageReactions()`
- `pinned_update` → força sync
- Status conectado/desconectado → `state_.SetSocketConnected()` + emit `join` + `join_conv`

### AppState (`app_state.cpp`)
- `screen_` (Auth / Chat / Call)
- `conversations_`, `messages_by_conversation_`, `selected_conversation_index_`
- `current_user_`, `composer_text_`, `busy_`, `socket_connected_`
- `AppendMessages()` deduplica por `id`
- `SetConversations()` preserva seleção atual por `id`
- `SeedShellData()` — dados mock para testes offline (não afeta fluxo real)

### Render (`render.cpp`)
- Tela de login (card centralizado, dois inputs, dois botões, hint)
- Chat shell: sidebar com lista de conversas + painel principal com header, lista de mensagens, composer
- Cada conversa: avatar (inicial colorida), nome, preview da última mensagem, timestamp
- Cada mensagem: balão colorido (própria/alheia), texto, timestamp no canto
- **Pílulas de reações**: desenhadas abaixo do balão, 46×22px cada, cor `#2d2d44`
- **Indicador de conexão**: ponto no header — verde (accent) = conectado, vermelho (`#ff6b6b`) = desconectado

### Layout (`ui_components.cpp`)
- `EstimateMessageHeight()`: base 40px + 20px por linha estimada (38 chars/linha)
- Mensagens com reações ganham +30px de espaço após o balão (para as pílulas)
- Sidebar: largura = `clamp(window_width * 0.26, 280, 340)`
- Bolhas: max-width 62% do painel

### Cache SQLite (`database.cpp`)
- `cached_conversations`: id, tipo, nome, foto_url, wallpaper_url, pinned_message_id, ultima_msg, ultima_msg_em
- `cached_messages`: id, conversation_id, sender_id, content, media_url, reply_to_id, subtopico_id, created_at
- WAL mode, índice composto em `(conversation_id, id)`

---

## Backend relevante (Flask, não modificar)

**URL base**: `https://tocachat.duckdns.org:8080`

### Rotas REST usadas
```
POST /api/login              body: {login, senha}
POST /api/registro           body: {username, senha}
POST /api/logout
GET  /api/me                 → {id, username, nome, bio, foto, wallpaper, autenticado}
GET  /api/conversas          → [{id, tipo, display_nome, display_foto, wallpaper, ultima_msg, ultima_msg_em, membros[], pinned_message_id}]
GET  /api/conversas/{id}     → conversa com membros[]
GET  /api/conversas/{id}/mensagens?after_id=X  → [mensagem]
POST /api/conversas/{id}/mensagens  body: {conteudo, media_url?, reply_to_id?, subtopico_id?}
GET  /api/chat/sync?conversa_id=X&after_id=Y   → {conversas[], mensagens[], deleted_ids[]}
POST /api/calls/signal       body: {conversa_id, destinatario_id, tipo, dados}
GET  /api/webrtc/config      → {rtc: {iceServers[], iceTransportPolicy}}
```

### Eventos Socket.IO emitidos pelo servidor
```
new_message            → objeto Mensagem completo (mesmo schema da rota REST)
message_deleted        → {id: int}
message_reaction_updated → {mensagem_id, conversa_id, reacoes: [{emoji, total, reagiu}], action, emoji}
pinned_update          → {conversa_id, subtopico_id?}
subtopic_reordered     → {conversa_id}
global_wallpaper_updated → {url}
call_signal            → {remetente_id, tipo, dados, conversa_id}
```

### Eventos que o cliente emite para o servidor
```
join       {user_id, conversa_id?}
join_conv  {conversa_id}
leave_conv {conversa_id}
```

---

## Modelos C++ relevantes

```cpp
// models.h
struct Usuario {
    int id = 0;
    std::wstring username, nome, bio, foto_url, wallpaper_url;
};

struct Mensagem {
    int id = 0, conversa_id = 0, remetente_id = 0;
    std::wstring conteudo, media_url, criado_em;
    std::optional<int> reply_to_id, subtopico_id;
    std::optional<std::wstring> excluido_em;
    std::map<std::wstring, std::vector<int>> reacoes;  // emoji → lista de user_ids (size = count)
};

struct Conversa {
    int id = 0;
    std::wstring tipo, nome, foto_url, wallpaper_url;
    std::optional<int> pinned_message_id;
    std::optional<Mensagem> ultima_mensagem;
    std::vector<Usuario> membros;
    std::vector<Subtopico> subtopicos;
};
```

---

## Principais limitações atuais (o que ainda NÃO funciona)

### Crítico / alta utilidade
1. **Sem scroll** — lista de mensagens e lista de conversas não scrollam. Só exibe o que cabe na tela. Para scroll, precisar implementar virtual scroll com `WM_MOUSEWHEEL` e offset de renderização.
2. **Sem badge de não-lidas** — quando `new_message` chega de outra conversa, o sidebar não mostra contador. Precisa adicionar `unordered_map<int, int> unread_counts_` em `AppState`, incrementar em `window.cpp` quando `msg.conversa_id != selected_conversation_id()`, zerar ao abrir conversa.
3. **Sem nome do autor nos grupos** — bolhas de grupo não mostram quem enviou. `Mensagem` tem `remetente_id` mas não `autor_nome`. Opções: (a) enriquecer o parse com o campo `autor_nome` que a API já retorna, (b) lookup em `conversa.membros`.
4. **Preview de reply não renderizado** — `reply_to_id` é parseado mas as bolhas não mostram o trecho de resposta acima do texto.

### Médio prazo
5. **Cursor do mouse** — clicar em botões não muda o cursor para `IDC_HAND`. Implementar com `WM_SETCURSOR`.
6. **Input de texto próprio** — digitação funciona mas não há cursor piscante nem seleção. Melhorar com timer de blink e caret.
7. **Scroll da sidebar** — quando há mais conversas do que cabem na tela, as extras somem.
8. **Notificações Windows Toast** — `notifications.h/.cpp` é stub vazio. Implementar com `ShellExecute` para toast simples ou COM `IUserNotification2`.
9. **System tray** — minimizar para bandeja ainda não existe.

### Longo prazo / risco alto
10. **Upload de mídia** — `media.h/.cpp` é stub. Implementar multipart/form-data manual com WinHTTP.
11. **WebRTC / chamadas** — `webrtc.h/.cpp` e `call_ui.h/.cpp` são stubs. Depende de libdatachannel (~1.5 MB).
12. **Upgrade WebSocket** — Socket.IO usa polling puro agora. `WinHttpWebSocketCompleteUpgrade` disponível no Win8+ poderia substituir o polling, reduzindo latência.

---

## Decisões de design já tomadas (não reverter)

- **Nomes de arquivos e classes**: não renomear
- **Comentários em português**: manter padrão nos comentários novos
- **Sem CRT pesada**: evitar `<iostream>`, `<regex>`, `<locale>` — custo de tamanho alto
- **Sem exceções**: `/EHs-c-` — não usar `try/catch` exceto onde já existe (parse de JSON)
- **Sem RTTI**: `/GR-` — sem `dynamic_cast`, sem `typeid`
- **Mudanças incrementais**: preferir patches pequenos e compiláveis a refatorações grandes
- **Backend intocável**: cliente consome a API, não muda o servidor

---

## Tarefa sugerida para começar

**Implementar badge de não-lidas na sidebar** (baixo risco, alta utilidade):

### Passo 1 — `app_state.h`
Adicionar ao bloco privado:
```cpp
std::unordered_map<int, int> unread_counts_;
```
Adicionar ao bloco público:
```cpp
void IncrementUnread(int conversation_id) noexcept;
void ClearUnread(int conversation_id) noexcept;
int unread_count(int conversation_id) const noexcept;
```

### Passo 2 — `app_state.cpp`
```cpp
void AppState::IncrementUnread(int conversation_id) noexcept {
    unread_counts_[conversation_id]++;
}
void AppState::ClearUnread(int conversation_id) noexcept {
    unread_counts_.erase(conversation_id);
}
int AppState::unread_count(int conversation_id) const noexcept {
    const auto it = unread_counts_.find(conversation_id);
    return it != unread_counts_.end() ? it->second : 0;
}
```
Também zerar no `ReturnToAuth()`:
```cpp
unread_counts_.clear();
```

### Passo 3 — `window.cpp`
No handler de `new_message` (dentro de `kSocketEventMessage`), após o `AppendMessages`:
```cpp
if (message.conversa_id != state_.selected_conversation_id()) {
    state_.IncrementUnread(message.conversa_id);
}
```
No handler de clique em conversa (`OnLeftButtonDown`), após `OpenConversation`:
```cpp
state_.ClearUnread(state_.selected_conversation_id());
```

### Passo 4 — `ui_components.h`
Adicionar campo em `ConversationRowLayout`:
```cpp
int unread_count = 0;
```

### Passo 5 — `ui_components.cpp`
Em `BuildChatLayout`, ao preencher `ConversationRowLayout`:
```cpp
row.unread_count = state.unread_count(conversations[index].id);
```

### Passo 6 — `render.cpp`
Em `DrawChatShell`, após desenhar o timestamp da conversa:
```cpp
if (row.unread_count > 0) {
    const std::wstring badge_text = row.unread_count < 100
        ? std::to_wstring(row.unread_count)
        : L"99+";
    const D2D1_RECT_F badge_rect = D2D1::RectF(
        row.rect.right - 36.0f, row.rect.top + 32.0f,
        row.rect.right - 12.0f, row.rect.top + 52.0f);
    DrawCard(badge_rect, theme_.accent, 10.0f);
    DrawLabel(badge_text, small_format_.Get(), badge_rect, theme_.text_primary,
              DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
}
```

---

## Segunda tarefa alternativa: scroll da lista de mensagens

Adicionar `scroll_offset_messages_` (float) em `AppState` e tratar `WM_MOUSEWHEEL` em `window.cpp` para ajustar o offset. Em `ui_components.cpp`, iniciar `bubble_top` com `layout.message_list.top + 8.0f - scroll_offset`. Clamp do offset entre 0 e `max(0, total_content_height - visible_height)`.

---

## Terceira tarefa alternativa: nome do autor nos grupos

O JSON da API já retorna `autor_nome` e `autor_username` nas mensagens. Adicionar em `models.h`:
```cpp
std::wstring autor_nome;
```
Em `chat.cpp` → `ParseMensagem()`, adicionar:
```cpp
message.autor_nome = JsonString(json_body, "autor_nome");
if (message.autor_nome.empty()) {
    message.autor_nome = JsonString(json_body, "autor_username");
}
```
Em `render.cpp` → `DrawChatShell`, antes do texto da mensagem, para mensagens alheias em grupos:
```cpp
if (!bubble.own_message && conversa->tipo == L"grupo" && !mensagem.autor_nome.empty()) {
    DrawLabel(mensagem.autor_nome, small_format_.Get(),
              D2D1::RectF(text_rect.left, text_rect.top, text_rect.right, text_rect.top + 18.0f),
              theme_.accent);
    // Ajustar text_rect para começar abaixo do nome
}
```

---

## Notas de compilação

- `msbuild TocaChat.sln /p:Configuration=Release /p:Platform=x64` no Developer Command Prompt do VS 2022
- Sem `msbuild` no PATH geral — usar "Developer Command Prompt for VS 2022"
- O projeto **não usa CMake**, só `.vcxproj`
- Includes de terceiros são relativos: `"json\\json.hpp"`, `"..\\third_party\\sqlite\\..."`
- `common.h` é o pré-cabeçalho de fato (incluído em todos os `.cpp`)

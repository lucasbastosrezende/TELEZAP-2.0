# 🦉 TocaChat (TELEZAP-2.0)

> Plataforma de comunicação em tempo real — chat privado e em grupo, chamadas de voz/vídeo P2P via WebRTC, upload de mídia, reações, stickers/GIFs, subtópicos de grupo, wallpapers personalizáveis e módulo acadêmico (agenda, sessões de estudo, simulados, dashboard).

Projeto full-stack auto-hospedado com **Flask + SocketIO** no backend e **SPA em Vanilla JS** no frontend, servido atrás de um **gateway Node.js/Express (HTTPS + WebSocket)** com certificado Let's Encrypt renovado via **DuckDNS**.

---

## 📑 Sumário

1. [Visão Geral](#-visão-geral)
2. [Arquitetura do Sistema](#-arquitetura-do-sistema)
3. [Stack Técnica](#-stack-técnica)
4. [Estrutura de Pastas](#-estrutura-de-pastas)
5. [Funcionalidades Completas](#-funcionalidades-completas)
6. [Banco de Dados](#-banco-de-dados)
7. [API REST (endpoints)](#-api-rest-endpoints)
8. [Eventos SocketIO](#-eventos-socketio)
9. [Camada de Chamadas (WebRTC)](#-camada-de-chamadas-webrtc)
10. [Segurança](#-segurança)
11. [Como Rodar](#-como-rodar)
12. [Pontos Importantes / Decisões de Design](#-pontos-importantes--decisões-de-design)
13. [🖥️ Versão Desktop em C++ Win32 (~5 MB)](#️-versão-desktop-em-c-win32-5-mb)

---

## 🔭 Visão Geral

| Item | Descrição |
|------|-----------|
| **Nome do produto** | TocaChat |
| **Domínio de produção** | `https://tocachat.duckdns.org:8080` |
| **Portas internas** | Flask `:3000` (HTTP) + Gateway `:8080` (HTTPS/WSS) |
| **Persistência** | SQLite local (`toca.db`) com WAL mode |
| **Autenticação** | Sessão Flask + bcrypt (senha ≥ 4 caracteres) |
| **Tempo real** | SocketIO (WebSockets) + polling `chat/sync` como fallback |
| **Mídia** | Upload até **500 MB** (vídeos HD), compressão client-side para WebP |
| **PWA** | `manifest.json` + `sw.js` (stale-while-revalidate) |
| **Clientes** | Web (SPA) + Android nativo (Kotlin/Jetpack Compose — em `/android-app`) |

---

## 🏗️ Arquitetura do Sistema

```
                        ┌───────────────────────────────┐
 Internet  ─── HTTPS ── │  Gateway Node.js (Express)    │
 (443/8080)             │  gateway.js — porta 8080      │
                        │  • TLS com Let's Encrypt       │
                        │  • Proxy HTTP + WebSocket      │
                        │  • Timeout 30 min              │
                        └──────────────┬────────────────┘
                                       │ proxy_pass localhost:3000
                                       ▼
                        ┌───────────────────────────────┐
                        │  Flask + SocketIO (server.py) │
                        │  • 60+ rotas REST             │
                        │  • Threading async mode       │
                        │  • ProxyFix (x-forwarded-*)   │
                        │  • bcrypt / sessions          │
                        └──────┬────────────────┬───────┘
                               │                │
                          ┌────▼────┐     ┌─────▼─────┐
                          │ SQLite  │     │ /static/  │
                          │ toca.db │     │ uploads/  │
                          │ (WAL)   │     │ stickers/ │
                          └─────────┘     └───────────┘

  Frontend (SPA):  templates/index.html + static/js/*.js + static/css/style.css
  Chamadas:        WebRTC P2P (mesh) — sinalização via SocketIO
                   ICE: STUN Google + TURN opcional (env TURN_URLS)
  DNS dinâmico:    duckdns_update.bat (atualiza IP a cada 5 min)
  Certificado TLS: get-cert.js (ACME dns-01 challenge via DuckDNS TXT)
```

---

## 🧰 Stack Técnica

### Backend
- **Python 3.11+**
- `flask==3.1.0` — framework web
- `flask-cors==5.0.1` — CORS restrito aos domínios do gateway
- `flask-socketio` (async_mode `threading`) — WebSockets
- `bcrypt==4.2.1` — hash de senhas
- `Pillow` — compressão de imagens (importada mas `compress_image` retorna o original — política de "máxima qualidade")
- `werkzeug.middleware.proxy_fix.ProxyFix` — confia nos cabeçalhos do gateway
- **SQLite** com `PRAGMA journal_mode=WAL` e `foreign_keys=ON`

### Gateway / TLS
- **Node.js**
- `express` — servidor HTTP
- `http-proxy` — proxy HTTP + upgrade WebSocket
- `acme-client` — emissão de certificado Let's Encrypt
- DNS: **DuckDNS** (domínio dinâmico gratuito) com desafio **dns-01**

### Frontend
- **Vanilla JS** (sem framework) — ~4.000 linhas em `chat.js` + `call.js`
- **CSS** — ~5.000 linhas em `style.css` (dark theme custom)
- **Socket.IO client** (via CDN carregada no `index.html`)
- **Tenor API** para GIFs (`TENOR_API_KEY=LIVDSRZULELA`)
- **Canvas API** para compressão WebP local antes do upload
- **Service Worker** para cache estático + SWR nas rotas de sync

---

## 📁 Estrutura de Pastas

```
TELEZAP-2.0/
├── server.py               # 1.822 linhas — servidor Flask + SocketIO (tudo em 1 arquivo)
├── database.py             # Schema SQLite + migrações seguras
├── gateway.js              # Proxy HTTPS + WSS (Node.js)
├── get-cert.js             # ACME dns-01 → Let's Encrypt
├── duckdns_update.bat      # Atualiza IP do domínio a cada 5 min
├── start.bat               # Sobe Flask + Gateway em 2 janelas
├── requirements.txt        # Dependências Python
├── package.json            # Dependências Node
├── sw.js                   # Service Worker (PWA)
├── init_wallpaper.py       # Script de seed de wallpapers
├── get_schema.py           # Dump do schema do SQLite
├── list_routes.py          # Lista todas as rotas Flask
├── test_clicks.js          # Script de teste de UI via Puppeteer
├── templates/
│   └── index.html          # 495 linhas — entrypoint SPA
├── static/
│   ├── css/style.css       # 4.911 linhas
│   ├── js/
│   │   ├── app.js          # SPA router + utilitários + compressImage
│   │   ├── auth.js         # Login/registro/perfil
│   │   ├── chat.js         # 2.408 linhas — TODA a lógica de chat
│   │   ├── call.js         # 1.198 linhas — WebRTC mesh + ring flow
│   │   ├── analytics.js    # Módulo analytics (acadêmico)
│   │   └── dashboard.js    # Dashboard (acadêmico)
│   ├── images/             # Wallpapers globais + logos
│   ├── stickers/           # default/ + customizados
│   └── uploads/            # Fotos de perfil, mídia de chat, wallpapers de grupo
├── tests/
│   └── test_chat_features.py
├── android-app/            # App Android Kotlin/Compose (mock repository)
└── toca.db                 # (gerado em runtime)
```

---

## ✨ Funcionalidades Completas

### 👤 Autenticação & Perfil
- Registro simples (apenas `username` + `senha`, email auto-gerado `{user}@local`)
- Login via sessão Flask (`app.secret_key = os.urandom(32)` — **rotaciona a cada restart**, invalida sessões)
- Senha com bcrypt + salt
- Perfil com nome, bio, foto, wallpaper pessoal (fallback caso conversa não tenha wallpaper próprio)

### 💬 Chat
- **Conversas diretas** (1 ↔ 1) e **grupos** (N membros)
- **Mensagens de texto** com auto-detecção de "emoji only" (1–3 emojis → renderização maior)
- **Mídia**: jpg/jpeg/png/webp/gif/mp4/webm/mov (500 MB max)
- **Reply** em mensagem (ponteiro `reply_to_id`)
- **Reações com emoji** (toggle por usuário, contador agregado)
- **Busca full-text** por conversa (`LIKE %termo%`)
- **Soft delete** (`excluido_em`) + **lixeira** (restauração por 24 h implícita)
- **Mensagem fixada** (pin) — escopo por conversa ou por subtópico
- **Subtópicos de grupo** (canais): nome, descrição, cor, ordem drag-and-drop, pin próprio
- **Sync incremental** via `/api/chat/sync?after_id=X` (polling + deltas)
- **Cache client-side** de mensagens com TTL 60s e máximo 20 conversas/50 msgs renderizadas (virtual scroll leve)

### 🎨 Personalização
- **Wallpaper global** (admin sobe, broadcast via socket `global_wallpaper_updated`)
- **Wallpaper por conversa** (grupo ou derivado do perfil do outro usuário)
- **Wallpaper por usuário** (fallback em DMs)
- Suporta **imagem E vídeo** (mp4/webm autoplay muted loop como wallpaper)
- **Stickers customizados** (upload PNG/WebP) + pasta `default/` do sistema
- **GIFs via Tenor API** (trending + busca em pt_BR)

### 📞 Chamadas (WebRTC Mesh)
- Voz + vídeo, **até N participantes** em topologia mesh (cada par = 1 RTCPeerConnection)
- Fluxo **Ring → Accept → Connect** (caller espera em lobby, auto-cancel em 30s)
- Toque (ringtone) sintetizado via Web Audio API
- Toggle câmera/microfone em tempo real
- **ICE config dinâmica** via `/api/webrtc/config` (STUN Google + TURN opcional via env `TURN_URLS`, `TURN_USERNAME`, `TURN_CREDENTIAL`)
- Suporta cenários **4G/CGNAT** (`iceTransportPolicy=relay` se configurado)
- Sinalização via **SocketIO** (`room=user_{id}`) — tipos: `ring`, `ring_accept`, `ring_decline`, `ring_cancel`, `join`, `leave`, `state`, `in_call`, `call_ended`

### 📚 Módulo Acadêmico (herdado do "TocaDoConhecimento")
- **Agenda** (eventos por mês/ano, matéria, horários, cor, concluído)
- **Sessões de estudo** (timer/pomodoro)
- **Simulados** (criar, responder, deletar)
- **Dashboard** com estatísticas agregadas
- **Analytics** de uso
- **Metas diárias** (horas de estudo, questões)

### 🌐 PWA
- `manifest.json` instalável
- Service Worker com **stale-while-revalidate** em `/api/conversas` e `/api/chat/sync`
- Ícone + theme-color `#0f0f1e`

---

## 🗄️ Banco de Dados

### Tabelas principais
- `usuarios` — id, username (UNIQUE), email, senha_hash, nome, bio, foto, **wallpaper**, timestamps
- `conversas` — id, tipo (`direto`/`grupo`), nome, descricao, foto, wallpaper, criado_por, **pinned_message_id**, timestamps
- `conversa_membros` — (conversa_id, usuario_id) UNIQUE, CASCADE
- `mensagens` — conteudo, subtopico_id, media_url, **reply_to_id**, criado_em, **excluido_em** (soft delete)
- `grupo_subtopicos` — nome, descricao, cor, ordem, pinned_message_id
- `mensagem_reacoes` — (mensagem_id, usuario_id, emoji) UNIQUE
- `sinais_call` — log histórico de sinalização (não usado em runtime; sinais são via socket)
- `mensagens_apagadas` — log para sync incremental após soft-delete
- `configuracoes_globais` — key-value (ex: `active_wallpaper`)
- `agenda_eventos`, `sessoes`, `simulados`, `metas_diarias` — módulo acadêmico

### Índices de performance
```sql
CREATE INDEX idx_mensagens_conversa_sub     ON mensagens(conversa_id, subtopico_id)
CREATE INDEX idx_mensagens_conversa_criado  ON mensagens(conversa_id, criado_em)
CREATE INDEX idx_mensagens_conversa_id      ON mensagens(conversa_id, id)  -- after_id queries
CREATE INDEX idx_conversa_membros_usuario   ON conversa_membros(usuario_id, conversa_id)
CREATE INDEX idx_usuarios_username          ON usuarios(username)
CREATE INDEX idx_reacoes_msg                ON mensagem_reacoes(mensagem_id)
CREATE INDEX idx_sinais_destinatario        ON sinais_call(destinatario_id)
```

### Migrações seguras
`database.py` usa padrão `ALTER TABLE ADD COLUMN` dentro de `try/except`, permitindo evolução sem perder dados. Colunas adicionadas via migração: `wallpaper`, `pinned_message_id`, `reply_to_id`, `excluido_em`, `subtopico_id`, `media_url`.

---

## 🌍 API REST (endpoints)

### Auth
| Método | Rota | Descrição |
|--------|------|-----------|
| POST | `/api/registro` | Cria conta + inicia sessão |
| POST | `/api/login` | Autentica (body: `login`, `senha`) |
| POST | `/api/logout` | Limpa sessão |
| GET | `/api/me` | Retorna usuário atual ou 401 |

### Perfil / Upload
| POST | `/api/perfil` (PUT) | Atualiza nome/bio |
| POST | `/api/upload-foto` | Avatar (256 px) ou wallpaper pessoal (1280 px) |
| GET | `/api/stickers` | Lista default + custom |
| POST | `/api/upload-sticker` | Novo sticker (512 px) |
| GET/POST | `/api/wallpaper-global` / `/api/upload-wallpaper-global` / `/api/set-wallpaper-global` | Wallpaper do sistema |
| GET | `/api/usuarios` | Lista todos (para iniciar DMs) |

### Conversas & Mensagens
| GET | `/api/conversas` | Lista conversas do user com `ultima_msg` |
| GET/PUT/DELETE | `/api/conversas/<id>` | Obter / editar / excluir (só criador) |
| POST | `/api/conversas/direto` | Cria/recupera DM |
| POST | `/api/conversas/grupo` | Cria grupo |
| POST | `/api/conversas/<id>/foto` + `/wallpaper` | Upload de grupo |
| POST | `/api/conversas/<id>/membros` | Adicionar membro |
| POST | `/api/conversas/<id>/sair` | Sair do grupo |
| GET/POST | `/api/conversas/<id>/mensagens` | Listar (paginação por `after_id`) / Enviar |
| GET | `/api/conversas/<id>/mensagens/busca?q=X` | Busca |
| POST | `/api/conversas/<id>/media` | Upload de mídia de chat |
| DELETE/POST | `/api/conversas/mensagens/<msg_id>` / `/restaurar` | Soft delete / restore |
| GET | `/api/conversas/<id>/lixeira` | Mensagens apagadas do user |
| POST | `/api/conversas/<id>/pin` | Fixar/desfixar (contexto subtopic opcional) |
| POST | `/api/mensagens/<id>/reacoes` | Toggle emoji |
| GET | `/api/chat/sync?after_id=X&conversa_id=Y&subtopico_id=Z` | Sync incremental completo |

### Subtópicos
| GET/POST | `/api/conversas/<id>/subtopicos` |
| PUT/DELETE | `/api/subtopicos/<id>` |
| PUT | `/api/conversas/<id>/subtopicos/reordenar` |

### Chamadas
| POST | `/api/calls/signal` | Envia sinal WebRTC via room `user_{dest}` |
| GET | `/api/webrtc/config` | ICE servers runtime |

### Módulo acadêmico
`/api/agenda`, `/api/sessoes`, `/api/simulados`, `/api/dashboard`, `/api/analytics`, `/api/metas` — CRUD completo.

---

## 📡 Eventos SocketIO

### Cliente → Servidor
- `join` `{user_id, conversa_id?}` → entra em room `user_{id}` (+ opcional `conv_{id}`)
- `join_conv` `{conversa_id}` → entra em `conv_{id}` (verifica membership)
- `leave_conv` `{conversa_id}`

### Servidor → Cliente
- `new_message` (broadcast em `conv_{id}`)
- `message_deleted` `{id}`
- `message_reaction_updated` `{mensagem_id, reacoes, action, emoji}`
- `pinned_update` `{conversa_id, subtopico_id?}`
- `subtopic_reordered` `{conversa_id}`
- `global_wallpaper_updated` `{url}` (broadcast global)
- `call_signal` (enviado em `user_{dest}`) — payload WebRTC

---

## 📞 Camada de Chamadas (WebRTC)

**Topologia:** Mesh — cada participante abre um `RTCPeerConnection` para cada outro. OK até ~6 pessoas; acima disso recomenda-se migrar para SFU (mediasoup, LiveKit, Janus).

**Fluxo de toque (caller → callee):**
1. Caller emite `ring` via `/api/calls/signal`
2. Callee recebe via socket, toca ringtone, mostra tela de aceitar/recusar
3. Callee responde `ring_accept` ou `ring_decline`
4. Se aceito, ambos trocam SDP (`offer`/`answer`) + ICE candidates
5. Após `iceconnectionstate=connected`, entra em estado `in_call`

**ICE/TURN:**
- Padrão: STUN Google (gratuito, funciona em ~85% dos NATs)
- Produção: configurar TURN próprio (coturn) via env vars para resolver CGNAT

---

## 🔒 Segurança

- **bcrypt** para senhas
- **Verificação de membership** em todas as operações de conversa (server-side)
- **CASCADE** no SQL para evitar registros órfãos
- **Soft-delete** com log para sync (permite auditar)
- **CORS restrito** aos domínios do gateway
- **ProxyFix** para respeitar `X-Forwarded-*` do gateway
- **MAX_CONTENT_LENGTH = 500 MB** (DoS por upload gigante mitigado)
- **HTTPS obrigatório** em produção (gateway não aceita HTTP puro)
- **Session cookie** (SameSite default)

### ⚠️ Pontos frágeis identificados
1. `secret_key = os.urandom(32)` rotaciona a cada restart → **todas as sessões caem**. Em produção: persistir em env var.
2. **Token DuckDNS hardcoded** em `get-cert.js` e `duckdns_update.bat` — **commitado no código**. Mover para env.
3. **Sem rate limiting** em `/api/login`, `/api/registro` — bruteforce fácil.
4. **Sem CSRF tokens** (mitigado por SameSite + JSON content-type, mas não 100%).
5. **Senha mínima 4 caracteres** — fraco.
6. `email` auto-gerado `{user}@local` — sem verificação real.
7. `compress_image` é no-op (retorna original) apesar de Pillow importado.

---

## 🚀 Como Rodar

### Pré-requisitos
- Python 3.11+
- Node.js 18+ (com `npm`)
- Domínio (DuckDNS gratuito) + porta 8080 aberta

### 1. Backend
```bash
pip install -r requirements.txt
python server.py
# → http://localhost:3000
```

### 2. Gateway (HTTPS)
```bash
npm install
# Gerar certificados (primeira vez):
node get-cert.js
# Rodar gateway:
node gateway.js
# → https://tocachat.duckdns.org:8080
```

### 3. DNS Dinâmico (Windows)
```bash
duckdns_update.bat   # mantém aberto, atualiza IP a cada 5 min
```

### 4. Atalho (Windows)
```bash
start.bat  # abre Flask + Gateway em janelas separadas
```

---

## 💡 Pontos Importantes / Decisões de Design

1. **Monólito Flask em 1 arquivo** (`server.py` — 1.822 linhas): simplicidade > modularização. Fácil de auditar, difícil de escalar além de ~100 usuários simultâneos.
2. **SQLite com WAL** permite leitores concorrentes; escrita é single-writer. Suficiente para grupos pequenos.
3. **Sem ORM** — SQL cru, queries otimizadas com JOINs e subqueries específicas (ver `/api/conversas` e `/api/chat/sync`).
4. **Polling + WebSocket híbrido**: mesmo com socket, o frontend faz `GET /api/chat/sync` periodicamente para resiliência. Isso é deliberado — sockets caem, HTTP é stateless.
5. **Compressão client-side** em `app.js#compressImage` (Canvas → WebP) antes de subir. Economiza banda em uploads de celular.
6. **Cache de mensagens client-side** com TTL, limite de conversas e virtual scroll básico — performance decente mesmo com 10k+ mensagens históricas.
7. **Timezone fixado em America/Manaus (UTC-4)** via `agora_manaus()`. Todas as datas gravadas naive + ISO.
8. **Fuso horário pode dar problema** para usuários fora do AM. Considerar migrar para UTC e formatar no cliente.
9. **Mesh WebRTC não escala** — ok para chamadas pequenas.
10. **Android app é um stub** — estrutura criada, repositório em memória, sem integração real com o backend ainda.

---

## 🖥️ Versão Desktop em C++ Win32 (~5 MB)

> Especificação alvo: **C++ Win32 puro / ~5 MB executável / ~2 MB RAM idle / performance Excelente / qualidade visual Alta**.

### 🎯 Por que C++ Win32?

| Critério | Electron | Tauri (Rust) | Qt | **C++ Win32** |
|----------|----------|--------------|-----|---------------|
| Tamanho | 150 MB | 10 MB | 30 MB | **~5 MB** ✅ |
| RAM idle | 200 MB | 50 MB | 80 MB | **~2 MB** ✅ |
| Startup | 2-3 s | 0.5 s | 1 s | **< 100 ms** ✅ |
| Dev speed | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐ |
| Portável | ✅ | ✅ | ✅ | ❌ só Windows |

O trade-off é claro: **mais código, menos dependências, app nativo minúsculo e extremamente rápido**.

### 📐 Arquitetura Proposta

```
┌─────────────────────────────────────────────────────────┐
│   TocaChat.exe (C++ / Win32 API pura / ~5 MB)           │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐   │
│  │   UI Layer   │  │  Net Layer   │  │ Media Layer  │   │
│  │  GDI+ / D2D  │  │  WinHTTP +   │  │  MediaFoun-  │   │
│  │  DirectWrite │  │  WinSock +   │  │  dation +    │   │
│  │  Custom ctrl │  │  Schannel    │  │  DShow/WASAPI│   │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘   │
│         │                 │                 │           │
│         └─────────────────┼─────────────────┘           │
│                           ▼                             │
│                    ┌─────────────┐                      │
│                    │ App State   │                      │
│                    │ (STL only)  │                      │
│                    └─────────────┘                      │
└─────────────────────────────────────────────────────────┘
       │              │              │              │
       ▼              ▼              ▼              ▼
   HTTPS API    WebSocket/WSS    WebRTC native    SQLite local
  (tocachat      (custom msg     (libwebrtc ou    (cache offline
  .duckdns.org)   framing)        Pion via FFI)    opcional)
```

### 🧱 Stack Técnica Sugerida

#### Camada UI — **~1 MB**
- **Win32 API pura** (`CreateWindowEx`, `WndProc`, subclassing)
- **Direct2D + DirectWrite** para rendering HiDPI, anti-aliasing, gradientes (mesmo visual do CSS atual)
- **GDI+** como fallback/secundário (para imagens GIF animadas)
- **WIC (Windows Imaging Component)** para decodificar PNG/JPEG/WebP/GIF nativamente
- **Custom layout engine** (emulando flexbox em ~500 linhas de C++)
- Animações via `QueryPerformanceCounter` + interpolação (easing)
- Ícones inline como **recursos .rc** (SVG → PNG no build)

#### Camada Rede — **~500 KB**
- **WinHTTP** para chamadas REST (HTTPS nativo, usa Schannel)
- **WebSocket**: implementação manual sobre WinHTTP (`WinHttpWebSocketCompleteUpgrade` disponível no Win8+)
- **JSON**: biblioteca single-header como **nlohmann/json** ou **simdjson** (o último para menor footprint)
- Cookies persistidos em `AppData\Roaming\TocaChat\session.dat`
- Retry/backoff exponencial, reconexão automática de socket

#### Camada WebRTC — **~2 MB (o maior peso)**
**Esta é a parte mais difícil.** Opções:

1. **libwebrtc (Google)** — oficial, completo, mas **GIGANTE** (~30 MB estático). ❌ fura o budget de 5 MB.
2. **MediaSoup client C++** — mais leve, mas focado em SFU.
3. **Pion (Go)** compilado como DLL via cgo — ~8 MB, ainda grande.
4. **Solução híbrida** ✅ recomendada: integrar apenas módulos mínimos do **libdatachannel** (~1,5 MB) que implementa WebRTC DataChannel + RTP puro em C++. Para mídia (voz/vídeo), usar:
   - **MediaFoundation** para captura de câmera/mic
   - **Opus** (codec de áudio, ~150 KB)
   - **OpenH264** via DLL Cisco (grátis, ~400 KB, livre de royalty)
   - Empacotar RTP manualmente (libdatachannel expõe track API)

#### Camada de persistência — **~200 KB**
- **SQLite amalgamation** (1 arquivo .c, ~800 KB compilado com -Os e LTO)
- Cache de mensagens offline, fila de envio quando offline
- Ou simplesmente arquivos binários simples se quiser economizar ainda mais

#### Build/Compilação
- **MSVC** (Visual Studio) com `/O2 /GL /Gy /Gw` + `/OPT:REF,ICF` no linker
- **LTCG** (Link-Time Code Generation)
- **Strip símbolos** de debug
- **Manifest embutido** para HiDPI + Common Controls v6
- **UPX** (opcional, comprime exe em mais ~40%)

### 📋 Bibliotecas Recomendadas

| Área | Biblioteca | Tamanho | Licença |
|------|-----------|---------|---------|
| JSON | nlohmann/json ou simdjson | header-only / ~100 KB | MIT |
| WebSocket | IXWebSocket ou uWebSockets | ~200 KB | BSD/Apache |
| WebRTC | libdatachannel | ~1,5 MB | MPL 2.0 |
| Áudio codec | Opus | ~150 KB | BSD |
| Vídeo codec | OpenH264 (DLL Cisco) | ~400 KB | BSD (Cisco paga royalty) |
| DB | SQLite amalgamation | ~800 KB | Public Domain |
| Imagens | WIC (nativo Win) | 0 | Windows |
| TLS | Schannel (nativo Win) | 0 | Windows |
| GIF animado | giflib | ~50 KB | MIT |

**Total estimado:** 3,2 MB de libs + 1,5 MB de código próprio = **~4,7 MB** ✅

### 🧭 Roadmap de Implementação (fases)

#### Fase 1 — Esqueleto (1-2 semanas)
- [ ] Janela principal com Direct2D
- [ ] Layout engine básico (containers vertical/horizontal, flex)
- [ ] Sistema de temas (clone das cores do CSS atual: `#0f0f1e` base, accent roxo)
- [ ] Loop de mensagens Win32 + renderização 60 fps

#### Fase 2 — Rede + Auth (1 semana)
- [ ] Cliente WinHTTP com sessão persistente
- [ ] Tela de login/registro (POST `/api/login`, `/api/registro`)
- [ ] Armazenamento seguro de sessão (DPAPI `CryptProtectData`)
- [ ] Chamada `/api/me` no boot

#### Fase 3 — Chat (2-3 semanas)
- [ ] Lista de conversas (sidebar) — renderização virtualizada
- [ ] Painel de mensagens (virtual scroll)
- [ ] Envio de texto (POST mensagens)
- [ ] WebSocket real-time (`new_message`, `message_deleted`, reações)
- [ ] Renderização de emojis (fonte "Segoe UI Emoji" nativa)
- [ ] Upload de arquivos (`multipart/form-data` manual com WinHTTP)
- [ ] Decoder de GIF animado (giflib + compositing frame-a-frame)
- [ ] Reply / pin / search / reactions

#### Fase 4 — Chamadas (3-4 semanas) — **fase mais longa**
- [ ] Captura de câmera (MediaFoundation)
- [ ] Captura de mic (WASAPI)
- [ ] Encoder H264 + Opus
- [ ] Integração libdatachannel (peer connection + signaling via socket existente)
- [ ] Renderização de vídeos remotos (Direct2D)
- [ ] UI de toque (ring in/out)
- [ ] Mixagem de áudio multi-peer

#### Fase 5 — Extras (1-2 semanas)
- [ ] System tray (minimizar)
- [ ] Notificações Windows Toast (`ShellAPI` + `WNS`)
- [ ] Autostart (registry `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`)
- [ ] Auto-update (download → hash check → replace exe → restart)
- [ ] Instalador (Inno Setup ou MSI) — exe único sem depender do Visual C++ Redist (compilar estático com `/MT`)

### 🧩 Desafios Principais

#### 1. **Layout responsivo sem framework**
HTML/CSS dão flexbox de graça. Em Win32 você escreve o algoritmo de layout manualmente. **Solução**: implementar um mini-engine com conceitos de `flex-direction`, `justify-content`, `align-items` em ~500-800 linhas.

#### 2. **WebRTC sem Chromium**
A implementação do WebRTC no Chrome/Firefox tem dezenas de MB. Reimplementar leve é o maior risco do projeto.
**Mitigação**: usar libdatachannel (suporta áudio/vídeo via track) + codecs separados. Fallback: usar **CEF lite** (3-4 MB stripped) **só** para a parte de mídia, escondido.

#### 3. **Reatividade de UI**
Sem React/Vue, re-render incremental exige cuidado. **Solução**: sistema de "dirty rects" — cada componente marca região suja, engine só repinta o que mudou (como faz o Chrome).

#### 4. **GIFs animados + vídeos inline**
Direct2D não decoda GIF animado nativamente. **Solução**: giflib + timer por frame. Para MP4/WebM inline, **Media Foundation** com `IMFMediaEngine`.

#### 5. **Texto bidirecional + emojis complexos**
DirectWrite resolve 95% dos casos. Para emojis com modificadores de pele (ZWJ sequences), exige shaping com **HarfBuzz** (+300 KB) ou confiar no `Segoe UI Emoji` que já vem no Win10+.

#### 6. **HiDPI**
Chamar `SetProcessDpiAwarenessContext(PER_MONITOR_AWARE_V2)` no boot. Todo o layout em DIPs, escalar para pixels físicos via `GetDpiForWindow`.

#### 7. **Tamanho binário**
Cada `#include <iostream>` custa 200 KB. **Regras**:
- `/MT` (estático) mas sem CRT pesada — usar CRT mínima ou `#define WIN32_LEAN_AND_MEAN`
- Evitar STL pesada (`<regex>`, `<iostream>`, `<locale>`)
- Preferir `std::span`, arrays fixos, `std::string` simples
- Remover exceções (`/EHs-c-`) e RTTI (`/GR-`) onde possível

#### 8. **Segurança de sessão**
Cookie Flask é assinado; o cliente nativo precisa armazenar e reenviar. Usar **DPAPI** para criptografar na máquina do usuário.

#### 9. **Certificado autoassinado / TLS**
O gateway usa Let's Encrypt válido → ok para Schannel. Mas se algum dia virar self-signed, precisa **whitelist manual** de cert pinning.

#### 10. **Compatibilidade backend**
Zero mudanças no backend são necessárias — o cliente nativo fala o mesmo REST+Socket.IO do navegador. **Socket.IO não é WebSocket puro!** Ele tem handshake próprio (polling inicial → upgrade). Duas opções:
   - Implementar o protocolo Socket.IO v4 em C++ (~600 linhas)
   - Adaptar o backend para aceitar WebSocket puro numa rota alternativa (mais limpo)

### 📊 Orçamento Final de Tamanho

```
Component                     Size (compressed)
────────────────────────────────────────────
Code próprio (UI + app logic) ~1.500 KB
libdatachannel                ~1.500 KB
SQLite amalgamation           ~  800 KB
OpenH264 DLL                  ~  400 KB
Opus                          ~  150 KB
simdjson/nlohmann             ~  100 KB
giflib                        ~   50 KB
Runtime C++ estático (/MT)    ~  400 KB
Recursos (ícones, fontes)     ~  100 KB
────────────────────────────────────────────
TOTAL                         ~5.000 KB  ✅
```

### 🧪 Testes & Qualidade

- **GoogleTest** para unit tests do layout engine, JSON, protocolo Socket.IO
- **DrMemory** ou **Application Verifier** para detectar leaks/use-after-free
- **Windows Performance Toolkit (WPT)** para medir startup, RAM, CPU
- Meta: **startup < 150 ms, RAM idle < 5 MB, RAM em chamada 2-way < 50 MB**

### 🔄 Trade-offs Finais

✅ **Ganhos**
- App nativo 30× menor que Electron
- Sem telas brancas, startup instantâneo
- Experiência "premium" (sensação de VS Code, Sublime Text)
- Bateria: consumo 10× menor que Electron
- Imune a problemas do V8 (JIT spikes, GC pauses)

❌ **Custos**
- 3-4 meses de desenvolvimento full-time
- Apenas Windows (para macOS/Linux seria preciso reescrever a camada UI)
- Debug mais difícil (sem DevTools)
- Cada mudança de UI exige recompilar
- WebRTC é a maior fonte de risco técnico

### 💡 Alternativa Pragmática

Se o budget de 5 MB não for absolutamente rígido, **Tauri 2.0** (Rust + WebView2) entrega:
- ~10-12 MB binário final
- ~40 MB RAM
- Reaproveita 100% do frontend atual (HTML/CSS/JS)
- Desenvolvimento em 2-3 semanas em vez de 3-4 meses
- Ainda é 15× mais leve que Electron

A escolha do C++ Win32 faz sentido se:
- O tamanho é **crítico** (distribuição via USB, áreas com internet ruim)
- Precisa rodar em máquinas muito antigas (Windows 7 SP1+)
- Quer uma vitrine técnica / portfólio impressionante
- O projeto tem tempo e equipe para absorver a complexidade

---

## 👤 Autor

Lucas Bastos Rezende — `lucasbastosrezende@gmail.com`

## 📜 Licença

Projeto privado/pessoal. Todos os direitos reservados.

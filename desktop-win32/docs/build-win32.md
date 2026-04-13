# Build da versao Win32

## Objetivo desta etapa

Este diretório contém o esqueleto inicial da versão desktop nativa do TocaChat para Windows 10+, usando:

- C++17
- Win32 API
- Direct2D
- DirectWrite
- WIC
- MSVC 2022

Nesta primeira entrega, a solução já organiza a arquitetura dos módulos e abre uma janela nativa com shell visual de login e chat.
Na etapa atual, o cliente também já possui:

- WinHTTP com requests JSON
- sessão persistida via DPAPI em `%AppData%\\TocaChatDesktop\\session.dat`
- login, registro, `/api/me`, `/api/conversas`, `/api/conversas/<id>/mensagens`
- sync incremental básico via `/api/chat/sync`
- cliente Socket.IO mínimo via polling para `new_message`
- cache local básico em SQLite (`%AppData%\\TocaChatDesktop\\cache.db`)

## Pré-requisitos

1. Visual Studio 2022 com workload `Desktop development with C++`
2. Windows 10 SDK 10.0.19041 ou superior
3. Git opcional para clonar/atualizar o projeto

## Como abrir

1. Abra `desktop-win32/TocaChat.sln`
2. Selecione a configuração `Debug|x64` ou `Release|x64`
3. Compile o projeto `TocaChat`

## Flags principais

Configuração `Release|x64`:

- `/O2`
- `/GL`
- `/Gy`
- `/Gw`
- `/MT`
- `/EHs-c-`
- `/GR-`
- `/LTCG`
- Linker: `/OPT:REF,ICF`

## Dependências externas planejadas

As pastas em `third_party/` estão reservadas para a próxima etapa. Neste momento, `nlohmann/json` e SQLite amalgamation já estão versionados no projeto.
Links recomendados:

- JSON: [nlohmann/json](https://github.com/nlohmann/json)
- SQLite amalgamation: [sqlite.org/download.html](https://www.sqlite.org/download.html)
- WebRTC leve: [libdatachannel](https://github.com/paullouisageneau/libdatachannel)
- Codec de áudio: [Opus](https://opus-codec.org/downloads/)
- Codec de vídeo: [OpenH264](https://github.com/cisco/openh264/releases)
- GIF animado: [giflib](https://sourceforge.net/projects/giflib/)

## Próximos passos

1. Evoluir `socketio.cpp` de polling para upgrade WebSocket/Engine.IO completo
2. Expandir eventos em tempo real para `message_deleted`, `message_reaction_updated`, `pinned_update` e sinalização de chamada
3. Expandir `database.cpp` para fila offline de envio e recuperação incremental mais rica
4. Integrar `media.cpp` para upload inline, preview e GIFs
5. Popular `third_party/` com giflib, Opus, libdatachannel e OpenH264

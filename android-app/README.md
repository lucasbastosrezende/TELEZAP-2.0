# Toca Chat Android (nativo)

Aplicativo Android nativo em **Kotlin + Jetpack Compose** para o Toca Chat.

## O que já está pronto

- Estrutura base Android (`app`).
- Navegação entre **Lista de conversas** e **Tela de chat**.
- Repositório local em memória (`ChatRepository`) para facilitar evolução.
- UI de chat preparada para trocar o datasource mock por API real.

---

## Tutorial: como abrir e rodar o projeto

## 1) Pré-requisitos

Antes de tudo, instale:

- **Android Studio** (Hedgehog ou mais novo).
- **Android SDK** com pelo menos:
  - Android SDK Platform 34
  - Android SDK Build-Tools
  - Android Emulator
- **JDK 17** (recomendado para este projeto).

> Dica: no Android Studio, use o JDK embutido ou configure explicitamente o JDK 17 em
> `Settings > Build, Execution, Deployment > Build Tools > Gradle`.

## 2) Abrir no Android Studio

1. Abra o Android Studio.
2. Clique em **Open**.
3. Selecione a pasta **`android-app`** (não a raiz inteira do backend).
4. Aguarde o **Gradle Sync** terminar.

Se o Android Studio pedir para instalar componentes do SDK, aceite.

## 3) Rodar no emulador

1. No Android Studio, abra **Device Manager**.
2. Crie um emulador (ex.: Pixel + Android 14/API 34).
3. Clique em **Run ▶** para executar o app.

## 4) Rodar em celular Android (USB)

1. Ative **Opções de desenvolvedor** no celular.
2. Ative **Depuração USB**.
3. Conecte via cabo USB.
4. Autorize a depuração quando o aparelho perguntar.
5. No Android Studio, selecione o device e clique em **Run ▶**.

## 5) Rodar via linha de comando (opcional)

Na pasta `android-app`:

```bash
./gradlew :app:assembleDebug
```

APK gerado em:

```text
app/build/outputs/apk/debug/app-debug.apk
```

## 6) Problemas comuns (e solução)

### Erro de JDK/Gradle

- Sintoma: erro durante sync/build relacionado à versão Java.
- Solução: garanta que o Gradle está usando **JDK 17**.

### Build muito lento na primeira vez

- Sintoma: primeira compilação demora bastante.
- Solução: é normal (download de dependências). As próximas builds ficam mais rápidas.

### Emulador não abre

- Verifique virtualização habilitada (BIOS/UEFI).
- No Windows, confirme aceleração (WHPX/HAXM, conforme setup).

---

## Estrutura rápida

- `app/src/main/java/com/tocachat/android/MainActivity.kt`: entrada do app + navegação.
- `app/src/main/java/com/tocachat/android/ui/`: telas Compose.
- `app/src/main/java/com/tocachat/android/data/`: modelos e repositório mock.

## Próximos passos para produção

1. Trocar `ChatRepository` por integração HTTP/WebSocket com backend do Toca Chat.
2. Adicionar autenticação (JWT/session).
3. Persistência offline com Room.
4. Push notifications com Firebase Cloud Messaging.
5. Testes instrumentados e CI para build de release.

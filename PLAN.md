# OpenSAMP — Open Source SA-MP Fake Client
> Открытый аналог RakSAMP Lite для автоматизации и тестирования SA-MP серверов

---

## Цель проекта

Создать минималистичный, чистый опенсорс fake client для SA-MP, управляемый через Lua API. Без вирусов, без закрытого кода — полностью прозрачный инструмент для разработчиков серверов и тестировщиков.

---

## Технологический стек

| Компонент | Язык | Назначение |
|---|---|---|
| `core/` | C++ | RakNet, SA-MP протокол, пакеты |
| `engine/` | Rust | Менеджер ботов, конфиги, логика |
| `scripting/` | Rust + C | Lua биндинги (mlua / LuaJIT) |
| `scripts/` | Lua | Примеры пользовательских скриптов |

---

## Структура репозитория

```
opensamp/
├── core/
│   ├── include/
│   │   ├── samp/
│   │   │   ├── packets.h        # SA-MP пакеты
│   │   │   ├── auth.h           # Авторизация
│   │   │   └── protocol.h      # Константы протокола
│   │   └── net/
│   │       └── client.h        # RakNet клиент
│   ├── src/
│   │   ├── samp/
│   │   │   ├── packets.cpp
│   │   │   ├── auth.cpp
│   │   │   └── handlers.cpp    # Обработчики входящих пакетов
│   │   └── net/
│   │       └── client.cpp
│   └── CMakeLists.txt
├── engine/
│   ├── src/
│   │   ├── main.rs
│   │   ├── bot.rs              # Структура бота
│   │   ├── manager.rs          # Менеджер нескольких ботов
│   │   └── config.rs           # Конфиги (TOML)
│   └── Cargo.toml
├── scripting/
│   ├── src/
│   │   ├── lua_api.rs          # Lua биндинги
│   │   └── events.rs           # Система событий
│   └── Cargo.toml
├── scripts/
│   ├── example_chat.lua
│   ├── example_connect.lua
│   └── example_dialog.lua
├── docs/
│   ├── protocol.md             # SA-MP протокол (референс)
│   └── lua_api.md              # Документация Lua API
├── PLAN.md
└── README.md
```

---

## Фазы разработки

### Фаза 1 — Core (C++): базовое подключение
**Цель:** подключиться к SA-MP серверу и авторизоваться с ником

- [ ] Интегрировать SLikeNet (форк RakNet)
- [ ] Реализовать RakNet handshake
- [ ] Реализовать SA-MP auth пакет (версия `0.3.7`, ник, пароль)
- [ ] Принять `ID_CONNECTION_REQUEST_ACCEPTED`
- [ ] Логировать входящие пакеты (hex dump для отладки)

**Результат:** бот подключается к серверу и не вылетает

---

### Фаза 2 — Core (C++): базовые пакеты
**Цель:** реализовать основные игровые пакеты

- [ ] `PACKET_PLAYER_SYNC` — синхронизация позиции игрока
- [ ] `PACKET_CHAT` — отправка и приём чата
- [ ] `PACKET_DIALOG_RESPONSE` — ответ на диалоги
- [ ] `PACKET_SPAWN` — спавн персонажа
- [ ] Keep-alive / ping пакеты (чтобы сервер не кикал)

**Результат:** бот держится на сервере, может писать в чат

---

### Фаза 3 — Engine (Rust): менеджер ботов
**Цель:** управлять несколькими ботами одновременно

- [ ] FFI bridge между C++ core и Rust engine
- [ ] Структура `Bot` (статус, соединение, состояние)
- [ ] `BotManager` — запуск/остановка нескольких ботов
- [ ] Конфиги через TOML (`config.toml`)
- [ ] Логирование через `tracing` крейт

**Результат:** можно запустить N ботов из одного конфига

---

### Фаза 4 — Scripting: Lua API
**Цель:** дать пользователям возможность писать скрипты

- [ ] Интегрировать `mlua` крейт
- [ ] Реализовать события:
  - `onConnect(botId)`
  - `onDisconnect(botId, reason)`
  - `onChat(botId, playerId, message)`
  - `onDialogShow(botId, dialogId, type, title, content)`
  - `onPlayerJoin(botId, playerId, name)`
  - `onPlayerLeave(botId, playerId)`
- [ ] Реализовать функции API:
  - `sendChat(botId, message)`
  - `sendDialogResponse(botId, dialogId, button, listItem, input)`
  - `getPosition(botId)` → `{x, y, z}`
  - `setPosition(botId, x, y, z)`
  - `getHealth(botId)`
  - `getBotNick(botId)`
- [ ] Загрузка скриптов из папки `scripts/`

**Результат:** пользователь пишет Lua скрипт и бот выполняет его

---

### Фаза 5 — Полировка
- [ ] README с инструкцией по сборке
- [ ] Документация Lua API (`docs/lua_api.md`)
- [ ] Примеры скриптов
- [ ] GitHub Actions (сборка на Windows/Linux)
- [ ] Лицензия (MIT или Apache 2.0)

---

## Ключевые зависимости

### C++
```cmake
# CMakeLists.txt
find_package(SLikeNet REQUIRED)  # или FetchContent
```
- **SLikeNet** — https://github.com/SLikeSoft/SLikeNet (форк RakNet, MIT лицензия)

### Rust
```toml
# Cargo.toml
[dependencies]
mlua = { version = "0.9", features = ["lua54", "vendored"] }
tokio = { version = "1", features = ["full"] }
serde = { version = "1", features = ["derive"] }
toml = "0.8"
tracing = "0.1"
tracing-subscriber = "0.3"
```

---

## SA-MP протокол — ключевые референсы

- open.mp исходник: https://github.com/openmultiplayer/open.mp
  - Смотреть: `Server/Components/*/` и `SDK/include/`
- SA-MP packet IDs: константы в `a_samp.inc` (Pawno includes)
- RakNet: `BitStream` для чтения/записи пакетов

### Auth пакет (Фаза 1 — самое важное)
```
Offset  Size  Field
0       1     Packet ID (0x06 — PACKET_AUTH)
1       4     SA-MP версия (0x03 = 0.3.7)
5       1+N   Ник (Pascal string: uint8 длина + байты)
6+N     1+M   Пароль (Pascal string: uint8 длина + байты, пустой если нет)
```

---

## Конфиг (config.toml)

```toml
[[bots]]
nick = "TestBot1"
server = "127.0.0.1"
port = 7777
password = ""
script = "scripts/example_connect.lua"

[[bots]]
nick = "TestBot2"
server = "127.0.0.1"
port = 7777
password = ""
script = "scripts/example_chat.lua"
```

---

## Пример Lua скрипта

```lua
-- scripts/example_chat.lua

function onConnect(botId)
    print("Бот подключился: " .. getBotNick(botId))
    sendChat(botId, "Привет, сервер!")
end

function onChat(botId, playerId, message)
    if message == "!ping" then
        sendChat(botId, "pong!")
    end
end

function onDialogShow(botId, dialogId, dType, title, content)
    -- Автоматически закрыть любой диалог
    sendDialogResponse(botId, dialogId, 1, 0, "")
end
```

---

## Порядок работы для AI агента

1. **Начни с Фазы 1** — только C++ core, только подключение
2. Используй SLikeNet как основу для RakNet
3. Реализуй handshake → auth → держать соединение
4. Логируй ВСЕ пакеты в hex на этапе разработки
5. Тестируй на локальном SA-MP / open.mp сервере
6. Только после стабильного подключения — переходи к Фазе 2
7. Rust engine пиши параллельно начиная с Фазы 3, после стабилизации core

---

## Лицензия

MIT — максимально открытая, без ограничений для комьюнити

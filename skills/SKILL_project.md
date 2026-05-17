---
name: opensamp-dev
description: >
  Skill for AI agents working on the OpenSAMP project — an open-source SA-MP fake client
  for server testing and automation. Use this skill whenever working on any part of the
  opensamp codebase: C++ core (RakNet/protocol), Rust engine (bot manager), Lua scripting
  API, build system, or documentation. Trigger on any task involving SA-MP packets,
  RakNet handshake, bot lifecycle, Lua bindings, or CMake/Cargo configuration for this project.
---

# OpenSAMP Dev Skill

Ты AI агент, который помогает разрабатывать **OpenSAMP** — открытый SA-MP fake client для
автоматизации и тестирования серверов. Проект написан на C++ (core/протокол) и Rust (engine/scripting).

Всегда читай `PLAN.md` в корне репозитория перед началом работы — там текущий статус фаз.

---

## Архитектура проекта

```
opensamp/
├── core/                  # C++ — RakNet + SA-MP протокол
│   ├── include/
│   │   ├── net/client.h   # Главный клиент
│   │   └── samp/
│   │       ├── protocol.h # Packet IDs, константы
│   │       ├── packets.h  # Структуры пакетов
│   │       └── auth.h     # Auth логика
│   ├── src/
│   │   ├── net/client.cpp
│   │   └── samp/
│   │       ├── packets.cpp
│   │       ├── auth.cpp     # Auth логика вынесена отдельно
│   │       └── handlers.cpp
│   └── CMakeLists.txt
├── engine/                # Rust — менеджер ботов
│   ├── src/
│   │   ├── main.rs
│   │   ├── bot.rs
│   │   ├── manager.rs
│   │   └── config.rs
│   └── Cargo.toml
├── scripting/             # Rust — Lua API
│   ├── src/
│   │   ├── lua_api.rs
│   │   └── events.rs
│   └── Cargo.toml
├── scripts/               # Пользовательские Lua скрипты
├── docs/
│   ├── protocol.md        # SA-MP протокол (референс)
│   └── lua_api.md         # Документация Lua API
├── README.md
└── PLAN.md
```

---

## Фазы разработки

Всегда проверяй PLAN.md — какие чекбоксы уже отмечены. Работай строго по фазам:

| Фаза | Что делаем | Критерий готовности |
|------|-----------|---------------------|
| 1 | C++ core: RakNet handshake + SA-MP auth | Бот подключается, не вылетает |
| 2 | C++ core: базовые пакеты | Бот держится, пишет в чат |
| 3 | Rust engine: менеджер ботов | N ботов из конфига |
| 4 | Scripting: Lua API | Скрипт управляет ботом |
| 5 | Полировка, документация | Готово к публикации |

---

## C++ Core — паттерны и правила

### Зависимости
- **SLikeNet** — форк RakNet, MIT лицензия
  - GitHub: https://github.com/SLikeSoft/SLikeNet
  - Подключай через CMake FetchContent
- **C++17** минимум

### CMake шаблон
```cmake
include(FetchContent)
FetchContent_Declare(
    slikenet
    GIT_REPOSITORY https://github.com/SLikeSoft/SLikeNet.git
    GIT_TAG master
)
FetchContent_MakeAvailable(slikenet)

target_link_libraries(opensamp_core PUBLIC SLikeNetLibStatic)
```

### RakNet паттерны

**Инициализация:**
```cpp
auto* peer = RakNet::RakPeerInterface::GetInstance();
RakNet::SocketDescriptor sd;
peer->Startup(1, &sd, 1);
peer->Connect(ip, port, nullptr, 0);
```

**Game loop (tick ~100ms):**
```cpp
RakNet::Packet* pkt;
while ((pkt = peer->Receive()) != nullptr) {
    unsigned char id = pkt->data[0];
    // обработка по id
    peer->DeallocatePacket(pkt);
}
```

**BitStream запись:**
```cpp
RakNet::BitStream bs;
bs.Write((unsigned char)PACKET_ID);
bs.Write(some_value);
// для строк:
unsigned char len = str.size();
bs.Write(len);
bs.Write(str.c_str(), len);
peer->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0,
           RakNet::UNASSIGNED_SYSTEM_ADDRESS, true);
```

**BitStream чтение:**
```cpp
RakNet::BitStream bs(pkt->data, pkt->length, false);
unsigned char id; bs.Read(id);
unsigned char len; bs.Read(len);
char buf[256]; bs.Read(buf, len); buf[len] = 0;
```

### SA-MP Protocol — важные packet IDs

> **Референс:** open.mp исходник — https://github.com/openmultiplayer/open.mp
> Смотреть: `Server/Components/*/` и `SDK/include/network.hpp`
> Также полезно: `a_samp.inc` из стандартных SA-MP includes

```cpp
// Системные (RakNet)
ID_CONNECTION_REQUEST_ACCEPTED = 14  // RakNet: соединение установлено
ID_DISCONNECTION_NOTIFICATION  = 19
ID_CONNECTION_LOST             = 21

// SA-MP пакеты (уточняй по open.mp)
PACKET_AUTH            = 0x06  // Авторизация клиента
PACKET_SPAWN           = 0x4A  // Игрок заспавнился  
PACKET_CHAT            = 0x65  // Сообщение в чат
PACKET_PLAYER_SYNC     = 0xA0  // Синхронизация позиции
PACKET_DIALOG_SHOW     = 0x58  // Сервер показывает диалог
PACKET_DIALOG_RESPONSE = 0x59  // Клиент отвечает на диалог
```

> ⚠️ **Важно:** Точные ID и структуры пакетов **обязательно** сверяй с open.mp исходником.
> SA-MP 0.3.7 протокол и open.mp полностью совместимы на уровне клиента.

### Auth пакет — структура

```
[0x06]          — Packet ID
[uint32]        — SA-MP версия (0x03 для 0.3.7)
[uint8 + bytes] — Ник (Pascal string: длина потом байты)
[uint8 + bytes] — Пароль (пустой = одни нули)
```

### Отладка пакетов

При разработке логируй ВСЁ что приходит:
```cpp
// В handlePacket, default ветка:
std::cout << "[DEBUG] packet 0x" << std::hex << (int)id
          << " len=" << std::dec << pkt->length << std::endl;
// Hex dump первых 32 байт:
for (int i = 0; i < std::min((int)pkt->length, 32); i++)
    std::cout << std::hex << std::setw(2) << std::setfill('0')
              << (int)pkt->data[i] << " ";
std::cout << std::endl;
```

### Keep-alive / синхронизация

Сервер кикает клиентов которые не шлют `PLAYER_SYNC`. Отправляй каждые 100-200мс:
```cpp
// Минимальный sync пакет — просто текущая позиция и состояние
// Структуру см. в open.mp PlayerSyncData
```

---

## Rust Engine — паттерны и правила

### Cargo.toml зависимости
```toml
[dependencies]
mlua = { version = "0.9", features = ["lua54", "vendored"] }
tokio = { version = "1", features = ["full"] }
serde = { version = "1", features = ["derive"] }
toml = "0.8"
tracing = "0.1"
tracing-subscriber = "0.3"
```

### FFI bridge (C++ ↔ Rust)

Экспортируй из C++ core через extern "C":
```cpp
// core/include/ffi.h
extern "C" {
    void* samp_client_create(const char* ip, uint16_t port,
                              const char* nick, const char* pass);
    void  samp_client_destroy(void* client);
    bool  samp_client_connect(void* client);
    void  samp_client_tick(void* client);
    void  samp_client_send_chat(void* client, const char* msg);
    // callbacks
    void  samp_client_set_on_connect(void* client, void(*cb)(void*), void* ud);
    void  samp_client_set_on_chat(void* client,
              void(*cb)(void*, int, const char*), void* ud);
}
```

Rust сторона:
```rust
// engine/src/ffi.rs
#[link(name = "opensamp_core")]
extern "C" {
    pub fn samp_client_create(ip: *const c_char, port: u16,
                               nick: *const c_char, pass: *const c_char) -> *mut c_void;
    pub fn samp_client_tick(client: *mut c_void);
    // ...
}
```

### Bot структура

```rust
// engine/src/bot.rs
pub struct Bot {
    pub id: usize,
    pub nick: String,
    pub config: BotConfig,
    client_ptr: *mut c_void,  // указатель на C++ SAMPClient
    pub state: BotState,
}

#[derive(Debug, Clone)]
pub enum BotState {
    Disconnected,
    Connecting,
    Connected,
    InGame,
}
```

### Конфиг (config.toml)

```toml
[[bots]]
nick = "TestBot1"
server = "127.0.0.1"
port = 7777
password = ""
script = "scripts/example.lua"
```

Rust парсинг:
```rust
#[derive(Deserialize)]
pub struct Config {
    pub bots: Vec<BotConfig>,
}

#[derive(Deserialize, Clone)]
pub struct BotConfig {
    pub nick: String,
    pub server: String,
    pub port: u16,
    pub password: String,
    pub script: String,
}
```

---

## Scripting — Lua API

### Список событий (для агента: реализуй все)

| Событие | Сигнатура Lua | Когда вызывается |
|---------|---------------|-----------------|
| `onConnect` | `(botId)` | Успешный спавн в игре |
| `onDisconnect` | `(botId, reason)` | Разрыв соединения |
| `onChat` | `(botId, playerId, message)` | Входящее сообщение |
| `onDialogShow` | `(botId, dialogId, type, title, content)` | Сервер показал диалог |
| `onPlayerJoin` | `(botId, playerId, name)` | Игрок зашёл на сервер |
| `onPlayerLeave` | `(botId, playerId)` | Игрок вышел |

### Список функций API (для агента: реализуй все)

| Функция | Возвращает | Описание |
|---------|-----------|----------|
| `sendChat(botId, msg)` | — | Написать в чат |
| `sendDialogResponse(botId, dialogId, button, listItem, input)` | — | Ответить на диалог |
| `getPosition(botId)` | `x, y, z` | Текущая позиция |
| `setPosition(botId, x, y, z)` | — | Установить позицию |
| `getBotNick(botId)` | `string` | Ник бота |
| `getHealth(botId)` | `float` | Здоровье |

### mlua паттерн

```rust
// scripting/src/lua_api.rs
use mlua::prelude::*;

pub fn setup_api(lua: &Lua, bot_id: usize) -> LuaResult<()> {
    let globals = lua.globals();

    // Пример: sendChat
    let send_chat = lua.create_function(move |_, (bid, msg): (usize, String)| {
        // вызов FFI
        unsafe { samp_client_send_chat(get_client(bid), msg.as_ptr() as _); }
        Ok(())
    })?;
    globals.set("sendChat", send_chat)?;

    Ok(())
}

// Вызов события из Rust:
pub fn fire_event(lua: &Lua, event: &str, args: impl IntoLuaMulti) {
    if let Ok(f) = lua.globals().get::<LuaFunction>(event) {
        let _ = f.call::<()>(args);
    }
}
```

---

## Типичные паттерны и ошибки

### ✅ Правильно — обработка disconnect

Всегда обрабатывай оба случая:
```cpp
case ID_DISCONNECTION_NOTIFICATION:  // сервер закрыл соединение
case ID_CONNECTION_LOST:             // потеря связи
    connected_ = false;
    if (on_disconnect_) on_disconnect_("connection_lost");
    break;
```

### ✅ Правильно — reconnect логика (Rust)

```rust
loop {
    bot.connect();
    while bot.is_connected() {
        bot.tick();
        tokio::time::sleep(Duration::from_millis(100)).await;
    }
    // переподключение через 5 секунд
    tokio::time::sleep(Duration::from_secs(5)).await;
}
```

### ❌ Ошибка — BitStream без сброса

```cpp
// НЕПРАВИЛЬНО: повторное использование BitStream без сброса
bs.Write(something);
peer->Send(&bs, ...);
bs.Write(more);  // данные накапливаются!

// ПРАВИЛЬНО: новый BitStream на каждый пакет
{
    RakNet::BitStream bs;
    bs.Write(something);
    peer->Send(&bs, ...);
}
```

### ❌ Ошибка — блокирующий tick в Rust

```rust
// НЕПРАВИЛЬНО: блокирует tokio runtime
loop { bot.tick(); }

// ПРАВИЛЬНО: через tokio
loop {
    bot.tick();
    tokio::time::sleep(Duration::from_millis(50)).await;
}
```

### ❌ Ошибка — забыть DeallocatePacket

```cpp
// Всегда освобождай пакет после обработки!
peer->DeallocatePacket(pkt);
```

---

## Тестирование

### Локальный SA-MP сервер для тестов

Поднять open.mp локально:
```bash
# Docker (самый простой способ)
docker run -p 7777:7777/udp openmultiplayer/omp-server
```

Или скачать бинарник: https://github.com/openmultiplayer/open.mp/releases

### Чеклист перед переходом к следующей фазе

**Фаза 1 готова если:**
- [ ] `samp_client_connect()` возвращает true
- [ ] Получен `ID_CONNECTION_REQUEST_ACCEPTED`
- [ ] Auth пакет отправлен
- [ ] Нет краша в течение 30 секунд

**Фаза 2 готова если:**
- [ ] `sendChat()` виден на сервере
- [ ] `onChat` коллбэк срабатывает
- [ ] Бот не вылетает через keep-alive таймаут (обычно 10-15 сек)

---

## Ссылки

- open.mp исходник: https://github.com/openmultiplayer/open.mp
- SLikeNet: https://github.com/SLikeSoft/SLikeNet
- mlua docs: https://docs.rs/mlua
- SA-MP includes (a_samp.inc): https://github.com/pawn-lang/samp-stdlib

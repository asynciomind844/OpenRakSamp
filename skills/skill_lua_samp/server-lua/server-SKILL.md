---
name: samp-lua/server
description: >
  Серверный SA:MP Lua скриптинг — геймоды, filterscript-ы, плагины на Lua.
  Читай этот файл когда пользователь пишет серверный скрипт для SA:MP или open.mp
  на Lua, упоминает lua-plugin, open.mp Lua, gamemode, filterscript, серверные
  колбэки (OnPlayerConnect, OnGameModeInit), серверные команды, работу с БД на сервере.
---

# Серверный SA:MP Lua Скриптинг

## ⚠️ Важно: спроси пользователя о фреймворке

Серверный Lua в SA:MP экосистеме сильно зависит от плагина/фреймворка. Перед написанием кода **обязательно уточни**:

```
Какой плагин/фреймворк используется для Lua на сервере?
Примеры:
  - lua-plugin (старый SA:MP)
  - open.mp с нативной Lua поддержкой
  - pawn-runner + Lua биндинги
  - другой (попроси описать API и доступные функции)

Также полезно знать:
  - Версия сервера (SA:MP 0.3.7 / open.mp)
  - Доступные плагины (MySQL, streamer, sscanf и т.д.)
  - Есть ли уже существующий код для контекста?
```

Код ниже даёт **универсальные паттерны** — конкретные имена функций могут отличаться.

---

## Структура серверного скрипта

### Типичная точка входа

```lua
-- gamemode.lua или filterscript.lua

-- Глобальное состояние
local Players  = {}   -- данные игроков
local Vehicles = {}   -- данные транспорта
local timers   = {}   -- активные таймеры

-- Инициализация (аналог OnGameModeInit / OnFilterScriptInit)
function OnInit()
    print("[MyGamemode] Инициализация...")
    
    initDatabase()
    createSpawnPoints()
    createVehicles()
    startGlobalTimers()
    
    print("[MyGamemode] Готов!")
end

-- Завершение работы
function OnExit()
    stopAllTimers()
    saveAllPlayerData()
    closeDatabase()
    print("[MyGamemode] Завершён")
end
```

### Структура данных игрока

```lua
-- Шаблон записи игрока
local function createPlayerData(playerId)
    return {
        id       = playerId,
        name     = getPlayerName(playerId),
        ip       = getPlayerIp(playerId),
        -- Статистика
        money    = 0,
        score    = 0,
        level    = 1,
        -- Состояние
        spawned  = false,
        alive    = false,
        team     = 0,
        skin     = 0,
        -- Временные данные (не сохраняются в БД)
        lastDmgTime   = 0,
        lastCheckpoint= nil,
        tempFlags     = {},
    }
end

-- Регистрация игрока при подключении
function OnPlayerConnect(playerId)
    Players[playerId] = createPlayerData(playerId)
    
    local name = Players[playerId].name
    print(string.format("[Connect] %s (id=%d)", name, playerId))
    
    -- Асинхронная загрузка из БД
    dbLoadPlayer(playerId, function(data)
        if data then
            -- Обновить данные из БД
            for k, v in pairs(data) do
                Players[playerId][k] = v
            end
        end
        showLoginDialog(playerId)
    end)
end

-- Очистка при отключении
function OnPlayerDisconnect(playerId, reason)
    if Players[playerId] then
        dbSavePlayer(playerId, Players[playerId])
        Players[playerId] = nil
    end
end
```

---

## Колбэки и события

### Основные колбэки

```lua
-- Спавн игрока
function OnPlayerSpawn(playerId)
    local p = Players[playerId]
    if not p then return end
    
    p.alive   = true
    p.spawned = true
    
    -- Восстановить деньги и скин
    givePlayerMoney(playerId, p.money)
    setPlayerSkin(playerId, p.skin)
    
    sendPlayerMessage(playerId, "Добро пожаловать, " .. p.name .. "!")
end

-- Смерть игрока
function OnPlayerDeath(playerId, killerId, weapon)
    local p = Players[playerId]
    if not p then return end
    
    p.alive = false
    
    if killerId ~= INVALID_PLAYER_ID then
        local killer = Players[killerId]
        if killer then
            killer.score = killer.score + 1
            setPlayerScore(killerId, killer.score)
        end
        
        local weaponName = getWeaponName(weapon)
        broadcastMessage(
            string.format("[Kill] %s убил %s (%s)", 
                killer and killer.name or "?",
                p.name,
                weaponName
            ),
            0xFF0000FF
        )
    end
end

-- Текстовое сообщение от игрока
function OnPlayerText(playerId, text)
    local p = Players[playerId]
    if not p then return false end
    
    -- Проверка мута
    if p.tempFlags.muted then
        sendPlayerMessage(playerId, "Вы замьючены")
        return false  -- не показывать сообщение
    end
    
    -- Антиспам
    local now = os.time()
    if p.lastMessageTime and (now - p.lastMessageTime) < 1 then
        sendPlayerMessage(playerId, "Не спамьте!")
        return false
    end
    p.lastMessageTime = now
    
    -- Форматирование чата
    local formatted = string.format(
        "[%d] %s: %s",
        p.level, p.name, text
    )
    broadcastMessage(formatted, 0xFFFFFFFF)
    
    return false  -- отменить стандартное сообщение, показали своё
end

-- Игрок написал команду
function OnPlayerCommandText(playerId, cmdtext)
    -- Парсинг команды
    local cmd, args = parseCommand(cmdtext)
    
    -- Роутинг на обработчик
    local handler = commandHandlers[cmd]
    if handler then
        handler(playerId, args)
        return true
    end
    
    sendPlayerMessage(playerId, "Неизвестная команда: " .. cmd)
    return false
end
```

### Система команд

```lua
local commandHandlers = {}

-- Регистрация команды
local function addCommand(name, handler, adminLevel)
    adminLevel = adminLevel or 0
    commandHandlers[name] = function(playerId, args)
        local p = Players[playerId]
        if not p then return end
        
        -- Проверка прав
        if p.adminLevel < adminLevel then
            sendPlayerMessage(playerId, "У вас нет прав")
            return
        end
        
        handler(playerId, args, p)
    end
end

-- Парсер команды "/cmd arg1 arg2"
local function parseCommand(text)
    local parts = {}
    for part in text:gmatch("%S+") do
        parts[#parts + 1] = part
    end
    local cmd = (parts[1] or ""):lower():sub(2)  -- убрать /
    local args = {}
    for i = 2, #parts do args[#args+1] = parts[i] end
    return cmd, args
end

-- Примеры команд
addCommand("heal", function(playerId, args, p)
    setPlayerHealth(playerId, 100)
    sendPlayerMessage(playerId, "Здоровье восстановлено")
end)

addCommand("tp", function(playerId, args, p)
    local targetName = args[1]
    if not targetName then
        sendPlayerMessage(playerId, "Использование: /tp <ник>")
        return
    end
    
    local targetId = findPlayerByName(targetName)
    if not targetId then
        sendPlayerMessage(playerId, "Игрок не найден: " .. targetName)
        return
    end
    
    local x, y, z = getPlayerPos(targetId)
    setPlayerPos(playerId, x, y + 1, z)
    sendPlayerMessage(playerId, "Телепортирован к " .. Players[targetId].name)
end, 1)  -- требует adminLevel >= 1

addCommand("kick", function(playerId, args, p)
    local targetId = tonumber(args[1])
    local reason   = table.concat(args, " ", 2) or "Причина не указана"
    
    if not targetId or not Players[targetId] then
        sendPlayerMessage(playerId, "Неверный ID игрока")
        return
    end
    
    broadcastMessage(
        string.format("[Kick] %s кикнут администратором %s (%s)",
            Players[targetId].name, p.name, reason),
        0xFF8C00FF
    )
    kickPlayer(targetId)
end, 2)
```

---

## Таймеры

```lua
-- Простой повторяющийся таймер
local function createTimer(intervalMs, callback)
    local t = {
        interval = intervalMs,
        callback = callback,
        lastRun  = 0,
        active   = true,
    }
    timers[#timers + 1] = t
    return t
end

local function stopTimer(t)
    t.active = false
end

-- Запуск таймеров (вызвать из основного луп или OnTick)
local function tickTimers()
    local now = getTickCount()  -- мс с запуска сервера
    for i = #timers, 1, -1 do
        local t = timers[i]
        if not t.active then
            table.remove(timers, i)
        elseif now - t.lastRun >= t.interval then
            t.lastRun = now
            local ok, err = pcall(t.callback)
            if not ok then
                print("[Timer Error] " .. tostring(err))
            end
        end
    end
end

-- Примеры таймеров
createTimer(60000, function()  -- каждую минуту
    for id, p in pairs(Players) do
        if p.alive then
            p.money = p.money + 100
            givePlayerMoney(id, 100)
        end
    end
end)

createTimer(300000, function()  -- каждые 5 минут
    autoSaveAllPlayers()
end)
```

---

## База данных

### Абстракция (адаптируй под свой плагин)

```lua
-- Паттерн работы с MySQL (через mysql-plugin, gmysql и т.д.)
local DB = {}

function DB.connect(host, user, pass, dbname)
    -- конкретная функция зависит от плагина
    -- например: mysql_connect(host, user, pass, dbname)
    DB.handle = mysql_connect(host, user, pass, dbname)
    if not DB.handle then
        error("Не удалось подключиться к БД")
    end
    print("[DB] Подключено к " .. dbname)
end

function DB.query(sql, ...)
    -- Экранирование параметров
    local params = {...}
    local escaped = {}
    for _, v in ipairs(params) do
        escaped[#escaped+1] = mysql_escape_string(tostring(v))
    end
    
    -- Подстановка параметров (простая реализация)
    local idx = 0
    local finalSQL = sql:gsub("%?", function()
        idx = idx + 1
        return "'" .. (escaped[idx] or "") .. "'"
    end)
    
    return mysql_query(DB.handle, finalSQL)
end

function DB.fetchRow(result)
    return mysql_fetch_row(result)
end

function DB.freeResult(result)
    mysql_free_result(result)
end

-- CRUD паттерны
function dbLoadPlayer(playerId, callback)
    local name = getPlayerName(playerId)
    local result = DB.query("SELECT * FROM players WHERE name = ?", name)
    local row    = DB.fetchRow(result)
    DB.freeResult(result)
    
    if callback then callback(row) end
    return row
end

function dbSavePlayer(playerId, data)
    local p = data
    DB.query(
        "INSERT INTO players (name, money, score, level) VALUES (?, ?, ?, ?) " ..
        "ON DUPLICATE KEY UPDATE money=VALUES(money), score=VALUES(score), level=VALUES(level)",
        p.name, p.money, p.score, p.level
    )
end

-- Создание таблиц при старте
function initDatabase()
    DB.connect("localhost", "root", "password", "samp_db")
    
    DB.query([[
        CREATE TABLE IF NOT EXISTS players (
            id      INT AUTO_INCREMENT PRIMARY KEY,
            name    VARCHAR(24) UNIQUE NOT NULL,
            money   INT DEFAULT 0,
            score   INT DEFAULT 0,
            level   INT DEFAULT 1,
            kills   INT DEFAULT 0,
            deaths  INT DEFAULT 0,
            created TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            lastseen TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
        )
    ]])
    
    print("[DB] Таблицы проверены/созданы")
end
```

### SQLite (без внешнего сервера)

```lua
-- Паттерн для SQLite (часто встроен или через sqlite3 плагин)
local sqlite = require "sqlite3"  -- зависит от плагина

local db = sqlite.open("server.db")

-- Инициализация схемы
db:exec([[
    CREATE TABLE IF NOT EXISTS players (
        name TEXT PRIMARY KEY,
        money INTEGER DEFAULT 0,
        score INTEGER DEFAULT 0
    )
]])

-- Параметризованные запросы (важно для защиты от SQLi)
local stmt = db:prepare("SELECT * FROM players WHERE name = ?")
stmt:bind(1, playerName)
local row = stmt:first_row()
stmt:finalize()

-- Транзакции для пакетного сохранения
function bulkSave(playerList)
    db:exec("BEGIN TRANSACTION")
    local ok, err = pcall(function()
        local stmt = db:prepare(
            "INSERT OR REPLACE INTO players (name, money, score) VALUES (?, ?, ?)"
        )
        for _, p in ipairs(playerList) do
            stmt:bind(1, p.name)
            stmt:bind(2, p.money)
            stmt:bind(3, p.score)
            stmt:step()
            stmt:reset()
        end
        stmt:finalize()
    end)
    
    if ok then
        db:exec("COMMIT")
    else
        db:exec("ROLLBACK")
        print("[DB] Ошибка транзакции: " .. tostring(err))
    end
end
```

---

## Работа с транспортом и объектами

```lua
-- Создание транспорта
local function spawnVehicle(modelId, x, y, z, angle, color1, color2)
    local vehId = createVehicle(modelId, x, y, z, angle, color1, color2, -1)
    Vehicles[vehId] = {
        id      = vehId,
        model   = modelId,
        owner   = nil,
        fuel    = 100.0,
        locked  = false,
        spawnX  = x, spawnY = y, spawnZ = z,
    }
    return vehId
end

-- Система топлива
createTimer(10000, function()
    for vehId, veh in pairs(Vehicles) do
        if isVehicleOccupied(vehId) and veh.fuel > 0 then
            veh.fuel = math.max(0, veh.fuel - 1)
            if veh.fuel == 0 then
                setVehicleEngineStatus(vehId, false)
                local driverId = getVehicleDriver(vehId)
                if driverId then
                    sendPlayerMessage(driverId, "Кончилось топливо!")
                end
            end
        end
    end
end)

-- Респавн уничтоженных машин
function OnVehicleDeath(vehId, killerId)
    local veh = Vehicles[vehId]
    if not veh then return end
    
    -- Отложенный респавн через 30 секунд
    local spawnX, spawnY, spawnZ = veh.spawnX, veh.spawnY, veh.spawnZ
    createDelayedAction(30000, function()
        respawnVehicle(vehId)
    end)
end
```

---

## Вспомогательные утилиты

### Работа с расстояниями и позициями

```lua
local function dist2D(x1, y1, x2, y2)
    return math.sqrt((x2-x1)^2 + (y2-y1)^2)
end

local function dist3D(x1, y1, z1, x2, y2, z2)
    return math.sqrt((x2-x1)^2 + (y2-y1)^2 + (z2-z1)^2)
end

local function isPlayerNearPoint(playerId, x, y, z, radius)
    local px, py, pz = getPlayerPos(playerId)
    return dist3D(px, py, pz, x, y, z) <= radius
end

-- Найти ближайшего игрока к точке
local function getNearestPlayer(x, y, z, exceptId)
    local nearest, nearestDist = nil, math.huge
    for id, p in pairs(Players) do
        if id ~= exceptId and p.alive then
            local px, py, pz = getPlayerPos(id)
            local d = dist3D(px, py, pz, x, y, z)
            if d < nearestDist then
                nearest, nearestDist = id, d
            end
        end
    end
    return nearest, nearestDist
end
```

### Форматирование сообщений

```lua
-- Отправить сообщение одному игроку (с цветом)
local function sendMsg(playerId, msg, color)
    sendClientMessage(playerId, color or 0xFFFFFFFF, msg)
end

-- Отправить всем
local function broadcast(msg, color)
    sendClientMessageToAll(color or 0xFFFFFFFF, msg)
end

-- Отправить в радиусе
local function broadcastNear(x, y, z, radius, msg, color)
    for id, p in pairs(Players) do
        if isPlayerNearPoint(id, x, y, z, radius) then
            sendMsg(id, msg, color)
        end
    end
end

-- Форматирование денег
local function formatMoney(amount)
    local s = tostring(math.abs(math.floor(amount)))
    local result = ""
    local len = #s
    for i = 1, len do
        result = result .. s:sub(i, i)
        if (len - i) % 3 == 0 and i ~= len then
            result = result .. ","
        end
    end
    return (amount < 0 and "-$" or "$") .. result
end
-- formatMoney(1500000) → "$1,500,000"
```

### Поиск игрока по имени

```lua
-- Поиск по точному совпадению или подстроке
local function findPlayerByName(query)
    query = query:lower()
    local exactMatch, partialMatches = nil, {}
    
    for id, p in pairs(Players) do
        local name = p.name:lower()
        if name == query then
            exactMatch = id
            break
        elseif name:find(query, 1, true) then
            partialMatches[#partialMatches + 1] = id
        end
    end
    
    if exactMatch then return exactMatch end
    if #partialMatches == 1 then return partialMatches[1] end
    if #partialMatches > 1 then
        -- вернуть nil и список совпадений
        return nil, partialMatches
    end
    return nil
end
```

---

## Защита от читов и санитизация

```lua
-- Антидрайв (запрет взрыва при наезде)
function OnVehicleDamageStatusUpdate(vehId, playerId)
    -- проверить что урон адекватный
end

-- Проверка скорости (упрощённая)
createTimer(1000, function()
    for id, p in pairs(Players) do
        if p.alive and not isPlayerInVehicle(id) then
            local x, y, z = getPlayerPos(id)
            if p.lastPos then
                local d = dist3D(x, y, z, table.unpack(p.lastPos))
                if d > 50 then  -- телепорт / speedhack
                    print(string.format("[AntiCheat] %s переместился на %.1f единиц за 1 сек", p.name, d))
                    -- teleport back, log, etc.
                end
            end
            p.lastPos = {x, y, z}
        end
    end
end)

-- Санитизация входных данных
local function sanitizeString(s, maxLen)
    if type(s) ~= "string" then return "" end
    s = s:match("^%s*(.-)%s*$")      -- trim
    s = s:gsub("[%z%c]", "")          -- убрать управляющие символы
    return s:sub(1, maxLen or 128)
end
```

---

## Отладка серверных скриптов

```lua
-- Логгер с уровнями
local Logger = {}
Logger.level = "INFO"  -- DEBUG / INFO / WARN / ERROR
local levels = {DEBUG=1, INFO=2, WARN=3, ERROR=4}

function Logger.log(level, fmt, ...)
    if levels[level] >= levels[Logger.level] then
        local msg = string.format(fmt, ...)
        local timestamp = os.date("%H:%M:%S")
        print(string.format("[%s][%s] %s", timestamp, level, msg))
    end
end

Logger.debug = function(...) Logger.log("DEBUG", ...) end
Logger.info  = function(...) Logger.log("INFO",  ...) end
Logger.warn  = function(...) Logger.log("WARN",  ...) end
Logger.error = function(...) Logger.log("ERROR", ...) end

-- Использование
Logger.info("Игрок %s подключился (id=%d)", name, id)
Logger.warn("Подозрительная скорость игрока %s: %.1f", name, speed)
Logger.error("Ошибка БД при сохранении %s: %s", name, err)

-- Дамп состояния (для дебага)
local function dumpServerState()
    Logger.debug("=== Server State ===")
    Logger.debug("Игроков онлайн: %d", tableLen(Players))
    Logger.debug("Транспорта: %d", tableLen(Vehicles))
    Logger.debug("Активных таймеров: %d", #timers)
    Logger.debug("===================")
end
```

---

## Типичные ошибки серверного скриптинга

### 1. Блокировка основного потока
```lua
-- ПЛОХО: синхронный запрос к БД блокирует весь сервер
function OnPlayerConnect(id)
    local result = mysql_query("SELECT * FROM players WHERE name='" .. name .. "'")
    -- сервер завис пока ждёт ответа БД!
end

-- ХОРОШО: использовать асинхронные запросы (если плагин поддерживает)
-- или кешировать часто запрашиваемые данные
```

### 2. SQL-инъекции
```lua
-- ПЛОХО
local sql = "SELECT * FROM players WHERE name='" .. playerInput .. "'"

-- ХОРОШО: всегда экранировать пользовательский ввод
local safe = mysql_escape_string(playerInput)
local sql  = "SELECT * FROM players WHERE name='" .. safe .. "'"
-- или параметризованные запросы (prepare/bind)
```

### 3. Гонка состояний при асинхронных операциях
```lua
-- ПЛОХО: игрок может отключиться пока грузятся данные из БД
function OnPlayerConnect(id)
    dbLoadAsync(id, function(data)
        -- игрок уже ушёл! Players[id] == nil
        Players[id].money = data.money  -- краш!
    end)
end

-- ХОРОШО: проверять что игрок ещё онлайн
function OnPlayerConnect(id)
    dbLoadAsync(id, function(data)
        if not Players[id] then return end  -- уже ушёл
        Players[id].money = data.money
    end)
end
```

### 4. Утечка данных игрока
```lua
-- ПЛОХО: данные не удаляются при дисконнекте
function OnPlayerConnect(id)
    Players[id] = createPlayerData(id)
end
-- Забыли OnPlayerDisconnect!

-- ХОРОШО: всегда чистить в OnPlayerDisconnect
function OnPlayerDisconnect(id, reason)
    if Players[id] then
        dbSavePlayer(id, Players[id])
        Players[id] = nil
    end
end
```

### 5. Неверная итерация при удалении
```lua
-- ПЛОХО
for id, p in pairs(Players) do
    if p.toKick then
        Players[id] = nil  -- изменение во время итерации
        kickPlayer(id)
    end
end

-- ХОРОШО
local toKick = {}
for id, p in pairs(Players) do
    if p.toKick then toKick[#toKick+1] = id end
end
for _, id in ipairs(toKick) do
    kickPlayer(id)
end
```

### 6. Деление на ноль / нет проверки типов
```lua
-- ПЛОХО
local avgScore = totalScore / playerCount  -- если playerCount == 0?

-- ХОРОШО
local avgScore = playerCount > 0 and (totalScore / playerCount) or 0
```

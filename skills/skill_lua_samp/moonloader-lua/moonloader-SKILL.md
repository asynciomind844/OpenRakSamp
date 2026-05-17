---
name: samp-lua/moonloader
description: >
  Клиентский SA:MP скриптинг через MoonLoader. Читай этот файл когда пользователь
  пишет или отлаживает .lua скрипт для папки moonloader/, упоминает mimgui, samp.events,
  memory, ffi, sampfuncs, renderlib, lua-скрипт для GTA SA клиента с SA:MP.
---

# MoonLoader — Клиентский SA:MP Lua Скриптинг

## Структура скрипта

```lua
-- Обязательные метаданные
script.Name    = "MyScript"
script.Author  = "Author"
script.Version = "1.0.0"
script.Description = "Описание скрипта"

-- Импорты — всегда в начале файла
require "lib.moonloader"        -- базовые хелперы
local samp    = require "samp.events"   -- SA:MP события
local mimgui  = require "mimgui"        -- ImGui
local ffi     = require "ffi"           -- C interop
local memory  = require "memory"        -- работа с памятью
local encoding= require "encoding"      -- кодировки (cp1251/utf8)
local inicfg  = require "inicfg"        -- .ini конфиги
local lfs     = require "lfs"           -- файловая система

encoding.default = "CP1251"             -- для кириллицы

-- Точка входа — основная корутина
function main()
    -- Ждём загрузки игры и SAMP
    if not isSampLoaded() then return end
    repeat wait(100) until isSampAvailable()

    -- Инициализация
    loadConfig()
    printToChat("{00FF00}[MyScript] {FFFFFF}Загружен!")

    -- Главный цикл
    while true do
        wait(0)  -- 0 = следующий фрейм, не блокирует игру
        onTick()
    end
end

-- Вызывается при выгрузке скрипта (reload, выход из игры)
function onScriptTerminate(script, quitGame)
    if script == thisScript() then
        saveConfig()
        printToChat("{FF0000}[MyScript] {FFFFFF}Выгружен")
    end
end
```

---

## Главный луп, корутины и таймеры

### wait() — основа тайминга

```lua
-- wait(ms) приостанавливает текущую корутину на N миллисекунд
-- wait(0) = пропустить один фрейм (≈ 1000/FPS мс)

function main()
    repeat wait(100) until isSampAvailable()

    while true do
        wait(0)         -- каждый фрейм
        -- логика здесь
    end
end
```

### Дочерние корутины

```lua
function main()
    repeat wait(100) until isSampAvailable()

    -- Запускаем параллельные задачи
    lua_thread.create(autoHealThread)
    lua_thread.create(espThread)
    lua_thread.create(chatMonitorThread)

    while true do
        wait(0)
        drawMainHUD()
    end
end

function autoHealThread()
    while true do
        wait(500)  -- каждые 500мс
        local hp = getCharHealth(PLAYER_PED)
        if hp < 300000 and not isKeyDown(VK_SHIFT) then
            setCharHealth(PLAYER_PED, 569000)  -- полное HP
        end
    end
end

function espThread()
    while true do
        wait(0)
        renderESP()
    end
end
```

### Таймеры через coroutine

```lua
-- Одноразовый таймер
lua_thread.create(function()
    wait(3000)
    printToChat("3 секунды прошло!")
end)

-- Повторяющийся таймер с счётчиком
lua_thread.create(function()
    local ticks = 0
    while true do
        wait(1000)
        ticks = ticks + 1
        if ticks >= 10 then break end
        printToChat("Тик " .. ticks)
    end
end)
```

---

## Библиотека samp.events — перехват событий

```lua
local samp = require "samp.events"

-- Входящее сообщение в чат
function samp.onServerMessage(color, message)
    -- можно вернуть false чтобы скрыть сообщение от игрока
    if message:find("BANNED") then
        return false  -- скрыть
    end
    -- изменить сообщение
    return {color, "[" .. os.date("%H:%M") .. "] " .. message}
end

-- Игрок написал команду /cmd
function samp.onSendChat(message)
    if message == "/mycommand" then
        doMyThing()
        return false  -- не отправлять на сервер
    end
end

-- Игрок нажал клавишу (клиентская команда)
addCommandHandler("myscript", function(args)
    local subcmd = args[1]
    if subcmd == "on" then enableScript() end
    if subcmd == "off" then disableScript() end
    if subcmd == "help" then showHelp() end
end)

-- Смерть персонажа
function samp.onPlayerDeath(playerId, killerId, reason)
    local nickname = sampGetPlayerNickname(playerId)
    printToChat(string.format("[Смерть] %s убит", nickname))
end

-- Пакет стримина игрока
function samp.onPlayerStreamIn(playerId, teamId)
    -- игрок появился в зоне видимости
end

-- Диалог с сервера
function samp.onShowDialog(id, style, title, button1, button2, text)
    if title:find("Регистрация") then
        -- автоматически заполнить
        sampSendDialogResponse(id, 1, -1, "MyPassword123")
        return false  -- скрыть диалог от игрока
    end
end

-- Серверный RPC
function samp.onSendGiveDamage(playerId, damage, weapon, bodypart)
    -- игрок нанёс урон
end
```

---

## SAMP API — игроки, транспорт, мир

### Локальный игрок

```lua
-- Основное
local myId = select(2, sampGetPlayerIdByCharHandle(PLAYER_PED))
local myNick = sampGetPlayerNickname(myId)
local x, y, z = getCharCoordinates(PLAYER_PED)
local hp = getCharHealth(PLAYER_PED)          -- 0–569000 внутри
local armour = getCharArmour(PLAYER_PED)       -- 0–100

-- Конвертация HP в проценты
local function getHPPercent()
    local hp = getCharHealth(PLAYER_PED)
    return math.max(0, math.floor((hp - 0) / 569000 * 100))
end

-- Состояние
local isInCar = isCharInAnyCar(PLAYER_PED)
local isOnFoot = not isInCar
local speed = getCarSpeed(storeCarCharIsInNoSave(PLAYER_PED))  -- если в машине
```

### Стримд игроки

```lua
-- Итерация по видимым игрокам
for i = 0, 1023 do
    if sampIsPlayerConnected(i) and sampIsPlayerStreamedIn(i) then
        local result, ped = sampGetCharHandleBySampPlayerId(i)
        if result then
            local name   = sampGetPlayerNickname(i)
            local hp     = sampGetPlayerHealth(i)
            local armour = sampGetPlayerArmour(i)
            local teamId = sampGetPlayerTeam(i)
            local score  = sampGetPlayerScore(i)
            local ping   = sampGetPlayerPing(i)
            local color  = sampGetPlayerColor(i)  -- RGBA

            local px, py, pz = getCharCoordinates(ped)
            local dist = getDistanceBetweenCoords3d(x, y, z, px, py, pz)
        end
    end
end
```

### Транспорт

```lua
-- Ближайший автомобиль
local vehicle = findNearestCar(x, y, z, 10.0)  -- в радиусе 10 единиц

-- Информация о транспорте в котором сидит игрок
if isCharInAnyCar(PLAYER_PED) then
    local car = storeCarCharIsInNoSave(PLAYER_PED)
    local vx, vy, vz = getCarCoordinates(car)
    local speed = getCarSpeed(car)            -- м/с умножить на 3.6 = км/ч
    local model = getCarModel(car)
    local hp    = getCarHealth(car)           -- 0–1000
end

-- Угон машины
taskWarpCharIntoCarAsDriver(PLAYER_PED, targetCar)
```

### Мировые объекты

```lua
-- Маркеры, пикапы
local pickup = createPickup(1242, 2, x, y, z, -1)
removePickup(pickup)

-- Текстлейблы
local label = sampAddClientLabel(x, y, z, "{FFFFFF}Метка", 0xFFFFFFFF, 30.0, false)
sampDeleteClientLabel(label)
```

---

## FFI — Работа с памятью игры

```lua
local ffi = require "ffi"
local memory = require "memory"

-- Читать/писать значения напрямую в память GTA SA
-- Адреса специфичны для версии SA (обычно 1.0 US)

-- Пример: отключить туман
local FOG_ADDR = 0xC81320
memory.setfloat(FOG_ADDR, 0.0)

-- Читать координаты из памяти
local CAM_POS_X = 0xB6F5F0
local camX = memory.getfloat(CAM_POS_X)

-- Вызвать игровую функцию через FFI
ffi.cdef[[
    typedef void (__thiscall *SetCharHealth_t)(void* ped, int hp);
]]
-- (реальные вызовы требуют точных адресов и соглашений о вызовах)

-- Безопасное чтение (с проверкой)
local function safeReadFloat(addr)
    local ok, val = pcall(memory.getfloat, addr)
    return ok and val or 0.0
end

-- Получить указатель на структуру игрока
local function getLocalPlayerPtr()
    local pedPoolPtr = memory.getint(0xB74490)
    if pedPoolPtr == 0 then return nil end
    return pedPoolPtr
end
```

### Структуры через ffi.cdef

```lua
ffi.cdef[[
    typedef struct {
        float x, y, z;
    } Vector3;

    typedef struct {
        Vector3 position;
        float   rotation;
        int     health;
        // ... другие поля
    } CEntity;
]]

local entity = ffi.cast("CEntity*", entityAddress)
local px = entity.position.x
```

---

## mimgui — ImGui Меню

```lua
local mimgui = require "mimgui"
local new, str, int = mimgui.new, ffi.string, ffi.new

-- Состояние окна
local isOpen    = new.bool(false)
local myFloat   = new.float(1.0)
local myInt     = new.int(0)
local myBool    = new.bool(false)
local myText    = new.char[256]()

-- Горячая клавиша для открытия/закрытия
local KEY_INSERT = 0x2D
addEventHandler("onWindowMessage", function(msg, wparam, lparam)
    if msg == 0x100 and wparam == KEY_INSERT then
        isOpen[0] = not isOpen[0]
    end
end)

-- Рендер ImGui (вызывается каждый фрейм автоматически)
mimgui.OnFrame(function()
    -- Только если окно открыто
    if not isOpen[0] then return end

    -- Стиль
    mimgui.PushStyleColor(mimgui.ImGuiCol.WindowBg, mimgui.ImVec4(0.1, 0.1, 0.1, 0.95))

    if mimgui.Begin("## MyScript", isOpen, mimgui.ImGuiWindowFlags.NoResize
        + mimgui.ImGuiWindowFlags.NoScrollbar) then

        mimgui.SetWindowSize(mimgui.ImVec2(300, 400), mimgui.ImGuiCond.Once)

        -- Заголовок с цветом
        mimgui.TextColored(mimgui.ImVec4(0, 1, 0.5, 1), "MyScript v1.0")
        mimgui.Separator()

        -- Табы
        if mimgui.BeginTabBar("Tabs") then
            if mimgui.BeginTabItem("Основное") then
                drawMainTab()
                mimgui.EndTabItem()
            end
            if mimgui.BeginTabItem("Настройки") then
                drawSettingsTab()
                mimgui.EndTabItem()
            end
            mimgui.EndTabBar()
        end
    end
    mimgui.End()
    mimgui.PopStyleColor()
end)

function drawMainTab()
    mimgui.Checkbox("Авто-HP", myBool)
    if myBool[0] then
        mimgui.SliderInt("Порог HP", myInt, 0, 100)
    end

    mimgui.Spacing()
    mimgui.Text("Скорость:")
    mimgui.SameLine()

    if mimgui.Button("Применить") then
        applySettings()
    end

    -- Текстовый ввод
    if mimgui.InputText("Ник", myText, 256) then
        local entered = str(myText)
        -- обработать введённый текст
    end

    -- Выпадающий список
    local items = {"Вариант 1", "Вариант 2", "Вариант 3"}
    local currentItem = new.int(0)
    if mimgui.BeginCombo("Выбор", items[currentItem[0] + 1]) then
        for i, item in ipairs(items) do
            local isSelected = (currentItem[0] == i - 1)
            if mimgui.Selectable(item, isSelected) then
                currentItem[0] = i - 1
            end
        end
        mimgui.EndCombo()
    end
end

function drawSettingsTab()
    mimgui.SliderFloat("Прозрачность", myFloat, 0.0, 1.0)
    mimgui.ColorEdit4("Цвет ESP", myFloat)  -- пример

    mimgui.Separator()
    if mimgui.Button("Сохранить") then saveConfig() end
    mimgui.SameLine()
    if mimgui.Button("Сбросить") then resetConfig() end
end
```

---

## Рендеринг — HUD, ESP, текст на экране

### renderlib / directx хелперы

```lua
-- MoonLoader предоставляет функции рисования через renderlib или прямые DX9 хелперы
-- Основные функции рендеринга вызываются в onD3DPresent или через mimgui окна

-- Получить экранные координаты из мировых
local function worldToScreen(wx, wy, wz)
    local sx, sy = convert3DCoordsToScreen(wx, wy, wz)
    return sx, sy
end

-- Проверка видимости позиции
local function isOnScreen(sx, sy)
    local resX, resY = getScreenResolution()
    return sx > 0 and sy > 0 and sx < resX and sy < resY
end
```

### ESP — отображение игроков

```lua
-- Конфиг ESP
local espConfig = {
    enabled   = false,
    showBox   = true,
    showName  = true,
    showHP    = true,
    showDist  = true,
    maxDist   = 150.0,
    colorEnemy= 0xFF0000FF,  -- RGBA красный
    colorTeam = 0xFF00FF00,  -- RGBA зелёный
}

local function renderESP()
    if not espConfig.enabled then return end
    if not isSampAvailable() then return end

    local myId = select(2, sampGetPlayerIdByCharHandle(PLAYER_PED))
    local myTeam = sampGetPlayerTeam(myId)
    local mx, my, mz = getCharCoordinates(PLAYER_PED)

    for i = 0, 1023 do
        if sampIsPlayerConnected(i) and sampIsPlayerStreamedIn(i) and i ~= myId then
            local result, ped = sampGetCharHandleBySampPlayerId(i)
            if result then
                local px, py, pz = getCharCoordinates(ped)
                local dist = getDistanceBetweenCoords3d(mx, my, mz, px, py, pz)

                if dist <= espConfig.maxDist then
                    local sx, sy = convert3DCoordsToScreen(px, py, pz + 1.0)
                    if isOnScreen(sx, sy) then
                        local name   = sampGetPlayerNickname(i)
                        local hp     = sampGetPlayerHealth(i)
                        local color  = (sampGetPlayerTeam(i) == myTeam)
                                       and espConfig.colorTeam
                                       or  espConfig.colorEnemy

                        -- Имя
                        if espConfig.showName then
                            renderFontDrawText(
                                fontHandle,
                                string.format("%s", name),
                                sx, sy - 20,
                                color
                            )
                        end

                        -- Дистанция
                        if espConfig.showDist then
                            renderFontDrawText(
                                fontHandle,
                                string.format("[%.0fm]", dist),
                                sx, sy - 8,
                                0xFFFFFFFF
                            )
                        end

                        -- HP бар
                        if espConfig.showHP then
                            local barW  = 40
                            local barH  = 4
                            local hpPct = math.max(0, math.min(hp, 100)) / 100
                            -- фон
                            renderDrawBox(sx - barW/2, sy + 2, barW, barH, 0xFF000000)
                            -- заполнение
                            local barColor = hp > 50 and 0xFF00FF00 or 0xFFFF0000
                            renderDrawBox(sx - barW/2, sy + 2, barW * hpPct, barH, barColor)
                        end
                    end
                end
            end
        end
    end
end

-- Инициализация шрифта для ESP
local fontHandle
lua_thread.create(function()
    repeat wait(100) until isSampAvailable()
    fontHandle = renderCreateFont("Arial", 8, 1)  -- имя, размер, флаги
end)

-- Регистрация рендера
addEventHandler("onD3DPresent", function()
    if espConfig.enabled then
        renderESP()
    end
end)
```

---

## Автоматизация — паттерны

### Триггер по условию с кулдауном

```lua
local lastTriggerTime = 0
local COOLDOWN_MS = 2000

local function tryAutoAction()
    local now = os.clock() * 1000
    if now - lastTriggerTime < COOLDOWN_MS then return end

    local hp = getCharHealth(PLAYER_PED)
    if hp < 150000 then  -- меньше ~26%
        -- выполнить действие
        sendChatMessage("/useitem 1")
        lastTriggerTime = now
    end
end
```

### Автокликер с нерегулярным интервалом (анти-детект)

```lua
local autoclicker = {
    enabled  = false,
    minDelay = 80,   -- мс
    maxDelay = 150,  -- мс
}

lua_thread.create(function()
    while true do
        if autoclicker.enabled and isKeyDown(VK_LBUTTON) then
            -- симулируем клик
            setKeyState(VK_LBUTTON, true)
            wait(math.random(30, 60))
            setKeyState(VK_LBUTTON, false)
            -- рандомная пауза между кликами
            wait(math.random(autoclicker.minDelay, autoclicker.maxDelay))
        else
            wait(10)
        end
    end
end)
```

### Чат-монитор — реакция на паттерны

```lua
local chatPatterns = {
    { pattern = "Автобус%s+%d+", action = function(m) onBusAnnounce(m) end },
    { pattern = "Ивент через (%d+)", action = function(m, t) scheduleEvent(tonumber(t)) end },
}

function samp.onServerMessage(color, message)
    for _, rule in ipairs(chatPatterns) do
        local match = {message:match(rule.pattern)}
        if #match > 0 then
            rule.action(message, table.unpack(match))
        end
    end
end
```

### Авто-диалог

```lua
local dialogRules = {
    { titlePattern = "Вход в систему", buttonIndex = 1, inputText = "mypassword" },
    { titlePattern = "Подтверждение",  buttonIndex = 1 },
    { titlePattern = "Реклама",        buttonIndex = 2 },  -- закрыть
}

function samp.onShowDialog(id, style, title, b1, b2, text)
    for _, rule in ipairs(dialogRules) do
        if title:find(rule.titlePattern) then
            lua_thread.create(function()
                wait(math.random(300, 700))  -- человекоподобная задержка
                sampSendDialogResponse(id, rule.buttonIndex, -1, rule.inputText or "")
            end)
            return false  -- скрыть диалог
        end
    end
end
```

---

## Конфиги — сохранение настроек

### Через inicfg (.ini файл)

```lua
local inicfg = require "inicfg"

local CONFIG_FILE = getWorkingDirectory() .. "\\config\\myscript.ini"
local config = {}

local DEFAULTS = {
    main = {
        enabled  = false,
        maxDist  = 150.0,
        hotkey   = 0x2D,  -- INSERT
    },
    esp = {
        showName = true,
        showHP   = true,
        colorR   = 255,
        colorG   = 0,
        colorB   = 0,
    }
}

function loadConfig()
    -- Создать папку если нет
    lfs.mkdir(getWorkingDirectory() .. "\\config")

    if doesFileExist(CONFIG_FILE) then
        config = inicfg.load(CONFIG_FILE)
    end

    -- Заполнить отсутствующие ключи дефолтами
    for section, values in pairs(DEFAULTS) do
        if not config[section] then config[section] = {} end
        for key, def in pairs(values) do
            if config[section][key] == nil then
                config[section][key] = def
            end
        end
    end
end

function saveConfig()
    inicfg.save(config, CONFIG_FILE)
end

-- Использование
-- config.main.enabled
-- config.esp.showName
```

### Через JSON

```lua
local json = require "json"  -- если доступен в сборке

local function loadJSON(path)
    local f = io.open(path, "r")
    if not f then return nil end
    local data = f:read("*a")
    f:close()
    local ok, parsed = pcall(json.decode, data)
    return ok and parsed or nil
end

local function saveJSON(path, data)
    local f = io.open(path, "w")
    if f then
        f:write(json.encode(data, {indent = true}))
        f:close()
    end
end
```

---

## Обработка клавиш

```lua
-- Виртуальные коды клавиш (VK_*)
local VK_INSERT = 0x2D
local VK_DELETE = 0x2E
local VK_F1     = 0x70
local VK_LSHIFT = 0xA0
local VK_LCTRL  = 0xA2

-- Проверка нажатия (удержание)
if isKeyDown(VK_F1) then ... end

-- Проверка с учётом фокуса окна (не срабатывает в чате)
if isKeyDown(VK_F1) and not sampIsChatInputActive()
   and not sampIsDialogActive() then
    -- клавиша нажата и мы не в чате
end

-- Хук на события окна (toggle по нажатию)
local keyStates = {}
addEventHandler("onWindowMessage", function(msg, wparam)
    -- WM_KEYDOWN = 0x100, WM_KEYUP = 0x101
    if msg == 0x100 and not keyStates[wparam] then
        keyStates[wparam] = true
        onKeyPress(wparam)
    elseif msg == 0x101 then
        keyStates[wparam] = false
    end
end)

function onKeyPress(vk)
    if vk == VK_INSERT then
        isOpen[0] = not isOpen[0]
    end
    if vk == VK_DELETE then
        espConfig.enabled = not espConfig.enabled
        printToChat("ESP: " .. (espConfig.enabled and "ВКЛ" or "ВЫКЛ"))
    end
end
```

---

## Работа с чатом

```lua
-- Вывод в чат клиенту (не виден серверу)
sampAddChatMessage("[Info] Привет!", 0x00FF00)

-- То же через форматирование цветов SA:MP
printToChat("{00FF00}[Info] {FFFFFF}Привет!")

-- Отправить сообщение на сервер (видит сервер)
sampSendChat("Привет всем!")

-- Команда серверу
sampSendChat("/q")

-- Проверить активен ли чат
if sampIsChatInputActive() then
    -- игрок печатает в чате
end

-- Получить текст из поля ввода чата
local text = sampGetChatInputText()

-- Отключить HUD чата SA:MP (показывать/скрывать)
sampSetChatDisplayMode(0)  -- скрыть
sampSetChatDisplayMode(2)  -- показать
```

---

## Отладка

### Вывод информации

```lua
-- В чат клиента (быстро и удобно)
sampAddChatMessage("[DEBUG] x=" .. tostring(x), 0xFFFF00)

-- В лог MoonLoader (moonloader/moonloader.log)
print("Debug: " .. tostring(value))

-- Красивый dump таблицы
local function dumpTable(t, indent)
    indent = indent or 0
    local prefix = string.rep("  ", indent)
    for k, v in pairs(t) do
        if type(v) == "table" then
            print(prefix .. tostring(k) .. ":")
            dumpTable(v, indent + 1)
        else
            print(prefix .. tostring(k) .. " = " .. tostring(v))
        end
    end
end
dumpTable(config)
```

### Перехват ошибок

```lua
-- Обернуть критические функции
local function safeTick()
    local ok, err = xpcall(onTick, function(e)
        return debug.traceback(e, 2)
    end)
    if not ok then
        sampAddChatMessage("[ERROR] " .. tostring(err), 0xFF0000)
    end
end

-- В главном луп
while true do
    wait(0)
    safeTick()
end
```

### Горячая перезагрузка

```lua
-- В чате набрать /reloadscript myscript
-- или через MoonLoader меню (правый Ctrl + правый Alt)
-- Скрипт перезагружается без перезапуска игры
-- onScriptTerminate вызовется перед перезагрузкой
```

---

## Типичные ошибки MoonLoader

### 1. Забыть `wait()` в луп-корутине → зависание игры
```lua
-- ПЛОХО: бесконечный цикл без wait — игра зависнет
while true do
    checkSomething()
    -- нет wait()!
end

-- ХОРОШО
while true do
    wait(0)
    checkSomething()
end
```

### 2. Вызов SAMP функций до isSampAvailable()
```lua
-- ПЛОХО: краш если samp не загружен
function main()
    local name = sampGetPlayerNickname(0)  -- слишком рано!
end

-- ХОРОШО
function main()
    repeat wait(100) until isSampAvailable()
    local name = sampGetPlayerNickname(0)
end
```

### 3. Блокировать главный поток синхронными операциями
```lua
-- ПЛОХО: чтение большого файла блокирует игру
function main()
    local data = io.open("huge_file.json"):read("*a")
    ...
end

-- ХОРОШО: в дочерней корутине или по частям
lua_thread.create(function()
    local data = io.open("huge_file.json"):read("*a")
    -- обработать data
end)
```

### 4. Не освобождать ресурсы
```lua
-- ПЛОХО: шрифты создаются каждый фрейм
addEventHandler("onD3DPresent", function()
    local font = renderCreateFont("Arial", 12, 0)  -- утечка!
    renderFontDrawText(font, "text", 0, 0, 0xFFFFFFFF)
end)

-- ХОРОШО: создать один раз при инициализации
local myFont
lua_thread.create(function()
    repeat wait(100) until isSampAvailable()
    myFont = renderCreateFont("Arial", 12, 0)
end)
addEventHandler("onD3DPresent", function()
    if myFont then
        renderFontDrawText(myFont, "text", 0, 0, 0xFFFFFFFF)
    end
end)
```

### 5. Кириллица в строках
```lua
-- MoonLoader ожидает CP1251, Lua файлы лучше сохранять в CP1251
-- Если файл в UTF-8:
local encoding = require "encoding"
encoding.default = "CP1251"
local u = encoding.UTF8
local cp = encoding.CP1251

-- Конвертация
local ruStr = cp:decode("Привет")  -- если файл в CP1251
local result = u:decode("Привет")  -- если файл в UTF-8
```

### 6. sampGetPlayerIdByCharHandle возвращает два значения
```lua
-- ПЛОХО
local myId = sampGetPlayerIdByCharHandle(PLAYER_PED)  -- вернёт true, id

-- ХОРОШО
local result, myId = sampGetPlayerIdByCharHandle(PLAYER_PED)
if not result then return end  -- персонаж не найден в SAMP
```

### 7. Забыть проверить результат sampGetCharHandleBySampPlayerId
```lua
-- ПЛОХО
local ped = sampGetCharHandleBySampPlayerId(i)  -- краш если nil
local x,y,z = getCharCoordinates(ped)

-- ХОРОШО
local result, ped = sampGetCharHandleBySampPlayerId(i)
if result and ped then
    local x,y,z = getCharCoordinates(ped)
end
```

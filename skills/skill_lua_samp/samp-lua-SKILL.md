---
name: samp-lua
description: >
  Use this skill whenever the user wants to write, debug, or improve Lua scripts for SA:MP
  (San Andreas Multiplayer). Covers both client-side MoonLoader scripting and server-side Lua
  scripting. Trigger on any mention of: SA:MP, MoonLoader, moon script, SAMP Lua, mimgui,
  samp.events, SA:MP server scripting, Lua plugin for SA:MP, open.mp Lua, gamemode Lua,
  filterscript Lua. Also trigger when the user pastes SA:MP Lua code for review or debugging.
---

# SA:MP Lua Scripting Skill

## Роутер: какой подскилл читать

| Контекст | Файл |
|---|---|
| Клиентский скрипт (MoonLoader, `.lua` файл в `moonloader/`) | `moonloader/SKILL.md` |
| Серверный скрипт (плагин, геймод, filterscript на Lua) | `server/SKILL.md` |
| Не ясно | Спроси пользователя: клиент или сервер? |

**Всегда читай этот файл до конца** — здесь общие основы Lua, применимые к обоим контекстам.

---

## Lua 5.1 — Основы для SA:MP

SA:MP экосистема (и MoonLoader, и большинство серверных плагинов) использует **Lua 5.1** или **LuaJIT** (совместимый с 5.1 с расширениями).

### Типы данных и nil

```lua
-- nil — отсутствие значения, не то же самое что false
local x = nil
if x then print("не выполнится") end
if x == nil then print("выполнится") end

-- Числа — только double (нет int/float разделения в Lua 5.1)
local n = 42
local f = 3.14

-- Строки иммутабельны
local s = "hello"
s = s .. " world"  -- создаёт новую строку
```

### Таблицы — основная структура данных

```lua
-- Таблица как массив (индексы с 1!)
local arr = {10, 20, 30}
print(arr[1])  -- 10, НЕ 0

-- Таблица как словарь
local player = {
    name = "Player1",
    health = 100,
    pos = {x = 0.0, y = 0.0, z = 0.0}
}
print(player.name)
print(player["health"])

-- Смешанный режим
local mixed = {1, 2, key = "value", 3}
-- mixed[1]=1, mixed[2]=2, mixed[3]=3, mixed.key="value"

-- Итерация по массиву
for i, v in ipairs(arr) do
    print(i, v)
end

-- Итерация по словарю (порядок не гарантирован)
for k, v in pairs(player) do
    print(k, v)
end

-- Длина массива (осторожно с дырками!)
print(#arr)  -- 3
```

### Функции и замыкания

```lua
-- Функции — first-class values
local function add(a, b)
    return a + b
end

-- Анонимная функция
local mul = function(a, b) return a * b end

-- Множественные возвращаемые значения
local function getPos()
    return 100.0, 200.0, 10.0
end
local x, y, z = getPos()

-- Varargs
local function sum(...)
    local args = {...}
    local total = 0
    for _, v in ipairs(args) do total = total + v end
    return total
end

-- Замыкание (closure) — функция захватывает переменные из внешней области
local function makeCounter()
    local count = 0
    return function()
        count = count + 1
        return count
    end
end
local counter = makeCounter()
counter()  -- 1
counter()  -- 2
```

### ООП через метатаблицы

```lua
-- Паттерн класса в Lua 5.1
local Player = {}
Player.__index = Player

function Player.new(name, health)
    local self = setmetatable({}, Player)
    self.name = name
    self.health = health or 100
    return self
end

function Player:takeDamage(amount)
    self.health = self.health - amount
    if self.health <= 0 then
        self:onDeath()
    end
end

function Player:onDeath()
    print(self.name .. " умер")
end

function Player:getInfo()
    return string.format("[%s] HP: %d", self.name, self.health)
end

-- Наследование
local Bot = setmetatable({}, {__index = Player})
Bot.__index = Bot

function Bot.new(name)
    local self = Player.new(name, 50)
    return setmetatable(self, Bot)
end

function Bot:onDeath()
    print(self.name .. " (бот) уничтожен, респавн через 5 сек")
end

-- Использование
local p = Player.new("Hero", 100)
p:takeDamage(30)
print(p:getInfo())  -- [Hero] HP: 70
```

### Обработка ошибок

```lua
-- pcall — защищённый вызов, возвращает статус + результат/ошибку
local ok, result = pcall(function()
    return riskyFunction()
end)
if not ok then
    print("Ошибка: " .. tostring(result))
end

-- xpcall — как pcall, но с обработчиком для traceback
local function errorHandler(err)
    return debug.traceback(err, 2)
end

local ok, result = xpcall(function()
    return riskyFunction()
end, errorHandler)

-- Бросить ошибку
local function validateHealth(hp)
    if type(hp) ~= "number" then
        error("health должен быть числом, получен: " .. type(hp), 2)
    end
    if hp < 0 or hp > 100 then
        error("health должен быть в диапазоне [0, 100]", 2)
    end
end
```

### Корутины

```lua
-- Корутина — кооперативная многозадачность
local co = coroutine.create(function(a, b)
    print("старт", a, b)
    local c = coroutine.yield(a + b)  -- приостановка, возврат значения
    print("продолжение", c)
    return "готово"
end)

-- Запуск/возобновление
local ok, val = coroutine.resume(co, 10, 20)  -- "старт 10 20", val = 30
local ok, val = coroutine.resume(co, 99)       -- "продолжение 99", val = "готово"

-- Обёртка для удобного использования
local function waitCoroutine(seconds)
    -- В MoonLoader: wait(ms), в серверном Lua — таймеры
    coroutine.yield(seconds)
end
```

### Строки — полезные функции

```lua
-- Форматирование
local s = string.format("Игрок %s имеет %d HP (%.1f%%)", name, hp, pct)

-- Поиск и замена
string.find("hello world", "world")      -- 7, 11
string.match("score: 42", "%d+")         -- "42"
string.gsub("a b c", " ", "_")           -- "a_b_c", 2

-- Работа с регистром
string.upper("hello")   -- "HELLO"
string.lower("WORLD")   -- "world"

-- Разбить строку по разделителю
local function split(str, sep)
    local result = {}
    for part in str:gmatch("[^" .. sep .. "]+") do
        result[#result + 1] = part
    end
    return result
end

-- Trim (обрезка пробелов)
local function trim(s)
    return s:match("^%s*(.-)%s*$")
end
```

### Модули

```lua
-- module.lua
local M = {}

M.VERSION = "1.0.0"

local private_var = "не виден снаружи"

function M.publicFunc()
    return private_var
end

return M

-- main.lua
local mymodule = require("module")
mymodule.publicFunc()
```

---

## Типичные Lua ошибки в SA:MP контексте

### 1. Индексация nil
```lua
-- ОШИБКА: attempt to index a nil value
local player = getPlayerData(id)
print(player.name)  -- если player == nil — краш

-- ИСПРАВЛЕНИЕ
local player = getPlayerData(id)
if player then
    print(player.name)
end
-- или
print(player and player.name or "неизвестно")
```

### 2. Глобальные переменные вместо локальных
```lua
-- ПЛОХО: глобальная переменная (медленнее + может конфликтовать)
function onTick()
    counter = counter + 1  -- глобальная!
end

-- ХОРОШО
local counter = 0
function onTick()
    counter = counter + 1
end
```

### 3. Изменение таблицы во время итерации
```lua
-- ОШИБКА: непредсказуемое поведение
for k, v in pairs(myTable) do
    if v.remove then myTable[k] = nil end  -- опасно!
end

-- ИСПРАВЛЕНИЕ: собрать ключи для удаления отдельно
local toRemove = {}
for k, v in pairs(myTable) do
    if v.remove then toRemove[#toRemove+1] = k end
end
for _, k in ipairs(toRemove) do
    myTable[k] = nil
end
```

### 4. Сравнение чисел с плавающей точкой
```lua
-- ОШИБКА
if distance == 0.0 then ... end  -- может не сработать из-за float

-- ИСПРАВЛЕНИЕ
local EPS = 0.001
if math.abs(distance) < EPS then ... end
```

### 5. Утечка через замыкания в циклах
```lua
-- ОШИБКА: все колбэки захватят i == 11
local callbacks = {}
for i = 1, 10 do
    callbacks[i] = function() print(i) end  -- замыкание на i!
end

-- ИСПРАВЛЕНИЕ: локальная копия
for i = 1, 10 do
    local idx = i
    callbacks[i] = function() print(idx) end
end
```

### 6. #table не работает надёжно с дырками
```lua
local t = {1, 2, nil, 4}
print(#t)  -- может вернуть 2 или 4, undefined behavior

-- Если нужна точная длина — считай вручную
local function tableLen(t)
    local count = 0
    for _ in pairs(t) do count = count + 1 end
    return count
end
```

---

После прочтения этого файла **перейди к нужному подскиллу**:
- Клиент → `moonloader/SKILL.md`
- Сервер → `server/SKILL.md`

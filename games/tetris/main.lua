local BOARD_W = 10
local BOARD_H = 20
local VISIBLE_H = 18
local VISIBLE_OFF = 2
local CELL = 5
local FIELD_X = 0
local FIELD_Y = 28
local FIELD_W = BOARD_W * CELL
local FIELD_H = VISIBLE_H * CELL
local GRAVITY_BASE = 1.0
local SPRITE_DIR = "games/tetris/sprites/"
-- audio.* paths are relative to game directory, no prefix needed

local function spritePath(name)
    return SPRITE_DIR .. name .. ".spr"
end

local PIECES = {
    {
        color = {0, 255, 255},
        sprite = spritePath("i"),
        rotations = {
            {{0,0},{0,1},{0,2},{0,3}},
            {{0,0},{1,0},{2,0},{3,0}},
            {{0,0},{0,1},{0,2},{0,3}},
            {{0,0},{1,0},{2,0},{3,0}},
        }
    },
    {
        color = {255, 255, 0},
        sprite = spritePath("o"),
        rotations = {
            {{0,0},{0,1},{1,0},{1,1}},
        }
    },
    {
        color = {170, 0, 170},
        sprite = spritePath("t"),
        rotations = {
            {{0,1},{1,0},{1,1},{1,2}},
            {{0,1},{1,1},{1,2},{2,1}},
            {{1,0},{1,1},{1,2},{2,1}},
            {{0,1},{1,0},{1,1},{2,1}},
        }
    },
    {
        color = {0, 255, 0},
        sprite = spritePath("s"),
        rotations = {
            {{0,1},{0,2},{1,0},{1,1}},
            {{0,0},{1,0},{1,1},{2,1}},
            {{0,1},{0,2},{1,0},{1,1}},
            {{0,0},{1,0},{1,1},{2,1}},
        }
    },
    {
        color = {255, 0, 0},
        sprite = spritePath("z"),
        rotations = {
            {{0,0},{0,1},{1,1},{1,2}},
            {{0,1},{1,0},{1,1},{2,0}},
            {{0,0},{0,1},{1,1},{1,2}},
            {{0,1},{1,0},{1,1},{2,0}},
        }
    },
    {
        color = {0, 0, 255},
        sprite = spritePath("j"),
        rotations = {
            {{0,0},{1,0},{1,1},{1,2}},
            {{0,1},{0,2},{1,1},{2,1}},
            {{1,0},{1,1},{1,2},{2,2}},
            {{0,1},{1,1},{2,0},{2,1}},
        }
    },
    {
        color = {255, 165, 0},
        sprite = spritePath("l"),
        rotations = {
            {{0,2},{1,0},{1,1},{1,2}},
            {{0,1},{1,1},{2,1},{2,2}},
            {{1,0},{1,1},{1,2},{2,0}},
            {{0,0},{0,1},{1,1},{2,1}},
        }
    },
}

local board = {}
local currentPiece = nil
local nextPieceType = nil
local score = 0
local level = 1
local lines = 0
local highScore = 0
local gravityTimer = 0
local gameState = "menu"
local lockTimer = 0
local lockDelay = 0.2
local bag = {}
local lineClearAnim = 0
local gameOverTimer = 0
local blinkTimer = 0
local pausedState = nil

local function centerText(s, y, color)
    render.text(0, y, s, color, render.getWidth(), 1)
end

local function initBoard()
    board = {}
    for y = 1, BOARD_H do
        board[y] = {}
        for x = 1, BOARD_W do
            board[y][x] = 0
        end
    end
end

local function fillBag()
    bag = {}
    for i = 1, 7 do
        bag[i] = i
    end
    for i = 7, 2, -1 do
        local j = math.random(1, i)
        bag[i], bag[j] = bag[j], bag[i]
    end
end

local function nextFromBag()
    if #bag == 0 then
        fillBag()
    end
    local t = bag[1]
    table.remove(bag, 1)
    return t
end

local function isValid(pType, rot, px, py)
    local cells = PIECES[pType].rotations[rot]
    for _, c in ipairs(cells) do
        local x = px + c[2]
        local y = py + c[1]
        if x < 1 or x > BOARD_W or y < 1 or y > BOARD_H then
            return false
        end
        if board[y][x] ~= 0 then
            return false
        end
    end
    return true
end

local function spawnPiece()
    if nextPieceType == nil then
        nextPieceType = nextFromBag()
    end
    local pType = nextPieceType
    nextPieceType = nextFromBag()
    local startX = 4
    local startY = 1
    if not isValid(pType, 1, startX, startY) then
        return false
    end
    currentPiece = {
        type = pType,
        rotation = 1,
        x = startX,
        y = startY,
    }
    gravityTimer = 0
    lockTimer = 0
    lineClearAnim = 0
    return true
end

local function lockPiece()
    audio.playSfx("drop.wav")
    local cells = PIECES[currentPiece.type].rotations[currentPiece.rotation]
    for _, c in ipairs(cells) do
        local x = currentPiece.x + c[2]
        local y = currentPiece.y + c[1]
        if y >= 1 and y <= BOARD_H and x >= 1 and x <= BOARD_W then
            board[y][x] = currentPiece.type
        end
    end
end

local function clearLines()
    local cleared = 0
    local y = BOARD_H
    while y >= 1 do
        local full = true
        for x = 1, BOARD_W do
            if board[y][x] == 0 then
                full = false
                break
            end
        end
        if full then
            for ry = y, 2, -1 do
                for x = 1, BOARD_W do
                    board[ry][x] = board[ry - 1][x]
                end
            end
            for x = 1, BOARD_W do
                board[1][x] = 0
            end
            cleared = cleared + 1
        else
            y = y - 1
        end
    end
    if cleared > 0 then
        audio.playSfx("lineClear.wav")
    end
    return cleared
end

local function doGameOver()
    audio.stopMusic()
    audio.playSfx("gameOver.wav")
    gameState = "gameover"
    gameOverTimer = 1.0
    if score > highScore then
        highScore = score
        save.write("tetris", tostring(highScore))
    end
end

local function saveGameState()
    pausedState = {
        board = {},
        currentPiece = currentPiece and { type = currentPiece.type, rotation = currentPiece.rotation, x = currentPiece.x, y = currentPiece.y } or nil,
        nextPieceType = nextPieceType,
        score = score, level = level, lines = lines,
        gravityTimer = gravityTimer, lockTimer = lockTimer,
        bag = {}, lineClearAnim = lineClearAnim,
    }
    for y = 1, BOARD_H do
        pausedState.board[y] = {}
        for x = 1, BOARD_W do
            pausedState.board[y][x] = board[y][x]
        end
    end
    for i = 1, #bag do pausedState.bag[i] = bag[i] end
end

local function restoreGameState()
    board = {}
    for y = 1, BOARD_H do
        board[y] = {}
        for x = 1, BOARD_W do
            board[y][x] = pausedState.board[y][x]
        end
    end
    currentPiece = pausedState.currentPiece and {
        type = pausedState.currentPiece.type, rotation = pausedState.currentPiece.rotation,
        x = pausedState.currentPiece.x, y = pausedState.currentPiece.y,
    } or nil
    nextPieceType = pausedState.nextPieceType
    score, level, lines = pausedState.score, pausedState.level, pausedState.lines
    gravityTimer, lockTimer = pausedState.gravityTimer, pausedState.lockTimer
    bag = {}
    for i = 1, #pausedState.bag do bag[i] = pausedState.bag[i] end
    lineClearAnim = pausedState.lineClearAnim
    pausedState = nil
end

local function startPlaying()
    gameState = "playing"
    audio.stopMusic()
    audio.playMusic("tetris.wav")
end

local function getGhostY()
    local gy = currentPiece.y
    while isValid(currentPiece.type, currentPiece.rotation, currentPiece.x, gy + 1) do
        gy = gy + 1
    end
    return gy
end

local function tryMove(dx, dy)
    local p = currentPiece
    if isValid(p.type, p.rotation, p.x + dx, p.y + dy) then
        p.x = p.x + dx
        p.y = p.y + dy
        return true
    end
    return false
end

local function tryRotate(dir)
    local p = currentPiece
    local nRot = #PIECES[p.type].rotations
    local newRot = ((p.rotation - 1 + dir + nRot) % nRot) + 1
    local kicks = {{0,0}, {-1,0}, {1,0}, {0,-1}, {-1,-1}, {1,-1}, {-2,0}, {2,0}, {0,-2}}
    for _, k in ipairs(kicks) do
        local nx = p.x + k[1]
        local ny = p.y + k[2]
        if isValid(p.type, newRot, nx, ny) then
            p.rotation = newRot
            p.x = nx
            p.y = ny
            return true
        end
    end
    return false
end

local function hardDrop()
    local p = currentPiece
    while isValid(p.type, p.rotation, p.x, p.y + 1) do
        p.y = p.y + 1
        score = score + 2
    end
    lockPiece()
    local n = clearLines()
    if n > 0 then
        local pts = {0, 100, 300, 500, 800}
        score = score + pts[n] * level
        lines = lines + n
        level = math.floor(lines / 10) + 1
        lineClearAnim = 0.3
    end
    if not spawnPiece() then
        doGameOver()
    end
end

local function drawCell(x, y, pType)
    render.sprite(x, y, PIECES[pType].sprite)
end

local function drawPreviewCell(x, y, pType)
    local c = PIECES[pType].color
    render.fillRect(x, y, 3, 3, Color(c[1], c[2], c[3]))
end

local function drawBoard()
    for y = 1 + VISIBLE_OFF, BOARD_H do
        local visY = y - VISIBLE_OFF
        for x = 1, BOARD_W do
            local sx = FIELD_X + (x - 1) * CELL
            local sy = FIELD_Y + (visY - 1) * CELL
            if board[y][x] ~= 0 then
                drawCell(sx, sy, board[y][x])
            else
                render.drawRect(sx, sy, CELL, CELL, Color(30, 30, 30))
            end
        end
    end
end

local function drawCurrentPiece()
    if not currentPiece then return end
    local p = currentPiece
    local cells = PIECES[p.type].rotations[p.rotation]
    for _, c in ipairs(cells) do
        local sx = FIELD_X + (p.x - 1 + c[2]) * CELL
        local sy = FIELD_Y + (p.y - 1 - VISIBLE_OFF + c[1]) * CELL
        if p.y + c[1] >= 1 + VISIBLE_OFF then
            drawCell(sx, sy, p.type)
        end
    end
end

local function drawGhost()
    if not currentPiece then return end
    local p = currentPiece
    local gy = getGhostY()
    local cells = PIECES[p.type].rotations[p.rotation]
    local gc = PIECES[p.type].color
    local ghostCol = Color(
        math.floor(gc[1] * 0.3),
        math.floor(gc[2] * 0.3),
        math.floor(gc[3] * 0.3)
    )
    for _, c in ipairs(cells) do
        local sx = FIELD_X + (p.x - 1 + c[2]) * CELL
        local sy = FIELD_Y + (gy - 1 - VISIBLE_OFF + c[1]) * CELL
        if gy + c[1] >= 1 + VISIBLE_OFF then
            render.drawRect(sx, sy, CELL, CELL, ghostCol)
        end
    end
end

local function drawNextPiece()
    local nx = 30
    local ny = 120
    local cells = PIECES[nextPieceType].rotations[1]
    local minC, maxC, minR, maxR = 4, 1, 4, 1
    for _, c in ipairs(cells) do
        if c[2] < minC then minC = c[2] end
        if c[2] > maxC then maxC = c[2] end
        if c[1] < minR then minR = c[1] end
        if c[1] > maxR then maxR = c[1] end
    end
    local cw = maxC - minC + 1
    local ch = maxR - minR + 1
    local ox = nx
    local oy = ny
    for _, c in ipairs(cells) do
        local sx = ox + (c[2] - minC) * 3
        local sy = oy + (c[1] - minR) * 3
        drawPreviewCell(sx, sy, nextPieceType)
    end
end

local function drawHUD()
    render.text(0, 0, "SC:" .. score, COLOR_WHITE)
    render.text(0, 8, "LV:" .. level, COLOR_WHITE)
    render.text(28, 8, "LN:" .. lines, COLOR_WHITE)
    render.text(0, 16, "HI:" .. highScore, COLOR_ACCENT)
end

local function drawFieldBorder()
    render.drawRect(FIELD_X - 1, FIELD_Y - 1, FIELD_W + 2, FIELD_H + 2, COLOR_GREY)
end

local function drawTitleScreen()
    render.clear(Color(10, 10, 10))
    centerText("TETRIS", 20, COLOR_PRIMARY)
    render.text(0, 60, "HIGH SCORE: " .. highScore, COLOR_TEXT_DIM, render.getWidth(), 1)
    centerText("PRESS ENTER", 76, COLOR_WHITE)
    if blinkTimer < 0.5 then
        centerText("TO START", 84, COLOR_WHITE)
    end
end

local function drawGameOver()
    render.clear(Color(10, 10, 10))
    centerText("GAME OVER", 20, COLOR_RED)
    render.text(0, 36, "SCORE: " .. score .. "  LEVEL: " .. level .. "  LINES: " .. lines, COLOR_WHITE, render.getWidth(), 1)
    if score >= highScore and score > 0 then
        centerText("NEW HIGH SCORE!", 74, COLOR_YELLOW)
    else
        render.text(0, 74, "HIGH SCORE: " .. highScore, COLOR_TEXT_DIM, render.getWidth(), 1)
    end
    centerText("ENTER TO PLAY", 92, COLOR_WHITE)
    centerText("ESC TO QUIT", 118, COLOR_GREY)
end

local function drawPlaying()
    render.clear(Color(10, 10, 10))
    drawHUD()
    drawFieldBorder()
    drawBoard()
    drawGhost()
    drawCurrentPiece()
    render.text(0, 120, "NEXT", COLOR_TEXT_DIM)
    drawNextPiece()
    if lineClearAnim > 0 then
        render.fillRect(FIELD_X, FIELD_Y, FIELD_W, FIELD_H, Color(60, 60, 60))
    end
end

function setup()
    math.randomseed(os.time())
    initBoard()
    fillBag()
    nextPieceType = nextFromBag()
    local s = save.read("tetris")
    if s ~= "" then
        highScore = tonumber(s) or 0
    end
    gameState = "menu"
    score = 0
    level = 1
    lines = 0
end

function loop(dt)
    while true do
        if gameState == "menu" then
            blinkTimer = blinkTimer + dt
            if blinkTimer >= 1.0 then blinkTimer = blinkTimer - 1.0 end
            if input.keyPress("KEY_ENTER") or input.keyPress("KEY_SPACE") then
                initBoard()
                fillBag()
                nextPieceType = nextFromBag()
                score = 0
                level = 1
                lines = 0
                if spawnPiece() then
                    startPlaying()
                end
            end
            if input.keyPress("KEY_ESCAPE") then
                quit()
            end
        end

        if gameState == "gameover" then
            if gameOverTimer > 0 then
                gameOverTimer = gameOverTimer - dt
                if gameOverTimer < 0 then gameOverTimer = 0 end
            end
            if gameOverTimer <= 0 and (input.keyPress("KEY_ENTER") or input.keyPress("KEY_SPACE")) then
                initBoard()
                fillBag()
                nextPieceType = nextFromBag()
                score = 0
                level = 1
                lines = 0
                if spawnPiece() then
                    startPlaying()
                end
            end
            if input.keyPress("KEY_ESCAPE") then
                quit()
            end
        end

        if gameState == "playing" then
            if lineClearAnim > 0 then
                lineClearAnim = lineClearAnim - dt
                if lineClearAnim < 0 then
                    lineClearAnim = 0
                end
            end

            if input.keyPress("KEY_ESCAPE") then
                saveGameState()
                audio.stopMusic()
                gameState = "paused"
            end

            if input.keyPress("KEY_LEFT") or input.keyPress("KEY_A") then
                if tryMove(-1, 0) then audio.playSfx("move.wav") end
                lockTimer = 0
            end
            if input.keyPress("KEY_RIGHT") or input.keyPress("KEY_D") then
                if tryMove(1, 0) then audio.playSfx("move.wav") end
                lockTimer = 0
            end
            if input.keyPress("KEY_UP") or input.keyPress("KEY_W") then
                if tryRotate(1) then audio.playSfx("rotate.wav") end
                lockTimer = 0
            end
            if input.keyPress("KEY_DOWN") or input.keyPress("KEY_S") then
                if tryMove(0, 1) then
                    audio.playSfx("move.wav")
                else
                    lockPiece()
                    local n = clearLines()
                    if n > 0 then
                        local pts = {0, 100, 300, 500, 800}
                        score = score + pts[n] * level
                        lines = lines + n
                        level = math.floor(lines / 10) + 1
                        lineClearAnim = 0.3
                    end
                    if not spawnPiece() then
                        doGameOver()
                    end
                end
                lockTimer = 0
            end
            if input.keyPress("KEY_SPACE") or input.keyPress("KEY_ENTER") then
                hardDrop()
            end

            local gravDelay = math.max(0.05, GRAVITY_BASE / level)
            gravityTimer = gravityTimer + dt
            if gravityTimer >= gravDelay then
                gravityTimer = 0
                if not tryMove(0, 1) then
                    lockTimer = lockTimer + dt
                    if lockTimer >= lockDelay then
                        lockPiece()
                        local n = clearLines()
                        if n > 0 then
                            local pts = {0, 100, 300, 500, 800}
                            score = score + pts[n] * level
                            lines = lines + n
                            level = math.floor(lines / 10) + 1
                            lineClearAnim = 0.3
                        end
                        if not spawnPiece() then
                            doGameOver()
                        end
                    end
                else
                    lockTimer = 0
                end
            end
        end

        if gameState == "paused" then
            if input.keyPress("KEY_ESCAPE") then
                audio.stopMusic()
                gameState = "menu"
                blinkTimer = 0
                pausedState = nil
            elseif input.keyPress("KEY_ENTER") or input.keyPress("KEY_SPACE") then
                restoreGameState()
                audio.playMusic("tetris.wav")
                gameState = "playing"
            end
        end

        if gameState == "menu" then
            drawTitleScreen()
        elseif gameState == "gameover" then
            drawGameOver()
        elseif gameState == "paused" then
            drawPlaying()
            render.fillRect(0, 44, render.getWidth(), 44, Color(10, 10, 10))
            render.text(0, 48, "PAUSED\nENTER: CONTINUE\nESC: EXIT", COLOR_WHITE, render.getWidth(), 1, true)
        else
            drawPlaying()
        end

        dt = yield()
    end
end

function shutdown()
    audio.stopMusic()
    if score > highScore then
        save.write("tetris", tostring(score))
    end
    render.clear(Color(10, 10, 10))
end

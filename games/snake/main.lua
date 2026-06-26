local CELL = 5
local COLS = 24
local ROWS = 10
local OFFSET_X = 4
local OFFSET_Y = 11
local GRID_W = COLS * CELL
local GRID_H = ROWS * CELL
local SPRITE_DIR = "games/snake/sprites/"
local SPEEDS = { [1] = 0.35, [2] = 0.25, [3] = 0.15 }
local DIFF_LABELS = { "Easy", "Medium", "Hard" }

local settings = { appleStyle = 1, colorStyle = 1 }

local dirs = { up = "up", down = "down", left = "left", right = "right" }
local opposites = { up = "down", down = "up", left = "right", right = "left" }

local pendingDir = nil

local dirOrder = { up = 0, right = 1, down = 2, left = 3 }

local function playTurnSound(oldDir, newDir)
  local diff = (dirOrder[newDir] - dirOrder[oldDir] + 4) % 4
  if diff == 1 then
    audio.playSfx("turn_right.wav")
  elseif diff == 3 then
    audio.playSfx("turn_left.wav")
  end
end

local function cellPos(gx, gy)
  return OFFSET_X + (gx - 1) * CELL, OFFSET_Y + (gy - 1) * CELL
end

local function insideGrid(x, y)
  return x >= 1 and x <= COLS and y >= 1 and y <= ROWS
end

local function dirOffset(dir)
  if dir == dirs.up then return 0, -1
  elseif dir == dirs.down then return 0, 1
  elseif dir == dirs.left then return -1, 0
  elseif dir == dirs.right then return 1, 0
  end
  return 0, 0
end

local function lerpColor(c1, c2, t)
  return Color(
    math.floor(c1.r + (c2.r - c1.r) * t),
    math.floor(c1.g + (c2.g - c1.g) * t),
    math.floor(c1.b + (c2.b - c1.b) * t)
  )
end

local function getColors()
  if settings.colorStyle == 1 then
    return {
      head = { r = 0, g = 220, b = 0 }, tail = { r = 0, g = 60, b = 0 },
      head1 = { r = 0, g = 220, b = 0 }, tail1 = { r = 0, g = 60, b = 0 },
      head2 = { r = 220, g = 220, b = 0 }, tail2 = { r = 80, g = 80, b = 0 },
    }
  else
    return {
      head = { r = 0, g = 120, b = 220 }, tail = { r = 0, g = 30, b = 80 },
      head1 = { r = 220, g = 0, b = 0 }, tail1 = { r = 80, g = 0, b = 0 },
      head2 = { r = 0, g = 0, b = 220 }, tail2 = { r = 0, g = 0, b = 80 },
    }
  end
end

local function applePath()
  return SPRITE_DIR .. (settings.appleStyle == 1 and "apple.spr" or "apple2.spr")
end

local function spawnApple(snakeCells)
  local occupied = {}
  for _, seg in ipairs(snakeCells) do
    occupied[seg.x .. "," .. seg.y] = true
  end
  local candidates = {}
  for x = 1, COLS do
    for y = 1, ROWS do
      if not occupied[x .. "," .. y] then
        table.insert(candidates, { x = x, y = y })
      end
    end
  end
  if #candidates > 0 then
    return candidates[math.random(1, #candidates)]
  end
  return nil
end

local function initClassic()
  local cx, cy = math.floor(COLS / 2), math.floor(ROWS / 2)
  local s = {
    { x = cx, y = cy },
    { x = cx - 1, y = cy },
    { x = cx - 2, y = cy },
  }
  local st = {
    snake = s,
    dir = dirs.right,
    moveTimer = 0,
    score = 0,
    alive = true,
    apple = nil,
    highScore = 0,
  }
  st.apple = spawnApple(st.snake)
  return st
end

local function initWorms()
  local w1 = {
    { x = 4, y = 3 },
    { x = 3, y = 3 },
    { x = 2, y = 3 },
  }
  local w2 = {
    { x = COLS - 3, y = ROWS - 2 },
    { x = COLS - 2, y = ROWS - 2 },
    { x = COLS - 1, y = ROWS - 2 },
  }
  return {
    worm1 = w1,
    worm2 = w2,
    dir1 = dirs.right,
    dir2 = dirs.left,
    moveTimer = 0,
    result = nil,
  }
end

local function drawGrid()
  render.drawRect(OFFSET_X - 1, OFFSET_Y - 1, GRID_W + 2, GRID_H + 2, Color(60, 60, 60))
end

local function drawCell(gx, gy, color)
  render.fillRect(OFFSET_X + (gx - 1) * CELL, OFFSET_Y + (gy - 1) * CELL, CELL, CELL, color)
end

local function drawGradientSnake(snake, headCol, tailCol)
  local n = #snake
  for i, seg in ipairs(snake) do
    local t = (i - 1) / math.max(n - 1, 1)
    drawCell(seg.x, seg.y, lerpColor(headCol, tailCol, t))
  end
end

local function drawClassic(st)
  render.clear(Color(10, 10, 10))
  drawGrid()
  local cols = getColors()
  drawGradientSnake(st.snake, cols.head, cols.tail)
  if st.apple then
    local ax, ay = cellPos(st.apple.x, st.apple.y)
    render.sprite(ax, ay, applePath(), 1)
  end
  render.text(0, 0, "SCORE: " .. st.score .. "  HI: " .. st.highScore, COLOR_WHITE, render.getWidth(), 1, true)
end

local function drawWorms(st)
  render.clear(Color(10, 10, 10))
  drawGrid()
  local cols = getColors()
  drawGradientSnake(st.worm1, cols.head1, cols.tail1)
  drawGradientSnake(st.worm2, cols.head2, cols.tail2)
  render.text(0, 0, "LENGTH: " .. #st.worm1, COLOR_WHITE, render.getWidth(), 1, true)
end

local function processClassicInput(st)
  local cur = st.dir
  if input.keyPress("KEY_UP") or input.keyPress("KEY_W") then
    if opposites[cur] ~= dirs.up and cur ~= dirs.up then pendingDir = dirs.up; playTurnSound(cur, dirs.up) end
  elseif input.keyPress("KEY_DOWN") or input.keyPress("KEY_S") then
    if opposites[cur] ~= dirs.down and cur ~= dirs.down then pendingDir = dirs.down; playTurnSound(cur, dirs.down) end
  elseif input.keyPress("KEY_LEFT") or input.keyPress("KEY_A") then
    if opposites[cur] ~= dirs.left and cur ~= dirs.left then pendingDir = dirs.left; playTurnSound(cur, dirs.left) end
  elseif input.keyPress("KEY_RIGHT") or input.keyPress("KEY_D") then
    if opposites[cur] ~= dirs.right and cur ~= dirs.right then pendingDir = dirs.right; playTurnSound(cur, dirs.right) end
  end
end

local function processWormsInput(st)
  if input.keyPress("KEY_W") then
    if opposites[st.dir1] ~= dirs.up and st.dir1 ~= dirs.up then playTurnSound(st.dir1, dirs.up); st.dir1 = dirs.up end
  elseif input.keyPress("KEY_S") then
    if opposites[st.dir1] ~= dirs.down and st.dir1 ~= dirs.down then playTurnSound(st.dir1, dirs.down); st.dir1 = dirs.down end
  elseif input.keyPress("KEY_A") then
    if opposites[st.dir1] ~= dirs.left and st.dir1 ~= dirs.left then playTurnSound(st.dir1, dirs.left); st.dir1 = dirs.left end
  elseif input.keyPress("KEY_D") then
    if opposites[st.dir1] ~= dirs.right and st.dir1 ~= dirs.right then playTurnSound(st.dir1, dirs.right); st.dir1 = dirs.right end
  end
  if input.keyPress("KEY_UP") then
    if opposites[st.dir2] ~= dirs.up and st.dir2 ~= dirs.up then playTurnSound(st.dir2, dirs.up); st.dir2 = dirs.up end
  elseif input.keyPress("KEY_DOWN") then
    if opposites[st.dir2] ~= dirs.down and st.dir2 ~= dirs.down then playTurnSound(st.dir2, dirs.down); st.dir2 = dirs.down end
  elseif input.keyPress("KEY_LEFT") then
    if opposites[st.dir2] ~= dirs.left and st.dir2 ~= dirs.left then playTurnSound(st.dir2, dirs.left); st.dir2 = dirs.left end
  elseif input.keyPress("KEY_RIGHT") then
    if opposites[st.dir2] ~= dirs.right and st.dir2 ~= dirs.right then playTurnSound(st.dir2, dirs.right); st.dir2 = dirs.right end
  end
end

local function moveClassic(st)
  if pendingDir then
    st.dir = pendingDir
    pendingDir = nil
  end

  local dx, dy = dirOffset(st.dir)
  local head = st.snake[1]
  local nx, ny = head.x + dx, head.y + dy

  if not insideGrid(nx, ny) then
    st.alive = false
    return
  end

  local eating = st.apple and nx == st.apple.x and ny == st.apple.y
  local limit = #st.snake
  if not eating then
    limit = limit - 1
  end
  for i = 1, limit do
    if st.snake[i].x == nx and st.snake[i].y == ny then
      st.alive = false
      return
    end
  end

  table.insert(st.snake, 1, { x = nx, y = ny })
  if eating then
    st.score = st.score + 1
    audio.playSfx("eat.wav")
    st.apple = spawnApple(st.snake)
  else
    table.remove(st.snake)
  end
end

local function moveWorms(st)
  local dx1, dy1 = dirOffset(st.dir1)
  local dx2, dy2 = dirOffset(st.dir2)

  local nx1 = st.worm1[1].x + dx1
  local ny1 = st.worm1[1].y + dy1
  local nx2 = st.worm2[1].x + dx2
  local ny2 = st.worm2[1].y + dy2

  local w1Dead = not insideGrid(nx1, ny1)
  local w2Dead = not insideGrid(nx2, ny2)

  if nx1 == nx2 and ny1 == ny2 then
    st.result = "draw"
    return
  end

  if not w1Dead then
    for i = 2, #st.worm1 do
      if st.worm1[i].x == nx1 and st.worm1[i].y == ny1 then
        w1Dead = true
        break
      end
    end
    if not w1Dead then
      for i = 2, #st.worm2 do
        if st.worm2[i].x == nx1 and st.worm2[i].y == ny1 then
          w1Dead = true
          break
        end
      end
    end
  end

  if not w2Dead then
    for i = 2, #st.worm2 do
      if st.worm2[i].x == nx2 and st.worm2[i].y == ny2 then
        w2Dead = true
        break
      end
    end
    if not w2Dead then
      for i = 2, #st.worm1 do
        if st.worm1[i].x == nx2 and st.worm1[i].y == ny2 then
          w2Dead = true
          break
        end
      end
    end
  end

  if w1Dead and w2Dead then
    st.result = "draw"
    return
  elseif w1Dead then
    st.result = "p2win"
    return
  elseif w2Dead then
    st.result = "p1win"
    return
  end

  table.insert(st.worm1, 1, { x = nx1, y = ny1 })
  table.insert(st.worm2, 1, { x = nx2, y = ny2 })
end

local function loadSettings()
  local s = save.read("settings")
  if s ~= "" then
    local parts = {}
    for token in string.gmatch(s, "[^,]+") do
      table.insert(parts, token)
    end
    if #parts >= 1 then settings.appleStyle = tonumber(parts[1]) or 1 end
    if #parts >= 2 then settings.colorStyle = tonumber(parts[2]) or 1 end
  end
end

local function saveSettings()
  save.write("settings", settings.appleStyle .. "," .. settings.colorStyle)
end

local function loadHighScore()
  local s = save.read("highscore")
  if s ~= "" then return tonumber(s) or 0 end
  return 0
end

local function saveHighScore(val)
  save.write("highscore", tostring(val))
end

local function showSubMenu(title, items)
  menu.create(title, items)
  local choice = 0
  while choice == 0 do
    choice = menu.tick()
    if choice == 0 then dt = yield() end
  end
  return choice
end

local function pickDifficulty()
  local c = showSubMenu("DIFFICULTY", { "Easy", "Medium", "Hard", "Back" })
  if c < 0 or c == 4 then return nil end
  return c
end

local function runClassic(diff)
  local st = initClassic()
  st.highScore = loadHighScore()
  local moveInterval = SPEEDS[diff]
  local dt = 0

  while st.alive do
    if input.keyPress("KEY_ESCAPE") then
      while true do
        if input.keyPress("KEY_ESCAPE") then return
        elseif input.keyPress("KEY_ENTER") or input.keyPress("KEY_SPACE") then break end
        drawClassic(st)
        render.fillRect(0, 18, render.getWidth(), 30, Color(10, 10, 10))
        render.text(0, 22, "PAUSED\nENTER: CONTINUE\nESC: EXIT", COLOR_WHITE, render.getWidth(), 1, true)
        dt = yield()
      end
    end

    processClassicInput(st)
    st.moveTimer = st.moveTimer + dt
    while st.moveTimer >= moveInterval do
      st.moveTimer = st.moveTimer - moveInterval
      moveClassic(st)
      if not st.alive then break end
    end

    drawClassic(st)
    dt = yield()
  end

  audio.playSfx("gameover.wav")

  if st.score > st.highScore then
    st.highScore = st.score
    saveHighScore(st.highScore)
  end

  local blinkTimer = 0
  local dismissTimer = 0.5
  dt = 0
  while true do
    if dismissTimer <= 0 and (input.keyPress("KEY_ESCAPE") or input.keyPress("KEY_ENTER") or input.keyPress("KEY_SPACE")) then
      return
    end
    dismissTimer = dismissTimer - dt
    blinkTimer = blinkTimer + dt
    render.clear(Color(10, 10, 10))
    render.text(0, 18, "GAME OVER", COLOR_RED, render.getWidth(), 1, true)
    render.text(0, 30, "SCORE: " .. st.score, COLOR_WHITE, render.getWidth(), 1, true)
    if st.score >= st.highScore and st.score > 0 then
      render.text(0, 40, "NEW HIGH SCORE!", COLOR_YELLOW, render.getWidth(), 1, true)
    end
    if math.floor(blinkTimer * 2) % 2 == 0 then
      render.text(0, 52, "PRESS ANY KEY", COLOR_TEXT_DIM, render.getWidth(), 1, true)
    end
    if blinkTimer >= 1.0 then blinkTimer = blinkTimer - 1.0 end
    dt = yield()
  end
end

local function runWorms(diff)
  local st = initWorms()
  local moveInterval = SPEEDS[diff]
  local dt = 0

  while not st.result do
    if input.keyPress("KEY_ESCAPE") then
      while true do
        if input.keyPress("KEY_ESCAPE") then return
        elseif input.keyPress("KEY_ENTER") or input.keyPress("KEY_SPACE") then break end
        drawWorms(st)
        render.fillRect(0, 18, render.getWidth(), 30, Color(10, 10, 10))
        render.text(0, 22, "PAUSED\nENTER: CONTINUE\nESC: EXIT", COLOR_WHITE, render.getWidth(), 1, true)
        dt = yield()
      end
    end

    processWormsInput(st)
    st.moveTimer = st.moveTimer + dt
    while st.moveTimer >= moveInterval do
      st.moveTimer = st.moveTimer - moveInterval
      moveWorms(st)
      if st.result then break end
    end

    drawWorms(st)
    dt = yield()
  end

  audio.playSfx("gameover.wav")

  local resultText = ""
  if st.result == "p1win" then resultText = "PLAYER 1 WINS!"
  elseif st.result == "p2win" then resultText = "PLAYER 2 WINS!"
  else resultText = "DRAW!" end

  local blinkTimer = 0
  local dismissTimer = 0.5
  dt = 0
  while true do
    if dismissTimer <= 0 and (input.keyPress("KEY_ESCAPE") or input.keyPress("KEY_ENTER") or input.keyPress("KEY_SPACE")) then
      return
    end
    dismissTimer = dismissTimer - dt
    blinkTimer = blinkTimer + dt
    render.clear(Color(10, 10, 10))
    render.text(0, 22, resultText, COLOR_ACCENT, render.getWidth(), 1, true)
    render.text(0, 34, "LENGTH: " .. #st.worm1, COLOR_WHITE, render.getWidth(), 1, true)
    if math.floor(blinkTimer * 2) % 2 == 0 then
      render.text(0, 48, "PRESS ANY KEY", COLOR_TEXT_DIM, render.getWidth(), 1, true)
    end
    if blinkTimer >= 1.0 then blinkTimer = blinkTimer - 1.0 end
    dt = yield()
  end
end

local function runSettings()
  local appleLabels = { "Classic", "Alt" }
  local colorLabels = { "Classic", "Alt" }
  local dt = 0

  while true do
    local appleItem = "Apple: " .. appleLabels[settings.appleStyle]
    local colorItem = "Color: " .. colorLabels[settings.colorStyle]
    local c = showSubMenu("SETTINGS", { appleItem, colorItem, "Back" })
    if c < 0 or c == 3 then return end

    if c == 1 then
      local sc = showSubMenu("APPLE STYLE", { "Classic", "Alt", "Back" })
      if sc == 1 then settings.appleStyle = 1; saveSettings()
      elseif sc == 2 then settings.appleStyle = 2; saveSettings() end
    elseif c == 2 then
      local sc = showSubMenu("COLOR STYLE", { "Classic", "Alt", "Back" })
      if sc == 1 then settings.colorStyle = 1; saveSettings()
      elseif sc == 2 then settings.colorStyle = 2; saveSettings() end
    end
  end
end

function setup()
    math.randomseed(os.time())
    loadSettings()

    for style = 1, 2 do
        settings.colorStyle = style
        local cols = getColors()
        render.mapColor(Color(cols.head.r, cols.head.g, cols.head.b), 255)
        render.mapColor(Color(cols.head1.r, cols.head1.g, cols.head1.b), 255)
        render.mapColor(Color(cols.head2.r, cols.head2.g, cols.head2.b), 255)
        render.mapColor(Color(cols.tail.r, cols.tail.g, cols.tail.b), 127)
        render.mapColor(Color(cols.tail1.r, cols.tail1.g, cols.tail1.b), 127)
        render.mapColor(Color(cols.tail2.r, cols.tail2.g, cols.tail2.b), 127)
    end
    settings.colorStyle = 1

    render.mapColor(Color(10, 10, 10), 0)
    render.mapColor(Color(60, 60, 60), 255)
    render.mapColorRange(0, 0, 0, 255, 0, 255, 127)
end

function loop(dt)
  while true do
    local c = showSubMenu("SNAKE", { "Classic", "Worms", "Settings", "Quit" })
    if c < 0 or c == 4 then quit(); return end

    if c == 1 then
      local d = pickDifficulty()
      if d then runClassic(d) end
    elseif c == 2 then
      local d = pickDifficulty()
      if d then runWorms(d) end
    elseif c == 3 then
      runSettings()
    end
  end
end

function shutdown()
  saveSettings()
  render.clear(Color(10, 10, 10))
end

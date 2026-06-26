local ALIEN_COLS = 7
local ALIEN_ROWS = 4
local ALIEN_W = 6
local ALIEN_H = 5
local ALIEN_GAP = 1
local ALIEN_STEP_W = ALIEN_W + ALIEN_GAP
local ALIEN_STEP_H = ALIEN_H + ALIEN_GAP
local GRID_W = ALIEN_COLS * ALIEN_STEP_W
local GRID_H = ALIEN_ROWS * ALIEN_STEP_H
local GRID_X = math.floor((128 - GRID_W) / 2)
local GRID_Y = 8
local PLAYER_W = 10
local PLAYER_H = 4
local PLAYER_Y = 58
local BULLET_W = 2
local BULLET_H = 5

local DIFFICULTIES = {
  { name = "Easy",      lives = 5, alienMove = 0.6, alienShoot = 4.5, alienBullets = 2, alienBulletSpeed = 1, playerBulletSpeed = 3, speedup = 0.04, scoreMult = 0.5 },
  { name = "Normal",    lives = 3, alienMove = 0.5, alienShoot = 3.0, alienBullets = 3, alienBulletSpeed = 2, playerBulletSpeed = 3, speedup = 0.08, scoreMult = 1.0 },
  { name = "Hard",      lives = 2, alienMove = 0.35, alienShoot = 2.0, alienBullets = 4, alienBulletSpeed = 3, playerBulletSpeed = 3, speedup = 0.12, scoreMult = 2.0 },
  { name = "Insane",    lives = 1, alienMove = 0.2, alienShoot = 1.5, alienBullets = 5, alienBulletSpeed = 4, playerBulletSpeed = 2, speedup = 0.18, scoreMult = 3.0 },
  { name = "Nightmare", lives = 1, alienMove = 0.12, alienShoot = 0.8, alienBullets = 6, alienBulletSpeed = 5, playerBulletSpeed = 2, speedup = 0.25, scoreMult = 5.0 },
}

local ALIEN_POINTS = { 30, 20, 10, 10 }
local ALIEN_COLORS = {
  Color(255, 0, 0),
  Color(255, 165, 0),
  Color(255, 255, 0),
  Color(0, 255, 0),
}

local diff = nil
local player = {}
local aliens = {}
local pBullets = {}
local aBullets = {}
local score = 0
local highScore = 0
local lives = 3
local gameState = "playing"
local alienDir = 1
local alienMoveTimer = 0
local alienShootTimer = 0
local alienSpeedup = 1
local blinkTimer = 0
local gameOverTimer = 0
local diffIndex = 2

local function initAliens()
  aliens = {}
  for row = 0, ALIEN_ROWS - 1 do
    for col = 0, ALIEN_COLS - 1 do
      table.insert(aliens, {
        x = GRID_X + col * ALIEN_STEP_W,
        y = GRID_Y + row * ALIEN_STEP_H,
        w = ALIEN_W,
        h = ALIEN_H,
        row = row,
        alive = true,
      })
    end
  end
end

local function initPlayer()
  player = {
    x = math.floor((128 - PLAYER_W) / 2),
    y = PLAYER_Y,
    w = PLAYER_W,
    h = PLAYER_H,
    respawnTimer = 0,
  }
end

local function resetGame()
  diff = DIFFICULTIES[diffIndex]
  initAliens()
  initPlayer()
  pBullets = {}
  aBullets = {}
  score = 0
  lives = diff.lives
  alienDir = 1
  alienMoveTimer = 0
  alienShootTimer = 0
  alienSpeedup = 1
  blinkTimer = 0
  gameOverTimer = 0
end

local function countAliens()
  local n = 0
  for _, a in ipairs(aliens) do
    if a.alive then n = n + 1 end
  end
  return n
end

local function lowestAlienY()
  local ly = 0
  for _, a in ipairs(aliens) do
    if a.alive and a.y > ly then ly = a.y end
  end
  return ly
end

local function drawAlien(a)
  local c = ALIEN_COLORS[a.row + 1]
  render.fillRect(a.x, a.y, a.w, a.h, c)
  render.fillRect(a.x + 1, a.y + 1, 1, 1, COLOR_WHITE)
  render.fillRect(a.x + a.w - 2, a.y + 1, 1, 1, COLOR_WHITE)
  render.fillRect(a.x + 1, a.y + a.h - 2, 1, 1, COLOR_WHITE)
  render.fillRect(a.x + a.w - 2, a.y + a.h - 2, 1, 1, COLOR_WHITE)
end

local function drawPlayer()
  if player.respawnTimer > 0 then
    if math.floor(player.respawnTimer * 8) % 2 == 0 then return end
  end
  render.fillRect(player.x, player.y, player.w, player.h, Color(0, 255, 255))
  render.fillRect(player.x + 2, player.y - 1, player.w - 4, 1, Color(0, 255, 255))
  render.fillRect(player.x + 4, player.y - 2, player.w - 8, 1, Color(0, 200, 200))
end

local function drawBullets()
  for _, b in ipairs(pBullets) do
    render.fillRect(b.x, b.y, b.w, b.h, COLOR_WHITE)
  end
  for _, b in ipairs(aBullets) do
    render.fillRect(b.x, b.y, b.w, b.h, COLOR_RED)
  end
end

local function drawAliens()
  for _, a in ipairs(aliens) do
    if a.alive then drawAlien(a) end
  end
end

local function drawHUD()
  render.text(0, 0, "SC:" .. score, COLOR_WHITE)
  render.text(60, 0, "HI:" .. highScore, COLOR_ACCENT)
  render.text(106, 0, "L:" .. lives, COLOR_GREEN)
end

local function drawGameOver()
  render.clear(Color(10, 10, 10))
  render.text(0, 2, "GAME OVER", COLOR_RED, 128, 1, true)
  render.text(0, 14, "SCORE: " .. score, COLOR_WHITE, 128, 1, true)
  if score >= highScore and score > 0 then
    render.text(0, 24, "NEW HIGH SCORE!", COLOR_YELLOW, 128, 1, true)
  end
  if gameOverTimer <= 0 then
    if blinkTimer < 0.5 then
      render.text(0, 38, "ENTER TO PLAY", COLOR_WHITE, 128, 1, true)
    end
    if blinkTimer < 0.5 then
      render.text(0, 48, "ESC TO MENU", COLOR_GREY, 128, 1, true)
    end
  end
end

local function drawPlaying()
  render.clear(Color(10, 10, 10))
  render.line(0, 7, 128, 7, Color(40, 40, 40))
  drawHUD()
  drawAliens()
  drawBullets()
  drawPlayer()
end

local function moveAliens()
  local edgeHit = false
  for _, a in ipairs(aliens) do
    if a.alive then
      local nx = a.x + alienDir * 1
      if nx <= 0 or nx + a.w >= 128 then
        edgeHit = true
        break
      end
    end
  end
  if edgeHit then
    alienDir = -alienDir
    for _, a in ipairs(aliens) do
      if a.alive then
        a.y = a.y + ALIEN_STEP_H
      end
    end
  else
    for _, a in ipairs(aliens) do
      if a.alive then
        a.x = a.x + alienDir * 1
      end
    end
  end
end

local function alienShoot()
  local alive = {}
  for _, a in ipairs(aliens) do
    if a.alive then table.insert(alive, a) end
  end
  if #alive == 0 or #aBullets >= diff.alienBullets then return end
  local shooter = alive[math.random(1, #alive)]
  table.insert(aBullets, {
    x = shooter.x + math.floor(shooter.w / 2) - 1,
    y = shooter.y + shooter.h,
    w = BULLET_W,
    h = BULLET_H,
  })
end

local function updateBullets()
  for i = #pBullets, 1, -1 do
    local b = pBullets[i]
    b.y = b.y - diff.playerBulletSpeed
    if b.y + b.h <= 0 then
      table.remove(pBullets, i)
    else
      for _, a in ipairs(aliens) do
        if a.alive and b.x < a.x + a.w and b.x + b.w > a.x and b.y < a.y + a.h and b.y + b.h > a.y then
          a.alive = false
          score = score + math.floor(ALIEN_POINTS[a.row + 1] * diff.scoreMult)
          audio.playSfx("explode.wav")
          table.remove(pBullets, i)
          break
        end
      end
    end
  end
  for i = #aBullets, 1, -1 do
    local b = aBullets[i]
    b.y = b.y + diff.alienBulletSpeed
    if b.y >= 64 then
      table.remove(aBullets, i)
    elseif player.respawnTimer <= 0 and b.x < player.x + player.w and b.x + b.w > player.x and b.y < player.y + player.h and b.y + b.h > player.y then
      table.remove(aBullets, i)
      lives = lives - 1
      audio.playSfx("hit.wav")
      if lives <= 0 then
        if score > highScore then
          highScore = score
          save.write("spaceinvaders", tostring(highScore))
        end
        audio.playSfx("gameover.wav")
        gameState = "gameover"
        gameOverTimer = 1.0
      else
        player.respawnTimer = 1.5
        pBullets = {}
      end
    end
  end
end

local function showMenu(title, items)
  menu.create(title, items)
  local choice = 0
  while choice == 0 do
    choice = menu.tick()
    if choice == 0 then dt = yield() end
  end
  return choice
end

local function playGame()
  resetGame()
  gameState = "playing"

  while gameState == "playing" or gameState == "gameover" do
    if gameState == "gameover" then
      blinkTimer = blinkTimer + dt
      if blinkTimer >= 1.0 then blinkTimer = blinkTimer - 1.0 end
      if gameOverTimer > 0 then
        gameOverTimer = gameOverTimer - dt
        if gameOverTimer < 0 then gameOverTimer = 0 end
      end
      if gameOverTimer <= 0 then
        if input.keyPress("KEY_ENTER") or input.keyPress("KEY_SPACE") then
          resetGame()
          gameState = "playing"
        end
        if input.keyPress("KEY_ESCAPE") then return end
      end
      drawGameOver()
      dt = yield()
    end

    if gameState == "playing" then
      if player.respawnTimer > 0 then
        player.respawnTimer = player.respawnTimer - dt
        if player.respawnTimer < 0 then player.respawnTimer = 0 end
      end
      if input.keyPress("KEY_ESCAPE") then
        while true do
          if input.keyPress("KEY_ESCAPE") then
            if score > highScore then
              highScore = score
              save.write("spaceinvaders", tostring(highScore))
            end
            return
          elseif input.keyPress("KEY_ENTER") or input.keyPress("KEY_SPACE") then
            break
          end
          drawPlaying()
          render.fillRect(0, 20, 128, 24, Color(10, 10, 10))
          render.text(0, 24, "PAUSED\nENTER: CONTINUE\nESC: EXIT", COLOR_WHITE, 128, 1, true)
          dt = yield()
        end
      end
      if player.respawnTimer <= 0 then
        if input.keyHeld("KEY_LEFT") or input.keyHeld("KEY_A") then
          player.x = player.x - 2
          if player.x < 0 then player.x = 0 end
        end
        if input.keyHeld("KEY_RIGHT") or input.keyHeld("KEY_D") then
          player.x = player.x + 2
          if player.x + player.w > 128 then player.x = 128 - player.w end
        end
        if input.keyPress("KEY_SPACE") then
          if #pBullets < 1 then
            audio.playSfx("shoot.wav")
            table.insert(pBullets, {
              x = player.x + math.floor(player.w / 2) - 1,
              y = player.y - BULLET_H,
              w = BULLET_W,
              h = BULLET_H,
            })
          end
        end
      end

      local n = countAliens()
      if n == 0 then
        if score > highScore then
          highScore = score
          save.write("spaceinvaders", tostring(highScore))
        end
        audio.playSfx("gameover.wav")
        gameState = "gameover"
        gameOverTimer = 1.0
      else
        alienSpeedup = 1 + (ALIEN_ROWS * ALIEN_COLS - n) * diff.speedup
        alienMoveTimer = alienMoveTimer + dt
        if alienMoveTimer >= diff.alienMove / alienSpeedup then
          alienMoveTimer = 0
          moveAliens()
        end
        if lowestAlienY() + ALIEN_H >= player.y then
          if score > highScore then
            highScore = score
            save.write("spaceinvaders", tostring(highScore))
          end
          audio.playSfx("gameover.wav")
          gameState = "gameover"
          gameOverTimer = 1.0
        end
        alienShootTimer = alienShootTimer + dt
        local shootInterval = math.max(0.5, diff.alienShoot / alienSpeedup)
        if alienShootTimer >= shootInterval then
          alienShootTimer = 0
          alienShoot()
        end
        updateBullets()
      end

      drawPlaying()
      dt = yield()
    end
  end
end

function setup()
  math.randomseed(os.time())
  local s = save.read("spaceinvaders")
  if s ~= "" then highScore = tonumber(s) or 0 end
  diff = DIFFICULTIES[diffIndex]
  resetGame()
end

function loop(dt)
  while true do
    local c = showMenu("SPACE INVADERS", { "Play", "Quit" })
    if c < 0 or c == 2 then quit(); return end

    local dc = showMenu("DIFFICULTY", { "Easy", "Normal", "Hard", "Insane", "Nightmare", "Back" })
    if dc >= 1 and dc <= #DIFFICULTIES then
      diffIndex = dc
      playGame()
    end
  end
end

function shutdown()
  if score > highScore then
    save.write("spaceinvaders", tostring(score))
  end
  render.clear(Color(10, 10, 10))
end

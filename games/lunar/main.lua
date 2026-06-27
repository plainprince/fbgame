local GRAVITY = 18
local MAIN_THRUST = 55
local ROTATION_SPEED = 2.5
local MAX_FUEL = 100
local MAX_SAFE_VSPEED = 15
local MAX_SAFE_HSPEED = 10
local MAX_SAFE_ANGLE = 0.25
local PAD_WIDTH = 22
local HW = 3
local HH = 5
local HB = 1
local LEG = 3
local TERRAIN_STEP = 4

local ship = {}
local highScore
local gameState
local blinkTimer
local wasThrusting
local cameraX
local padX1, padX2, padY
local wallLeft, wallRight
local terrainSamples = {}

local function hash11(x)
  x = ((x >> 13) ~ x) * 0x45d9f3b
  x = ((x >> 13) ~ x) * 0x45d9f3b
  x = (x >> 13) ~ x
  return (x & 0x7fffffff) / 0x7fffffff
end

local function sampleHeight(wx)
  local key = math.floor(wx / TERRAIN_STEP) * TERRAIN_STEP
  if terrainSamples[key] then
    return terrainSamples[key]
  end
  local h
  if key >= padX1 and key <= padX2 then
    h = padY
  else
    local d = math.min(math.abs(key - padX1), math.abs(key - padX2))
    local r1 = hash11(key)
    local r2 = hash11(key * 7 + 12345)
    local rough = math.min(d / 200, 1)
    local amp = 2 + rough * 22
    local n = r1 - 0.5
    if n < 0 then
      h = padY + n * amp * 2.0
    else
      h = padY + n * amp * 0.6
    end
    h = h + (r2 - 0.5) * rough * 12
    if d < 32 then
      local blend = d / 32
      h = h * blend + padY * (1 - blend)
    end
    if h < 15 then h = 15 end
    if h > 62 then h = 62 end
  end
  terrainSamples[key] = h
  return h
end

local function getTerrainY(wx)
  local x0 = math.floor(wx / TERRAIN_STEP) * TERRAIN_STEP
  local x1 = x0 + TERRAIN_STEP
  local t = (wx - x0) / TERRAIN_STEP
  local h = sampleHeight(x0) + (sampleHeight(x1) - sampleHeight(x0)) * t

  if wx >= wallRight then
    local t = math.min((wx - wallRight) / 15, 1)
    local steep = t * t * (3 - 2 * t)
    h = h - (h + 100) * steep
  end
  if wx <= wallLeft then
    local t = math.min((wallLeft - wx) / 15, 1)
    local steep = t * t * (3 - 2 * t)
    h = h - (h + 100) * steep
  end

  return h
end

local function pruneTerrain()
  local keepMin = cameraX - 32
  local keepMax = cameraX + 160
  for k, _ in pairs(terrainSamples) do
    if k < keepMin or k > keepMax then
      terrainSamples[k] = nil
    end
  end
end

local function initShip()
  padX1 = math.random(35, 75)
  padX2 = padX1 + PAD_WIDTH
  padY = math.random(48, 54)
  wallLeft = -300
  wallRight = 300

  local padCenter = padX1 + PAD_WIDTH / 2
  local dir = (math.random() < 0.5) and 1 or -1
  local sx = padCenter + dir * (15 + math.random() * 15)
  if sx < 10 then sx = 10 end
  if sx > 118 then sx = 118 end

  ship = {
    x = sx, y = 12,
    vx = (math.random() - 0.5) * 4,
    vy = 0,
    angle = 0,
    fuel = MAX_FUEL,
    landed = false,
    crashed = false,
  }
  cameraX = ship.x - 64
end

local function initGame()
  terrainSamples = {}
  initShip()
  gameState = "playing"
  blinkTimer = 0
  wasThrusting = false
end

local function drawStars()
  local wxStart = math.floor(cameraX)
  for sx = 0, 127 do
    local wx = wxStart + sx
    if hash11(wx * 17 + 999) > 0.9 then
      local sy = math.floor(hash11(wx * 31 + 555) * 64)
      if sy >= 0 and sy < 64 then
        render.pixel(sx, sy, Color(60, 60, 60))
      end
    end
  end
end

local function drawTerrain()
  for sx = 0, 127 do
    local wx = sx + math.floor(cameraX)
    local wy = getTerrainY(wx)
    local y = math.floor(wy)
    if y < 0 then y = 0 end
    if y > 63 then y = 63 end
    render.fillRect(sx, y, 1, 64 - y, Color(80, 65, 40))
  end
  local on = math.floor(blinkTimer * 3) % 2 == 0
  local markerCol = on and Color(0, 255, 0) or Color(30, 30, 30)
  local padCol = on and Color(0, 200, 0) or Color(40, 60, 40)
  local psx1 = math.floor(padX1 - cameraX)
  local psx2 = math.floor(padX2 - cameraX)
  render.fillRect(psx1, padY, psx2 - psx1, 1, padCol)
  render.fillRect(psx1, padY - 1, 3, 2, markerCol)
  render.fillRect(psx2 - 2, padY - 1, 3, 2, markerCol)
end

local function drawShip()
  local sx = math.floor(ship.x - cameraX)
  local sy = math.floor(ship.y)
  local a = ship.angle
  local ca = math.cos(a)
  local sa = math.sin(a)

  if ship.crashed then
    render.line(sx - 4, sy - 4, sx + 4, sy + 4, Color(255, 80, 0))
    render.line(sx + 4, sy - 4, sx - 4, sy + 4, Color(255, 80, 0))
    render.fillCircle(sx, sy, 2, Color(255, 40, 0))
    return
  end

  local tipX = sx + math.floor(HH * sa)
  local tipY = sy - math.floor(HH * ca)
  local blX = sx + math.floor(-HW * ca - HB * sa)
  local blY = sy + math.floor(-HW * sa + HB * ca)
  local brX = sx + math.floor(HW * ca - HB * sa)
  local brY = sy + math.floor(HW * sa + HB * ca)
  local llX = sx + math.floor(-HW * ca - LEG * sa)
  local llY = sy + math.floor(-HW * sa + LEG * ca)
  local lrX = sx + math.floor(HW * ca - LEG * sa)
  local lrY = sy + math.floor(HW * sa + LEG * ca)

  if ship.landed then
    local bodyCol = Color(180, 180, 180)
    local legCol = Color(140, 140, 140)
    render.line(tipX, tipY, blX, blY, bodyCol)
    render.line(tipX, tipY, brX, brY, bodyCol)
    render.line(blX, blY, brX, brY, bodyCol)
    render.line(blX, blY, llX, llY, legCol)
    render.line(brX, brY, lrX, lrY, legCol)
    render.fillCircle(sx, sy - math.floor(HH * ca * 0.5), 1, Color(100, 200, 255))
    return
  end

  render.line(tipX, tipY, blX, blY, Color(255, 255, 255))
  render.line(tipX, tipY, brX, brY, Color(255, 255, 255))
  render.line(blX, blY, brX, brY, Color(255, 255, 255))
  render.line(blX, blY, llX, llY, Color(200, 200, 200))
  render.line(brX, brY, lrX, lrY, Color(200, 200, 200))
  render.fillCircle(sx, sy - math.floor(HH * ca * 0.5), 1, Color(100, 200, 255))

  local thrusting = input.keyHeld("KEY_UP") or input.keyHeld("KEY_W")
  local left = input.keyHeld("KEY_LEFT") or input.keyHeld("KEY_A")
  local right = input.keyHeld("KEY_RIGHT") or input.keyHeld("KEY_D")

  if thrusting and ship.fuel > 0 then
    local fuelScale = ship.fuel / MAX_FUEL
    local fl = 2 + math.random() * 5 * fuelScale
    local fbx = sx + math.floor(-HB * sa)
    local fby = sy + math.floor(HB * ca)
    local fex = sx + math.floor(-(HB + fl) * sa)
    local fey = sy + math.floor((HB + fl) * ca)
    render.line(fbx - 1, fby, fex - 1, fey, Color(255, 150, 0))
    render.line(fbx + 1, fby, fex + 1, fey, Color(255, 150, 0))
    render.line(fbx, fby, fex, fey, Color(255, 255, 100))
  end

  if left and ship.fuel > 0 then
    local sx2 = sx + math.floor(-HW * ca)
    local sy2 = sy + math.floor(-HW * sa)
    local l = 3
    render.line(sx2, sy2, sx2 - math.floor(l * ca), sy2 - math.floor(l * sa), Color(255, 150, 0))
  end
  if right and ship.fuel > 0 then
    local sx2 = sx + math.floor(HW * ca)
    local sy2 = sy + math.floor(HW * sa)
    local l = 3
    render.line(sx2, sy2, sx2 + math.floor(l * ca), sy2 + math.floor(l * sa), Color(255, 150, 0))
  end
end

local function drawHUD()
  local alt = math.floor(getTerrainY(ship.x) - (ship.y + 3))
  if alt < 0 then alt = 0 end
  render.text(0, 0, "F:" .. math.floor(ship.fuel), Color(180, 180, 180))
  render.text(92, 0, "A:" .. alt, Color(180, 180, 180))
  local vs = math.floor(ship.vy * 10) / 10
  local hs = math.floor(ship.vx * 10) / 10
  render.text(0, 9, "V:" .. vs, Color(180, 180, 180))
  render.text(92, 9, "H:" .. hs, Color(180, 180, 180))
end

function setup()
  math.randomseed(os.time())
  highScore = tonumber(save.read("lunar_high")) or 0
  render.mapColor(Color(10, 10, 10), 0)
  render.mapColor(Color(30, 30, 30), 40)
  render.mapColor(Color(80, 65, 40), 120)
  render.mapColor(Color(60, 60, 60), 100)
  render.mapColor(Color(140, 140, 140), 180)
  render.mapColor(Color(180, 180, 180), 220)
  render.mapColor(Color(200, 200, 200), 240)
  render.mapColor(Color(255, 255, 255), 255)
  render.mapColor(Color(255, 150, 0), 200)
  render.mapColor(Color(0, 200, 0), 220)
  render.mapColor(Color(255, 80, 0), 255)
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

local function runGame()
  audio.stopAll()
  initGame()
  local dt = 0

  while gameState == "playing" do
    if input.keyPress("KEY_ESCAPE") then
      local paused = true
      while paused do
        if input.keyPress("KEY_ESCAPE") then
          paused = false
          gameState = "menu"
          break
        elseif input.keyPress("KEY_ENTER") or input.keyPress("KEY_SPACE") then
          paused = false
        end
        render.clear(Color(10, 10, 10))
        drawStars()
        drawTerrain()
        drawShip()
        drawHUD()
        render.fillRect(0, 20, 128, 24, Color(10, 10, 10))
        render.text(0, 24, "PAUSED\nENTER:CONTINUE\nESC:EXIT", Color(180, 180, 180), 128, 1, true)
        dt = yield()
      end
      if gameState == "menu" then
        audio.stopAll()
        break
      end
    end

    blinkTimer = blinkTimer + dt

    local thrusting = input.keyHeld("KEY_UP") or input.keyHeld("KEY_W")
    local left = input.keyHeld("KEY_LEFT") or input.keyHeld("KEY_A")
    local right = input.keyHeld("KEY_RIGHT") or input.keyHeld("KEY_D")

    if left then
      ship.angle = ship.angle - ROTATION_SPEED * dt
    end
    if right then
      ship.angle = ship.angle + ROTATION_SPEED * dt
    end

    local ca = math.cos(ship.angle)
    local sa = math.sin(ship.angle)
    local ax, ay = 0, GRAVITY

    if thrusting then
      if ship.fuel > 0 then
        if not wasThrusting then
          audio.playMusic("thrust.wav")
        end
        ax = ax + MAIN_THRUST * sa
        ay = ay - MAIN_THRUST * ca
        ship.fuel = ship.fuel - 25 * dt
        if ship.fuel < 0 then ship.fuel = 0 end
      else
        if wasThrusting then
          audio.stopMusic()
        end
      end
    elseif wasThrusting then
      audio.stopMusic()
    end
    wasThrusting = thrusting

    ship.vx = ship.vx + ax * dt
    ship.vy = ship.vy + ay * dt
    ship.x = ship.x + ship.vx * dt
    ship.y = ship.y + ship.vy * dt

    if ship.y < 2 then ship.y = 2; ship.vy = 1 end
    if ship.y > 62 then ship.y = 62; ship.vy = -ship.vy * 0.3 end

    local bodyY = ship.y + HB
    local terrainAtCenter = getTerrainY(ship.x)
    if bodyY >= terrainAtCenter then
      ship.crashed = true
      gameState = "lose"
      audio.stopAll()
      audio.playSfx("explosion.wav")
    end

    if not ship.crashed then
      local pts = {
        { x = ship.x - HW * ca - LEG * sa, y = ship.y - HW * sa + LEG * ca, kind = "leg" },
        { x = ship.x + HW * ca - LEG * sa, y = ship.y + HW * sa + LEG * ca, kind = "leg" },
        { x = ship.x - HW * ca, y = ship.y - HW * sa, kind = "body" },
        { x = ship.x + HW * ca, y = ship.y + HW * sa, kind = "body" },
        { x = ship.x + HH * sa, y = ship.y - HH * ca, kind = "body" },
      }
      local anyIn = false
      local maxPush = 0
      for _, p in ipairs(pts) do
        local ty = getTerrainY(p.x)
        if p.y >= ty then
          anyIn = true
          local d = p.y - ty + 1
          if d > maxPush then maxPush = d end
          if p.kind == "body" then
            ship.crashed = true
            gameState = "lose"
            audio.stopAll()
            audio.playSfx("explosion.wav")
            break
          end
        end
      end

      if not ship.crashed and anyIn then
        ship.y = ship.y - maxPush

        local onPad = ship.x >= padX1 and ship.x <= padX2
        local level = math.abs(ship.angle) <= MAX_SAFE_ANGLE
        local safeV = math.abs(ship.vy) <= MAX_SAFE_VSPEED
        local safeH = math.abs(ship.vx) <= MAX_SAFE_HSPEED

        if level and safeV and safeH then
          ship.landed = true
          ship.vy = 0
          ship.vx = 0
          ship.angle = 0
          gameState = "win"
          audio.stopAll()
          audio.playSfx("land.wav")
        else
          ship.crashed = true
          gameState = "lose"
          audio.stopAll()
          audio.playSfx("explosion.wav")
        end
      end
    end

    cameraX = ship.x - 64
    pruneTerrain()

    render.clear(Color(10, 10, 10))
    drawStars()
    drawTerrain()
    drawShip()
    drawHUD()
    dt = yield()
  end
end

function loop(dt)
  while true do
    menu.create("LUNAR LANDER", { "Play", "Quit" })
    local c = 0
    while c == 0 do
      c = menu.tick()
      if c == 0 then
        render.text(0, 56, "BEST: " .. highScore, Color(100, 100, 100))
        dt = yield()
      end
    end
    if c < 0 or c == 2 then
      audio.stopAll()
      quit()
      return
    end
    if c == 1 then
      runGame()
      audio.stopAll()
      if gameState ~= "menu" then
        local score = 0
        local onPad = false
        if gameState == "win" then
          onPad = ship.x >= padX1 and ship.x <= padX2
          local padBonus = onPad and 50 or 0
          score = math.floor(ship.fuel * 10 + math.max(0, MAX_SAFE_VSPEED - math.abs(ship.vy)) * 3 + padBonus)
          if score > highScore then
            highScore = score
            save.write("lunar_high", tostring(highScore))
          end
        end
        local rDt = 0
        local rBlink = 0
        local dismiss = 0.5
        while true do
          if dismiss <= 0 and (input.keyPress("KEY_ESCAPE") or input.keyPress("KEY_ENTER") or input.keyPress("KEY_SPACE")) then
            break
          end
          dismiss = dismiss - rDt
          rBlink = rBlink + rDt
          render.clear(Color(10, 10, 10))
          if gameState == "win" then
            render.text(0, 8, "SUCCESSFUL LANDING!", Color(0, 200, 0), 128, 1, true)
            if onPad then
              render.text(0, 20, "ON PAD +50!", Color(255, 255, 100), 128, 1, true)
            end
            render.text(0, onPad and 32 or 22, "SCORE: " .. score, Color(180, 180, 180), 128, 1, true)
            if score >= highScore and score > 0 then
              render.text(0, 44, "NEW HIGH SCORE!", Color(255, 255, 100), 128, 1, true)
            end
          else
            render.text(0, 22, "CRASHED!", Color(255, 50, 0), 128, 1, true)
          end
          if math.floor(rBlink * 2) % 2 == 0 then
            render.text(0, 54, "PRESS ANY KEY", Color(100, 100, 100), 128, 1, true)
          end
          if rBlink >= 1.0 then rBlink = rBlink - 1.0 end
          rDt = yield()
        end
      end
    end
  end
end

function shutdown()
  audio.stopAll()
  render.clear(Color(10, 10, 10))
end

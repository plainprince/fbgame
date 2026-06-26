local COLS = 28
local ROWS = 15
local CELL = 4
local OX = math.floor((128 - COLS * CELL) / 2)
local OY = 4
local TUNNEL_ROW = 8
local SPR = "games/pacman/sprites/"

local DIR_NONE, DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT = 0, 1, 2, 3, 4
local DX = { [0]=0, [1]=0, [2]=0, [3]=-1, [4]=1 }
local DY = { [0]=0, [1]=-1, [2]=1, [3]=0, [4]=0 }
local OPP = { [0]=0, [1]=2, [2]=1, [3]=4, [4]=3 }
local ALL_DIRS = { DIR_UP, DIR_LEFT, DIR_DOWN, DIR_RIGHT }
local DIR_ORDER = { DIR_UP, DIR_LEFT, DIR_DOWN, DIR_RIGHT }

local SCATTER, CHASE, FRIGHTENED, EATEN, IN_HOUSE = 0, 1, 2, 3, 4

local PAC_SPEED = 3.5
local GHOST_SPEED = 2.8
local FRIGHT_SPEED = 1.5
local EATEN_SPEED = 5.0
local MODE_TIMERS = { 7, 20, 7, 20, 5, 20, 5, 999 }
local FRIGHT_TIME = 6.0
local RELEASE_TIMES = { 0, 2, 5, 8 }
local GHOST_NAMES = { "blinky", "pinky", "inky", "clyde" }
local SCATTER_POS = { { x=25, y=0 }, { x=2, y=0 }, { x=27, y=14 }, { x=0, y=14 } }

local map = {}
local pellets = {}
local pelletCount = 0
local score = 0
local lives = 3
local level = 1
local highScore = 0
local state = "menu"
local modeIdx = 1
local modeTimer = 0
local mode = SCATTER
local frightTimer = 0
local frightFlash = false
local combo = 0
local mouthAnim = 0

local pac = {}
local ghosts = {}

local function at(gx, gy)
  if gy < 0 or gy >= ROWS then return 1 end
  if gx < 0 or gx >= COLS then return (gy == TUNNEL_ROW and 0 or 1) end
  return map[gy+1][gx+1]
end

local function walkable(gx, gy, isGhost)
  local t = at(gx, gy)
  if isGhost then return t ~= 1 and t ~= 4 end
  return t ~= 1 and t ~= 4 and t ~= 5
end

local function cloneMap(src)
  local m = {}
  for y = 1, ROWS do
    m[y] = {}
    for x = 1, COLS do m[y][x] = src[y][x] end
  end
  return m
end

local function floodFillCount(m, sx, sy)
  if sx < 0 or sx >= COLS or sy < 0 or sy >= ROWS then return 0 end
  if m[sy+1][sx+1] ~= 0 then return 0 end
  local visited = {}
  local count = 0
  local queue = { { x=sx, y=sy } }
  while #queue > 0 do
    local p = table.remove(queue, 1)
    local key = p.x .. "," .. p.y
    if not visited[key] then
      visited[key] = true
      count = count + 1
      for _, d in ipairs(DIR_ORDER) do
        local nx = p.x + DX[d]
        local ny = p.y + DY[d]
        if ny == TUNNEL_ROW then
          if nx < 0 then nx = COLS - 1
          elseif nx >= COLS then nx = 0 end
        end
        if nx >= 0 and nx < COLS and ny >= 0 and ny < ROWS then
          if m[ny+1][nx+1] == 0 then
            table.insert(queue, { x=nx, y=ny })
          end
        end
      end
    end
  end
  return count
end

local function generateMap()
  local TEMPLATE = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,1,1,0,1,1,0,1,1,0,1,1,1,1,1,1,0,1,1,0,1,1,0,1,1,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,1,1,0,1,1,0,1,1,0,1,1,1,1,1,1,0,1,1,0,1,1,0,1,1,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,1,1,0,1,1,0,1,1,0,1,1,0,0,1,1,0,1,1,0,1,1,0,1,1,0,1},
    {1,0,1,1,0,1,1,0,1,1,0,4,4,5,5,4,4,0,1,1,0,1,1,0,1,1,0,1},
    {0,0,0,0,0,0,0,0,0,0,0,4,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0},
    {1,0,1,1,0,1,1,0,1,1,0,4,4,4,4,4,4,0,1,1,0,1,1,0,1,1,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,1,1,0,1,1,0,1,1,0,1,1,1,1,1,1,0,1,1,0,1,1,0,1,1,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,0,1,1,0,1,1,0,1,1,0,1,1,1,1,1,1,0,1,1,0,1,1,0,1,1,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
  }

  local m = cloneMap(TEMPLATE)
  local removable = {}
  for y = 1, ROWS do
    for x = 1, COLS do
      if m[y][x] == 0 then
        removable[#removable+1] = { x=x-1, y=y-1 }
      end
    end
  end

  local protected = { ["14,12"]=true, ["13,7"]=true, ["13,8"]=true, ["14,8"]=true, ["15,8"]=true }
  for i = #removable, 1, -1 do
    if math.random() < 0.05 then
      local c = removable[i]
      if not protected[c.x..","..c.y] and (c.y ~= TUNNEL_ROW or (c.x < 6 or c.x > 21)) then
        local blocked = 0
        for _, d in ipairs(DIR_ORDER) do
          local nx, ny = c.x + DX[d], c.y + DY[d]
          if nx >= 0 and nx < COLS and ny >= 0 and ny < ROWS and m[ny+1][nx+1] == 1 then blocked = blocked + 1 end
        end
        if blocked >= 2 then m[c.y+1][c.x+1] = 1 end
      end
    end
  end

  local openCount = 0
  for y = 1, ROWS do
    for x = 1, COLS do
      if m[y][x] == 0 then openCount = openCount + 1 end
    end
  end

  local fills = floodFillCount(m, 14, 10)
  if fills < openCount * 0.8 then
    m = cloneMap(TEMPLATE)
  end

  map = m
end

local function placePellets()
  pellets = {}
  pelletCount = 0
  for y = 1, ROWS do
    pellets[y] = {}
    for x = 1, COLS do
      local px, py = x - 1, y - 1
      if map[y][x] == 0 then
        if py == TUNNEL_ROW then
          pellets[y][x] = 0
        elseif (px == 1 or px == 26) and (py == 3 or py == 12) then
          pellets[y][x] = 3; pelletCount = pelletCount + 1
        else
          pellets[y][x] = 2; pelletCount = pelletCount + 1
        end
      else
        pellets[y][x] = 0
      end
    end
  end
end

local function initPac()
  pac = { gx=14, gy=12, dir=DIR_NONE, ndir=DIR_NONE, prog=0 }
end

local function initGhosts()
  ghosts = {}
  local spawns = { {13,7}, {13,8}, {14,8}, {15,8} }
  for i = 1, 4 do
    ghosts[i] = {
      gx=spawns[i][1], gy=spawns[i][2],
      dir=DIR_UP, prog=0,
      mode=(i==1 and CHASE or IN_HOUSE),
      prev=SCATTER,
      releaseTimer=RELEASE_TIMES[i],
      frightTimer=0,
      scatter=SCATTER_POS[i],
      name=GHOST_NAMES[i],
    }
  end
end

local function gx(g) return OX + g.gx * CELL end
local function gy(g) return OY + g.gy * CELL end

local function pacTarget(ahead)
  local d = pac.dir
  if d == DIR_NONE then d = DIR_LEFT end
  return { x = pac.gx + DX[d] * ahead, y = pac.gy + DY[d] * ahead }
end

local function ghostTarget(i)
  local g = ghosts[i]
  if g.mode == SCATTER then return g.scatter
  elseif g.mode == EATEN then return { x=13, y=8 }
  elseif g.mode == FRIGHTENED then return { x=0, y=0 }
  end
  if i == 1 then return { x=pac.gx, y=pac.gy } end
  if i == 2 then return pacTarget(4) end
  if i == 3 then
    local t = pacTarget(2)
    return { x = t.x + (t.x - ghosts[1].gx), y = t.y + (t.y - ghosts[1].gy) }
  end
  if i == 4 then
    local d = math.abs(pac.gx - g.gx) + math.abs(pac.gy - g.gy)
    return (d > 8 and { x=pac.gx, y=pac.gy } or g.scatter)
  end
  return { x=0, y=0 }
end

local function otherGhostPositions(skipIdx)
  local pos = {}
  for j = 1, 4 do
    if j ~= skipIdx then
      local gj = ghosts[j]
      pos[gj.gx..","..gj.gy] = true
    end
  end
  return pos
end

local function ghostPath(i)
  local g = ghosts[i]
  local target = ghostTarget(i)
  local occupied = otherGhostPositions(i)

  if g.mode == FRIGHTENED then
    local valid = {}
    for _, d in ipairs(ALL_DIRS) do
      if d ~= OPP[g.dir] and walkable(g.gx + DX[d], g.gy + DY[d], true) then
        valid[#valid+1] = d
      end
    end
    if #valid == 0 then
      for _, d in ipairs(ALL_DIRS) do
        if walkable(g.gx + DX[d], g.gy + DY[d], true) then valid[#valid+1] = d end
      end
    end
    local free, occ = {}, {}
    for _, d in ipairs(valid) do
      local k = (g.gx + DX[d])..","..(g.gy + DY[d])
      if occupied[k] then occ[#occ+1] = d else free[#free+1] = d end
    end
    local pool = #free > 0 and free or occ
    return #pool > 0 and pool[math.random(#pool)] or g.dir
  end

  local visited = {}
  local prev = {}
  local q = { { x=g.gx, y=g.gy } }
  visited[g.gx..","..g.gy] = true
  local found = nil

  while #q > 0 and not found do
    local cur = table.remove(q, 1)
    if cur.x == target.x and cur.y == target.y then found = cur; break end
    for _, d in ipairs(DIR_ORDER) do
      local nx = cur.x + DX[d]
      local ny = cur.y + DY[d]
      if ny == TUNNEL_ROW then
        if nx < 0 then nx = COLS - 1 elseif nx >= COLS then nx = 0 end
      end
      local k = nx..","..ny
      if not visited[k] and walkable(nx, ny, true) then
        visited[k] = true
        prev[k] = { x=cur.x, y=cur.y, dir=d }
        if nx == target.x and ny == target.y then
          found = { x=nx, y=ny }
          break
        end
        table.insert(q, { x=nx, y=ny })
      end
    end
  end

  if found then
    local cx, cy = found.x, found.y
    while true do
      local pk = cx..","..cy
      local p = prev[pk]
      if not p then break end
      if p.x == g.gx and p.y == g.gy then
        if p.dir ~= OPP[g.dir] then
          local nk = (g.gx + DX[p.dir])..","..(g.gy + DY[p.dir])
          if not occupied[nk] then return p.dir end
        end
        break
      end
      cx, cy = p.x, p.y
    end
  end

  local dirs = {}
  for _, d in ipairs(ALL_DIRS) do
    if d ~= OPP[g.dir] then
      local nx, ny = g.gx + DX[d], g.gy + DY[d]
      if ny == TUNNEL_ROW then
        if nx < 0 then nx = COLS - 1 elseif nx >= COLS then nx = 0 end
      end
      if walkable(nx, ny, true) then
        local penalty = occupied[nx..","..ny] and 100 or 0
        local dd = math.abs(nx - target.x) + math.abs(ny - target.y) + penalty
        dirs[#dirs+1] = { d=d, dd=dd }
      end
    end
  end
  table.sort(dirs, function(a, b) return a.dd < b.dd end)
  return #dirs > 0 and dirs[1].d or g.dir
end

local function tickEntity(e, speed, dt, isGhost, getDirFn)
  e.prog = e.prog + speed * dt
  while e.prog >= 1 do
    e.prog = e.prog - 1
    if e.dir ~= DIR_NONE then
      e.gx = e.gx + DX[e.dir]
      e.gy = e.gy + DY[e.dir]
      if e.gy == TUNNEL_ROW then
        if e.gx < 0 then e.gx = COLS - 1 elseif e.gx >= COLS then e.gx = 0 end
      end
    end
    if getDirFn then
      local nd = getDirFn()
      if nd then e.dir = nd end
    end
    if e.dir ~= DIR_NONE and not walkable(e.gx + DX[e.dir], e.gy + DY[e.dir], isGhost) then
      e.dir = DIR_NONE; e.prog = 0
    end
  end
end

local function pacDir()
  if walkable(pac.gx + DX[pac.ndir], pac.gy + DY[pac.ndir], false) then
    return pac.ndir
  end
  if pac.dir ~= DIR_NONE and walkable(pac.gx + DX[pac.dir], pac.gy + DY[pac.dir], false) then
    return pac.dir
  end
  return nil
end

local function updatePac(dt)
  if pac.dir == DIR_NONE and pac.ndir ~= DIR_NONE then
    if walkable(pac.gx + DX[pac.ndir], pac.gy + DY[pac.ndir], false) then
      pac.dir = pac.ndir
    end
  end
  tickEntity(pac, PAC_SPEED, dt, false, pacDir)
  local px, py = pac.gx + 1, pac.gy + 1
  if px >= 1 and px <= COLS and py >= 1 and py <= ROWS then
    local p = pellets[py][px]
    if p == 2 then
      pellets[py][px] = 0; pelletCount = pelletCount - 1; score = score + 10
      audio.playSfx("chomp.wav")
    elseif p == 3 then
      pellets[py][px] = 0; pelletCount = pelletCount - 1; score = score + 50
      frightTimer = FRIGHT_TIME; combo = 0; audio.playSfx("powerup.wav")
      for _, g in ipairs(ghosts) do
        if g.mode == CHASE or g.mode == SCATTER then
          g.prev = g.mode; g.mode = FRIGHTENED; g.frightTimer = FRIGHT_TIME; g.dir = OPP[g.dir]
        end
      end
    end
  end
end

local function updateGhosts(dt)
  for i = 1, 4 do
    local g = ghosts[i]
    if g.mode == IN_HOUSE then
      g.releaseTimer = g.releaseTimer - dt
      if g.releaseTimer <= 0 then g.mode = CHASE; g.dir = DIR_UP; g.prog = 0 end
    end
    if g.mode == FRIGHTENED then
      g.frightTimer = g.frightTimer - dt
      if g.frightTimer <= 0 then g.mode = g.prev end
    end
    if g.mode ~= IN_HOUSE then
      local spd = (g.mode == EATEN and EATEN_SPEED or (g.mode == FRIGHTENED and FRIGHT_SPEED or GHOST_SPEED))
      tickEntity(g, spd, dt, true, function() return ghostPath(i) end)
      if g.mode == EATEN and g.gx == 13 and g.gy == 8 then
        g.mode = CHASE; g.dir = DIR_UP; g.prog = 0
      end
    end
  end
end

local function pixelPos(e)
  local px = OX + e.gx * CELL + DX[e.dir] * e.prog * CELL
  local py = OY + e.gy * CELL + DY[e.dir] * e.prog * CELL
  return px, py
end

local function checkCollision()
  local px, py = pixelPos(pac)
  for _, g in ipairs(ghosts) do
    if g.mode ~= IN_HOUSE and g.mode ~= EATEN then
      local gx, gy = pixelPos(g)
      if math.abs(px - gx) < CELL and math.abs(py - gy) < CELL then
        if g.mode == FRIGHTENED then
          g.mode = EATEN; g.dir = OPP[g.dir]; combo = combo + 1
          score = score + 200 * math.pow(2, combo - 1)
          audio.playSfx("ghost_eat.wav")
        else
          return true
        end
      end
    end
  end
  return false
end

local function modeTick(dt)
  if frightTimer > 0 then
    frightTimer = frightTimer - dt
    if frightTimer < 0 then frightTimer = 0 end
    frightFlash = frightTimer < 2 and math.floor(frightTimer * 5) % 2 == 0
    return
  end
  modeTimer = modeTimer + dt
  if modeTimer >= MODE_TIMERS[modeIdx] then
    modeTimer = 0; modeIdx = math.min(modeIdx + 1, #MODE_TIMERS)
    mode = (mode == SCATTER and CHASE or SCATTER)
    for _, g in ipairs(ghosts) do
      if g.mode == CHASE or g.mode == SCATTER then g.mode = mode; g.dir = OPP[g.dir] end
    end
  end
end

local function resetGame()
  score = 0; lives = 3; level = 1; combo = 0; mouthAnim = 0; frightTimer = 0
  modeIdx = 1; modeTimer = 0; mode = SCATTER
  generateMap()
  placePellets()
  initPac()
  initGhosts()
end

local function nextLevel()
  level = level + 1; combo = 0; mouthAnim = 0
  modeIdx = 1; modeTimer = 0; mode = SCATTER; frightTimer = 0
  generateMap()
  placePellets()
  initPac()
  initGhosts()
end

function drawMap()
  for y = 1, ROWS do
    for x = 1, COLS do
      local v = map[y][x]
      local px = OX + (x - 1) * CELL
      local py = OY + (y - 1) * CELL
      if v == 1 then
        render.sprite(px, py, SPR .. "wall.spr")
      elseif v == 4 then
        render.sprite(px, py, SPR .. "hwall.spr")
      elseif v == 5 then
        render.sprite(px, py, SPR .. "gate.spr")
      end
    end
  end
  for y = 1, ROWS do
    for x = 1, COLS do
      local p = pellets[y][x]
      if p == 2 then
        render.sprite(OX + (x-1)*CELL + 1, OY + (y-1)*CELL + 1, SPR .. "pellet.spr")
      elseif p == 3 then
        render.sprite(OX + (x-1)*CELL, OY + (y-1)*CELL, SPR .. "powerpellet.spr")
      end
    end
  end
end

local PACMAN_OPEN = {
  [DIR_RIGHT] = "pacman_r.spr",
  [DIR_LEFT] = "pacman_l.spr",
  [DIR_UP] = "pacman_u.spr",
  [DIR_DOWN] = "pacman_d.spr",
}

function drawPac()
  local px = OX + pac.gx * CELL
  local py = OY + pac.gy * CELL
  mouthAnim = mouthAnim + 1
  local open = math.abs(math.sin(mouthAnim * 0.15)) > 0.3 and pac.dir ~= DIR_NONE
  if open then
    render.sprite(px, py, SPR .. PACMAN_OPEN[pac.dir])
  else
    render.sprite(px, py, SPR .. "pacman.spr")
  end
end

function drawGhost(i)
  local g = ghosts[i]
  local px = OX + g.gx * CELL
  local py = OY + g.gy * CELL
  if g.mode == EATEN then
    render.sprite(px, py, SPR .. "eyes.spr")
    return
  end
  if g.mode == FRIGHTENED then
    local spr = frightFlash and "eyes.spr" or "fright.spr"
    render.sprite(px, py, SPR .. spr)
    return
  end
  render.sprite(px, py, SPR .. g.name .. ".spr")
end

function drawHUD()
  render.text(0, 0, "S:" .. score, Color(255, 255, 255))
  render.text(80, 0, "L:" .. level, Color(255, 255, 255))
  for i = 1, lives - 1 do
    render.fillRect(100 + (i-1) * 10, 1, 8, 3, Color(255, 255, 0))
  end
end

function drawPlaying()
  render.clear(Color(10, 10, 10))
  drawMap()
  for i = 1, 4 do drawGhost(i) end
  drawPac()
  drawHUD()
end

function setup()
  math.randomseed(os.time())
  local s = save.read("pacman")
  if s ~= "" then highScore = tonumber(s) or 0 end
  generateMap()
  placePellets()
  initPac()
  initGhosts()
end

function loop(dt)
  while true do
    if state == "menu" then
      render.clear(Color(10, 10, 10))
      render.text(0, 8, "PAC-MAN", Color(255, 255, 0), 128, 1, true)
      render.text(0, 22, "HIGH: " .. highScore, Color(255, 255, 255), 128, 1, true)
      render.text(0, 36, "PRESS ENTER", Color(255, 255, 255), 128, 1, true)
      if input.keyPress("KEY_ENTER") or input.keyPress("KEY_SPACE") then
        resetGame(); state = "ready"
      end
      if input.keyPress("KEY_ESCAPE") then quit(); return end
    end

    if state == "ready" then
      audio.playSfx("levelstart.wav")
      local t = 1.5
      while t > 0 do
        t = t - dt
        render.clear(Color(10, 10, 10))
        render.text(0, 20, "LEVEL " .. level, Color(255, 255, 255), 128, 1, true)
        render.text(0, 34, "READY!", Color(255, 255, 0), 128, 1, true)
        dt = yield()
      end
      state = "playing"
    end

    if state == "playing" then
      if input.keyPress("KEY_ESCAPE") then
        while true do
          if input.keyPress("KEY_ESCAPE") then state = "menu"; break end
          if input.keyPress("KEY_ENTER") or input.keyPress("KEY_SPACE") then break end
          drawPlaying()
          render.fillRect(0, 20, 128, 32, Color(10, 10, 10))
          render.text(0, 24, "PAUSED\nENTER: CONT\nESC: EXIT", Color(255, 255, 255), 128, 1, true)
          dt = yield()
        end
      end

      if input.keyPress("KEY_UP") or input.keyPress("KEY_W") then pac.ndir = DIR_UP
      elseif input.keyPress("KEY_DOWN") or input.keyPress("KEY_S") then pac.ndir = DIR_DOWN
      elseif input.keyPress("KEY_LEFT") or input.keyPress("KEY_A") then pac.ndir = DIR_LEFT
      elseif input.keyPress("KEY_RIGHT") or input.keyPress("KEY_D") then pac.ndir = DIR_RIGHT
      end

      modeTick(dt)
      updatePac(dt)
      updateGhosts(dt)

      if checkCollision() then
        lives = lives - 1
        audio.playSfx("death.wav")
        if lives <= 0 then
          if score > highScore then highScore = score; save.write("pacman", tostring(highScore)) end
          audio.playSfx("gameover.wav")
          state = "gameover"
        else
          initPac()
          state = "ready"
        end
      end

      if pelletCount <= 0 then
        nextLevel()
        state = "ready"
      end

      drawPlaying()
    end

    if state == "gameover" then
      local fade = 1.0
      while fade > 0 do
        fade = fade - dt
        drawPlaying()
        render.fillRect(0, 0, 128, 64, Color(10, 10, 10))
        render.text(0, 20, "GAME OVER", Color(255, 0, 0), 128, 1, true)
        render.text(0, 34, "SCORE: " .. score, Color(255, 255, 255), 128, 1, true)
        if score >= highScore and score > 0 then
          render.text(0, 46, "NEW HIGH!", Color(255, 255, 0), 128, 1, true)
        end
        dt = yield()
      end
      while true do
        if input.keyPress("KEY_ENTER") or input.keyPress("KEY_SPACE") then
          resetGame(); state = "ready"; audio.playSfx("levelstart.wav"); break
        end
        if input.keyPress("KEY_ESCAPE") then state = "menu"; break end
        render.clear(Color(10, 10, 10))
        render.text(0, 8, "GAME OVER", Color(255, 0, 0), 128, 1, true)
        render.text(0, 24, "SCORE: " .. score, Color(255, 255, 255), 128, 1, true)
        if score >= highScore and score > 0 then
          render.text(0, 36, "NEW HIGH!", Color(255, 255, 0), 128, 1, true)
        end
        render.text(0, 48, "ENTER: PLAY\nESC: MENU", Color(255, 255, 255), 128, 1, true)
        dt = yield()
      end
    end

    dt = yield()
  end
end

function shutdown()
  if score > highScore then save.write("pacman", tostring(score)) end
  render.clear(Color(10, 10, 10))
end

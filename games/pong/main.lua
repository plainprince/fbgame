local PADDLE_W = 3
local PADDLE_H = 12
local PADDLE_MARGIN = 4
local PADDLE_SPEED = 55
local BALL_SIZE = 3
local WIN_SCORE = 5
local MAX_ANGLE_RAD = 55 * math.pi / 180

local BALL_SPEEDS = { [1] = 30, [2] = 48, [3] = 70 }
local AI_CONFIGS = {
  [1] = { speed = 15 },
  [2] = { speed = 35 },
  [3] = { speed = 55, predict = true },
}

local ballX, ballY, ballVX, ballVY
local paddle1Y, paddle2Y
local score1, score2
local gameOver, gameWinner
local aiTimer
local blinkTimer, dismissTimer
local speedMult, rallyCount
local aiConfig

local trailStore = {}
local trailDrawPositions = {}
local TRAIL_COLORS = { Color(204, 204, 204), Color(153, 153, 153), Color(102, 102, 102), Color(63, 63, 63), Color(38, 38, 38), Color(20, 20, 20) }

function setup()
  math.randomseed(os.time())
  render.setFPS(30)
  render.mapColor(Color(10, 10, 10), 0)
  render.mapColor(Color(255, 255, 255), 255)
  render.mapColor(Color(40, 40, 40), 128)
  render.mapColor(Color(204, 204, 204), 0)
  render.mapColor(Color(153, 153, 153), 0)
  render.mapColor(Color(102, 102, 102), 0)
  render.mapColor(Color(63, 63, 63), 0)
  render.mapColor(Color(38, 38, 38), 0)
  render.mapColor(Color(20, 20, 20), 0)
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

local function resetBall(dir)
  ballX = 64 - math.floor(BALL_SIZE / 2)
  ballY = 32 - math.floor(BALL_SIZE / 2)
  if not dir then dir = math.random() < 0.5 and 1 or -1 end
  local angleDeg = (math.random() * 60 - 30)
  local angleRad = angleDeg * math.pi / 180
  speedMult = 1.0
  rallyCount = 0
  local spd = BALL_SPEEDS[diff] * speedMult
  ballVX = dir * spd * math.cos(angleRad)
  ballVY = spd * math.sin(angleRad)
end

local function resetGame()
  paddle1Y = 26
  paddle2Y = 26
  score1 = 0
  score2 = 0
  gameOver = false
  gameWinner = ""
  aiTimer = 0
  blinkTimer = 0
  dismissTimer = 0.5
  trailStore = {}
  trailDrawPositions = {}
  resetBall()
end

local function drawCenterLine()
  for y = 0, 60, 8 do
    render.fillRect(63, y, 2, 4, Color(40, 40, 40))
  end
end

local function drawPaddles()
  render.fillRect(PADDLE_MARGIN, math.floor(paddle1Y + 0.5), PADDLE_W, PADDLE_H, COLOR_WHITE)
  render.fillRect(128 - PADDLE_MARGIN - PADDLE_W, math.floor(paddle2Y + 0.5), PADDLE_W, PADDLE_H, COLOR_WHITE)
end

local function drawBallFn()
  for i = #trailDrawPositions, 1, -1 do
    local p = trailDrawPositions[i]
    render.fillRect(math.floor(p.x + 0.5), math.floor(p.y + 0.5), BALL_SIZE, BALL_SIZE, TRAIL_COLORS[i])
  end
  render.fillRect(math.floor(ballX + 0.5), math.floor(ballY + 0.5), BALL_SIZE, BALL_SIZE, COLOR_WHITE)
end

local function drawScores()
  render.text(30, 0, tostring(score1), COLOR_WHITE, 20, 1, true)
  render.text(78, 0, tostring(score2), COLOR_WHITE, 20, 1, true)
end

local function drawGame()
  render.clear(Color(10, 10, 10))
  drawCenterLine()
  drawPaddles()
  drawBallFn()
  drawScores()
end

local function drawGameOverScreen()
  render.clear(Color(10, 10, 10))
  render.text(30, 8, tostring(score1), COLOR_WHITE, 20, 1, true)
  render.text(78, 8, tostring(score2), COLOR_WHITE, 20, 1, true)
  render.text(0, 22, gameWinner, COLOR_ACCENT, 128, 1, true)
  if math.floor(blinkTimer * 2) % 2 == 0 then
    render.text(0, 40, "PRESS ANY KEY", COLOR_TEXT_DIM, 128, 1, true)
  end
end

local function clampPaddle(paddleId)
  if paddleId == 1 then
    if paddle1Y < 0 then paddle1Y = 0 end
    if paddle1Y + PADDLE_H > 64 then paddle1Y = 64 - PADDLE_H end
  else
    if paddle2Y < 0 then paddle2Y = 0 end
    if paddle2Y + PADDLE_H > 64 then paddle2Y = 64 - PADDLE_H end
  end
end

local function handleP1Input(dt)
  if input.keyHeld("KEY_W") then
    paddle1Y = paddle1Y - PADDLE_SPEED * dt
    clampPaddle(1)
  end
  if input.keyHeld("KEY_S") then
    paddle1Y = paddle1Y + PADDLE_SPEED * dt
    clampPaddle(1)
  end
  if mode == 1 then
    if input.keyHeld("KEY_UP") then
      paddle1Y = paddle1Y - PADDLE_SPEED * dt
      clampPaddle(1)
    end
    if input.keyHeld("KEY_DOWN") then
      paddle1Y = paddle1Y + PADDLE_SPEED * dt
      clampPaddle(1)
    end
  end
end

local function handleP2Input(dt)
  if input.keyHeld("KEY_UP") then
    paddle2Y = paddle2Y - PADDLE_SPEED * dt
    clampPaddle(2)
  end
  if input.keyHeld("KEY_DOWN") then
    paddle2Y = paddle2Y + PADDLE_SPEED * dt
    clampPaddle(2)
  end
end

local function reflect(y, min, max)
  local range = max - min
  if range <= 0 then return min end
  local off = y - min
  local wrapped = off % (range * 2)
  if wrapped < 0 then wrapped = wrapped + range * 2 end
  if wrapped <= range then return min + wrapped end
  return max - (wrapped - range)
end

local function handleAI(dt)
  local ballCenterY = ballY + BALL_SIZE / 2
  local paddleCenter = paddle2Y + PADDLE_H / 2

  local targetY = ballCenterY

  if aiConfig.predict and ballVX > 0 then
    local dx = (128 - PADDLE_MARGIN - PADDLE_W) - ballX
    if dx > 0 and math.abs(ballVX) > 1 then
      local t = dx / ballVX
      local half = BALL_SIZE / 2
      targetY = reflect(ballCenterY + ballVY * t, half, 64 - half)
    end
  end

  local diffY = targetY - paddleCenter
  if math.abs(diffY) > 1 then
    local step = aiConfig.speed * dt
    if diffY > 0 then
      paddle2Y = paddle2Y + step
    else
      paddle2Y = paddle2Y - step
    end
    clampPaddle(2)
  end
end

local function paddleHit(paddleX, paddleY)
  audio.playSfx("hit.wav")
  local paddleCenterY = paddleY + PADDLE_H / 2
  local ballCenterY = ballY + BALL_SIZE / 2
  local relY = (ballCenterY - paddleCenterY) / (PADDLE_H / 2)
  if relY < -1 then relY = -1 end
  if relY > 1 then relY = 1 end
  local angle = relY * MAX_ANGLE_RAD
  local spd = BALL_SPEEDS[diff] * speedMult
  if spd > BALL_SPEEDS[3] * 2.5 then spd = BALL_SPEEDS[3] * 2.5 end
  if paddleX < 64 then
    ballVX = spd * math.cos(angle)
  else
    ballVX = -spd * math.cos(angle)
  end
  ballVY = spd * math.sin(angle)
  rallyCount = rallyCount + 1
  speedMult = 1.0 + rallyCount * 0.04
end

local function checkPaddleCollision()
  if ballVX < 0 then
    if ballX <= PADDLE_MARGIN + PADDLE_W and ballX + BALL_SIZE >= PADDLE_MARGIN then
      if ballY + BALL_SIZE > paddle1Y and ballY < paddle1Y + PADDLE_H then
        ballX = PADDLE_MARGIN + PADDLE_W
        paddleHit(PADDLE_MARGIN, paddle1Y)
      end
    end
  else
    local rpx = 128 - PADDLE_MARGIN - PADDLE_W
    if ballX + BALL_SIZE >= rpx and ballX <= rpx + PADDLE_W then
      if ballY + BALL_SIZE > paddle2Y and ballY < paddle2Y + PADDLE_H then
        ballX = rpx - BALL_SIZE
        paddleHit(rpx, paddle2Y)
      end
    end
  end
end

local function checkWallCollision()
  if ballY <= 0 then
    ballY = 0
    ballVY = -ballVY
    audio.playSfx("wall.wav")
  elseif ballY + BALL_SIZE >= 64 then
    ballY = 64 - BALL_SIZE
    ballVY = -ballVY
    audio.playSfx("wall.wav")
  end
end

local function checkScoring()
  if ballX + BALL_SIZE < 0 then
    score2 = score2 + 1
    if score2 >= WIN_SCORE then
      if mode == 1 then gameWinner = "YOU LOSE!" else gameWinner = "PLAYER 2 WINS!" end
      gameOver = true
      audio.playSfx("gameover.wav")
    else
      audio.playSfx("score.wav")
      resetBall(1)
    end
    return true
  elseif ballX > 128 then
    score1 = score1 + 1
    if score1 >= WIN_SCORE then
      if mode == 1 then gameWinner = "YOU WIN!" else gameWinner = "PLAYER 1 WINS!" end
      gameOver = true
      audio.playSfx("gameover.wav")
    else
      audio.playSfx("score.wav")
      resetBall(-1)
    end
    return true
  end
  return false
end

local function updateBall(dt)
  local maxMove = math.max(math.abs(ballVX), math.abs(ballVY)) * dt
  local steps = math.max(1, math.ceil(maxMove / 1.5))
  local subDt = dt / steps
  for _ = 1, steps do
    ballX = ballX + ballVX * subDt
    ballY = ballY + ballVY * subDt
    checkWallCollision()
    checkPaddleCollision()
    if checkScoring() then break end
  end
  table.insert(trailStore, 1, { x = ballX, y = ballY })
  if #trailStore > 40 then
    table.remove(trailStore)
  end
  trailDrawPositions = {}
  local si, fromX, fromY = 2, ballX, ballY
  while #trailDrawPositions < 6 and si <= #trailStore do
    local toX, toY = trailStore[si].x, trailStore[si].y
    local dx, dy = toX - fromX, toY - fromY
    local seg = math.sqrt(dx * dx + dy * dy)
    if seg > 0 then
      local nx, ny = dx / seg, dy / seg
      local step = 1
      while step <= seg and #trailDrawPositions < 6 do
        table.insert(trailDrawPositions, { x = fromX + nx * step, y = fromY + ny * step })
        step = step + 1
      end
    end
    fromX, fromY = toX, toY
    si = si + 1
  end
end

local function runSingleplayer()
  mode = 1
  aiConfig = AI_CONFIGS[diff]
  resetGame()
  local dt = 0
  while not gameOver do
    if input.keyPress("KEY_ESCAPE") then
      while true do
        if input.keyPress("KEY_ESCAPE") then return
        elseif input.keyPress("KEY_ENTER") or input.keyPress("KEY_SPACE") then break end
        drawGame()
        render.fillRect(0, 20, 128, 24, Color(10, 10, 10))
        render.text(0, 24, "PAUSED\nENTER: CONTINUE\nESC: EXIT", COLOR_WHITE, 128, 1, true)
        dt = yield()
      end
    end
    handleP1Input(dt)
    handleAI(dt)
    updateBall(dt)
    drawGame()
    dt = yield()
  end
  blinkTimer = 0
  dismissTimer = 0.5
  dt = 0
  while true do
    if dismissTimer <= 0 and (input.keyPress("KEY_ESCAPE") or input.keyPress("KEY_ENTER") or input.keyPress("KEY_SPACE")) then
      return
    end
    dismissTimer = dismissTimer - dt
    blinkTimer = blinkTimer + dt
    drawGameOverScreen()
    if blinkTimer >= 1.0 then blinkTimer = blinkTimer - 1.0 end
    dt = yield()
  end
end

local function runMultiplayer()
  mode = 2
  resetGame()
  local dt = 0
  while not gameOver do
    if input.keyPress("KEY_ESCAPE") then
      while true do
        if input.keyPress("KEY_ESCAPE") then return
        elseif input.keyPress("KEY_ENTER") or input.keyPress("KEY_SPACE") then break end
        drawGame()
        render.fillRect(0, 20, 128, 24, Color(10, 10, 10))
        render.text(0, 24, "PAUSED\nENTER: CONTINUE\nESC: EXIT", COLOR_WHITE, 128, 1, true)
        dt = yield()
      end
    end
    handleP1Input(dt)
    handleP2Input(dt)
    updateBall(dt)
    drawGame()
    dt = yield()
  end
  blinkTimer = 0
  dismissTimer = 0.5
  dt = 0
  while true do
    if dismissTimer <= 0 and (input.keyPress("KEY_ESCAPE") or input.keyPress("KEY_ENTER") or input.keyPress("KEY_SPACE")) then
      return
    end
    dismissTimer = dismissTimer - dt
    blinkTimer = blinkTimer + dt
    drawGameOverScreen()
    if blinkTimer >= 1.0 then blinkTimer = blinkTimer - 1.0 end
    dt = yield()
  end
end

function loop(dt)
  while true do
    local c = showMenu("PONG", { "Singleplayer", "Multiplayer", "Quit" })
    if c < 0 or c == 3 then quit(); return end
    if c == 1 then
      local dc = showMenu("AI DIFFICULTY", { "Easy", "Medium", "Hard", "Back" })
      if dc >= 1 and dc <= 3 then diff = dc; runSingleplayer() end
    elseif c == 2 then
      local dc = showMenu("BALL SPEED", { "Slow", "Normal", "Fast", "Back" })
      if dc >= 1 and dc <= 3 then diff = dc; runMultiplayer() end
    end
  end
end

function shutdown()
  render.clear(Color(10, 10, 10))
end

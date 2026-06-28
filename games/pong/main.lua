local PADDLE_W = 3
local PADDLE_H = 12
local PADDLE_MARGIN = 4
local PADDLE_SPEED = 55
local BALL_SIZE = 3
local MAX_ANGLE_RAD = 55 * math.pi / 180

local SCORING_FIRST = 1
local SCORING_COUNTDOWN = 2
local SCORING_FOREVER = 3

local BALL_SPEEDS = { [1] = 30, [2] = 48, [3] = 70, [4] = 100, [5] = 100, [6] = 100 }
local AI_CONFIGS = {
  [1] = { speed = 15 },
  [2] = { speed = 35 },
  [3] = { speed = 48, predict = true },
  [4] = { speed = PADDLE_SPEED, predict = true, preReflect = true, aggressive = true },
  [5] = { speed = PADDLE_SPEED, predict = true, preReflect = true },
  [6] = { rl = true, speed = PADDLE_SPEED },
}

local ballX, ballY, ballVX, ballVY
local paddle1Y, paddle2Y
local score1, score2
local gameOver, gameWinner
local aiTimer
local blinkTimer, dismissTimer
local speedMult, rallyCount
local aiConfig
local diff1, diff2

local rl_net
local rl_net2
local rl_paddle
local rl_path = "games/pong/rl_weights.dat"
local rl_path2 = "games/pong/rl_weights2.dat"

local scoringMode = SCORING_FIRST
local winScore = 5
local countdownDuration = 180
local countdownRemaining = 0
local ballHitCount = 0

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

  if type(rl) == "table" and rl.new then
    rl_net = rl.new()
    rl_net2 = rl.new()
    if rl_net then rl_net:load(rl_path) end
    if rl_net2 then rl_net2:load(rl_path2) end
  end
end

local function rlObs(paddleId)
  local ey = paddleId == 1 and paddle2Y or paddle1Y
  return {
    ballX / 128, ballY / 64, ballVX / 100, ballVY / 100,
    (paddleId == 1 and paddle1Y or paddle2Y) / 64, ey / 64
  }
end

local function rlAct(paddleId)
  local net = paddleId == 1 and rl_net or rl_net2
  if not net then return 1 end
  local obs = rlObs(paddleId)
  local ok, action = pcall(function() return rl.act(net, obs) end)
  if not ok then return 1 end
  return action
end

local function rlTrainFrame(paddleId, reward)
  local net = paddleId == 1 and rl_net or rl_net2
  if not net then return end
  local obs = rlObs(paddleId)
  pcall(function() rl.trainFrame(net, reward, obs) end)
end

local function rlSave()
  if not rl_net then return end
  pcall(function() rl.save(rl_net, rl_path) end)
  pcall(function() rl.save(rl_net2, rl_path2) end)
end

local function formatTime(seconds)
  return math.floor(seconds / 60) .. ":" .. string.format("%02d", seconds % 60)
end

local function scoringLabel()
  if scoringMode == SCORING_FIRST then return "First to " .. winScore
  elseif scoringMode == SCORING_COUNTDOWN then return "Countdown " .. formatTime(countdownDuration)
  else return "Forever" end
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
  paddle1Y = 26
  paddle2Y = 26
  if not dir then dir = math.random() < 0.5 and 1 or -1 end
  local angleDeg = (math.random() * 60 - 30)
  local angleRad = angleDeg * math.pi / 180
  speedMult = 1.0
  rallyCount = 0
  ballHitCount = 0
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
  ballHitCount = 0
  rl_paddle = nil
  if scoringMode == SCORING_COUNTDOWN then
    countdownRemaining = countdownDuration
  end
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

local function drawHUD()
  if scoringMode == SCORING_COUNTDOWN then
    local rem = math.max(0, math.ceil(countdownRemaining))
    render.text(0, 1, formatTime(rem), COLOR_ACCENT, 128, 1, true)
  end
  render.text(0, 56, tostring(ballHitCount), COLOR_TEXT_DIM, 128, 1, true)
end

local function drawGame()
  render.clear(Color(10, 10, 10))
  drawCenterLine()
  drawPaddles()
  drawBallFn()
  drawScores()
  drawHUD()
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

local function simulateBounce(ballCX, ballCY, ballVX, ballVY, hitterId, receiverId, hitterSpeed, receiverSpeed, depth, aiId)
  local half = BALL_SIZE / 2
  local halfPad = PADDLE_H / 2
  local recvEdge = receiverId == 1 and (PADDLE_MARGIN + PADDLE_W) or (128 - PADDLE_MARGIN - PADDLE_W)
  local dx = math.abs(recvEdge - ballCX)
  if dx <= 0 or math.abs(ballVX) <= 1 then return 0 end
  local t = dx / math.abs(ballVX)
  local arrivalCY = reflect(ballCY + ballVY * t, half, 64 - half)

  if depth <= 0 then
    local recvY = receiverId == 1 and paddle1Y or paddle2Y
    local recvCenter = recvY + halfPad
    local reach = receiverSpeed * t
    local dist = math.abs(arrivalCY - recvCenter)
    local canReach = dist <= reach + halfPad
    if receiverId == aiId then
      return canReach and 1 or 0
    else
      return (not canReach) and 1 or 0
    end
  end

  local recvY = receiverId == 1 and paddle1Y or paddle2Y
  local recvReach = receiverSpeed * t
  local minCenter = math.max(halfPad, arrivalCY - halfPad - half + 1)
  local maxCenter = math.min(64 - halfPad, arrivalCY + halfPad + half - 1)
  local totalCount = 0
  local scoreSum = 0

  local yPos = math.ceil(minCenter)
  while yPos <= maxCenter do
    local distToPos = math.abs(yPos - (recvY + halfPad))
    if distToPos <= recvReach + halfPad then
      totalCount = totalCount + 1
      local relY = (arrivalCY - yPos) / halfPad
      if relY < -1 then relY = -1 end
      if relY > 1 then relY = 1 end
      local angle = relY * MAX_ANGLE_RAD
      local spd = BALL_SPEEDS[diff] * speedMult
      local newVX = (recvEdge < 64) and spd * math.cos(angle) or -spd * math.cos(angle)
      local newVY = spd * math.sin(angle)
      local hitEdge = hitterId == 1 and (PADDLE_MARGIN + PADDLE_W) or (128 - PADDLE_MARGIN - PADDLE_W)
      local returnDx = math.abs(hitEdge - recvEdge)
      local returnT = returnDx / math.abs(newVX)
      local ballYatHitter = reflect(arrivalCY + newVY * returnT, half, 64 - half)
      local subScore = simulateBounce(recvEdge, ballYatHitter, newVX, newVY, receiverId, hitterId, receiverSpeed, hitterSpeed, depth - 1, aiId)
      scoreSum = scoreSum + subScore
    end
    yPos = yPos + 2
  end

  if totalCount == 0 then return 1.0 end
  return scoreSum / totalCount
end

local function handleAI(paddleId, config, dt, enemyPaddleId, enemySpeed)
  local ballCenterY = ballY + BALL_SIZE / 2
  local paddleY = paddleId == 1 and paddle1Y or paddle2Y
  local paddleCenter = paddleY + PADDLE_H / 2

  local targetY = ballCenterY

  if config.rl then
    rl_paddle = paddleId
    local obs = rlObs(paddleId)
    local ballCenterY = ballY + BALL_SIZE / 2
    local myCenter = paddleId == 1 and paddle1Y + PADDLE_H/2 or paddle2Y + PADDLE_H/2
    local dist = math.abs(ballCenterY - myCenter)
    local reward = -dist * 0.002
    if ballVX > 0 and paddleId == 2 then reward = reward + 0.005 end
    if ballVX < 0 and paddleId == 1 then reward = reward + 0.005 end
    local ok, action = pcall(function() return rl.trainFrame(net, reward, obs) end)
    if not ok then action = 1 end
    if action == 0 then
      targetY = paddleY - config.speed * dt
    elseif action == 2 then
      targetY = paddleY + config.speed * dt
    else
      targetY = paddleY
    end
    local diffY = targetY - paddleCenter
    if math.abs(diffY) > 1 then
      local step = config.speed * dt
      if diffY > 0 then
        if paddleId == 1 then paddle1Y = paddle1Y + step else paddle2Y = paddle2Y + step end
      else
        if paddleId == 1 then paddle1Y = paddle1Y - step else paddle2Y = paddle2Y - step end
      end
      clampPaddle(paddleId)
    end
    return
  end

  if config.predict then
    local ballMovingToward = (paddleId == 1 and ballVX < 0) or (paddleId == 2 and ballVX > 0)
    if ballMovingToward then
      local paddleEdge = paddleId == 1 and (PADDLE_MARGIN + PADDLE_W) or (128 - PADDLE_MARGIN - PADDLE_W)
      local dx = math.abs(paddleEdge - ballX)
      if dx > 0 and math.abs(ballVX) > 1 then
        local t = dx / math.abs(ballVX)
        local half = BALL_SIZE / 2
        local arrivalBallCenter = reflect(ballCenterY + ballVY * t, half, 64 - half)

        if config.preReflect and config.aggressive then
          local enemyEdgeX = enemyPaddleId == 1 and (PADDLE_MARGIN + PADDLE_W) or (128 - PADDLE_MARGIN - PADDLE_W)

          local halfPad = PADDLE_H / 2
          local minCenter = math.max(halfPad, arrivalBallCenter - halfPad - half + 1)
          local maxCenter = math.min(64 - halfPad, arrivalBallCenter + halfPad + half - 1)

          local aiReachNow = config.speed * t

          local depth = math.max(1, math.min(3, 5 - math.floor(dt * 100)))

          local cornerTop = math.ceil(arrivalBallCenter - halfPad)
          local cornerBot = math.floor(arrivalBallCenter + halfPad)

          local function evalShot(yPos)
            local relY = (arrivalBallCenter - yPos) / halfPad
            if relY < -1 then relY = -1 end
            if relY > 1 then relY = 1 end
            local angle = relY * MAX_ANGLE_RAD
            local mySpd = BALL_SPEEDS[diff] * speedMult
            local myVX = (paddleId == 1) and mySpd * math.cos(angle) or -mySpd * math.cos(angle)
            local myVY = mySpd * math.sin(angle)
            return simulateBounce(paddleEdge, arrivalBallCenter, myVX, myVY, paddleId, enemyPaddleId, config.speed, enemySpeed, depth, paddleId)
          end

          local safeCorner = nil
          local corners = {cornerTop, cornerBot}
          for _, cy in ipairs(corners) do
            if cy >= minCenter and cy <= maxCenter then
              local s = evalShot(cy)
              if s >= 0.85 then safeCorner = cy; break end
            end
          end

          if safeCorner then
            targetY = safeCorner
          else
            local bestCenter = arrivalBallCenter
            local bestScore = -math.huge
            local yPos = math.ceil(math.max(minCenter, paddleCenter - aiReachNow))
            local yEnd = math.floor(math.min(maxCenter, paddleCenter + aiReachNow))

            while yPos <= yEnd do
              local score = evalShot(yPos)
              if score > bestScore then
                bestScore = score
                bestCenter = yPos
              end
              yPos = yPos + 2
            end
            targetY = bestCenter
          end
        else
          targetY = arrivalBallCenter
        end
      end
    elseif config.preReflect then
      local enemyPaddleYVal = enemyPaddleId == 1 and paddle1Y or paddle2Y
      local enemyEdgeX = enemyPaddleId == 1 and (PADDLE_MARGIN + PADDLE_W) or (128 - PADDLE_MARGIN - PADDLE_W)
      local dx = math.abs(enemyEdgeX - ballX)
      if dx > 0 and math.abs(ballVX) > 1 then
        local t = dx / math.abs(ballVX)
        local half = BALL_SIZE / 2
        local predictedBallY = ballCenterY + ballVY * t
        local clampedPredictedY = reflect(predictedBallY, half, 64 - half)

        local enemyReachMin = math.max(0, enemyPaddleYVal - enemySpeed * t)
        local enemyReachMax = math.min(64 - PADDLE_H, enemyPaddleYVal + enemySpeed * t)

        local hitYPositions = {}
        local yStart = math.ceil(enemyReachMin)
        local yEnd = math.floor(enemyReachMax)
        for yPos = yStart, yEnd, 2 do
          if clampedPredictedY + half > yPos and clampedPredictedY - half < yPos + PADDLE_H then
            local paddleCenterY = yPos + PADDLE_H / 2
            local relY = (predictedBallY - paddleCenterY) / (PADDLE_H / 2)
            if relY < -1 then relY = -1 end
            if relY > 1 then relY = 1 end
            local angle = relY * MAX_ANGLE_RAD
            local spd = BALL_SPEEDS[diff] * speedMult
            local newVX = (enemyEdgeX < 64) and spd * math.cos(angle) or -spd * math.cos(angle)
            local newVY = spd * math.sin(angle)

            local ourEdgeX = paddleId == 1 and (PADDLE_MARGIN + PADDLE_W) or (128 - PADDLE_MARGIN - PADDLE_W)
            local returnDx = math.abs(ourEdgeX - enemyEdgeX)
            local returnT = returnDx / math.abs(newVX)
            local finalY = reflect(clampedPredictedY + newVY * returnT, half, 64 - half)
            table.insert(hitYPositions, finalY)
          end
        end
        if #hitYPositions > 0 then
          local sum = 0
          for _, v in ipairs(hitYPositions) do sum = sum + v end
          targetY = sum / #hitYPositions
        else
          targetY = ballCenterY
        end
      end
    end
  end

  local diffY = targetY - paddleCenter
  if math.abs(diffY) > 1 then
    local step = config.speed * dt
    if diffY > 0 then
      if paddleId == 1 then paddle1Y = paddle1Y + step else paddle2Y = paddle2Y + step end
    else
      if paddleId == 1 then paddle1Y = paddle1Y - step else paddle2Y = paddle2Y - step end
    end
    clampPaddle(paddleId)
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
  if paddleX < 64 then
    ballVX = spd * math.cos(angle)
  else
    ballVX = -spd * math.cos(angle)
  end
  ballVY = spd * math.sin(angle)
  rallyCount = rallyCount + 1
  speedMult = 1.0 + rallyCount * 0.04
  ballHitCount = ballHitCount + 1
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
    onPoint(2)
    if scoringMode == SCORING_FIRST and score2 >= winScore then
      if mode == 1 then gameWinner = "YOU LOSE!"
      elseif mode == 3 then gameWinner = "AI 2 WINS!"
      else gameWinner = "PLAYER 2 WINS!" end
      gameOver = true
      audio.playSfx("gameover.wav")
    else
      audio.playSfx("score.wav")
      resetBall(1)
    end
    return true
  elseif ballX > 128 then
    score1 = score1 + 1
    onPoint(1)
    if scoringMode == SCORING_FIRST and score1 >= winScore then
      if mode == 1 then gameWinner = "YOU WIN!"
      elseif mode == 3 then gameWinner = "AI 1 WINS!"
      else gameWinner = "PLAYER 1 WINS!" end
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

function onPoint(scoringPaddle)
  if not rl_net then return end
  if rl_paddle then
    if scoringPaddle == rl_paddle then
      rlTrainFrame(rl_paddle, 1.0)
    elseif scoringPaddle ~= 0 then
      local other = scoringPaddle == 1 and 2 or 1
      if other == rl_paddle then
        rlTrainFrame(rl_paddle, -1.0)
      end
    end
  end
  if diff1 == 6 and scoringPaddle ~= 0 then
    rlTrainFrame(1, scoringPaddle == 1 and 1.0 or -1.0)
  end
  if diff2 == 6 and scoringPaddle ~= 0 then
    rlTrainFrame(2, scoringPaddle == 2 and 1.0 or -1.0)
  end
  rlSave()
end

local function updateBall(dt)
  local maxMove = math.max(math.abs(ballVX), math.abs(ballVY)) * dt
  local steps = math.max(1, math.ceil(maxMove / 1.0))
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

local function chooseSettings()
  local c = showMenu("SCORING", { "First to", "Countdown", "Forever", "Cancel" })
  if c < 0 or c == 4 then return end
  if c == 1 then
    local n = showMenu("FIRST TO", { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "Cancel" })
    if n >= 1 and n <= 10 then winScore = n; scoringMode = SCORING_FIRST end
  elseif c == 2 then
    local n = showMenu("COUNTDOWN", { "1:00", "3:00", "5:00", "10:00", "Cancel" })
    local durations = { [1] = 60, [2] = 180, [3] = 300, [4] = 600 }
    if n >= 1 and n <= 4 then countdownDuration = durations[n]; scoringMode = SCORING_COUNTDOWN end
  elseif c == 3 then
    scoringMode = SCORING_FOREVER
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
    handleAI(2, aiConfig, dt, 1, PADDLE_SPEED)
    updateBall(dt)
    if scoringMode == SCORING_COUNTDOWN then
      countdownRemaining = countdownRemaining - dt
      if countdownRemaining <= 0 then
        gameOver = true
        if score1 > score2 then gameWinner = "YOU WIN!"
        elseif score2 > score1 then gameWinner = "YOU LOSE!"
        else gameWinner = "DRAW!" end
        audio.playSfx("gameover.wav")
      end
    end
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
    if scoringMode == SCORING_COUNTDOWN then
      countdownRemaining = countdownRemaining - dt
      if countdownRemaining <= 0 then
        gameOver = true
        if score1 > score2 then gameWinner = "PLAYER 1 WINS!"
        elseif score2 > score1 then gameWinner = "PLAYER 2 WINS!"
        else gameWinner = "DRAW!" end
        audio.playSfx("gameover.wav")
      end
    end
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

local function runAivsAI()
  mode = 3
  diff = math.max(diff1, diff2)
  local aiConfig1 = AI_CONFIGS[diff1]
  local aiConfig2 = AI_CONFIGS[diff2]
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
    handleAI(1, aiConfig1, dt, 2, AI_CONFIGS[diff2].speed)
    handleAI(2, aiConfig2, dt, 1, AI_CONFIGS[diff1].speed)
    updateBall(dt)
    if scoringMode == SCORING_COUNTDOWN then
      countdownRemaining = countdownRemaining - dt
      if countdownRemaining <= 0 then
        gameOver = true
        if score1 > score2 then gameWinner = "AI 1 WINS!"
        elseif score2 > score1 then gameWinner = "AI 2 WINS!"
        else gameWinner = "DRAW!" end
        audio.playSfx("gameover.wav")
      end
    end
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
    local c = showMenu("PONG", { "Singleplayer", "Multiplayer", "AI vs AI", "Scoring: " .. scoringLabel(), "Quit" })
    if c < 0 or c == 5 then quit(); return end
    if c == 1 then
      local dc = showMenu("AI DIFFICULTY", { "Easy", "Medium", "Hard", "Impossible offensive", "Impossible defensive", "RL", "Back" })
      if dc >= 1 and dc <= 6 then diff = dc; runSingleplayer() end
    elseif c == 2 then
      local dc = showMenu("BALL SPEED", { "Slow", "Normal", "Fast", "Defensive", "Impossible", "Back" })
      if dc >= 1 and dc <= 5 then diff = dc; runMultiplayer() end
    elseif c == 3 then
      while true do
        local dc = showMenu("AI 1 DIFFICULTY", { "Easy", "Medium", "Hard", "Impossible offensive", "Impossible defensive", "RL", "Back" })
        if dc < 1 or dc > 6 then break end
        diff1 = dc
        local dc2 = showMenu("AI 2 DIFFICULTY", { "Easy", "Medium", "Hard", "Impossible offensive", "Impossible defensive", "RL", "Back" })
        if dc2 >= 1 and dc2 <= 6 then diff2 = dc2; runAivsAI(); break end
      end
    elseif c == 4 then
      chooseSettings()
    end
  end
end

function shutdown()
  rlSave()
  render.clear(Color(10, 10, 10))
end

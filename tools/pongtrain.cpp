#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <sys/stat.h>
#include <thread>
#include <vector>

static std::atomic<bool> trainingCancelled{false};

extern "C" void handleSigInt(int) {
    trainingCancelled.store(true, std::memory_order_relaxed);
}

// ====== RL constants and types ======
#define RL_INPUTS 6
#define RL_ACTIONS 3
#define RL_BINS_X 8
#define RL_BINS_Y 8
#define RL_BINS_V 3
#define RL_BINS_P 8
#define RL_STATES (RL_BINS_X * RL_BINS_Y * RL_BINS_V * RL_BINS_V * RL_BINS_P)
#define RL_ALPHA 0.2f
#define RL_GAMMA 0.9f

struct RLNet {
    float q[RL_STATES][RL_ACTIONS];
    float epsilon;
    int lastState;
    int lastAction;
};

static void rlInit(RLNet* net) {
    net->epsilon = 0.5f;
    net->lastState = -1;
    net->lastAction = 1;
    for (int s = 0; s < RL_STATES; s++)
        for (int a = 0; a < RL_ACTIONS; a++)
            net->q[s][a] = 1.0f;
}

static int rlEncodeState(const float* in) {
    int bx = (int)std::floor(in[0] * (RL_BINS_X - 1));
    if (bx < 0) bx = 0;
    if (bx >= RL_BINS_X) bx = RL_BINS_X - 1;
    int by = (int)std::floor(in[1] * (RL_BINS_Y - 1));
    if (by < 0) by = 0;
    if (by >= RL_BINS_Y) by = RL_BINS_Y - 1;
    int vx = in[2] > 0.5f ? 2 : (in[2] < -0.5f ? 0 : 1);
    int vy = in[3] > 0.5f ? 2 : (in[3] < -0.5f ? 0 : 1);
    int py = (int)std::floor(in[4] * (RL_BINS_P - 1));
    if (py < 0) py = 0;
    if (py >= RL_BINS_P) py = RL_BINS_P - 1;
    int v = (bx * RL_BINS_Y + by) * RL_BINS_V + vx;
    return v * RL_BINS_V * RL_BINS_P + vy * RL_BINS_P + py;
}

static int rlTrainFrame(RLNet* net, float reward, const float* in) {
    int newState = rlEncodeState(in);
    int lastState = net->lastState;
    int lastAction = net->lastAction;
    if (lastState >= 0) {
        float maxNextQ = net->q[newState][0];
        for (int a = 1; a < RL_ACTIONS; a++)
            if (net->q[newState][a] > maxNextQ) maxNextQ = net->q[newState][a];
        float oldQ = net->q[lastState][lastAction];
        net->q[lastState][lastAction] = oldQ + RL_ALPHA * (reward + RL_GAMMA * maxNextQ - oldQ);
    }
    int action;
    if ((float)rand() / (float)RAND_MAX < net->epsilon) {
        action = rand() % RL_ACTIONS;
    } else {
        float bestQ = net->q[newState][0];
        int bestCount = 1;
        int bestActions[RL_ACTIONS] = {0};
        for (int a = 1; a < RL_ACTIONS; a++) {
            if (net->q[newState][a] > bestQ) {
                bestQ = net->q[newState][a];
                bestCount = 1;
                bestActions[0] = a;
            } else if (net->q[newState][a] == bestQ) {
                bestActions[bestCount++] = a;
            }
        }
        action = bestActions[rand() % bestCount];
    }
    net->lastState = newState;
    net->lastAction = action;
    return action;
}

static void rlSave(const RLNet* net, const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) { fprintf(stderr, "rlSave: fopen %s failed\n", path); return; }
    fwrite(net, sizeof(RLNet), 1, f);
    fclose(f);
}

static void ensureDir(const char* path) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", path);
    for (char* p = buf; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(buf, 0755);
            *p = '/';
        }
    }
}

static bool rlLoad(RLNet* net, const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    bool ok = fread(net, sizeof(RLNet), 1, f) == 1;
    fclose(f);
    return ok;
}

// ====== Pong constants ======
static const int PADDLE_W = 3;
static const int PADDLE_H = 12;
static const int PADDLE_MARGIN = 4;
static const int PADDLE_SPEED = 55;
static const int BALL_SIZE = 3;
static const float MAX_ANGLE_RAD = 55.0f * (float)M_PI / 180.0f;
static const float BALL_SPEEDS[7] = {0, 30, 48, 70, 100, 100, 100};

static const int FIELD_W = 128;

// ====== Pong simulation ======
struct PongState {
    float ballX, ballY, ballVX, ballVY;
    float paddle1Y, paddle2Y;
    int score1, score2;
    int rallyCount;
    float speedMult;
    int diff;
    int winScore;
    bool gameOver;

    void reset() {
        paddle1Y = 26.0f;
        paddle2Y = 26.0f;
        score1 = 0;
        score2 = 0;
        gameOver = false;
        diff = 6;
        winScore = 5;
        resetBall(0);
    }

    void resetBall(int dir) {
        ballX = 64.0f - BALL_SIZE / 2.0f;
        ballY = 32.0f - BALL_SIZE / 2.0f;
        paddle1Y = 26.0f;
        paddle2Y = 26.0f;
        if (dir == 0) dir = (rand() % 2 == 0) ? 1 : -1;
        float angleDeg = (float)(rand() % 60 - 30);
        float angleRad = angleDeg * (float)M_PI / 180.0f;
        speedMult = 1.0f;
        rallyCount = 0;
        float spd = BALL_SPEEDS[diff] * speedMult;
        ballVX = (float)dir * spd * cosf(angleRad);
        ballVY = spd * sinf(angleRad);
    }

    float reflect(float y, float min, float max) const {
        float range = max - min;
        if (range <= 0) return min;
        float off = y - min;
        float wrapped = fmodf(off, range * 2.0f);
        if (wrapped < 0) wrapped += range * 2.0f;
        if (wrapped <= range) return min + wrapped;
        return max - (wrapped - range);
    }

    void paddleHit(float paddleX, float paddleY) {
        float paddleCenterY = paddleY + PADDLE_H / 2.0f;
        float ballCenterY = ballY + BALL_SIZE / 2.0f;
        float relY = (ballCenterY - paddleCenterY) / (PADDLE_H / 2.0f);
        if (relY < -1.0f) relY = -1.0f;
        if (relY > 1.0f) relY = 1.0f;
        float angle = relY * MAX_ANGLE_RAD;
        float spd = BALL_SPEEDS[diff] * speedMult;
        if (paddleX < 64)
            ballVX = spd * cosf(angle);
        else
            ballVX = -spd * cosf(angle);
        ballVY = spd * sinf(angle);
        rallyCount++;
        speedMult = 1.0f + rallyCount * 0.04f;
    }

    void checkWallCollision() {
        if (ballY <= 0) {
            ballY = 0;
            ballVY = -ballVY;
        } else if (ballY + BALL_SIZE >= 64) {
            ballY = 64 - BALL_SIZE;
            ballVY = -ballVY;
        }
    }

    void checkPaddleCollision() {
        if (ballVX < 0) {
            if (ballX <= PADDLE_MARGIN + PADDLE_W && ballX + BALL_SIZE >= PADDLE_MARGIN) {
                if (ballY + BALL_SIZE > paddle1Y && ballY < paddle1Y + PADDLE_H) {
                    ballX = (float)(PADDLE_MARGIN + PADDLE_W);
                    paddleHit((float)PADDLE_MARGIN, paddle1Y);
                }
            }
        } else {
            int rpx = FIELD_W - PADDLE_MARGIN - PADDLE_W;
            if (ballX + BALL_SIZE >= rpx && ballX <= rpx + PADDLE_W) {
                if (ballY + BALL_SIZE > paddle2Y && ballY < paddle2Y + PADDLE_H) {
                    ballX = (float)(rpx - BALL_SIZE);
                    paddleHit((float)rpx, paddle2Y);
                }
            }
        }
    }

    bool checkScoring() {
        if (ballX + BALL_SIZE < 0) {
            score2++;
            if (score2 >= winScore) { gameOver = true; return true; }
            resetBall(1);
            return true;
        } else if (ballX > FIELD_W) {
            score1++;
            if (score1 >= winScore) { gameOver = true; return true; }
            resetBall(-1);
            return true;
        }
        return false;
    }

    void updateBall(float dt) {
        float maxMove = std::max(fabsf(ballVX), fabsf(ballVY)) * dt;
        int steps = std::max(1, (int)ceilf(maxMove / 1.0f));
        float subDt = dt / steps;
        for (int i = 0; i < steps; i++) {
            ballX += ballVX * subDt;
            ballY += ballVY * subDt;
            checkWallCollision();
            checkPaddleCollision();
            if (checkScoring()) break;
        }
    }

    void getObs(float* out, int paddleId) const {
        float ey = (paddleId == 1) ? paddle2Y : paddle1Y;
        float myY = (paddleId == 1) ? paddle1Y : paddle2Y;
        out[0] = ballX / 128.0f;
        out[1] = ballY / 64.0f;
        out[2] = ballVX / 100.0f;
        out[3] = ballVY / 100.0f;
        out[4] = myY / 64.0f;
        out[5] = ey / 64.0f;
    }

    float computeReward(int paddleId) const {
        float ballCenterY = ballY + BALL_SIZE / 2.0f;
        float myCenter = (paddleId == 1) ? paddle1Y + PADDLE_H / 2.0f : paddle2Y + PADDLE_H / 2.0f;
        float predictedY = ballCenterY;
        float paddleEdge = (paddleId == 1) ? (float)(PADDLE_MARGIN + PADDLE_W) : (float)(FIELD_W - PADDLE_MARGIN - PADDLE_W);
        float dx = fabsf(paddleEdge - ballX);
        if (dx > 0 && fabsf(ballVX) > 1) {
            float t = dx / fabsf(ballVX);
            predictedY = reflect(ballCenterY + ballVY * t, BALL_SIZE / 2.0f, 64.0f - BALL_SIZE / 2.0f);
        }
        float dist = fabsf(predictedY - myCenter);
        return dist <= PADDLE_H / 2.0f ? 0.1f : -0.1f;
    }

    void stepAI_CPU(int paddleId, float dt, int strength) {
        static const float CPU_SPEEDS[6] = {0, 15, 35, 48, 55, 55};
        float speed = CPU_SPEEDS[strength];
        bool predict = strength >= 3;
        bool preReflect = strength >= 5;

        float& paddleY = (paddleId == 1) ? paddle1Y : paddle2Y;
        float paddleCenter = paddleY + PADDLE_H / 2.0f;
        float ballCenterY = ballY + BALL_SIZE / 2.0f;
        float targetY = ballCenterY;

        bool ballMovingToward = (paddleId == 1 && ballVX < 0) || (paddleId == 2 && ballVX > 0);
        float paddleEdge = (paddleId == 1) ? (float)(PADDLE_MARGIN + PADDLE_W) : (float)(FIELD_W - PADDLE_MARGIN - PADDLE_W);

        if (predict && ballMovingToward) {
            float dx = fabsf(paddleEdge - ballX);
            if (dx > 0 && fabsf(ballVX) > 1) {
                float t = dx / fabsf(ballVX);
                float half = BALL_SIZE / 2.0f;
                targetY = reflect(ballCenterY + ballVY * t, half, 64.0f - half);
            }
        } else if (preReflect && !ballMovingToward) {
            int enemyPaddleId = (paddleId == 1) ? 2 : 1;
            float enemyPaddleY = (enemyPaddleId == 1) ? paddle1Y : paddle2Y;
            float enemyEdgeX = (enemyPaddleId == 1) ? (float)(PADDLE_MARGIN + PADDLE_W) : (float)(FIELD_W - PADDLE_MARGIN - PADDLE_W);
            float dx = fabsf(enemyEdgeX - ballX);
            if (dx > 0 && fabsf(ballVX) > 1) {
                float t = dx / fabsf(ballVX);
                float half = BALL_SIZE / 2.0f;
                float predictedBallY = ballCenterY + ballVY * t;
                float clampedPredictedY = reflect(predictedBallY, half, 64.0f - half);

                float enemyReachMin = std::max(0.0f, enemyPaddleY - speed * t);
                float enemyReachMax = std::min(64.0f - PADDLE_H, enemyPaddleY + speed * t);

                std::vector<float> hitYPositions;
                float yStart = std::ceil(enemyReachMin);
                float yEnd = std::floor(enemyReachMax);
                for (float yPos = yStart; yPos <= yEnd; yPos += 2.0f) {
                    if (clampedPredictedY + half > yPos && clampedPredictedY - half < yPos + PADDLE_H) {
                        float pCenterY = yPos + PADDLE_H / 2.0f;
                        float relY = (predictedBallY - pCenterY) / (PADDLE_H / 2.0f);
                        if (relY < -1.0f) relY = -1.0f;
                        if (relY > 1.0f) relY = 1.0f;
                        float angle = relY * MAX_ANGLE_RAD;
                        float spd = BALL_SPEEDS[diff] * speedMult;
                        float newVX = (enemyEdgeX < 64) ? spd * cosf(angle) : -spd * cosf(angle);
                        float newVY = spd * sinf(angle);

                        float ourEdgeX = (paddleId == 1) ? (float)(PADDLE_MARGIN + PADDLE_W) : (float)(FIELD_W - PADDLE_MARGIN - PADDLE_W);
                        float returnDx = fabsf(ourEdgeX - enemyEdgeX);
                        float returnT = returnDx / fabsf(newVX);
                        float finalY = reflect(clampedPredictedY + newVY * returnT, half, 64.0f - half);
                        hitYPositions.push_back(finalY);
                    }
                }
                if (!hitYPositions.empty()) {
                    float sum = 0;
                    for (float v : hitYPositions) sum += v;
                    targetY = sum / hitYPositions.size();
                }
            }
        }

        float diffY = targetY - paddleCenter;
        if (fabsf(diffY) > 1.0f) {
            float step = speed * dt;
            if (diffY > 0) paddleY += step;
            else paddleY -= step;
            if (paddleY < 0) paddleY = 0;
            if (paddleY + PADDLE_H > 64) paddleY = 64.0f - PADDLE_H;
        }
    }

    void stepAI(int paddleId, float dt, RLNet* net) {
        float obs[RL_INPUTS];
        getObs(obs, paddleId);
        float reward = computeReward(paddleId);
        int action = rlTrainFrame(net, reward, obs);
        float paddleY = (paddleId == 1) ? paddle1Y : paddle2Y;
        float paddleCenter = paddleY + PADDLE_H / 2.0f;
        float targetY = paddleCenter;
        if (action == 0) {
            targetY = paddleCenter - PADDLE_SPEED * dt;
        } else if (action == 2) {
            targetY = paddleCenter + PADDLE_SPEED * dt;
        } else {
            targetY = paddleCenter;
        }
        float diff = targetY - paddleCenter;
        if (fabsf(diff) > 1.0f) {
            float step = PADDLE_SPEED * dt;
            if (diff > 0) paddleY += step;
            else paddleY -= step;
            if (paddleY < 0) paddleY = 0;
            if (paddleY + PADDLE_H > 64) paddleY = 64.0f - PADDLE_H;
            if (paddleId == 1) paddle1Y = paddleY;
            else paddle2Y = paddleY;
        }
    }
};

// ====== Shared progress across threads ======
struct SharedProgress {
    std::atomic<long long> score1{0};
    std::atomic<long long> score2{0};
    std::atomic<int> maxRally{0};
    std::atomic<int> difficulty{1};
    std::atomic<int> episodesDone{0};
    int totalEpisodes;
};

// ====== Per-thread training result ======
struct ThreadResult {
    RLNet net;
    int episodes;
    int totalPoints1, totalPoints2;
    float totalFrames;
    int maxRallyCount;
    int finalDiff;
};

static ThreadResult trainThread(RLNet initialNet, int numEpisodes, SharedProgress* prog) {
    ThreadResult res;
    res.net = initialNet;
    res.episodes = 0;
    res.totalPoints1 = 0;
    res.totalPoints2 = 0;
    res.totalFrames = 0;
    res.maxRallyCount = 0;
    res.finalDiff = 1;

    float dt = 1.0f / 30.0f;
    int localMaxRally = 0;
    long long accum1 = 0, accum2 = 0;
    int batchSize = 10;

    float epsStart = res.net.epsilon;
    float epsEnd = 0.01f;
    float epsDecay = (epsStart - epsEnd) / numEpisodes;

    int currentDiff = 1;
    const int WINDOW = 80;
    int scoreHist[WINDOW] = {0};
    int histIdx = 0;
    int histCount = 0;
    float histSum = 0;
    int completedEpisodes = 0;

    for (int ep = 0; ep < numEpisodes; ep++) {
        if (trainingCancelled.load(std::memory_order_relaxed)) break;
        completedEpisodes++;
        PongState game;
        game.diff = currentDiff;
        game.reset();

        int frames = 0;
        while (!game.gameOver) {
            game.stepAI(1, dt, &res.net);
            game.stepAI_CPU(2, dt, currentDiff);

            int prev1 = game.score1, prev2 = game.score2;
            game.updateBall(dt);

            if (game.score1 > prev1) {
                if (res.net.lastState >= 0) {
                    float oldQ = res.net.q[res.net.lastState][res.net.lastAction];
                    res.net.q[res.net.lastState][res.net.lastAction] = oldQ + RL_ALPHA * (5.0f - oldQ);
                }
                res.net.lastState = -1;
            } else if (game.score2 > prev2) {
                if (res.net.lastState >= 0) {
                    float oldQ = res.net.q[res.net.lastState][res.net.lastAction];
                    res.net.q[res.net.lastState][res.net.lastAction] = oldQ + RL_ALPHA * (-5.0f - oldQ);
                }
                res.net.lastState = -1;
            }

            frames++;
        }

        res.net.epsilon = std::max(epsEnd, res.net.epsilon - epsDecay);

        if (game.rallyCount > localMaxRally) localMaxRally = game.rallyCount;
        accum1 += game.score1;
        accum2 += game.score2;

        res.totalPoints1 += game.score1;
        res.totalPoints2 += game.score2;
        res.totalFrames += frames;

        // Curriculum: update sliding window and check difficulty
        if (histCount < WINDOW) histCount++;
        histSum -= scoreHist[histIdx];
        scoreHist[histIdx] = game.score1;
        histSum += game.score1;
        histIdx = (histIdx + 1) % WINDOW;

        if (histCount >= WINDOW) {
            float avgP1 = histSum / WINDOW;
            if (avgP1 >= 1.0f && currentDiff < 5) {
                currentDiff++;
                histCount = 0;
                histSum = 0;
                memset(scoreHist, 0, sizeof(scoreHist));
            } else if (avgP1 < 0.1f && currentDiff > 1) {
                currentDiff--;
                histCount = 0;
                histSum = 0;
                memset(scoreHist, 0, sizeof(scoreHist));
            }
        }

        if (prog && (ep + 1) % batchSize == 0) {
            prog->score1.fetch_add(accum1, std::memory_order_relaxed);
            prog->score2.fetch_add(accum2, std::memory_order_relaxed);
            accum1 = 0; accum2 = 0;
            if (localMaxRally > prog->maxRally.load(std::memory_order_relaxed))
                prog->maxRally.store(localMaxRally, std::memory_order_relaxed);
            int prevDiff = prog->difficulty.load(std::memory_order_relaxed);
            while (currentDiff > prevDiff && !prog->difficulty.compare_exchange_weak(prevDiff, currentDiff, std::memory_order_relaxed));
            prog->episodesDone.fetch_add(batchSize, std::memory_order_relaxed);
        }
    }

    int rem = numEpisodes % batchSize;
    if (prog && rem > 0) {
        prog->score1.fetch_add(accum1, std::memory_order_relaxed);
        prog->score2.fetch_add(accum2, std::memory_order_relaxed);
        if (localMaxRally > prog->maxRally.load(std::memory_order_relaxed))
            prog->maxRally.store(localMaxRally, std::memory_order_relaxed);
        int prevDiff = prog->difficulty.load(std::memory_order_relaxed);
        while (currentDiff > prevDiff && !prog->difficulty.compare_exchange_weak(prevDiff, currentDiff, std::memory_order_relaxed));
        prog->episodesDone.fetch_add(rem, std::memory_order_relaxed);
    }

    res.episodes = completedEpisodes;
    res.maxRallyCount = localMaxRally;
    res.finalDiff = currentDiff;
    return res;
}

// ====== Main ======
int main(int argc, char* argv[]) {
    srand((unsigned int)time(nullptr));

    int numThreads = (int)std::thread::hardware_concurrency();
    int episodesPerThread = 1000;

    if (argc > 1) episodesPerThread = atoi(argv[1]);
    if (argc > 2) numThreads = atoi(argv[2]);

    if (numThreads < 1) numThreads = 1;

    printf("Pong RL Trainer\n");
    printf("  Threads:     %d\n", numThreads);
    printf("  Episodes:    %d per thread (%d total)\n", episodesPerThread, episodesPerThread * numThreads);

    RLNet globalNet;
    rlInit(&globalNet);

    const char* savePath1 = "saves/pong/rl_weights.dat";
    if (rlLoad(&globalNet, savePath1)) {
        printf("  Loaded:      %s (epsilon=%.3f)\n", savePath1, globalNet.epsilon);
    } else {
        printf("  No weights found, starting fresh\n");
    }

    SharedProgress prog;
    prog.totalEpisodes = episodesPerThread * numThreads;

    std::vector<std::thread> threads;
    std::vector<ThreadResult> results(numThreads);

    auto wallStart = std::chrono::steady_clock::now();
    auto nextReport = wallStart;

    signal(SIGINT, handleSigInt);

    for (int t = 0; t < numThreads; t++) {
        threads.emplace_back([&results, &globalNet, &prog, t, episodesPerThread]() {
            results[t] = trainThread(globalNet, episodesPerThread, &prog);
        });
    }

    while (!trainingCancelled.load(std::memory_order_relaxed) &&
           prog.episodesDone.load(std::memory_order_relaxed) < prog.totalEpisodes) {
        auto now = std::chrono::steady_clock::now();
        if (now >= nextReport) {
            int done = prog.episodesDone.load(std::memory_order_relaxed);
            long long s1 = prog.score1.load(std::memory_order_relaxed);
            long long s2 = prog.score2.load(std::memory_order_relaxed);
            int maxR = prog.maxRally.load(std::memory_order_relaxed);
            int diff = prog.difficulty.load(std::memory_order_relaxed);
            int total = prog.totalEpisodes;

            auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(now - wallStart).count();
            double rate = elapsedSec > 0 ? done / (double)elapsedSec : 0;
            int remaining = total - done;
            int etaSec = rate > 0 ? (int)(remaining / rate) : 0;

            printf("[%llds] Ep %d/%d  %lld:%lld  Max rally: %d  D:%d  %.0f ep/s  ETA: %ds\n",
                   (long long)elapsedSec, done, total, s1, s2, maxR, diff, rate, etaSec);
            fflush(stdout);

            nextReport = now + std::chrono::seconds(2);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    if (trainingCancelled.load(std::memory_order_relaxed)) {
        printf("SIGINT caught, finishing current episodes and saving...\n");
        fflush(stdout);
    }

    for (auto& th : threads) th.join();

    double elapsed = (double)std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - wallStart).count() / 1000.0;

    // Merge Q-tables (average across threads)
    for (int t = 1; t < numThreads; t++) {
        for (int s = 0; s < RL_STATES; s++)
            for (int a = 0; a < RL_ACTIONS; a++)
                globalNet.q[s][a] += results[t].net.q[s][a];
    }
    float scale = 1.0f / numThreads;
    for (int s = 0; s < RL_STATES; s++)
        for (int a = 0; a < RL_ACTIONS; a++)
            globalNet.q[s][a] *= scale;

    // Average epsilon
    float avgEpsilon = globalNet.epsilon;
    for (int t = 1; t < numThreads; t++)
        avgEpsilon += results[t].net.epsilon;
    avgEpsilon /= numThreads;
    globalNet.epsilon = avgEpsilon;

    // Stats
    int totalEpisodes = 0;
    int totalP1 = 0, totalP2 = 0;
    float totalFrames = 0;
    int maxRallyOverall = 0;
    int highestDiff = 0;
    for (int t = 0; t < numThreads; t++) {
        totalEpisodes += results[t].episodes;
        totalP1 += results[t].totalPoints1;
        totalP2 += results[t].totalPoints2;
        totalFrames += results[t].totalFrames;
        if (results[t].maxRallyCount > maxRallyOverall)
            maxRallyOverall = results[t].maxRallyCount;
        if (results[t].finalDiff > highestDiff)
            highestDiff = results[t].finalDiff;
    }

    printf("\n=== Results ===\n");
    printf("  Episodes:    %d\n", totalEpisodes);
    printf("  Score:       %d - %d (P1:P2)\n", totalP1, totalP2);
    printf("  Max rally:   %d\n", maxRallyOverall);
    printf("  Difficulty:  %d\n", highestDiff);
    printf("  Avg frames:  %.1f per episode\n", totalFrames / totalEpisodes);
    printf("  Time:        %.2f seconds\n", elapsed);
    printf("  Throughput:  %.0f episodes/sec\n", totalEpisodes / elapsed);
    printf("  Epsilon:     %.4f\n", avgEpsilon);

    ensureDir("saves/pong/");
    rlSave(&globalNet, savePath1);

    printf("  Saved to:    %s\n", savePath1);
    return 0;
}

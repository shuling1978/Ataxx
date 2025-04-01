// 同化棋（Ataxx）蒙特卡洛策略（带剪枝优化和增强评估函数）
// 原作者：zhouhy zys
// 修改：添加剪枝优化的蒙特卡洛树搜索和增强评估函数
//         改进连通性评估（考虑几何多样性）、平衡复制与切割决策、动态调整区域权重等
// 游戏信息：http://www.botzone.org/games#Ataxx

#include <iostream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <queue>
using namespace std;
/*
2
0 0 0 1
0 6 2 5
0 0 1 0
*/

int totalPieces = 0;          // 棋盘上棋子总数
int totalPiecesMain = 0;      // 棋盘上棋子总数（主函数用）
int currBotColor;             // 我所执子颜色（1为黑，-1为白，棋盘状态亦同）
unsigned long long blackGrid; // 用位图表示黑棋位置
unsigned long long whiteGrid; // 用位图表示白棋位置
int blackPieceCount = 2, whitePieceCount = 2;//////全局的棋子数量维护
static int delta[24][2] = {
    { 1,1 },{ 0,1 },{ -1,1 },{ -1,0 },
    { -1,-1 },{ 0,-1 },{ 1,-1 },{ 1,0 },
    { 2,0 },{ 2,1 },{ 2,2 },{ 1,2 },
    { 0,2 },{ -1,2 },{ -2,2 },{ -2,1 },
    { -2,0 },{ -2,-1 },{ -2,-2 },{ -1,-2 },
    { 0,-2 },{ 1,-2 },{ 2,-2 },{ 2,-1 }
};

// 位置权重矩阵（角落最高，边缘次之，中间较低）
const int POSITION_WEIGHT[7][7] = {
    {10, -20, 10,  5, 10, -20, 10},
    {-20, -50, -2, -2, -2, -50, -20},
    {10,  -2,  5,  1,  5,  -2,  10},
    {5,   -2,  1,  0,  1,  -2,   5},
    {10,  -2,  5,  1,  5,  -2,  10},
    {-20, -50, -2, -2, -2, -50, -20},
    {10, -20, 10,  5, 10, -20, 10}
};

// 判断是否在地图内
inline bool inMap(int x, int y)
{
    return x >= 0 && x < 7 && y >= 0 && y < 7;
}

// 动态位置权重，根据游戏阶段和局面状态调整
struct DynamicPositionWeight {
    int weights[7][7];

    // 初始化权重
    void initialize() {
        for (int y = 0; y < 7; y++)
            for (int x = 0; x < 7; x++)
                weights[x][y] = POSITION_WEIGHT[x][y];
    }

    // 更新函数，先根据局面阶段调整，再更新捕获比率及复制/切割风险
    void update(int totalPieces, int color);

    // 新增：捕获比率和复制/切割风险评估
    void updateCaptureAndMoveEfficiency(int color);

    // 以下为原有阶段权重更新函数
    void updateEarlyGameWeights(int color) {
        // 增加中央位置权重以促进扩张
        for (int y = 2; y < 5; y++) {
            for (int x = 2; x < 5; x++) {
                weights[x][y] += 5;
            }
        }
        // 降低远离现有棋子的位置权重
        for (int y = 0; y < 7; y++) {
            for (int x = 0; x < 7; x++) {
                if ((blackGrid | whiteGrid) & (1ULL << (x * 7 + y))) continue;
                bool isReachable = false;
                for (int y0 = 0; y0 < 7 && !isReachable; y0++) {
                    for (int x0 = 0; x0 < 7; x0++) {
                        if (!((blackGrid & (1ULL << (x0 * 7 + y0))) ^ (whiteGrid & (1ULL << (x0 * 7 + y0))))) continue;
                        int dx = abs(x - x0);
                        int dy = abs(y - y0);
                        if (dx <= 2 && dy <= 2 && !(dx == 0 && dy == 0)) {
                            isReachable = true;
                            break;
                        }
                    }
                }
                if (!isReachable) {
                    weights[x][y] -= 15;
                }
            }
        }
    }

    void updateMidGameWeights(int color) {
        // 中盘阶段：更具侵略性，优先翻转对手棋子
        for (int y = 0; y < 7; y++) {
            for (int x = 0; x < 7; x++) {
                if ((blackGrid | whiteGrid) & (1ULL << (x * 7 + y))) continue;
                int captureCount = 0;
                for (int dir = 0; dir < 8; dir++) {
                    int nx = x + delta[dir][0];
                    int ny = y + delta[dir][1];
                    if (inMap(nx, ny) && (whiteGrid & (1ULL << (nx * 7 + ny))) == (1ULL << (nx * 7 + ny)) ) {
                        captureCount++;
                    }
                }
                weights[x][y] += captureCount * captureCount * 3;
            }
        }
        for (int y0 = 0; y0 < 7; y0++) {
            for (int x0 = 0; x0 < 7; x0++) {
                if ((whiteGrid & (1ULL << (x0 * 7 + y0))) == 0) continue;
                for (int dir = 0; dir < 8; dir++) {
                    int x = x0 + delta[dir][0];
                    int y = y0 + delta[dir][1];
                    if (inMap(x, y) && ((blackGrid | whiteGrid) & (1ULL << (x * 7 + y))) == 0) {
                        weights[x][y] += 8;
                    }
                }
            }
        }
    }

    void updateLateGameWeights(int color) {
        // 残局阶段：巩固优势，控制关键位置
        for (int i = 0; i < 7; i++) {
            weights[0][i] += 10;
            weights[6][i] += 10;
            weights[i][0] += 10;
            weights[i][6] += 10;
        }
        weights[0][0] += 15; weights[0][6] += 15;
        weights[6][0] += 15; weights[6][6] += 15;
        for (int y = 0; y < 7; y++) {
            for (int x = 0; x < 7; x++) {
                if ((blackGrid | whiteGrid) & (1ULL << (x * 7 + y))) continue;
                int captureCount = 0;
                for (int dir = 0; dir < 8; dir++) {
                    int nx = x + delta[dir][0];
                    int ny = y + delta[dir][1];
                    if (inMap(nx, ny) && (whiteGrid & (1ULL << (nx * 7 + ny))) == (1ULL << (nx * 7 + ny))) {
                        captureCount++;
                    }
                }
                weights[x][y] += captureCount * captureCount * 5;
            }
        }
    }
};

void DynamicPositionWeight::update(int totalPieces, int color)
{
    // 重置为基础权重
    initialize();

    if (totalPieces < 15) {
        updateEarlyGameWeights(color);
    } else if (totalPieces < 30) {
        updateMidGameWeights(color);
    } else {
        updateLateGameWeights(color);
    }

    // 更新捕获比率和复制/切割风险
    updateCaptureAndMoveEfficiency(color);
}

void DynamicPositionWeight::updateCaptureAndMoveEfficiency(int color)
{
    for (int y = 0; y < 7; y++)
    {
        for (int x = 0; x < 7; x++)
        {
            if ((blackGrid | whiteGrid) & (1ULL << (x * 7 + y))) continue;

            int maxCaptureWithCopy = 0;
            int maxCaptureWithJump = 0;
            bool canCopy = false;
            bool canJump = false;

            // 检查所有可能的来源位置
            for (int y0 = 0; y0 < 7; y0++)
            {
                for (int x0 = 0; x0 < 7; x0++)
                {
                    if ((currBotColor == 1 && (blackGrid & (1ULL << (x0 * 7 + y0))) == 0) ||
                        (currBotColor == -1 && (whiteGrid & (1ULL << (x0 * 7 + y0))) == 0)) continue;

                    int dx = abs(x0 - x);
                    int dy = abs(y0 - y);

                    if (dx <= 1 && dy <= 1 && !(dx == 0 && dy == 0))
                    {
                        canCopy = true;
                        int captures = 0;
                        for (int dir = 0; dir < 8; dir++)
                        {
                            int nx = x + delta[dir][0];
                            int ny = y + delta[dir][1];
                            if (inMap(nx, ny) && (whiteGrid & (1ULL << (nx * 7 + ny))) == (1ULL << (nx * 7 + ny)))
                                captures++;
                        }
                        maxCaptureWithCopy = max(maxCaptureWithCopy, captures);
                    }
                    else if (dx <= 2 && dy <= 2 && !(dx == 0 && dy == 0))
                    {
                        canJump = true;
                        int captures = 0;
                        for (int dir = 0; dir < 8; dir++)
                        {
                            int nx = x + delta[dir][0];
                            int ny = y + delta[dir][1];
                            if (inMap(nx, ny) && (whiteGrid & (1ULL << (nx * 7 + ny))) == (1ULL << (nx * 7 + ny)))
                                captures++;
                        }
                        maxCaptureWithJump = max(maxCaptureWithJump, captures);
                    }
                }
            }

            if (canCopy && maxCaptureWithCopy > 0)
            {
                weights[x][y] += maxCaptureWithCopy * maxCaptureWithCopy * 4;
                if (totalPieces >= 15 && totalPieces < 30)
                {
                    weights[x][y] += maxCaptureWithCopy * 6;
                }
            }

            if (canJump && maxCaptureWithJump > 0)
            {
                int jumpBonus = maxCaptureWithJump * maxCaptureWithJump * 2;
                if (maxCaptureWithJump < 3 && totalPieces > 25)
                {
                    jumpBonus -= 10;
                }
                weights[x][y] += jumpBonus;
            }

            if (canCopy && canJump)
            {
                if (maxCaptureWithCopy >= maxCaptureWithJump - 1)
                {
                    weights[x][y] += 15;
                }
            }

            if ((x == 0 || x == 6 || y == 0 || y == 6) && (canCopy || maxCaptureWithCopy > 0 || maxCaptureWithJump > 1))
            {
                weights[x][y] += 12;
            }
        }
    }
}

// 创建全局动态权重实例
DynamicPositionWeight dynamicWeights;

// 向指定方向移动坐标
inline bool MoveStep(int &x, int &y, int Direction)
{
    x += delta[Direction][0];
    y += delta[Direction][1];
    return inMap(x, y);
}
// 在坐标处落子，检查是否合法或模拟落子
bool ProcStep(int x0, int y0, int x1, int y1, int color, bool simulate = false)
{
    if (color == 0) return false; // 检查颜色是否有效
    if (x1 == -1) return true;    // 如果目标位置为 -1，表示不落子，直接返回 true
    if (!inMap(x0, y0) || !inMap(x1, y1)) return false; // 检查起始和目标位置是否在棋盘范围内
    if ((color == 1 && !(blackGrid & (1ULL << (x0 * 7 + y0)))) ||
        (color == -1 && !(whiteGrid & (1ULL << (x0 * 7 + y0))))) return false; // 检查起始位置是否有当前颜色的棋子

    int dx = abs(x0 - x1), dy = abs(y0 - y1);
    if ((dx == 0 && dy == 0) || dx > 2 || dy > 2) return false; // 检查移动范围是否合法
    if ((blackGrid | whiteGrid) & (1ULL << (x1 * 7 + y1))) return false; // 检查目标位置是否为空

    // 保存原始状态（用于模拟模式）
    unsigned long long originalBlack = blackGrid, originalWhite = whiteGrid;
    int originalBlackCount = blackPieceCount, originalWhiteCount = whitePieceCount;
    if (simulate) {
        originalBlack = blackGrid;
        originalWhite = whiteGrid;
    }

    // 更新棋盘状态
    if (dx == 2 || dy == 2) {
        // 跳跃：移除起始位置的棋子
        if (color == 1)
            blackGrid &= ~(1ULL << (x0 * 7 + y0));
        else
            whiteGrid &= ~(1ULL << (x0 * 7 + y0));
    } else {
        // 复制：增加当前颜色的棋子数量
        if (color == 1)
            blackPieceCount++;
        else
            whitePieceCount++;
    }

    // 在目标位置放置棋子
    if (color == 1)
        blackGrid |= 1ULL << (x1 * 7 + y1);
    else
        whiteGrid |= 1ULL << (x1 * 7 + y1);

    // 翻转目标位置周围的对手棋子
    int currCount = 0;
    for (int dir = 0; dir < 8; dir++) {
        int x = x1 + delta[dir][0];
        int y = y1 + delta[dir][1];
        if (!inMap(x, y)) continue;

        if (color == 1 && (whiteGrid & (1ULL << (x * 7 + y)))) {
            // 翻转白棋为黑棋
            currCount++;
            whiteGrid &= ~(1ULL << (x * 7 + y));
            blackGrid |= 1ULL << (x * 7 + y);
        } else if (color == -1 && (blackGrid & (1ULL << (x * 7 + y)))) {
            // 翻转黑棋为白棋
            currCount++;
            blackGrid &= ~(1ULL << (x * 7 + y));
            whiteGrid |= 1ULL << (x * 7 + y);
        }
    }

    // 更新棋子数量
    if (currCount != 0) {
        if (color == 1) {
            blackPieceCount += currCount;
            whitePieceCount -= currCount;
        } else {
            whitePieceCount += currCount;
            blackPieceCount -= currCount;
        }
    }

    // 如果是模拟模式，恢复原始状态
    if (simulate) {
        blackGrid = originalBlack;
        whiteGrid = originalWhite;
        blackPieceCount = originalBlackCount;
        whitePieceCount = originalWhiteCount;
    } else {
        // 输出棋盘状态（用于调试）
        /* for (int i = 0; i < 7; i++) {
            for (int j = 0; j < 7; j++) {
                if (blackGrid & (1ULL << (i * 7 + j))) {
                    cout << 1 << " ";
                } else if (whiteGrid & (1ULL << (i * 7 + j))) {
                    cout << -1 << " ";
                } else {
                    cout << 0 << " ";
                }
            }
            cout << endl;
        }*/

    }

    return true; // 落子成功
}

// 改进的增强连通性评估 - 考虑几何多样性
int countConnectedPieces(int color)
{
    bool visited[7][7] = {false};
    int totalConnectivityScore = 0;

    int directionDiversity[7][7] = {0};
    int geometricBonus[7][7] = {0};

    // 计算每个己方棋子的方向多样性
    for (int y = 0; y < 7; y++)
    {
        for (int x = 0; x < 7; x++)
        {
            if ((blackGrid & (1ULL << (x * 7 + y))) == color)
            {
                bool hasDirection[8] = {false};
                int dirCount = 0;
                for (int dir = 0; dir < 8; dir++)
                {
                    int nx = x + delta[dir][0];
                    int ny = y + delta[dir][1];
                    if (inMap(nx, ny) && (blackGrid & (1ULL << (nx * 7 + ny))) == color)
                    {
                        hasDirection[dir] = true;
                        dirCount++;
                    }
                }
                directionDiversity[x][y] = dirCount;
                // 检查几何多样性：若存在两个不在同一直线上的方向
                for (int i = 0; i < 8; i++)
                {
                    if (!hasDirection[i]) continue;
                    for (int j = i + 1; j < 8; j++)
                    {
                        if (!hasDirection[j]) continue;
                        if (abs(i - j) != 4 && (i + j) % 8 != 0)
                        {
                            geometricBonus[x][y] += 3;
                            break;
                        }
                    }
                    if (geometricBonus[x][y] > 0) break;
                }
            }
        }
    }

    // 使用BFS计算连通区域得分
    for (int y = 0; y < 7; y++)
    {
        for (int x = 0; x < 7; x++)
        {
            if ((blackGrid & (1ULL << (x * 7 + y))) == color && !visited[x][y])
            {
                int regionSize = 0;
                int regionGeometricScore = 0;
                int regionDirectionScore = 0;
                queue<pair<int, int>> q;
                q.push({x, y});
                visited[x][y] = true;
                regionSize++;
                regionGeometricScore += geometricBonus[x][y];
                regionDirectionScore += directionDiversity[x][y];

                while (!q.empty())
                {
                    auto curr = q.front();
                    q.pop();
                    for (int dir = 0; dir < 8; dir++)
                    {
                        int nx = curr.first + delta[dir][0];
                        int ny = curr.second + delta[dir][1];
                        if (inMap(nx, ny) && (blackGrid & (1ULL << (nx * 7 + ny))) == color && !visited[nx][ny])
                        {
                            visited[nx][ny] = true;
                            q.push({nx, ny});
                            regionSize++;
                            regionGeometricScore += geometricBonus[nx][ny];
                            regionDirectionScore += directionDiversity[nx][ny];
                            regionDirectionScore += directionDiversity[nx][ny];
                        }
                    }
                }
                int regionScore = regionSize * regionSize;
                regionScore += regionGeometricScore * 2;
                regionScore += (regionDirectionScore * 3) / regionSize;
                totalConnectivityScore += regionScore;
            }
        }
    }

    return totalConnectivityScore;
}

// 计算对手潜在威胁
int countOpponentThreats(int color)
{
    int threatCount = 0;
    int opponentColor = -color;
    for (int y0 = 0; y0 < 7; y0++)
    {
        for (int x0 = 0; x0 < 7; x0++)
        {
            if ((currBotColor == 1 && (whiteGrid & (1ULL << (x0 * 7 + y0))) == 0) ||
                (currBotColor == -1 && (blackGrid & (1ULL << (x0 * 7 + y0))) == 0))
                continue;
            for (int dir = 0; dir < 24; dir++)
            {
                int x1 = x0 + delta[dir][0];
                int y1 = y0 + delta[dir][1];
                if (!inMap(x1, y1) || ((blackGrid | whiteGrid) & (1ULL << (x1 * 7 + y1))) != 0)
                    continue;
                int captureCount = 0;
                for (int adjDir = 0; adjDir < 8; adjDir++)
                {
                    int ax = x1 + delta[adjDir][0];
                    int ay = y1 + delta[adjDir][1];
                    if (inMap(ax, ay) && ((currBotColor == 1 && (blackGrid & (1ULL << (ax * 7 + ay))) != 0) ||
                                          (currBotColor == -1 && (whiteGrid & (1ULL << (ax * 7 + ay))) != 0)))
                        captureCount++;
                }
                threatCount += captureCount * captureCount;
            }
        }
    }
    return threatCount;
}

// 评估复制与切割操作的收益差异
int evaluateMoveType(int x0, int y0, int x1, int y1, int color)
{
    int moveScore = 0;
    int dx = abs(x0 - x1), dy = abs(y0 - y1);
    bool isCopy = (dx <= 1 && dy <= 1);

    int captureCount = 0;
    for (int dir = 0; dir < 8; dir++)
    {
        int nx = x1 + delta[dir][0];
        int ny = y1 + delta[dir][1];
        if (inMap(nx, ny) && ((currBotColor == 1 && (whiteGrid & (1ULL << (nx * 7 + ny))) != 0) ||
                              (currBotColor == -1 && (blackGrid & (1ULL << (nx * 7 + ny))) != 0)))
            captureCount++;
    }

    totalPieces = blackPieceCount + whitePieceCount;

    if (isCopy)
    {
        moveScore += 8; // 保留原棋子的奖励
        if (captureCount > 0)
        {
            moveScore += captureCount * captureCount * 3;
            if (totalPieces >= 15 && totalPieces < 35)
                moveScore += captureCount * 5;
        }
        else if (totalPieces > 20)
        {
            moveScore -= 5;
        }
    }
    else
    {
        moveScore -= 5; // 切割操作扣分
        if (captureCount >= 3)
            moveScore += (captureCount - 2) * 5;
        if (totalPieces > 35)
            moveScore -= 8;
    }

    moveScore += POSITION_WEIGHT[x1][y1] / 2;

    int formingTriangle = 0;
    bool hasDirection[8] = {false};
    for (int dir = 0; dir < 8; dir++)
    {
        int nx = x1 + delta[dir][0];
        int ny = y1 + delta[dir][1];
        if (inMap(nx, ny) && ((currBotColor == 1 && (blackGrid & (1ULL << (nx * 7 + ny))) != 0) ||
                              (currBotColor == -1 && (whiteGrid & (1ULL << (nx * 7 + ny))) != 0)))
            hasDirection[dir] = true;
    }
    for (int i = 0; i < 8; i++)
    {
        if (!hasDirection[i]) continue;
        for (int j = i + 1; j < 8; j++)
        {
            if (!hasDirection[j] || abs(i - j) == 4) continue;
            formingTriangle++;
        }
    }
    moveScore += formingTriangle * 4;

    return moveScore;
}

// 计算某一方的合法棋步数量
int numLegalMoves(int color) {
    int legalMoves = 0;

    for (int y0 = 0; y0 < 7; y0++) {
        for (int x0 = 0; x0 < 7; x0++) {
            if ((currBotColor == 1 && (blackGrid & (1ULL << (x0 * 7 + y0))) == 0) ||
                (currBotColor == -1 && (whiteGrid & (1ULL << (x0 * 7 + y0))) == 0)) continue;

            for (int dir = 0; dir < 24; dir++) {
                int x1 = x0 + delta[dir][0];
                int y1 = y0 + delta[dir][1];

                // 检查目标位置是否在地图内且为空
                if (inMap(x1, y1) && ((blackGrid | whiteGrid) & (1ULL << (x1 * 7 + y1))) == 0) {
                    legalMoves++;
                }
            }
        }
    }

    return legalMoves;
}

// 计算某一方的稳定棋子数量
int countStablePieces(int color) {
    bool stable[7][7] = {false}; // 标记棋子是否稳定
    int stableCount = 0;

    // 检查四个角落的稳定性
    const int corners[4][2] = {{0, 0}, {0, 6}, {6, 0}, {6, 6}};
    for (int i = 0; i < 4; i++) {
        int x = corners[i][0], y = corners[i][1];
        if ((currBotColor == 1 && (blackGrid & (1ULL << (x * 7 + y))) != 0) ||
            (currBotColor == -1 && (whiteGrid & (1ULL << (x * 7 + y))) != 0)) {
            stable[x][y] = true;
            stableCount++;
        }
    }

    // 检查边缘的稳定性
    for (int i = 0; i < 7; i++) {
        // 上边缘
        if ((currBotColor == 1 && (blackGrid & (1ULL << i)) != 0 && (i == 0 || stable[i - 1][0])) ||
            (currBotColor == -1 && (whiteGrid & (1ULL << i)) != 0 && (i == 0 || stable[i - 1][0]))) {
            stable[i][0] = true;
            stableCount++;
        }
        // 下边缘
        if ((currBotColor == 1 && (blackGrid & (1ULL << (i + 6 * 7))) != 0 && (i == 0 || stable[i - 1][6])) ||
            (currBotColor == -1 && (whiteGrid & (1ULL << (i + 6 * 7))) != 0 && (i == 0 || stable[i - 1][6]))) {
            stable[i][6] = true;
            stableCount++;
        }
        // 左边缘
        if ((currBotColor == 1 && (blackGrid & (1ULL << (0 * 7 + i))) != 0 && (i == 0 || stable[0][i - 1])) ||
            (currBotColor == -1 && (whiteGrid & (1ULL << (0 * 7 + i))) != 0 && (i == 0 || stable[0][i - 1]))) {
            stable[0][i] = true;
            stableCount++;
        }
        // 右边缘
        if ((currBotColor == 1 && (blackGrid & (1ULL << (6 * 7 + i))) != 0 && (i == 0 || stable[6][i - 1])) ||
            (currBotColor == -1 && (whiteGrid & (1ULL << (6 * 7 + i))) != 0 && (i == 0 || stable[6][i - 1]))) {
            stable[6][i] = true;
            stableCount++;
        }
    }

    // 检查内部棋子的稳定性
    for (int y = 1; y < 6; y++) {
        for (int x = 1; x < 6; x++) {
            if ((currBotColor == 1 && (blackGrid & (1ULL << (x * 7 + y))) != 0) ||
                (currBotColor == -1 && (whiteGrid & (1ULL << (x * 7 + y))) != 0)) {
                bool isStable = true;
                for (int dir = 0; dir < 8; dir++) {
                    int nx = x + delta[dir][0];
                    int ny = y + delta[dir][1];
                    if (inMap(nx, ny) && ((currBotColor == 1 && (blackGrid & (1ULL << (nx * 7 + ny))) == 0) ||
                                          (currBotColor == -1 && (whiteGrid & (1ULL << (nx * 7 + ny))) == 0))) {
                        isStable = false;
                        break;
                    }
                }
                if (isStable) {
                    stable[x][y] = true;
                    stableCount++;
                }
            }
        }
    }

    return stableCount;
}

// 改进后的增强评估函数，整合多项改进
int Evaluate()
{
    totalPieces = blackPieceCount + whitePieceCount;
    dynamicWeights.update(totalPieces, currBotColor);

    int score = 0;
    int pieceScoreFactor = 20;
    if (totalPieces < 15)
        pieceScoreFactor = 150;
    else if (totalPieces < 30)
        pieceScoreFactor = 200;
    else
        pieceScoreFactor = 170;

    if (currBotColor == 1)
        score += pieceScoreFactor * (blackPieceCount - whitePieceCount);
    else
        score += pieceScoreFactor * (whitePieceCount - blackPieceCount);

    int myMoves = numLegalMoves(currBotColor);
    int oppMoves = numLegalMoves(-currBotColor);
    int mobilityFactor = 3;
    if (totalPieces < 15)
        mobilityFactor = 3;
    else if (totalPieces >= 35)
        mobilityFactor = 5;
    score += mobilityFactor * (myMoves - oppMoves);

    const int corners[4][2] = { {0,0}, {0,6}, {6,0}, {6,6} };
    int cornerScore = 0;
    for (int i = 0; i < 4; i++) {
        int x = corners[i][0], y = corners[i][1];
        if ((currBotColor == 1 && (blackGrid & (1ULL << (x * 7 + y))) != 0) ||
            (currBotColor == -1 && (whiteGrid & (1ULL << (x * 7 + y))) != 0))
            cornerScore += 100;
        else if ((currBotColor == 1 && (whiteGrid & (1ULL << (x * 7 + y))) != 0) ||
                 (currBotColor == -1 && (blackGrid & (1ULL << (x * 7 + y))) != 0))
            cornerScore -= 100;
    }
    score += cornerScore;

    for (int y = 0; y < 7; y++) {
        for (int x = 0; x < 7; x++) {
            if ((currBotColor == 1 && (blackGrid & (1ULL << (x * 7 + y))) != 0) ||
                (currBotColor == -1 && (whiteGrid & (1ULL << (x * 7 + y))) != 0))
                score += dynamicWeights.weights[x][y];
            else if ((currBotColor == 1 && (whiteGrid & (1ULL << (x * 7 + y))) != 0) ||
                     (currBotColor == -1 && (blackGrid & (1ULL << (x * 7 + y))) != 0))
                score -= dynamicWeights.weights[x][y];
        }
    }

    int stabilityFactor = 2;
    if (totalPieces > 30)
        stabilityFactor = 3;
    score += stabilityFactor * countStablePieces(currBotColor);

    int connectivityFactor = 3;
    if (totalPieces < 15)
        connectivityFactor = 4;
    else if (totalPieces >= 30)
        connectivityFactor = 5;
    score += connectivityFactor * countConnectedPieces(currBotColor);

    int threatFactor = 5;
    if (totalPieces >= 15 && totalPieces < 30)
        threatFactor = 8;
    score -= threatFactor * countOpponentThreats(currBotColor);

    return score;
}

// 检查游戏是否结束
bool IsGameOver()
{
    if (blackPieceCount == 0 || whitePieceCount == 0)
        return true;

    bool blackCanMove = false, whiteCanMove = false;
    for (int y0 = 0; y0 < 7; y0++)
    {
        for (int x0 = 0; x0 < 7; x0++)
        {
            if ((currBotColor == 1 && (blackGrid & (1ULL << (x0 * 7 + y0))) != 0) ||
                (currBotColor == -1 && (whiteGrid & (1ULL << (x0 * 7 + y0))) != 0))
            {
                for (int dir = 0; dir < 24; dir++)
                {
                    int x1 = x0 + delta[dir][0];
                    int y1 = y0 + delta[dir][1];
                    if (inMap(x1, y1) && ((blackGrid | whiteGrid) & (1ULL << (x1 * 7 + y1))) == 0)
                    {
                        blackCanMove = true;
                        break;
                    }
                }
            }
            else if ((currBotColor == -1 && (blackGrid & (1ULL << (x0 * 7 + y0))) != 0) ||
                     (currBotColor == 1 && (whiteGrid & (1ULL << (x0 * 7 + y0))) != 0))
            {
                for (int dir = 0; dir < 24; dir++)
                {
                    int x1 = x0 + delta[dir][0];
                    int y1 = y0 + delta[dir][1];
                    if (inMap(x1, y1) && ((blackGrid | whiteGrid) & (1ULL << (x1 * 7 + y1))) == 0)
                    {
                        whiteCanMove = true;
                        break;
                    }
                }
            }
            if (blackCanMove && whiteCanMove)
                return false;
        }
    }
    return true;
}

int MonteCarloSimulation(int startX, int startY, int resultX, int resultY, int simulations = 100)
{
    unsigned long long originalBlack = blackGrid;
    unsigned long long originalWhite = whiteGrid;
    int originalBlackCount = blackPieceCount, originalWhiteCount = whitePieceCount;
//存储原来的棋盘和黑白棋子count

    int wins=0;
    // 执行当前走法
    if (!ProcStep(startX, startY, resultX, resultY, currBotColor,1)) {//难道不是模拟吗
        return -1000000; // 返回一个极低的分数，表示非法棋步
    }

    // 使用新的评估函数计算初始分数，并加入走法类型评估
    int initialScore = Evaluate();
    int moveTypeScore = evaluateMoveType(startX, startY, resultX, resultY, currBotColor);
    initialScore += moveTypeScore;
    totalPieces = blackPieceCount + whitePieceCount;
    int maxDepth = 30;
    if (totalPieces > 35) {
        maxDepth = 50;
        simulations = 500;
    } else if (totalPieces < 10) {
        maxDepth = 60;
    }

    //模拟sim次
    for (int i = 0; i < simulations; i++)
    {
        //开始使用sim头
        unsigned long long simBlack = blackGrid;
        unsigned long long simWhite = whiteGrid;
        int simBlackCount = blackPieceCount, simWhiteCount = whitePieceCount;

        int currentColor = -currBotColor;
        bool gameOver = false;
        int steps = 0;

        while (!gameOver && steps < maxDepth)//往下搜
        {
            vector<pair<pair<int, int>, pair<int, int>>> moves;
            vector<int> moveScores;

            for (int y0 = 0; y0 < 7; y0++)
                for (int x0 = 0; x0 < 7; x0++)
                    if ((currentColor == 1 && (simBlack & (1ULL << (x0 * 7 + y0))) != 0) ||
                        (currentColor == -1 && (simWhite & (1ULL << (x0 * 7 + y0))) != 0))
                        for (int dir2 = 0; dir2 < 8; dir2++)
{
    int tempBlackCount=0;int tempWhiteCount=0;//这个移动temp
    int x1 = x0 + delta[dir2][0];
    int y1 = y0 + delta[dir2][1];
    if (!inMap(x1, y1))
        continue;

        int currCount=0;//////周围被反转的？
    if (((blackGrid & (1ULL << (x1 * 7 + y1))) && currentColor != 1) ||
        ((whiteGrid & (1ULL << (x1 * 7 + y1))) && currentColor != -1) ||
        !((blackGrid |whiteGrid) & (1ULL << (x1 * 7 + y1)))) {
            return false; // 位置为空
        }

        if (currentColor == 1 && (whiteGrid & (1ULL << (x1 * 7 + y1)))) {
    currCount++;
    whiteGrid &= ~(1ULL << (x1 * 7 + y1)); // 移除白棋
    blackGrid |= 1ULL << (x1 * 7 + y1);    // 添加黑棋
} else if (currentColor == -1 && (blackGrid & (1ULL << (x1 * 7 + y1)))) {
    currCount++;
    blackGrid &= ~(1ULL << (x1 * 7 + y1)); // 移除黑棋
    whiteGrid |= 1ULL << (x1 * 7 + y1);    // 添加白棋
}


        if (currCount != 0) {
            if (currentColor == 1) {
                tempBlackCount += currCount;
                tempWhiteCount -= currCount;
            } else {
                tempWhiteCount += currCount;
                tempBlackCount -= currCount;
            }
        }

        int score = 0;
        score += 5 * (currentColor == 1 ? (tempBlackCount - tempWhiteCount) : (tempWhiteCount - tempBlackCount));
        score += POSITION_WEIGHT[x1][y1] * (currentColor == currBotColor ? 1 : -1);

        moves.push_back({{x0, y0}, {x1, y1}});
        moveScores.push_back(score);
}


//已经完成所有move,choice指向被选择的
        int choice = 0;
        if (rand() % 10 < 8) {
            if (currentColor == currBotColor) {
                choice = max_element(moveScores.begin(), moveScores.end()) - moveScores.begin();
            } else {
                choice = min_element(moveScores.begin(), moveScores.end()) - moveScores.begin();
            }
        } else {
            choice = rand() % moves.size();
        }

        auto move = moves[choice];

        int dx = abs(move.first.first - move.second.first);
        int dy = abs(move.first.second - move.second.second);
        if (dx == 2 || dy == 2)
            simBlack &= ~(1ULL << (move.first.first * 7 + move.first.second));
        else if (currentColor == 1)
            simBlackCount++;
        else
            simWhiteCount++;

        simBlack |= 1ULL << (move.second.first * 7 + move.second.second);
        int captureCount = 0;

        for (int adjDir = 0; adjDir < 8; adjDir++) {
            int x = move.second.first + delta[adjDir][0];
            int y = move.second.second + delta[adjDir][1];
            if (!inMap(x, y))
                continue;
            if ((simBlack & (1ULL << (x * 7 + y))) == -currentColor) {
                captureCount++;
                simBlack |= 1ULL << (x * 7 + y);
            }
        }

        if (captureCount != 0) {
            if (currentColor == 1) {
                simBlackCount += captureCount;
                simWhiteCount -= captureCount;
            } else {
                simWhiteCount += captureCount;
                simBlackCount -= captureCount;
            }
        }

        currentColor = -currentColor;
        steps++;

        }
        //每次模拟结束
        gameOver = (simBlackCount == 0 || simWhiteCount == 0) || (steps >= maxDepth);
        int finalScore = (currBotColor == 1) ? (simBlackCount - simWhiteCount) : (simWhiteCount - simBlackCount);
        if (finalScore > 0)
            wins++;
    }


//可是就没有操作？可能是其他函数操作了grid
            // 恢复原始状态
            blackGrid = originalBlack;
            whiteGrid = originalWhite;
            blackPieceCount = originalBlackCount;
            whitePieceCount = originalWhiteCount;

            totalPieces = blackPieceCount + whitePieceCount;
            float evalWeight = 0.4;
            if (totalPieces < 15)
                evalWeight = 0.3;
            else if (totalPieces > 30)
                evalWeight = 0.6;

            if (totalPieces >= 15 && totalPieces < 35)
                return wins + int(evalWeight * initialScore) + moveTypeScore * 3;

            return wins + int(evalWeight * initialScore) + moveTypeScore;
}

        int main()
        {
            ios::sync_with_stdio(false);
            srand(time(0));

            dynamicWeights.initialize();

            int x0, y0, x1, y1;
            blackGrid = (1ULL << (0 * 7 + 0)) | (1ULL << (6 * 7 + 6));   // |黑|白|
            whiteGrid = (1ULL << (6 * 7 + 0)) | (1ULL << (0 * 7 + 6));   // |白|黑|

            int turnID;
            currBotColor = -1; // 第一回合收到坐标为 -1, -1 表示我是黑方
            cin >> turnID;

            for (int i = 0; i < turnID - 1; i++)
            {
                cin >> x0 >> y0 >> x1 >> y1;
                if (x1 >= 0)
                    ProcStep(x0, y0, x1, y1, -currBotColor);


                else
                    currBotColor = 1;

                cin >> x0 >> y0 >> x1 >> y1;
                if (x1 >= 0)
                    ProcStep(x0, y0, x1, y1, currBotColor);
            }



            cin >> x0 >> y0 >> x1 >> y1;
            if (turnID == 1 && x1 < 0) {
                cout << "0 0 1 1" << endl;
                return 0;
            }
            if (x1 >= 0)
                ProcStep(x0, y0, x1, y1, -currBotColor);
            else
                currBotColor = 1;

            vector<pair<pair<int, int>, pair<int, int>>> moves;
            for (int y0 = 0; y0 < 7; y0++)
            {
                for (int x0 = 0; x0 < 7; x0++)
                {
                    if ((currBotColor == 1 && (blackGrid & (1ULL << (x0 * 7 + y0))) == 0) ||
                        (currBotColor == -1 && (whiteGrid & (1ULL << (x0 * 7 + y0))) == 0))
                        continue;
                    for (int dir = 0; dir < 24; dir++)
                    {
                        x1 = x0 + delta[dir][0];
                        y1 = y0 + delta[dir][1];
                        if (!inMap(x1, y1)) continue;
                        if ((blackGrid | whiteGrid) & (1ULL << (x1 * 7 + y1))) continue;
                        moves.push_back({{x0, y0}, {x1, y1}});
                    }
                }
            }

            int bestStartX = -1, bestStartY = -1, bestResultX = -1, bestResultY = -1;
            int bestScore = -1000000000;

            if (!moves.empty())
            {
                vector<pair<int, int>> moveScores(moves.size());
                for (size_t i = 0; i < moves.size(); i++)
                {
                    auto &move = moves[i];
                    unsigned long long tempBlack = blackGrid;
                    unsigned long long tempWhite = whiteGrid;
                    int tempBlackCount = blackPieceCount, tempWhiteCount = whitePieceCount;

                    ProcStep(move.first.first, move.first.second, move.second.first, move.second.second, currBotColor, true);
                    moveScores[i] = {Evaluate(), (int)i};

                    blackGrid = tempBlack;
                    whiteGrid = tempWhite;
                    blackPieceCount = tempBlackCount;
                    whitePieceCount = tempWhiteCount;
                }

                sort(moveScores.begin(), moveScores.end(), greater<pair<int, int>>());

                totalPiecesMain = blackPieceCount + whitePieceCount;
                int topN = 10;
                int simCount = 30;
                if (totalPieces < 10) {
                    topN = min(15, (int)moves.size());
                    simCount = 300;
                } else if (totalPieces > 35) {
                    topN = min(5, (int)moves.size());
                    simCount = 300;
                } else {
                    topN = min(8, (int)moves.size());
                    simCount = 500;
                }

                for (int i = 0; i < topN && i < (int)moveScores.size(); i++)
                {
                    int idx = moveScores[i].second;
                    auto &move = moves[idx];
                    int wins = MonteCarloSimulation(move.first.first, move.first.second, move.second.first, move.second.second, simCount);
                    if (wins > bestScore)
                    {
                        bestScore = wins;
                        bestStartX = move.first.first;
                        bestStartY = move.first.second;
                        bestResultX = move.second.first;
                        bestResultY = move.second.second;
                    }
                }
            }

            cout << bestStartX << " " << bestStartY << " " << bestResultX << " " << bestResultY;
            return 0;
        }

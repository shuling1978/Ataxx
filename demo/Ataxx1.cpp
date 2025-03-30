// 同化棋（Ataxx）蒙特卡洛策略（带剪枝优化和增强评估函数）
// 原作者：zhouhy zys
// 修改：添加剪枝优化的蒙特卡洛树搜索和增强评估函数
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

int totalPieces = 0; // 棋盘上棋子总数
int totalPiecesMain = 0; // 棋盘上棋子总数（主函数用）
int currBotColor; // 我所执子颜色（1为黑，-1为白，棋盘状态亦同）
int gridInfo[7][7] = { 0 }; // 先x后y，记录棋盘状态
int blackPieceCount = 2, whitePieceCount = 2;
static int delta[24][2] = { { 1,1 },{ 0,1 },{ -1,1 },{ -1,0 },
{ -1,-1 },{ 0,-1 },{ 1,-1 },{ 1,0 },
{ 2,0 },{ 2,1 },{ 2,2 },{ 1,2 },
{ 0,2 },{ -1,2 },{ -2,2 },{ -2,1 },
{ -2,0 },{ -2,-1 },{ -2,-2 },{ -1,-2 },
{ 0,-2 },{ 1,-2 },{ 2,-2 },{ 2,-1 } };

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

// 向Direction方向改动坐标，并返回是否越界
inline bool MoveStep(int &x, int &y, int Direction)
{
    x += delta[Direction][0];
    y += delta[Direction][1];
    return inMap(x, y);
}

// 在坐标处落子，检查是否合法或模拟落子
bool ProcStep(int x0, int y0, int x1, int y1, int color, bool simulate = false)
{
    if (color == 0)
        return false;
    if (x1 == -1) // 无路可走，跳过此回合
        return true;
    if (!inMap(x0, y0) || !inMap(x1, y1)) // 超出边界
        return false;
    if (gridInfo[x0][y0] != color)
        return false;

    int dx = abs(x0 - x1), dy = abs(y0 - y1);
    if ((dx == 0 && dy == 0) || dx > 2 || dy > 2) // 保证不会移动到原来位置，而且移动始终在5×5区域内
        return false;
    if (gridInfo[x1][y1] != 0) // 保证移动到的位置为空
        return false;

    // 保存原始状态以便回滚
    int originalGrid[7][7];
    int originalBlack = blackPieceCount, originalWhite = whitePieceCount;
    if (simulate)
    {
        memcpy(originalGrid, gridInfo, sizeof(gridInfo));
    }

    if (dx == 2 || dy == 2) // 如果走的是5×5的外围，则不是复制粘贴
        gridInfo[x0][y0] = 0;
    else if (color == 1)
        blackPieceCount++;
    else
        whitePieceCount++;

    gridInfo[x1][y1] = color;
    int currCount = 0;

    for (int dir = 0; dir < 8; dir++) // 影响邻近8个位置
    {
        int x = x1 + delta[dir][0];
        int y = y1 + delta[dir][1];
        if (!inMap(x, y))
            continue;
        if (gridInfo[x][y] == -color)
        {
            currCount++;
            gridInfo[x][y] = color;
        }
    }

    if (currCount != 0)
    {
        if (color == 1)
        {
            blackPieceCount += currCount;
            whitePieceCount -= currCount;
        }
        else
        {
            whitePieceCount += currCount;
            blackPieceCount -= currCount;
        }
    }

    // 如果只是模拟，恢复原始状态
    if (simulate)
    {
        memcpy(gridInfo, originalGrid, sizeof(gridInfo));
        blackPieceCount = originalBlack;
        whitePieceCount = originalWhite;
    }

    return true;
}

// 计算指定颜色的合法移动数量
int numLegalMoves(int color)
{
    int moveCount = 0;
    
    for (int y0 = 0; y0 < 7; y0++)
    {
        for (int x0 = 0; x0 < 7; x0++)
        {
            if (gridInfo[x0][y0] != color)
                continue;

            for (int dir = 0; dir < 24; dir++)
            {
                int x1 = x0 + delta[dir][0];
                int y1 = y0 + delta[dir][1];
                if (inMap(x1, y1) && gridInfo[x1][y1] == 0)
                {
                    moveCount++;
                }
            }
        }
    }
    
    return moveCount;
}

// 计算稳定棋子数量（不容易被对手翻转的棋子）
int countStablePieces(int color)
{
    int stableCount = 0;
    
    // 角落非常稳定
    const int corners[4][2] = {{0,0}, {0,6}, {6,0}, {6,6}};
    for (auto &corner : corners)
    {
        int x = corner[0], y = corner[1];
        if (gridInfo[x][y] == color)
            stableCount += 3;
    }
    
    // 边缘相对稳定
    for (int i = 1; i < 6; i++)
    {
        if (gridInfo[0][i] == color) stableCount++;
        if (gridInfo[6][i] == color) stableCount++;
        if (gridInfo[i][0] == color) stableCount++;
        if (gridInfo[i][6] == color) stableCount++;
    }
    
    // 棋子周围被同色棋子包围也更稳定
    for (int y = 1; y < 6; y++)
    {
        for (int x = 1; x < 6; x++)
        {
            if (gridInfo[x][y] != color)
                continue;
                
            int sameColorNeighbors = 0;
            for (int dir = 0; dir < 8; dir++)
            {
                int nx = x + delta[dir][0];
                int ny = y + delta[dir][1];
                if (inMap(nx, ny) && gridInfo[nx][ny] == color)
                    sameColorNeighbors++;
            }
            
            if (sameColorNeighbors >= 4)
                stableCount++;
        }
    }
    
    return stableCount;
}

// 广度优先搜索辅助函数，用于计算连通区域
void bfs(int x, int y, int color, bool visited[7][7], int& count)
{
    queue<pair<int, int>> q;
    q.push({x, y});
    visited[x][y] = true;
    count++;
    
    while (!q.empty())
    {
        auto curr = q.front();
        q.pop();
        
        for (int dir = 0; dir < 8; dir++)
        {
            int nx = curr.first + delta[dir][0];
            int ny = curr.second + delta[dir][1];
            
            if (inMap(nx, ny) && gridInfo[nx][ny] == color && !visited[nx][ny])
            {
                visited[nx][ny] = true;
                q.push({nx, ny});
                count++;
            }
        }
    }
}

// 计算连通棋子数量（连接在一起的棋子形成更强的控制）
int countConnectedPieces(int color)
{
    bool visited[7][7] = {false};
    vector<int> regions;
    
    for (int y = 0; y < 7; y++)
    {
        for (int x = 0; x < 7; x++)
        {
            if (gridInfo[x][y] == color && !visited[x][y])
            {
                int regionSize = 0;
                bfs(x, y, color, visited, regionSize);
                regions.push_back(regionSize);
            }
        }
    }
    
    // 大连通区域比多个小区域更有价值
    int connectedScore = 0;
    for (int regionSize : regions)
    {
        connectedScore += regionSize * regionSize;
    }
    
    return connectedScore;
}

// 计算对手潜在威胁
int countOpponentThreats(int color)
{
    int threatCount = 0;
    int opponentColor = -color;
    
    // 检查对手棋子是否能在下一步吃到我方棋子
    for (int y0 = 0; y0 < 7; y0++)
    {
        for (int x0 = 0; x0 < 7; x0++)
        {
            if (gridInfo[x0][y0] != opponentColor)
                continue;

            for (int dir = 0; dir < 24; dir++)
            {
                int x1 = x0 + delta[dir][0];
                int y1 = y0 + delta[dir][1];
                if (!inMap(x1, y1) || gridInfo[x1][y1] != 0)
                    continue;
                    
                // 计算这步移动能吃掉多少我方棋子
                int captureCount = 0;
                for (int adjDir = 0; adjDir < 8; adjDir++)
                {
                    int ax = x1 + delta[adjDir][0];
                    int ay = y1 + delta[adjDir][1];
                    if (inMap(ax, ay) && gridInfo[ax][ay] == color)
                        captureCount++;
                }
                
                // 高威胁移动权重更高
                threatCount += captureCount * captureCount;
            }
        }
    }
    
    return threatCount;
}

// 优化后的评估函数，结合子力差、行动力、角落、位置权重、稳定性、连通性、对手威胁
int Evaluate()
{
    int score = 0;
    if (currBotColor == 1)
        score += 20 * (blackPieceCount - whitePieceCount);
    else
        score += 20 * (whitePieceCount - blackPieceCount);
    
    int myMoves = numLegalMoves(currBotColor);
    int oppMoves = numLegalMoves(-currBotColor);
    score += 3 * (myMoves - oppMoves);
    
    const int corners[4][2] = { {0,0}, {0,6}, {6,0}, {6,6} };
    int cornerScore = 0;
    for (int i = 0; i < 4; i++) {
        int x = corners[i][0], y = corners[i][1];
        if (gridInfo[x][y] == currBotColor)
            cornerScore += 100;
        else if (gridInfo[x][y] == -currBotColor)
            cornerScore -= 100;
    }
    score += cornerScore;
    
    for (int y = 0; y < 7; y++)
        for (int x = 0; x < 7; x++) {
            if (gridInfo[x][y] == currBotColor)
                score += POSITION_WEIGHT[x][y];
            else if (gridInfo[x][y] == -currBotColor)
                score -= POSITION_WEIGHT[x][y];
        }
    
    score += 4 * countStablePieces(currBotColor);
    score += 5 * countConnectedPieces(currBotColor);
    score -= 5 * countOpponentThreats(currBotColor);
    
    return score;
}

// 检查游戏是否结束
bool IsGameOver()
{
    if (blackPieceCount == 0 || whitePieceCount == 0)
        return true;

    // 检查双方是否还有合法移动
    bool blackCanMove = false, whiteCanMove = false;

    for (int y0 = 0; y0 < 7; y0++)
    {
        for (int x0 = 0; x0 < 7; x0++)
        {
            if (gridInfo[x0][y0] == 1) // 黑子
            {
                for (int dir = 0; dir < 24; dir++)
                {
                    int x1 = x0 + delta[dir][0];
                    int y1 = y0 + delta[dir][1];
                    if (inMap(x1, y1) && gridInfo[x1][y1] == 0)
                    {
                        blackCanMove = true;
                        break;
                    }
                }
            }
            else if (gridInfo[x0][y0] == -1) // 白子
            {
                for (int dir = 0; dir < 24; dir++)
                {
                    int x1 = x0 + delta[dir][0];
                    int y1 = y0 + delta[dir][1];
                    if (inMap(x1, y1) && gridInfo[x1][y1] == 0)
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

// 优化的蒙特卡洛模拟
int MonteCarloSimulation(int startX, int startY, int resultX, int resultY, int simulations = 100)
{
    int wins = 0;
    int originalGrid[7][7];
    int originalBlack = blackPieceCount, originalWhite = whitePieceCount;

    // 保存原始状态
    memcpy(originalGrid, gridInfo, sizeof(gridInfo));

    // 执行当前移动
    ProcStep(startX, startY, resultX, resultY, currBotColor);

    // 初始评估
    int initialScore = Evaluate();

    // 根据当前局面阶段动态调整模拟次数和深度
    totalPieces = blackPieceCount + whitePieceCount;
    int maxDepth = 16;  // 默认最大模拟深度
    
    // 棋盘接近满时降低模拟深度
    if (totalPieces > 35) {
        maxDepth = 30;
        simulations = 70;
    } else if (totalPieces < 10) {
        // 棋盘较空时增加模拟深度
        maxDepth = 50;
    }

    for (int i = 0; i < simulations; i++)
    {
        // 复制当前状态
        int simGrid[7][7];
        int simBlack = blackPieceCount, simWhite = whitePieceCount;
        memcpy(simGrid, gridInfo, sizeof(gridInfo));

        int currentColor = -currBotColor; // 对手回合开始
        bool gameOver = false;
        int steps = 0;

        while (!gameOver && steps < maxDepth)
        {
            // 找出所有合法移动
            vector<pair<pair<int, int>, pair<int, int>>> moves;
            vector<int> moveScores;

            for (int y0 = 0; y0 < 7; y0++)
            {
                for (int x0 = 0; x0 < 7; x0++)
                {
                    if (simGrid[x0][y0] == currentColor)
                    {
                        for (int dir = 0; dir < 24; dir++)
                        {
                            int x1 = x0 + delta[dir][0];
                            int y1 = y0 + delta[dir][1];
                            if (inMap(x1, y1) && simGrid[x1][y1] == 0)
                            {
                                // 模拟这个移动并评估
                                int tempGrid[7][7];
                                int tempBlack = simBlack, tempWhite = simWhite;
                                memcpy(tempGrid, simGrid, sizeof(simGrid));

                                // 执行模拟移动
                                int dx = abs(x0 - x1), dy = abs(y0 - y1);
                                if (dx == 2 || dy == 2)
                                    tempGrid[x0][y0] = 0;
                                else if (currentColor == 1)
                                    tempBlack++;
                                else
                                    tempWhite++;

                                tempGrid[x1][y1] = currentColor;
                                int currCount = 0;

                                for (int dir2 = 0; dir2 < 8; dir2++)
                                {
                                    int x = x1 + delta[dir2][0];
                                    int y = y1 + delta[dir2][1];
                                    if (!inMap(x, y))
                                        continue;
                                    if (tempGrid[x][y] == -currentColor)
                                    {
                                        currCount++;
                                        tempGrid[x][y] = currentColor;
                                    }
                                }

                                if (currCount != 0)
                                {
                                    if (currentColor == 1)
                                    {
                                        tempBlack += currCount;
                                        tempWhite -= currCount;
                                    }
                                    else
                                    {
                                        tempWhite += currCount;
                                        tempBlack -= currCount;
                                    }
                                }

                                // 快速评估这个移动（简化版评估以加速模拟）
                                int score = 0;
                                // 先考虑子力差
                                score += 5 * (currentColor == 1 ? (tempBlack - tempWhite) : (tempWhite - tempBlack));
                                
                                // 再加上位置权重
                                score += POSITION_WEIGHT[x1][y1] * (currentColor == currBotColor ? 1 : -1);
                                
                                moves.push_back({{x0, y0}, {x1, y1}});
                                moveScores.push_back(score);
                            }
                        }
                    }
                }
            }

            if (moves.empty())
            {
                currentColor = -currentColor; // 切换玩家
                gameOver = IsGameOver();
                steps++;
                continue;
            }

            // 根据评估分数选择移动
            int choice = 0;
            
            // 有概率随机选择移动以增加多样性
            if (rand() % 10 < 8)  // 80%概率选择贪心移动
            {
                if (currentColor == currBotColor)
                {
                    // 我们选择最好的移动
                    choice = max_element(moveScores.begin(), moveScores.end()) - moveScores.begin();
                }
                else
                {
                    // 对手选择对我们最差的移动
                    choice = min_element(moveScores.begin(), moveScores.end()) - moveScores.begin();
                }
            }
            else  // 20%概率随机选择（避免陷入局部最优）
            {
                choice = rand() % moves.size();
            }

            auto move = moves[choice];
            
            // 模拟当前移动
            int dx = abs(move.first.first - move.second.first);
            int dy = abs(move.first.second - move.second.second);
            if (dx == 2 || dy == 2)
                simGrid[move.first.first][move.first.second] = 0;
            else if (currentColor == 1)
                simBlack++;
            else
                simWhite++;

            simGrid[move.second.first][move.second.second] = currentColor;
            int captureCount = 0;

            for (int adjDir = 0; adjDir < 8; adjDir++)
            {
                int x = move.second.first + delta[adjDir][0];
                int y = move.second.second + delta[adjDir][1];
                if (!inMap(x, y))
                    continue;
                if (simGrid[x][y] == -currentColor)
                {
                    captureCount++;
                    simGrid[x][y] = currentColor;
                }
            }

            if (captureCount != 0)
            {
                if (currentColor == 1)
                {
                    simBlack += captureCount;
                    simWhite -= captureCount;
                }
                else
                {
                    simWhite += captureCount;
                    simBlack -= captureCount;
                }
            }

            currentColor = -currentColor; // 切换玩家
            steps++;
            gameOver = (simBlack == 0 || simWhite == 0) || (steps >= maxDepth);
        }

        // 判断胜负
        int finalScore = (currBotColor == 1) ? (simBlack - simWhite) : (simWhite - simBlack);
        if (finalScore > 0)
            wins++;
    }

    // 恢复原始状态
    memcpy(gridInfo, originalGrid, sizeof(gridInfo));
    blackPieceCount = originalBlack;
    whitePieceCount = originalWhite;

    // 结合初始评估和模拟结果，权重根据游戏阶段动态调整
    totalPieces = blackPieceCount + whitePieceCount;
    float evalWeight = 0.4;  // 初始评估权重
    
    // 游戏后期更信任启发式评估
    if (totalPieces > 30) {
        evalWeight = 0.7;
    }
    
    return wins + int(evalWeight * initialScore);
}

int main()
{
    istream::sync_with_stdio(false);
    srand(time(0));

    int x0, y0, x1, y1;

    // 初始化棋盘
    gridInfo[0][0] = gridInfo[6][6] = 1;  //|黑|白|
    gridInfo[6][0] = gridInfo[0][6] = -1; //|白|黑|

    // 分析自己收到的输入和自己过往的输出，并恢复状态
    int turnID;
    currBotColor = -1; // 第一回合收到坐标是-1, -1，说明我是黑方
    cin >> turnID;

    for (int i = 0; i < turnID - 1; i++)
    {
        // 根据这些输入输出逐渐恢复状态到当前回合
        cin >> x0 >> y0 >> x1 >> y1;
        if (x1 >= 0)
            ProcStep(x0, y0, x1, y1, -currBotColor); // 模拟对方落子
        else
            currBotColor = 1;

        cin >> x0 >> y0 >> x1 >> y1;
        if (x1 >= 0)
            ProcStep(x0, y0, x1, y1, currBotColor); // 模拟己方落子
    }

    // 看看自己本回合输入
    cin >> x0 >> y0 >> x1 >> y1;
    if(turnID==1&&x1<0){
        // 第一回合对方没有落子，说明我方是黑方，选择一个好的开局
        cout << "0 0 1 1" << endl;
        return 0;
    }
    if (x1 >= 0)
        ProcStep(x0, y0, x1, y1, -currBotColor); // 模拟对方落子
    else
        currBotColor = 1;

    // 找出所有合法落子点
    vector<pair<pair<int, int>, pair<int, int>>> moves;

    for (int y0 = 0; y0 < 7; y0++)
    {
        for (int x0 = 0; x0 < 7; x0++)
        {
            if (gridInfo[x0][y0] != currBotColor)
                continue;

            for (int dir = 0; dir < 24; dir++)
            {
                x1 = x0 + delta[dir][0];
                y1 = y0 + delta[dir][1];
                if (!inMap(x1, y1))
                    continue;
                if (gridInfo[x1][y1] != 0)
                    continue;
                moves.push_back({{x0, y0}, {x1, y1}});
            }
        }
    }

    // 做出决策
    int bestStartX = -1, bestStartY = -1, bestResultX = -1, bestResultY = -1;
    int bestScore = -1e9;

    if (!moves.empty())
    {
        // 先根据启发式评估排序，优先评估看起来好的移动
        vector<pair<int, int>> moveScores(moves.size());
        for (size_t i = 0; i < moves.size(); i++)
        {
            auto &move = moves[i];
            int tempGrid[7][7];
            memcpy(tempGrid, gridInfo, sizeof(gridInfo));
            int tempBlack = blackPieceCount, tempWhite = whitePieceCount;

            ProcStep(move.first.first, move.first.second, move.second.first, move.second.second, currBotColor, true);
            moveScores[i] = {Evaluate(), i};

            memcpy(gridInfo, tempGrid, sizeof(gridInfo));
            blackPieceCount = tempBlack;
            whitePieceCount = tempWhite;
        }

        // 按评估分数排序
        sort(moveScores.begin(), moveScores.end(), greater<pair<int, int>>());

        // 根据当前局面复杂度动态调整评估数量和模拟次数
        totalPiecesMain = blackPieceCount + whitePieceCount;
        int topN = 10;  // 默认评估前10个移动
        int simCount = 30;  // 默认每个移动模拟30次
        
        // 根据游戏阶段动态调整
        if (totalPieces < 10) {
            // 开局阶段：更多的时间评估更多的可能性
            topN = min(15, (int)moves.size());
            simCount = 70;
        } else if (totalPieces > 35) {
            // 残局阶段：减少评估数量，增加模拟精度
            topN = min(5, (int)moves.size());
            simCount = 50;
        } else {
            // 中局
            topN = min(8, (int)moves.size());
            simCount = 100;
        }
        
        // 只评估前topN个最好的移动（剪枝）
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

    // 输出结果
    cout << bestStartX << " " << bestStartY << " " << bestResultX << " " << bestResultY;
    return 0;
}
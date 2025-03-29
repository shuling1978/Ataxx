// 同化棋（Ataxx）蒙特卡洛策略（带剪枝优化）
// 作者：zhouhy zys
// 修改：添加剪枝优化的蒙特卡洛树搜索
// 游戏信息：http://www.botzone.org/games#Ataxx

#include <iostream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <algorithm>
#include <cmath>
#include<cstring>
using namespace std;

int currBotColor; // 我所执子颜色（1为黑，-1为白，棋盘状态亦同）
int gridInfo[7][7] = { 0 }; // 先x后y，记录棋盘状态
int blackPieceCount = 2, whitePieceCount = 2;
static int delta[24][2] = { { 1,1 },{ 0,1 },{ -1,1 },{ -1,0 },
{ -1,-1 },{ 0,-1 },{ 1,-1 },{ 1,0 },
{ 2,0 },{ 2,1 },{ 2,2 },{ 1,2 },
{ 0,2 },{ -1,2 },{ -2,2 },{ -2,1 },
{ -2,0 },{ -2,-1 },{ -2,-2 },{ -1,-2 },
{ 0,-2 },{ 1,-2 },{ 2,-2 },{ 2,-1 } };

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

// 评估当前局面得分（启发式函数）
int Evaluate()
{
    int score = 0;
    int mobility = 0;
    int stability = 0;
    int cornerValue = 0;

    // 子力差
    score += currBotColor == 1 ? (blackPieceCount - whitePieceCount) : (whitePieceCount - blackPieceCount);

    // 行动力（可走位置数量）
    for (int y0 = 0; y0 < 7; y0++)
    {
        for (int x0 = 0; x0 < 7; x0++)
        {
            if (gridInfo[x0][y0] == currBotColor)
            {
                for (int dir = 0; dir < 24; dir++)
                {
                    int x1 = x0 + delta[dir][0];
                    int y1 = y0 + delta[dir][1];
                    if (inMap(x1, y1) && gridInfo[x1][y1] == 0)
                    {
                        mobility++;
                    }
                }
            }
        }
    }
    score += mobility / 5; // 行动力权重

    // 角点控制（角点非常重要）
    const int corners[4][2] = {{0,0}, {0,6}, {6,0}, {6,6}};
    for (auto &corner : corners)
    {
        int x = corner[0], y = corner[1];
        if (gridInfo[x][y] == currBotColor)
            cornerValue += 10;
        else if (gridInfo[x][y] == -currBotColor)
            cornerValue -= 10;
    }
    score += cornerValue;

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

// 带剪枝的蒙特卡洛模拟
int MonteCarloSimulation(int startX, int startY, int resultX, int resultY, int simulations = 50)
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

    for (int i = 0; i < simulations; i++)
    {
        // 复制当前状态
        int simGrid[7][7];
        int simBlack = blackPieceCount, simWhite = whitePieceCount;
        memcpy(simGrid, gridInfo, sizeof(gridInfo));

        int currentColor = -currBotColor; // 对手回合开始
        bool gameOver = false;
        int steps = 0;

        while (!gameOver && steps < 20) // 限制模拟步数
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

                                // 评估这个移动
                                int score = currentColor == 1 ? (tempBlack - tempWhite) : (tempWhite - tempBlack);
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

            // 根据评估分数选择移动（对手会选择对我们最不利的移动）
            int choice = 0;
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

            auto move = moves[choice];
            ProcStep(move.first.first, move.first.second, move.second.first, move.second.second, currentColor);

            currentColor = -currentColor; // 切换玩家
            steps++;
            gameOver = IsGameOver();
        }

        // 判断胜负
        int finalScore = currBotColor == 1 ? (blackPieceCount - whitePieceCount) : (whitePieceCount - blackPieceCount);
        if (finalScore > 0)
            wins++;

        // 恢复模拟状态
        memcpy(gridInfo, simGrid, sizeof(gridInfo));
        blackPieceCount = simBlack;
        whitePieceCount = simWhite;
    }

    // 恢复原始状态
    memcpy(gridInfo, originalGrid, sizeof(gridInfo));
    blackPieceCount = originalBlack;
    whitePieceCount = originalWhite;

    // 结合初始评估和模拟结果
    return wins + initialScore / 10;
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
        cout<<0<<" "<<0<<" "<<0<<" "<<1<<endl;
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

        // 只评估前N个最好的移动（剪枝）
        int topN = min(10, (int)moves.size());
        for (int i = 0; i < topN; i++)
        {
            int idx = moveScores[i].second;
            auto &move = moves[idx];

            int wins = MonteCarloSimulation(move.first.first, move.first.second, move.second.first, move.second.second, 30);

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

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
#define ULL unsigned long long int
using namespace std;

int totalPieces = 0;          // 棋盘上棋子总数
int totalPiecesMain = 0;      // 棋盘上棋子总数（主函数用）
int currBotColor;             // 我所执子颜色（1为黑，-1为白，棋盘状态亦同）
//nt gridInfo[7][7] = { 0 };   // 先x后y，记录棋盘状态
int blackPieceCount = 2, whitePieceCount = 2;

// 使用位运算表示棋盘（示例）
struct BitBoard {
    uint64_t black; // 黑子位置
    uint64_t white; // 白子位置


    BitBoard() : black(0), white(0) {
        // 初始化棋盘
        black |= (1ULL << (0 * 7 + 0)) | (1ULL << (6 * 7 + 6));
        white |= (1ULL << (0 * 7 + 6)) | (1ULL << (6 * 7 + 0));

      //  for(int i=0;i<7;i++)for(int j=0;j<7;j++)black_scc[i][j]=0;black_scc_cnt=0;
 //for(int i=0;i<7;i++)for(int j=0;j<7;j++)while_scc[i][j]=0;white_scc_cnt=0;
    }

    // 判断(x,y)是否为空
    bool isEmpty(int x, int y) const {
        uint64_t pos = 1ULL << (x * 7 + y);
        return !(black & pos) && !(white & pos);//白子也不在，黑子也不在
    }
    bool issame(int x,int y,int color){
 uint64_t pos = 1ULL << (x * 7 + y);
    if(color==1&&black&pos)return 1;
    if(color==-1&&white&pos)return 1;
        return 0;
    }

void setColor(int x,int y,int color){
    uint64_t pos = 1ULL << (x * 7 + y);
    if(color==1){
        black |= pos;
        white &= ~pos;
    }else {
        white |= pos;
        black &= ~pos;
    }
}

void eraseColor(int x,int y){
    uint64_t mask = ~(1ULL << (x*7+y));
    white &= mask;
    black &= mask;
}
    int get_black(){
        uint64_t t=black;
        int res=0;while(t){res+=t&1;t>>=1;}
        return res;
    }
    int get_white(){
 uint64_t t=white;
        int res=0;while(t){res+=t&1;t>>=1;}
        return res;
    }

    // 其他操作...
}gridInfo;

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

inline int lowbit(int x){
return x&(-x);
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

        //给所有远离的减少权
        //找到连通分量外的
        //尽量位运算吧
        uint64_t isReachable=gridInfo.black&(gridInfo.black<<1);//左右相邻
        isReachable|=gridInfo.black&(gridInfo.black<<7);//上下相邻
        uint64_t noReachable=isReachable^1;
        uint64_t temp=noReachable;
        while(temp){
            uint64_t now=lowbit(temp);temp-=(now<<1);
            int x=now/7;int y=now%7;//之后算一下
            weights[x][y]-=8;//之前出现白子黑子各下各的？情况，可能是早期给高了持续了，改了下，15->8
        }
    }


    void updateMidGameWeights(int color) {
        // 中盘阶段：更具侵略性，优先翻转对手棋子

//跳到对方内部的点能吃到多少个棋子
//能否看对方中部空缺，给这样的格子加权？
uint64_t isVillageinCity;
if(color==-1){//执白

isVillageinCity=(gridInfo.black^1)&(gridInfo.white^1);//但这个没填
isVillageinCity&=(gridInfo.black)&(gridInfo.black<<1);//左右为对方棋子
isVillageinCity&=(gridInfo.black)&(gridInfo.black<<7);//上下为对方棋子
}else {

isVillageinCity=(gridInfo.black^1)&(gridInfo.white^1);//但这个没填
isVillageinCity&=(gridInfo.white)&(gridInfo.white<<1);//左右为对方棋子
isVillageinCity&=(gridInfo.white)&(gridInfo.white<<7);//上下为对方棋子

}

        uint64_t temp=isVillageinCity;
        while(temp){
            uint64_t now=lowbit(temp);temp-=(now<<1);
            int x=now/7;int y=now%7;
            int cnt=4;//等下换
            weights[x][y]+=cnt*cnt*3;
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

        //对方内部的加权更多
       uint64_t isVillageinCity;
if(color==-1){//执白

isVillageinCity=(gridInfo.black^1)&(gridInfo.white^1);//但这个没填
isVillageinCity&=(gridInfo.black)&(gridInfo.black<<1);//左右为对方棋子
isVillageinCity&=(gridInfo.black)&(gridInfo.black<<7);//上下为对方棋子
}else {

isVillageinCity=(gridInfo.black^1)&(gridInfo.white^1);//但这个没填
isVillageinCity&=(gridInfo.white)&(gridInfo.white<<1);//左右为对方棋子
isVillageinCity&=(gridInfo.white)&(gridInfo.white<<7);//上下为对方棋子

}

        uint64_t temp=isVillageinCity;
        while(temp){
            uint64_t now=lowbit(temp);temp-=(now<<1);
            int x=now/7;int y=now%7;
            int cnt=4;//等下换
            weights[x][y]+=cnt*cnt*3;
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
            if (!gridInfo.isEmpty(x,y)) continue;

            int maxCaptureWithCopy = 0;
            int maxCaptureWithJump = 0;
            bool canCopy = false;
            bool canJump = false;

            // 检查所有可能的来源位置
            for (int y0 = 0; y0 < 7; y0++)
            {
                for (int x0 = 0; x0 < 7; x0++)
                {
                    if (color==-1&&gridInfo.black<<(x0*7+y0)&1) continue;
                    if(color==1&&gridInfo.white<<(x0*7+y0)&1)continue;


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
                            if (inMap(nx, ny) && !gridInfo.isEmpty(nx,ny)&&!gridInfo.issame(nx,ny,color))
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
                            if (inMap(nx, ny) && !gridInfo.isEmpty(nx,ny)&&!gridInfo.issame(nx,ny,color))
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
    if (color == 0) return false;
    if (x1 == -1) return true;
    if (!inMap(x0, y0) || !inMap(x1, y1)) return false;
    //if (gridInfo[x0][y0] != color) return false;
    if (color==1 && (gridInfo.black & (1ULL << (x0*7+y0))) ||(color==-1 && (gridInfo.white & (1ULL << (x0*7+y0))))) ;
    else return false;



    int dx = abs(x0 - x1), dy = abs(y0 - y1);
    if ((dx == 0 && dy == 0) || dx > 2 || dy > 2) return false;
    //if (gridInfo[x1][y1] != 0) return false;

    //int originalGrid[7][7];
    //int originalBlack = blackPieceCount, originalWhite = whitePieceCount;
    struct BitBoard originalGrid=gridInfo;
   // if (simulate) {
      //  memcpy(originalGrid, gridInfo, sizeof(gridInfo));

  //  }

    if (dx == 2 || dy == 2);
        //gridInfo[x0][y0] = 0;
      //  gridInfo.black//为什么是gridinfo加？
    else if (color == 1)
        blackPieceCount++;
    else
        whitePieceCount++;

    //gridInfo[x1][y1] = color;
    if(color==-1)gridInfo.white|=(1<<(x1*7+y1));
    else gridInfo.black|=(1<<(x1*7+y1));
    int currCount = 0;
    for (int dir = 0; dir < 8; dir++)
    {
        int x = x1 + delta[dir][0];
        int y = y1 + delta[dir][1];
        if (!inMap(x, y))
            continue;
        if (!gridInfo.isEmpty(x,y)&&!gridInfo.issame(x,y,color)) {
            currCount++;
            gridInfo.setColor(x,y,color);

        }
    }

    if (currCount != 0)
    {
        if (color == 1) {
            blackPieceCount += currCount;
            whitePieceCount -= currCount;
        } else {
            whitePieceCount += currCount;
            blackPieceCount -= currCount;
        }
    }

    if (simulate) {
       // memcpy(gridInfo, originalGrid, sizeof(gridInfo));
       gridInfo=originalGrid;
        blackPieceCount = originalGrid.get_black();
        whitePieceCount = originalGrid.get_white();
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

            if (!gridInfo.isEmpty(x0,y0)||!gridInfo.issame(x0,y0,color)) continue;
            for (int dir = 0; dir < 24; dir++)
            {
                int x1 = x0 + delta[dir][0];
                int y1 = y0 + delta[dir][1];
                if (inMap(x1, y1) && gridInfo.isEmpty(x1,y1))
                    moveCount++;
            }
        }
    }
    return moveCount;
}

// 计算稳定棋子数量（不易被翻转）
int countStablePieces(int color)
{
    int stableCount = 0;
    const int corners[4][2] = { {0,0}, {0,6}, {6,0}, {6,6} };
    for (auto &corner : corners)
    {
        int x = corner[0], y = corner[1];
        if (!gridInfo.isEmpty(x,y)&&gridInfo.issame(x,y,color))
            stableCount += 3;
    }
    for (int i = 1; i < 6; i++)
    {
        /*if (gridInfo[0][i] == color) stableCount++;
        if (gridInfo[6][i] == color) stableCount++;
        if (gridInfo[i][0] == color) stableCount++;
        if (gridInfo[i][6] == color) stableCount++;*/
        if(!gridInfo.isEmpty(0,i)&&gridInfo.issame(0,i,color))stableCount++;
 if(!gridInfo.isEmpty(6,i)&&gridInfo.issame(6,i,color))stableCount++;
  if(!gridInfo.isEmpty(i,0)&&gridInfo.issame(i,0,color))stableCount++;
   if(!gridInfo.isEmpty(i,6)&&gridInfo.issame(i,6,color))stableCount++;
    }
    for (int y = 1; y < 6; y++)
    {
        for (int x = 1; x < 6; x++)
        {
            if (!gridInfo.isEmpty(x,y)&&!gridInfo.issame(x,y,color)) continue;
            int sameColorNeighbors = 0;
            for (int dir = 0; dir < 8; dir++)
            {
                int nx = x + delta[dir][0];
                int ny = y + delta[dir][1];
                if (inMap(nx, ny) && !gridInfo.isEmpty(x,y)&&gridInfo.issame(x,y,color))
                    sameColorNeighbors++;
            }
            if (sameColorNeighbors >= 4)
                stableCount++;
        }
    }
    return stableCount;
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
            if (!gridInfo.isEmpty(x,y)&&!gridInfo.issame(x,y,color))
                continue;
            bool hasDirection[8] = {false};
            int dirCount = 0;
            for (int dir = 0; dir < 8; dir++)
            {
                int nx = x + delta[dir][0];
                int ny = y + delta[dir][1];
                if (inMap(nx, ny) && !gridInfo.isEmpty(x,y)&&gridInfo.issame(x,y,color))
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

    // 使用BFS计算连通区域得分
    for (int y = 0; y < 7; y++)
    {
        for (int x = 0; x < 7; x++)
        {
            if (!gridInfo.isEmpty(x,y)&&gridInfo.issame(x,y,color)&& !visited[x][y])
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
                        if (inMap(nx, ny) && !gridInfo.isEmpty(x,y)&&gridInfo.issame(x,y,color) && !visited[nx][ny])
                        {
                            visited[nx][ny] = true;
                            q.push({nx, ny});
                            regionSize++;
                            regionGeometricScore += geometricBonus[nx][ny];
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

    for (int y0 = 0; y0 < 7; y0++)
    {
        for (int x0 = 0; x0 < 7; x0++)
        {
            if (!gridInfo.isEmpty(x0,y0)&&gridInfo.issame(x0,y0,-color))
                continue;
            for (int dir = 0; dir < 24; dir++)
            {
                int x1 = x0 + delta[dir][0];
                int y1 = y0 + delta[dir][1];
                if (!inMap(x1, y1) || !gridInfo.isEmpty(x1,y1))
                    continue;
                int captureCount = 0;
                for (int adjDir = 0; adjDir < 8; adjDir++)
                {
                    int ax = x1 + delta[adjDir][0];
                    int ay = y1 + delta[adjDir][1];
                    if (inMap(ax, ay) && !gridInfo.isEmpty(ax,ay)&&gridInfo.issame(ax,ay,color))
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
        if (inMap(nx, ny) && !gridInfo.isEmpty(nx,ny)&&gridInfo.issame(nx,ny,-color))
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
        if (inMap(nx, ny) && ((!gridInfo.isEmpty(nx,ny)&&gridInfo.issame(nx,ny,color) )|| (nx == x0 && ny == y0)))
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
        if (!gridInfo.isEmpty(x,y)&&gridInfo.issame(x,y,currBotColor))

            cornerScore += 100;
        else if (!gridInfo.isEmpty(x,y)&&!gridInfo.issame(x,y,currBotColor))
            cornerScore -= 100;
    }
    score += cornerScore;

    for (int y = 0; y < 7; y++) {
        for (int x = 0; x < 7; x++) {
            if (!gridInfo.isEmpty(x,y)&&gridInfo.issame(x,y,currBotColor))
                score += dynamicWeights.weights[x][y];
            else if (!gridInfo.isEmpty(x,y)&&!gridInfo.issame(x,y,currBotColor))
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
            if (!gridInfo.isEmpty(x0,y0)&&gridInfo.issame(x0,y0,1))
            {
                for (int dir = 0; dir < 24; dir++)
                {
                    int x1 = x0 + delta[dir][0];
                    int y1 = y0 + delta[dir][1];
                    if (inMap(x1, y1) && gridInfo.isEmpty(x1,y1))
                    {
                        blackCanMove = true;
                        break;
                    }
                }
            }
            else if (!gridInfo.isEmpty(x0,y0)&&gridInfo.issame(x0,y0,-1))
            {
                for (int dir = 0; dir < 24; dir++)
                {
                    int x1 = x0 + delta[dir][0];
                    int y1 = y0 + delta[dir][1];
                    if (inMap(x1, y1) && gridInfo.isEmpty(x1,y1))
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


// 优化的蒙特卡洛模拟（模拟过程部分保持原有逻辑，示意部分以省略号代替）
int MonteCarloSimulation(int startX, int startY, int resultX, int resultY, int simulations = 100)
{
    int wins = 0;
    //int originalGrid[7][7];
    struct BitBoard originalGrid;
    int originalBlack = blackPieceCount, originalWhite = whitePieceCount;

   // memcpy(originalGrid, gridInfo, sizeof(gridInfo));
    originalGrid=gridInfo;
    // 先执行当前走法
    ProcStep(startX, startY, resultX, resultY, currBotColor);

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

    for (int i = 0; i < simulations; i++)
    {
        //int simGrid[7][7];
        int simBlack = blackPieceCount, simWhite = whitePieceCount;
        //memcpy(simGrid, gridInfo, sizeof(gridInfo));
        struct BitBoard simGrid=gridInfo;
        int currentColor = -currBotColor;
        bool gameOver = false;
        int steps = 0;

        while (!gameOver && steps < maxDepth)
        {
            vector<pair<pair<int, int>, pair<int, int>>> moves;
            vector<int> moveScores;

            for (int y0 = 0; y0 < 7; y0++)
            {
                for (int x0 = 0; x0 < 7; x0++)
                {
                    //if (simGrid[x0][y0] == currentColor)
                    if(!simGrid.isEmpty(x0,y0)&&simGrid.issame(x0,y0,currentColor))
                    {
                        for (int dir = 0; dir < 24; dir++)
                        {
                            int x1 = x0 + delta[dir][0];
                            int y1 = y0 + delta[dir][1];
                            if (inMap(x1, y1) && simGrid.isEmpty(x1,y1))
                            {
                                //int tempGrid[7][7];
                                int tempBlack = simBlack, tempWhite = simWhite;
                               // memcpy(tempGrid, simGrid, sizeof(simGrid));
                               struct BitBoard tempGrid=simGrid;

                                int dx = abs(x0 - x1), dy = abs(y0 - y1);
                                if (dx == 2 || dy == 2)
                                    tempGrid.white&=(1<<(x0*7+y0))^1,
                                tempGrid.black&=(1<<(x0*7+y0))^1;
                                    //tempGrid[x0][y0] = 0;
                                else if (currentColor == 1)
                                    tempBlack++;
                                else
                                    tempWhite++;

                                //tempGrid[x1][y1] = currentColor;
                                tempGrid.setColor(x1,y1,currentColor);
                                int currCount = 0;

                                for (int dir2 = 0; dir2 < 8; dir2++)
                                {
                                    int x = x1 + delta[dir2][0];
                                    int y = y1 + delta[dir2][1];
                                    if (!inMap(x, y))
                                        continue;
                                    if (!tempGrid.isEmpty(x,y)&&tempGrid.issame(x,y,currentColor))
                                    {
                                        currCount++;
                                        //tempGrid[x][y] = currentColor;
                                        tempGrid.setColor(x,y,currentColor);
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

                                int score = 0;
                                score += 5 * (currentColor == 1 ? (tempBlack - tempWhite) : (tempWhite - tempBlack));
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
                currentColor = -currentColor;
                gameOver = IsGameOver();
                steps++;
                continue;
            }

            int choice = 0;
            if (rand() % 10 < 8)
            {
                if (currentColor == currBotColor)
                {
                    choice = max_element(moveScores.begin(), moveScores.end()) - moveScores.begin();
                }
                else
                {
                    choice = min_element(moveScores.begin(), moveScores.end()) - moveScores.begin();
                }
            }
            else
            {
                choice = rand() % moves.size();
            }

            auto move = moves[choice];

            int dx = abs(move.first.first - move.second.first);
            int dy = abs(move.first.second - move.second.second);

if (dx == 2 || dy == 2)
    simGrid.black &= ~(1ULL << (move.first.first*7 + move.first.second)),
    simGrid.white &= ~(1ULL << (move.first.first*7 + move.first.second));
            else if (currentColor == 1)
                simBlack++;
            else
                simWhite++;

           // simGrid[move.second.first][move.second.second] = currentColor;
           simGrid.setColor(move.second.first,move.second.second,currentColor);
            int captureCount = 0;

            for (int adjDir = 0; adjDir < 8; adjDir++)
            {
                int x = move.second.first + delta[adjDir][0];
                int y = move.second.second + delta[adjDir][1];
                if (!inMap(x, y))
                    continue;
               // if (simGrid[x][y] == -currentColor)
               if(!simGrid.isEmpty(x,y)&&!simGrid.issame(x,y,-currentColor))
                {
                    captureCount++;
                    //simGrid[x][y] = currentColor;
                    simGrid.setColor(x,y,currentColor);
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

            currentColor = -currentColor;
            steps++;
            gameOver = (simBlack == 0 || simWhite == 0) || (steps >= maxDepth);
        }
        int finalScore = (currBotColor == 1) ? (simBlack - simWhite) : (simWhite - simBlack);
        if (finalScore > 0)
            wins++;
    }

    // 恢复原始状态
    //memcpy(gridInfo, originalGrid, sizeof(gridInfo));
    gridInfo=originalGrid;
    blackPieceCount = originalBlack;
    whitePieceCount = originalWhite;

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

    //gridInfo[0][0] = gridInfo[6][6] = 1;   // |黑|白|
    //gridInfo[6][0] = gridInfo[0][6] = -1;  // |白|黑|

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
           // if (gridInfo[x0][y0] != currBotColor)

if(gridInfo.isEmpty(x0,y0) || !gridInfo.issame(x0,y0,currBotColor))
                continue;
            for (int dir = 0; dir < 24; dir++)
            {
                x1 = x0 + delta[dir][0];
                y1 = y0 + delta[dir][1];
                if (!inMap(x1, y1)) continue;
                //if (gridInfo[x1][y1] != 0) continue;
                if(!gridInfo.isEmpty(x1,y1))continue;
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
            //int tempGrid[7][7];
            //memcpy(tempGrid, gridInfo, sizeof(gridInfo));
            BitBoard tempGrid=gridInfo;
            int tempBlack = blackPieceCount, tempWhite = whitePieceCount;

            ProcStep(move.first.first, move.first.second, move.second.first, move.second.second, currBotColor, true);
            moveScores[i] = {Evaluate(), (int)i};

            //memcpy(gridInfo, tempGrid, sizeof(gridInfo));
            gridInfo=tempGrid;
            blackPieceCount = tempBlack;
            whitePieceCount = tempWhite;
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


#include <iostream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <climits>
using namespace std;

// —— Bitboard 定义 —— 
using U64 = unsigned long long;
const int BOARD_SIZE = 7;
const int MAX_CELLS = BOARD_SIZE * BOARD_SIZE;
const U64 FULL_MASK = (1ULL << MAX_CELLS) - 1;

// 预计算掩码表
U64 mask[MAX_CELLS];
U64 clonemask[MAX_CELLS];
U64 jumpmask[MAX_CELLS];

// 将 (x,y) 转换到 bit 索引
inline int idx(int x, int y) { return x * BOARD_SIZE + y; }

// 大道至简,疯狂杀杀杀
int evaluate(U64 board, U64 active) {
    int my = __builtin_popcountll(board & active);
    int opp = __builtin_popcountll((~board) & active);
    return my - opp;
}

// 生成所有子节点
void generate_moves(U64 board, U64 active, vector<pair<U64,U64>>& out) {
    U64 mycells = board & active;
    U64 clone_moves = 0;
    // 克隆
    for (U64 b = mycells; b; b &= b-1) {
        int i = __builtin_ctzll(b);
        clone_moves |= clonemask[i];
    }
    clone_moves &= ~active;
    for (U64 m = clone_moves; m; m &= m-1) {
        int to = __builtin_ctzll(m);
        U64 nb = board | (1ULL<<to) | clonemask[to];
        U64 na = active | (1ULL<<to);
        out.emplace_back(nb, na);
    }
    // 跳跃
    for (U64 b = mycells; b; b &= b-1) {
        int i = __builtin_ctzll(b);
        U64 jm = jumpmask[i] & ~active;
        for (U64 m = jm; m; m &= m-1) {
            int to = __builtin_ctzll(m);
            U64 nb = board | (1ULL<<to) | clonemask[to];
            U64 na = (active | (1ULL<<to)) & ~(1ULL<<i);
            out.emplace_back(nb, na);
        }
    }
    // 如果没有走法，则“pass”
    if (out.empty()) {
        out.emplace_back(board, active);
    }
}

// Negamax + alpha-beta 剪枝
// 之前的蒙特卡洛模拟,在我查阅论文的时候发现好像并不适合Ataxx,其中一个表现非常好的是这个negamax算法,它是修改版本的minimax算法
int negamax(U64 board, U64 active, int depth, int alpha, int beta, U64 &outB, U64 &outA) {
    if (depth == 0) {
        return evaluate(board, active);
    }
    vector<pair<U64,U64>> moves;
    generate_moves(board, active, moves);
    int bestScore = INT_MIN;
    U64 bestB = board, bestA = active;
    for (auto &mv : moves) {
        U64 nb = mv.first, na = mv.second;
        int score = -negamax(~nb & FULL_MASK, na, depth-1, -beta, -alpha, outB, outA);
        if (score > bestScore) {
            bestScore = score;
            bestB = nb;
            bestA = na;
        }
        alpha = max(alpha, score);
        if (alpha >= beta) break;
    }
    outB = bestB;
    outA = bestA;
    return bestScore;
}

int currBotColor; // 我所执子颜色（1为黑，-1为白）
int gridInfo[7][7] = { 0 }; // 先 x 后 y，记录棋盘状态
int blackPieceCount = 2, whitePieceCount = 2;

inline bool inMap(int x, int y) {
    return x >= 0 && x < 7 && y >= 0 && y < 7;
}

bool ProcStep(int x0, int y0, int x1, int y1, int color) {
    if (color == 0) return false;
    if (x1 == -1) return true; // pass
    if (!inMap(x0,y0) || !inMap(x1,y1)) return false;
    if (gridInfo[x0][y0] != color) return false;
    int dx = abs(x0-x1), dy = abs(y0-y1);
    if ((dx==0 && dy==0) || dx>2 || dy>2) return false;
    if (gridInfo[x1][y1] != 0) return false;
    // 克隆 or 跳跃
    if (dx==2 || dy==2) {
        gridInfo[x0][y0] = 0;
        if (color==1) blackPieceCount--; else whitePieceCount--;
    } else {
        if (color==1) blackPieceCount++; else whitePieceCount++;
    }
    gridInfo[x1][y1] = color;
    // 翻转邻近
    int flips=0;
    for (int dx2=-1;dx2<=1;dx2++) for(int dy2=-1;dy2<=1;dy2++){
        if (dx2==0 && dy2==0) continue;
        int xx=x1+dx2, yy=y1+dy2;
        if (!inMap(xx,yy)) continue;
        if (gridInfo[xx][yy]==-color) {
            gridInfo[xx][yy]=color;
            flips++;
        }
    }
    if (flips) {
        if (color==1) { blackPieceCount+=flips; whitePieceCount-=flips; }
        else          { whitePieceCount+=flips; blackPieceCount-=flips; }
    }
    return true;
}

int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // —— 初始化位棋盘掩码 —— 
    for (int i = 0; i < MAX_CELLS; i++) {
        mask[i] = 1ULL << i;
    }
    // 预计算 delta
    vector<pair<int,int>> dirs;
    for (int dx=-2;dx<=2;dx++) for(int dy=-2;dy<=2;dy++){
        if (dx==0 && dy==0) continue;
        if (abs(dx)<=2 && abs(dy)<=2) dirs.emplace_back(dx,dy);
    }
    // 填 clonemask/jumpmask
    for (int x=0;x<7;x++) for(int y=0;y<7;y++){
        int i = idx(x,y);
        U64 cm=0, jm=0;
        for (auto &d: dirs) {
            int nx = x+d.first, ny = y+d.second;
            if (!inMap(nx,ny)) continue;
            int j = idx(nx,ny);
            if (abs(d.first)<=1 && abs(d.second)<=1) {
                cm |= mask[j];
            } else {
                jm |= mask[j];
            }
        }
        clonemask[i] = cm;
        jumpmask[i]  = jm;
    }

    // —— 初始化 gridInfo 与 Botzone 协议恢复 —— 
    gridInfo[0][0] = gridInfo[6][6] = 1;
    gridInfo[6][0] = gridInfo[0][6] = -1;

    int turnID;
    cin >> turnID;
    currBotColor = -1; // 初始假设白方
    int x0,y0,x1,y1;
    for (int t=0; t<turnID-1; t++) {
        cin >> x0>>y0>>x1>>y1;
        if (x1>=0) ProcStep(x0,y0,x1,y1, -currBotColor);
        else currBotColor = 1;
        cin >> x0>>y0>>x1>>y1;
        if (x1>=0) ProcStep(x0,y0,x1,y1, currBotColor);
    }
    // 本回合对方
    cin >> x0>>y0>>x1>>y1;
    if (x1>=0) ProcStep(x0,y0,x1,y1, -currBotColor);
    else currBotColor = 1;

    // 构造 bitboard
    U64 board = 0, active = 0;
    for (int i=0;i<7;i++) for(int j=0;j<7;j++){
        if (gridInfo[i][j]!=0) {
            active |= mask[idx(i,j)];
            if (gridInfo[i][j] == currBotColor) {
                board |= mask[idx(i,j)];
            }
        }
    }
    // 调用搜索（深度可根据需要调整）
    U64 nextB, nextA;
    int DEPTH = 5;
    negamax(board, active, DEPTH, INT_MIN/2, INT_MAX/2, nextB, nextA);

    // 找到差异格子：从 board->nextB 和 active->nextA 推出走法
    U64 added = nextA & ~active;
    U64 removed = (active & ~nextA);
    int sx=-1, sy=-1, tx=-1, ty=-1;
    if (added) {
        int to = __builtin_ctzll(added);
        tx = to / BOARD_SIZE; ty = to % BOARD_SIZE;
    }
    if (removed) {
        int fr = __builtin_ctzll(removed);
        sx = fr / BOARD_SIZE; sy = fr % BOARD_SIZE;
    }
    // 如果没有 removed（克隆走法），则起点任取 added 邻近的己方原点
    if (sx<0 && added) {
        for (int d=0;d<MAX_CELLS;d++){
            if ((board & mask[d]) && (clonemask[d] & added)) {
                sx = d / BOARD_SIZE; sy = d % BOARD_SIZE;
                break;
            }
        }
    }

    // 输出结果
    if (sx<0) {
        cout << "-1 -1 -1 -1\n"; // pass
    } else {
        cout << sx << " " << sy << " " << tx << " " << ty << "\n";
    }
    return 0;
}

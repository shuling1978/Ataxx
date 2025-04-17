#include <iostream>
#include <vector>
#include <cmath>
#include <fstream>
#include <random>
#include <algorithm>
#include <bitset>
#include <tuple>
#include <climits>
using namespace std;

using U64 = unsigned long long;
const int BOARD_SIZE = 7;
const int MAX_CELLS = BOARD_SIZE * BOARD_SIZE;
const U64 FULL_MASK = (1ULL << MAX_CELLS) - 1;
const U64 FULL_BOARD = (1ULL << 49) - 1;

// Precomputed masks
U64 mask[MAX_CELLS];
U64 clonemask[MAX_CELLS];
U64 jumpmask[MAX_CELLS];

inline int idx(int x, int y) { return x * BOARD_SIZE + y; }

struct Layer {
    vector<vector<double>> weights;
    vector<double> biases;
    string activation;

    Layer(int input_size, int output_size, const string& act = "relu")
        : activation(act) {
        random_device rd;
        mt19937 gen(rd());
        normal_distribution<> dist(0.0, sqrt(2.0 / input_size));

        weights.resize(output_size, vector<double>(input_size));
        biases.resize(output_size, 0.1);

        for (auto& row : weights) {
            for (auto& w : row) {
                w = dist(gen);
            }
        }
    }

    inline double activate(double x) const {
        if (activation == "relu") return max(0.0, x);
        if (activation == "tanh") return tanh(x);
        if (activation == "sigmoid") return 1.0 / (1.0 + exp(-x));
        return x;
    }

    vector<double> forward(const vector<double>& inputs) const {
        vector<double> outputs(biases.size());
        for (size_t i = 0; i < biases.size(); ++i) {
            double sum = biases[i];
            for (size_t j = 0; j < inputs.size(); ++j) {
                sum += weights[i][j] * inputs[j];
            }
            outputs[i] = activate(sum);
        }
        return outputs;
    }
};

struct NeuralNetwork {
    vector<Layer> layers;

    NeuralNetwork(const vector<int>& layer_sizes, const vector<string>& activations) {
        for (size_t i = 1; i < layer_sizes.size(); ++i) {
            layers.emplace_back(layer_sizes[i-1], layer_sizes[i],
                              i < activations.size() ? activations[i-1] : "relu");
        }
    }

    double predict(const vector<double>& inputs) const {
        vector<double> activation = inputs;
        for (const auto& layer : layers) {
            activation = layer.forward(activation);
        }
        return activation[0];
    }

    void train(const vector<vector<double>>& batch_inputs,
               const vector<double>& batch_targets,
               double learning_rate) {
        const double alpha = learning_rate / batch_inputs.size();

        for (size_t i = 0; i < batch_inputs.size(); ++i) {
            vector<vector<double>> activations;
            vector<double> input = batch_inputs[i];

            activations.push_back(input);
            for (const auto& layer : layers) {
                input = layer.forward(input);
                activations.push_back(input);
            }

            double error = activations.back()[0] - batch_targets[i];
            vector<double> delta(1, error);

            for (int l = layers.size()-1; l >= 0; --l) {
                vector<double> new_delta(layers[l].weights[0].size(), 0.0);

                for (size_t n = 0; n < layers[l].weights.size(); ++n) {
                    double grad = delta[n] * (1.0 - pow(activations[l+1][n], 2));

                    for (size_t w = 0; w < layers[l].weights[n].size(); ++w) {
                        layers[l].weights[n][w] -= alpha * grad * activations[l][w];
                        new_delta[w] += grad * layers[l].weights[n][w];
                    }
                    layers[l].biases[n] -= alpha * grad;
                }
                delta = move(new_delta);
            }
        }
    }
};

void generate_moves(U64 board, U64 active, vector<pair<U64,U64>>& out) {
    U64 mycells = board & active;
    U64 clone_moves = 0;

    // Clone moves
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

    // Jump moves
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
}

int negamax(NeuralNetwork& nn, U64 my_pieces, U64 active, int depth, int alpha, int beta,
            U64& outB, U64& outA, int player) {
    if (depth == 0 || active == FULL_BOARD) {
        vector<double> input(49);
        for (int i = 0; i < 49; ++i) {
            input[i] = (my_pieces & (1ULL << i)) ? 1.0 :
                      ((active & (1ULL << i)) ? -1.0 : 0.0);
        }
        return nn.predict(input);
    }

    vector<pair<U64, U64>> moves;
    generate_moves(my_pieces, active, moves);

    if (moves.empty()) {
        outB = my_pieces;
        outA = active;
        vector<double> input(49);
        for (int i = 0; i < 49; ++i) {
            input[i] = (my_pieces & (1ULL << i)) ? 1.0 :
                      ((active & (1ULL << i)) ? -1.0 : 0.0);
        }
        return nn.predict(input);
    }

    int bestScore = INT_MIN;
    U64 bestMyPieces = my_pieces;
    U64 bestActive = active;

    for (auto &mv : moves) {
        U64 new_my_pieces = mv.first;
        U64 new_active = mv.second;

        int score = -negamax(nn, ~new_my_pieces & FULL_MASK, new_active,
                            depth-1, -beta, -alpha, outB, outA, -player);

        if (score > bestScore) {
            bestScore = score;
            bestMyPieces = new_my_pieces;
            bestActive = new_active;
            if (score > alpha) alpha = score;
            if (alpha >= beta) break;
        }
    }

    outB = bestMyPieces;
    outA = bestActive;
    return bestScore;
}

int currBotColor;
int gridInfo[7][7] = {0};
int blackPieceCount = 2, whitePieceCount = 2;

inline bool inMap(int x, int y) { return x >= 0 && x < 7 && y >= 0 && y < 7; }

bool ProcStep(int x0, int y0, int x1, int y1, int color) {
    if (color == 0 || !inMap(x0,y0) || !inMap(x1,y1)) return false;
    if (x1 == -1) return true;
    if (gridInfo[x0][y0] != color) return false;

    int dx = abs(x0-x1), dy = abs(y0-y1);
    if ((dx==0 && dy==0) || dx>2 || dy>2 || gridInfo[x1][y1] != 0) return false;

    if (dx==2 || dy==2) {
        gridInfo[x0][y0] = 0;
        if (color==1) blackPieceCount--; else whitePieceCount--;
    } else {
        if (color==1) blackPieceCount++; else whitePieceCount++;
    }

    gridInfo[x1][y1] = color;
    int flips = 0;
    for (int dx2=-1; dx2<=1; dx2++) {
        for (int dy2=-1; dy2<=1; dy2++) {
            if (dx2==0 && dy2==0) continue;
            int xx=x1+dx2, yy=y1+dy2;
            if (inMap(xx,yy) && gridInfo[xx][yy]==-color) {
                gridInfo[xx][yy] = color;
                flips++;
            }
        }
    }

    if (flips) {
        if (color==1) { blackPieceCount += flips; whitePieceCount -= flips; }
        else { whitePieceCount += flips; blackPieceCount -= flips; }
    }
    return true;
}

void load_model(NeuralNetwork& nn, const string& filename) {
    ifstream file(filename, ios::binary);
    for (auto& layer : nn.layers) {
        for (auto& weights : layer.weights) {
            file.read(reinterpret_cast<char*>(weights.data()), weights.size() * sizeof(double));
        }
        file.read(reinterpret_cast<char*>(layer.biases.data()), layer.biases.size() * sizeof(double));
    }
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    NeuralNetwork nn({49, 128, 64, 1}, {"relu", "relu", "tanh"});
    load_model(nn, "data\\ataxx_model.bin");

    // Initialize masks
    for (int i = 0; i < MAX_CELLS; i++) {
        mask[i] = 1ULL << i;
    }

    vector<pair<int,int>> dirs;
    for (int dx=-2; dx<=2; dx++) {
        for (int dy=-2; dy<=2; dy++) {
            if (dx==0 && dy==0) continue;
            if (abs(dx)<=2 && abs(dy)<=2) dirs.emplace_back(dx,dy);
        }
    }

    for (int x=0; x<7; x++) {
        for (int y=0; y<7; y++) {
            int i = idx(x,y);
            U64 cm=0, jm=0;
            for (auto &d: dirs) {
                int nx = x+d.first, ny = y+d.second;
                if (!inMap(nx,ny)) continue;
                int j = idx(nx,ny);
                if (abs(d.first)<=1 && abs(d.second)<=1) cm |= mask[j];
                else jm |= mask[j];
            }
            clonemask[i] = cm;
            jumpmask[i] = jm;
        }
    }

    gridInfo[0][0] = gridInfo[6][6] = 1;
    gridInfo[6][0] = gridInfo[0][6] = -1;

    int turnID;
    cin >> turnID;
    currBotColor = -1;
    int x0,y0,x1,y1;

    for (int t=0; t<turnID-1; t++) {
        cin >> x0>>y0>>x1>>y1;
        if (x1>=0) ProcStep(x0,y0,x1,y1, -currBotColor);
        else currBotColor = 1;
        cin >> x0>>y0>>x1>>y1;
        if (x1>=0) ProcStep(x0,y0,x1,y1, currBotColor);
    }

    cin >> x0>>y0>>x1>>y1;
    if (x1>=0) ProcStep(x0,y0,x1,y1, -currBotColor);
    else currBotColor = 1;

    U64 board = 0, active = 0;
    for (int i=0; i<7; i++) {
        for (int j=0; j<7; j++) {
            if (gridInfo[i][j] != 0) {
                active |= mask[idx(i,j)];
                if (gridInfo[i][j] == currBotColor) {
                    board |= mask[idx(i,j)];
                }
            }
        }
    }

    U64 nextB, nextA;
    const int DEPTH = 5;
    negamax(nn, board, active, DEPTH, INT_MIN/2, INT_MAX/2, nextB, nextA, currBotColor);

    U64 added = nextA & ~active;
    U64 removed = (active & ~nextA);
    int sx=-1, sy=-1, tx=-1, ty=-1;

    if (added) {
        int to = __builtin_ctzll(added);
        tx = to / BOARD_SIZE;
        ty = to % BOARD_SIZE;
    }
    if (removed) {
        int fr = __builtin_ctzll(removed);
        sx = fr / BOARD_SIZE;
        sy = fr % BOARD_SIZE;
    }

    if (sx<0 && added) {
        for (int d=0; d<MAX_CELLS; d++) {
            if ((board & mask[d]) && (clonemask[d] & added)) {
                sx = d / BOARD_SIZE;
                sy = d % BOARD_SIZE;
                break;
            }
        }
    }

    if (sx<0) {
        cout << "-1 -1 -1 -1\n";
    } else {
        cout << sx << " " << sy << " " << tx << " " << ty << "\n";
    }
    return 0;
}

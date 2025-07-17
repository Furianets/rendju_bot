#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <boost/asio.hpp>
#include <json/json.h>
#include <algorithm>
#include <limits>
#include <unordered_map>
#include <random>

using boost::asio::ip::tcp;

class RenjuBot {
private:
    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    std::vector<std::vector<char>> board_;
    const int BOARD_SIZE = 31;
    const int WIN_LENGTH = 5;
    const std::chrono::seconds MOVE_TIMEOUT{5};
    const std::string TEAM_NAME = "TEAM ANGLERS";
    const int MAX_DEPTH = 1;
    std::chrono::steady_clock::time_point start_time;
    bool is_black_turn = true;
    mutable std::unordered_map<std::string, int> evaluation_cache;
    std::random_device rd_;
    std::mt19937 gen_;

    std::string board_to_string() const {
        std::string state;
        for (const auto& row : board_) {
            for (char cell : row) {
                state += cell;
            }
        }
        return state;
    }

    void initialize_board() {
        board_ = std::vector<std::vector<char>>(BOARD_SIZE, std::vector<char>(BOARD_SIZE, '.'));
        evaluation_cache.clear();
        gen_ = std::mt19937(rd_());
    }

    bool is_valid_move(int x, int y) const {
        return x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE && board_[x][y] == '.';
    }

    bool is_center_move(int x, int y) const {
        return x == 15 && y == 15;
    }

    bool check_win(int x, int y, char player) const {
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx == 0 && dy == 0) continue;
                int count = 1;
                for (int step = 1; step < WIN_LENGTH; ++step) {
                    int nx = x + dx * step;
                    int ny = y + dy * step;
                    if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board_[nx][ny] == player) {
                        count++;
                    } else {
                        break;
                    }
                }
                for (int step = 1; step < WIN_LENGTH; ++step) {
                    int nx = x - dx * step;
                    int ny = y - dy * step;
                    if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board_[nx][ny] == player) {
                        count++;
                    } else {
                        break;
                    }
                }
                if (count >= WIN_LENGTH) {
                    return true;
                }
            }
        }
        return false;
    }

    int evaluate_position(char player, char opponent) const {
    std::string board_state = board_to_string();
    if (evaluation_cache.find(board_state) != evaluation_cache.end()) {
        return evaluation_cache.at(board_state);
    }

    int score = 0;
    int center = BOARD_SIZE / 2;
    for (int x = 0; x < BOARD_SIZE; ++x) {
        for (int y = 0; y < BOARD_SIZE; ++y) {
            if (board_[x][y] != '.') continue;
            int center_bonus = 20 - (std::abs(x - center) + std::abs(y - center)) * 2;
            int line_bonus = 0;
            
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    if (dx == 0 && dy == 0) continue;
                    int count = 0;
                    for (int step = 1; step < WIN_LENGTH; ++step) {
                        int nx = x + dx * step;
                        int ny = y + dy * step;
                        if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board_[nx][ny] == player) {
                            count++;
                        } else {
                            break;
                        }
                    }
                    line_bonus += (count == 2 ? 50 : count == 3 ? 200 : 0);
                }
            }
            int min_distance = BOARD_SIZE;
            for (int i = 0; i < BOARD_SIZE; ++i) {
                for (int j = 0; j < BOARD_SIZE; ++j) {
                    if (board_[i][j] != '.') {
                        int dist = std::abs(x - i) + std::abs(y - j);
                        min_distance = std::min(min_distance, dist);
                    }
                }
            }
            score += center_bonus + line_bonus + (min_distance > 2 ? 10 : 0);
        }
    }
    evaluation_cache[board_state] = score;
    return score;
}

    bool find_blocking_move(int& best_x, int& best_y, char player, char opponent) {
    for (int x = 0; x < BOARD_SIZE; ++x) {
        for (int y = 0; y < BOARD_SIZE; ++y) {
            if (!is_valid_move(x, y)) continue;
            board_[x][y] = opponent;
            bool is_threat = false;
            
            if (check_win(x, y, opponent)) {
                is_threat = true;
            } else {
                
                for (int dx = -1; dx <= 1; ++dx) {
                    for (int dy = -1; dy <= 1; ++dy) {
                        if (dx == 0 && dy == 0) continue;
                        int count = 1;
                        for (int step = 1; step < WIN_LENGTH - 1; ++step) {
                            int nx = x + dx * step;
                            int ny = y + dy * step;
                            if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board_[nx][ny] == opponent) {
                                count++;
                            } else {
                                break;
                            }
                        }
                        if (count >= 4) is_threat = true;
                        
                        if (count == 3) {
                            for (int ddx = -1; ddx <= 1; ++ddx) {
                                for (int ddy = -1; ddy <= 1; ++ddy) {
                                    if ((ddx == 0 && ddy == 0) || (ddx == dx && ddy == dy)) continue;
                                    int other_count = 0;
                                    for (int step = 1; step < WIN_LENGTH - 1; ++step) {
                                        int nx = x + ddx * step;
                                        int ny = y + ddy * step;
                                        if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board_[nx][ny] == opponent) {
                                            other_count++;
                                        } else {
                                            break;
                                        }
                                    }
                                    if (other_count >= 3) is_threat = true;
                                }
                            }
                        }
                    }
                }
            }
            if (is_threat) {
                board_[x][y] = '.';
                if (is_valid_move(x, y)) {
                    best_x = x;
                    best_y = y;
                    return true;
                }
            }
            board_[x][y] = '.';
        }
    }
    return false;
}

    int minimax(int depth, int alpha, int beta, bool maximizing, char player, char opponent) {
        if (std::chrono::steady_clock::now() - start_time > MOVE_TIMEOUT || depth < 0) {
            return 0;
        }

        for (int x = 0; x < BOARD_SIZE; ++x) {
            for (int y = 0; y < BOARD_SIZE; ++y) {
                if (!is_valid_move(x, y)) continue;
                board_[x][y] = maximizing ? player : opponent;
                if (check_win(x, y, maximizing ? player : opponent)) {
                    board_[x][y] = '.';
                    return maximizing ? 1000000 - depth : -1000000 + depth;
                }
                board_[x][y] = '.';
            }
        }

        if (depth == 0) return evaluate_position(player, opponent);

        std::vector<std::pair<int, int>> moves;
        for (int x = 0; x < BOARD_SIZE; ++x) {
            for (int y = 0; y < BOARD_SIZE; ++y) {
                if (is_valid_move(x, y)) moves.emplace_back(x, y);
            }
        }
        if (moves.empty()) return 0;

        int best_score = maximizing ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();
        for (const auto& move : moves) {
            int x = move.first, y = move.second;
            board_[x][y] = maximizing ? player : opponent;
            int score = minimax(depth - 1, alpha, beta, !maximizing, player, opponent);
            board_[x][y] = '.';
            if (maximizing) {
                best_score = std::max(best_score, score);
                alpha = std::max(alpha, best_score);
            } else {
                best_score = std::min(best_score, score);
                beta = std::min(beta, best_score);
            }
            if (beta <= alpha) break;
        }
        return best_score;
    }

    bool find_first_valid_move(int& best_x, int& best_y) {
        for (int x = 0; x < BOARD_SIZE; ++x) {
            for (int y = 0; y < BOARD_SIZE; ++y) {
                if (is_valid_move(x, y)) {
                    best_x = x;
                    best_y = y;
                    return true;
                }
            }
        }
        return false;
    }

    void find_best_move(int opponent_x, int opponent_y, int& best_x, int& best_y, bool is_first_move) {
        start_time = std::chrono::steady_clock::now();

        if (is_first_move && is_black_turn) {
            if (is_valid_move(15, 15)) {
                best_x = 15;
                best_y = 15;
                return;
            } else if (find_first_valid_move(best_x, best_y)) {
                return;
            }
        }

        char player = is_black_turn ? 'B' : 'W';
        char opponent = is_black_turn ? 'W' : 'B';

        
        if (find_blocking_move(best_x, best_y, player, opponent)) {
            if (is_valid_move(best_x, best_y)) {
                return;
            }
        }

        
        int best_score = std::numeric_limits<int>::min();
        best_x = -1;
        best_y = -1;

        int search_range = 4;
        int start_x = std::max(0, opponent_x - search_range);
        int end_x = std::min(BOARD_SIZE - 1, opponent_x + search_range);
        int start_y = std::max(0, opponent_y - search_range);
        int end_y = std::min(BOARD_SIZE - 1, opponent_y + search_range);

        std::vector<std::pair<int, int>> best_moves;
        for (int x = start_x; x <= end_x; ++x) {
            for (int y = start_y; y <= end_y; ++y) {
                if (is_valid_move(x, y)) {
                    best_moves.emplace_back(x, y);
                }
            }
        }

        if (best_moves.empty()) {
            for (int x = 0; x < BOARD_SIZE; ++x) {
                for (int y = 0; y < BOARD_SIZE; ++y) {
                    if (is_valid_move(x, y)) {
                        best_moves.emplace_back(x, y);
                    }
                }
            }
        }

        if (best_moves.empty() && find_first_valid_move(best_x, best_y)) {
            return;
        }

        for (const auto& move : best_moves) {
            int x = move.first, y = move.second;
            board_[x][y] = player;
            if (check_win(x, y, player)) {
                board_[x][y] = '.';
                best_x = x;
                best_y = y;
                return;
            }
            int score = minimax(MAX_DEPTH - 1, std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), false, player, opponent);
            board_[x][y] = '.';
            if (score > best_score) {
                best_score = score;
                best_x = x;
                best_y = y;
            }
            if (std::chrono::steady_clock::now() - start_time > MOVE_TIMEOUT) {
                break;
            }
        }

        if (best_x == -1 || best_y == -1 || !is_valid_move(best_x, best_y)) {
            if (!find_first_valid_move(best_x, best_y)) {
                best_x = -1;
                best_y = -1;
            }
        }
    }

public:
    RenjuBot(boost::asio::io_context& io_context, int port)
        : io_context_(io_context), acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        initialize_board();
    }

    void start() {
        for (;;) {
            tcp::socket socket(io_context_);
            acceptor_.accept(socket);

            boost::asio::streambuf buffer;
            boost::system::error_code error;

            boost::asio::read_until(socket, buffer, "\n", error);
            if (error && error != boost::asio::error::eof) {
                continue;
            }

            std::istream is(&buffer);
            std::string line;
            std::getline(is, line);

            if (line.empty()) {
                continue;
            }

            Json::CharReaderBuilder builder;
            Json::Value root;
            std::string errs;
            std::istringstream s(line);
            if (!Json::parseFromStream(builder, s, &root, &errs)) {
                Json::Value response;
                response["error"] = "Invalid JSON format: " + errs;
                Json::StreamWriterBuilder writer;
                std::string output = Json::writeString(writer, response) + "\n";
                boost::asio::write(socket, boost::asio::buffer(output), error);
                continue;
            }

            Json::Value response;
            std::string command = root.get("command", "").asString();

            auto move_start_time = std::chrono::steady_clock::now();
            if (std::chrono::steady_clock::now() - move_start_time > MOVE_TIMEOUT) {
                response["error"] = "Move timeout";
            } else if (command == "start") {
                if (!is_black_turn) {
                    response["error"] = "White cannot make first move";
                } else {
                    int x = 15, y = 15;
                    if (!is_valid_move(x, y)) {
                        response["error"] = "Center is occupied";
                    } else {
                        board_[x][y] = 'B';
                        response["move"]["x"] = x;
                        response["move"]["y"] = y;
                        response["team"] = TEAM_NAME;
                    }
                }
            } else if (command == "move") {
                auto opponentMove = root["opponentMove"];
                int x = opponentMove.get("x", -1).asInt();
                int y = opponentMove.get("y", -1).asInt();
                if (x == -1 || y == -1 || !is_valid_move(x, y)) {
                    response["error"] = "Invalid opponent move";
                } else {
                    board_[x][y] = is_black_turn ? 'W' : 'B';
                    int nx, ny;
                    find_best_move(x, y, nx, ny, false);
                    if (nx == -1 || ny == -1 || !is_valid_move(nx, ny)) {
                        if (find_first_valid_move(nx, ny)) {
                            board_[nx][ny] = is_black_turn ? 'B' : 'W';
                            response["move"]["x"] = nx;
                            response["move"]["y"] = ny;
                            response["team"] = TEAM_NAME;
                        } else {
                            response["error"] = "No valid move available";
                        }
                    } else {
                        board_[nx][ny] = is_black_turn ? 'B' : 'W';
                        response["move"]["x"] = nx;
                        response["move"]["y"] = ny;
                        response["team"] = TEAM_NAME;
                    }
                }
            } else if (command == "reset") {
                initialize_board();
                is_black_turn = !is_black_turn;
                response["reply"] = "ok";
            } else {
                response["error"] = "Unknown command";
            }

            Json::StreamWriterBuilder writer;
            std::string output = Json::writeString(writer, response) + "\n";
            boost::asio::write(socket, boost::asio::buffer(output), error);

            socket.shutdown(tcp::socket::shutdown_both);
            socket.close();
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: rendju-bot -p<port>\n";
        return 1;
    }

    int port = 0;
    std::string port_arg = argv[1];
    if (port_arg.size() > 2 && port_arg.substr(0, 2) == "-p") {
        port = std::stoi(port_arg.substr(2));
        if (port < 1024 || port > 65535) {
            std::cerr << "Port must be between 1024 and 65535\n";
            return 1;
        }
    } else {
        std::cerr << "Invalid port argument\n";
        return 1;
    }

    boost::asio::io_context io_context;
    RenjuBot bot(io_context, port);
    bot.start();

    return 0;
}

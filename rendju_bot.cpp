#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <boost/asio.hpp>
#include <json/json.h>

using boost::asio::ip::tcp;

class RenjuBot {
private:
    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    std::vector<std::vector<char>> board_;
    const int BOARD_SIZE = 31;
    const int WIN_LENGTH = 5;
    const std::chrono::seconds MOVE_TIMEOUT{ 5 };
    const std::string TEAM_NAME = "TEAM ANGLERS";

    void initialize_board() {
        board_ = std::vector<std::vector<char>>(BOARD_SIZE, std::vector<char>(BOARD_SIZE, '.'));
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
                    }
                    else {
                        break;
                    }
                }
                for (int step = 1; step < WIN_LENGTH; ++step) {
                    int nx = x - dx * step;
                    int ny = y - dy * step;
                    if (nx >= 0 && nx < BOARD_SIZE && ny >= 0 && ny < BOARD_SIZE && board_[nx][ny] == player) {
                        count++;
                    }
                    else {
                        break;
                    }
                }
                if (count >= WIN_LENGTH) return true;
            }
        }
        return false;
    }

    void find_best_move(int opponent_x, int opponent_y, int& best_x, int& best_y) {
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx == 0 && dy == 0) continue;
                for (int step = -2; step <= 2; ++step) {
                    if (step == 0) continue;
                    int nx = opponent_x + dx * step;
                    int ny = opponent_y + dy * step;
                    if (is_valid_move(nx, ny)) {
                        board_[nx][ny] = 'B';
                        if (check_win(nx, ny, 'B')) {
                            board_[nx][ny] = '.';
                            best_x = nx;
                            best_y = ny;
                            return;
                        }
                        board_[nx][ny] = '.';
                    }
                }
            }
        }
        int directions[8][2] = { {0,1}, {1,0}, {0,-1}, {-1,0}, {1,1}, {1,-1}, {-1,1}, {-1,-1} };
        for (const auto& dir : directions) {
            int nx = opponent_x + dir[0];
            int ny = opponent_y + dir[1];
            if (is_valid_move(nx, ny)) {
                best_x = nx;
                best_y = ny;
                return;
            }
        }
        for (int x = 0; x < BOARD_SIZE; ++x) {
            for (int y = 0; y < BOARD_SIZE; ++y) {
                if (is_valid_move(x, y)) {
                    best_x = x;
                    best_y = y;
                    return;
                }
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
            auto start_time = std::chrono::steady_clock::now();
            try {
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
                    continue;
                }

                Json::Value response;
                std::string command = root.get("command", "").asString();

                if (std::chrono::steady_clock::now() - start_time > MOVE_TIMEOUT) {
                    response["error"] = "Move timeout";
                }
                else if (command == "start") {
                    int x = 15, y = 15;
                    if (!is_center_move(x, y)) {
                        response["error"] = "First move must be at center (15,15)";
                    }
                    else {
                        board_[x][y] = 'B';
                        response["move"]["x"] = x;
                        response["move"]["y"] = y;
                        response["team"] = TEAM_NAME;
                    }
                }
                else if (command == "move") {
                    auto opponentMove = root["opponentMove"];
                    int x = opponentMove.get("x", -1).asInt();
                    int y = opponentMove.get("y", -1).asInt();
                    if (!is_valid_move(x, y)) {
                        response["error"] = "Invalid opponent move";
                    }
                    else {
                        board_[x][y] = 'W';
                        int nx, ny;
                        find_best_move(x, y, nx, ny);
                        board_[nx][ny] = 'B';
                        response["move"]["x"] = nx;
                        response["move"]["y"] = ny;
                        response["team"] = TEAM_NAME;
                    }
                }
                else if (command == "reset") {
                    initialize_board();
                    response["reply"] = "ok";
                }
                else {
                    response["error"] = "Unknown command";
                }

                Json::StreamWriterBuilder writer;
                std::string output = Json::writeString(writer, response) + "\n";
                boost::asio::write(socket, boost::asio::buffer(output), error);

                socket.shutdown(tcp::socket::shutdown_both);
                socket.close();
            }
            catch (const std::exception& e) {
            }
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
    try {
        if (port_arg.size() > 2 && port_arg.substr(0, 2) == "-p") {
            port = std::stoi(port_arg.substr(2));
            if (port < 1024 || port > 65535) {
                std::cerr << "Port must be between 1024 and 65535\n";
                return 1;
            }
        }
        else {
            std::cerr << "Invalid port argument\n";
            return 1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Invalid port number: " << e.what() << std::endl;
        return 1;
    }

    try {
        boost::asio::io_context io_context;
        RenjuBot bot(io_context, port);
        bot.start();
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

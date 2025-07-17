#include <iostream>
#include <string>
#include <boost/asio.hpp>
#include <json/json.h>

using boost::asio::ip::tcp;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: rendju-bot -p<port>\n";
        return 1;
    }

    int port = 0;
    std::string port_arg = argv[1];
    if (port_arg.size() > 2 && port_arg.substr(0, 2) == "-p") {
        port = std::stoi(port_arg.substr(2));
    }
    else {
        std::cerr << "Invalid port argument\n";
        return 1;
    }

    try {
        boost::asio::io_context io_context;

        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));
        std::cout << "Server started on port " << port << std::endl;

        for (;;) {
            tcp::socket socket(io_context);
            acceptor.accept(socket);

            boost::asio::streambuf buffer;
            boost::system::error_code error;

            boost::asio::read_until(socket, buffer, "\n", error);

            if (error && error != boost::asio::error::eof) {
                std::cerr << "Read error: " << error.message() << std::endl;
                continue;
            }

            std::istream is(&buffer);
            std::string line;
            std::getline(is, line);

            if (line.empty()) continue;

            Json::CharReaderBuilder builder;
            Json::Value root;
            std::string errs;

            std::istringstream s(line);
            bool parsingSuccessful = Json::parseFromStream(builder, s, &root, &errs);
            if (!parsingSuccessful) {
                std::cerr << "Failed to parse JSON: " << errs << std::endl;
                continue;
            }

            Json::Value response;

            std::string command = root.get("command", "").asString();

            if (command == "start") {
                response["move"]["x"] = 15;
                response["move"]["y"] = 15;
            }
            else if (command == "move") {
                auto opponentMove = root["opponentMove"];
                int x = opponentMove.get("x", 0).asInt();
                int y = opponentMove.get("y", 0).asInt();

                int nx = (x + 1 < 31) ? x + 1 : x;
                int ny = (y - 1 >= 0) ? y - 1 : y;

                response["move"]["x"] = nx;
                response["move"]["y"] = ny;
            }
            else if (command == "reset") {
                response["reply"] = "ok";
            }
            else {
                response["error"] = "Unknown command";
            }

            Json::StreamWriterBuilder writer;
            std::string output = Json::writeString(writer, response);
            output += "\n";

            boost::asio::write(socket, boost::asio::buffer(output), error);

            socket.shutdown(tcp::socket::shutdown_both);
            socket.close();
        }
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}


#include <boost/asio.hpp>

#include <array>
#include <iostream>
#include <string>

namespace asio = boost::asio;
using udp = asio::ip::udp;
using ErrorCode = boost::system::error_code;

int main()
{
    try
    {
        asio::io_context io;
        udp::socket socket(io, udp::v4());

        udp::endpoint server(
            asio::ip::make_address("127.0.0.1"),
            8082
        );

        std::string message = "Hello over UDP";

        socket.send_to(
            asio::buffer(message),
            server
        );

        std::array<char, 1024> buffer{};
        udp::endpoint sender;
        ErrorCode ec;

        std::size_t bytesReceived = socket.receive_from(
            asio::buffer(buffer),
            sender,
            0,
            ec
        );

        if (ec)
        {
            std::cerr << "Receive error: "
                      << ec.message() << '\n';
            return 1;
        }

        std::cout << "Server replied: ";

        std::cout.write(
            buffer.data(),
            static_cast<std::streamsize>(bytesReceived)
        );

        std::cout << '\n';
    }
    catch (const std::exception& e)
    {
        std::cerr << "Client error: "
                  << e.what() << '\n';
        return 1;
    }
}

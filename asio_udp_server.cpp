
#include <boost/asio.hpp>

#include <array>
#include <iostream>

namespace asio = boost::asio;
using udp = asio::ip::udp;
using ErrorCode = boost::system::error_code;

int main()
{
    try
    {
        asio::io_context io;

        udp::socket socket(
            io,
            udp::endpoint(udp::v4(), 8082)
        );

        std::array<char, 1024> buffer{};

        std::cout << "UDP Server listening on port 8082\n";

        while (true)
        {
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
                continue;
            }

            std::cout << "Received "
                      << bytesReceived << " bytes: ";

            std::cout.write(
                buffer.data(),
                static_cast<std::streamsize>(bytesReceived)
            );

            std::cout << '\n';

            const std::size_t bytesSent = socket.send_to(
                asio::buffer(buffer.data(), bytesReceived),
                sender,
                0,
                ec
            );

            if (ec)
            {
                std::cerr << "Send error: "
                          << ec.message() << '\n';
                continue;
            }

            std::cout << "Echoed "
                      << bytesSent << " bytes\n";
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Server error: "
                  << e.what() << '\n';
        return 1;
    }
}

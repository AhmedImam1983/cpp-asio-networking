
#include <boost/asio.hpp>
#include <array>
#include <iostream>

using boost::asio::ip::tcp;

int main()
{
    try
    {
        boost::asio::io_context io;

        tcp::acceptor acceptor(
            io,
            tcp::endpoint(tcp::v4(), 8080)
        );

        std::cout << "Boost.Asio Server listening on port 8080\n";

        while (true)
        {
            tcp::socket socket(io);

            acceptor.accept(socket);

            std::cout << "Client connected\n";

            std::array<char, 1024> buffer{};

            while (true)
            {
                boost::system::error_code ec;

                std::size_t bytesReceived =
                    socket.read_some(
                        boost::asio::buffer(buffer),
                        ec
                    );

                if (bytesReceived > 0)
                {
                    std::cout.write(
                        buffer.data(),
                        static_cast<std::streamsize>(bytesReceived)
                    );

                    std::cout << '\n';
                }

                if (ec == boost::asio::error::eof)
                {
                    std::cout << "Client disconnected\n";
                    break;
                }

                if (ec)
                {
                    std::cerr << "Read error: "
                              << ec.message() << '\n';
                    break;
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Server error: " << e.what() << '\n';
        return 1;
    }
}

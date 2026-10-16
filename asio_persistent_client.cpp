
#include <boost/asio.hpp>

#include <iostream>
#include <memory>
#include <string>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using ErrorCode = boost::system::error_code;

class Client : public std::enable_shared_from_this<Client>
{
private:
    tcp::socket socket_;
    asio::streambuf inputBuffer_;

    void readNextMessage()
    {
        auto self = shared_from_this();

        asio::async_read_until(
            socket_,
            inputBuffer_,
            '\n',
            [self](const ErrorCode& ec, std::size_t)
            {
                if (ec)
                {
                    std::cerr << "Connection ended: "
                              << ec.message() << '\n';
                    return;
                }

                std::istream input(&self->inputBuffer_);
                std::string message;
                std::getline(input, message);

                std::cout << "Server says: "
                          << message << '\n';

                if (message == "SHUTDOWN")
                {
                    std::cout << "Shutdown command received\n";

                    ErrorCode ignored;
                    self->socket_.close(ignored);
                    return;
                }

                // Keep listening for the next message.
                self->readNextMessage();
            }
        );
    }

public:
    explicit Client(asio::io_context& io)
        : socket_(io)
    {
    }

    void start()
    {
        auto self = shared_from_this();

        tcp::endpoint server(
            asio::ip::make_address("127.0.0.1"),
            8081
        );

        socket_.async_connect(
            server,
            [self](const ErrorCode& ec)
            {
                if (ec)
                {
                    std::cerr << "Connect error: "
                              << ec.message() << '\n';
                    return;
                }

                std::cout << "Connected; waiting for commands\n";
                self->readNextMessage();
            }
        );
    }
};

int main()
{
    try
    {
        asio::io_context io;
        auto client = std::make_shared<Client>(io);

        client->start();
        io.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Client error: " << e.what() << '\n';
        return 1;
    }
}

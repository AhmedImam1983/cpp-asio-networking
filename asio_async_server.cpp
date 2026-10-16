
#include <boost/asio.hpp>

#include <array>
#include <iostream>
#include <memory>
#include <utility>

namespace asio = boost::asio;

using tcp = asio::ip::tcp;
using ErrorCode = boost::system::error_code;

class Session : public std::enable_shared_from_this<Session>
{
private:
    tcp::socket socket_;
    boost::asio::streambuf inputBuffer_;
    std::string acknowledgement_ = "ACK\n";

    void doRead()
    {
        auto self = shared_from_this();

        boost::asio::async_read_until(
            socket_,
            inputBuffer_,
            '\n',
            [self](const ErrorCode& ec, std::size_t)
            {
                if (ec)
                {
                    if (ec == boost::asio::error::eof)
                    {
                        std::cout << "Client disconnected\n";
                    }
                    else
                    {
                        std::cerr << "Read error: "
                                  << ec.message() << '\n';
                    }

                    return;
                }

                // Extract one complete line from the input buffer.
                std::istream input(&self->inputBuffer_);
                std::string message;
                std::getline(input, message);

                std::cout << "Received: " << message << '\n';

                // Acknowledge only after receiving the complete line.
                boost::asio::async_write(
                    self->socket_,
                    boost::asio::buffer(self->acknowledgement_),
                    [self](const ErrorCode& writeError,
                           std::size_t bytesSent)
                    {
                        if (writeError)
                        {
                            std::cerr << "Write error: "
                                      << writeError.message() << '\n';
                            return;
                        }

                        std::cout << "ACK sent: "
                                  << bytesSent << " bytes\n";

                        // Wait for the next message.
                        self->doRead();
                    }
                );
            }
        );
    }

public:
    explicit Session(tcp::socket socket)
        : socket_(std::move(socket))
    {
    }

    void start()
    {
        doRead();
    }
};

class Server
{
private:
    tcp::acceptor acceptor_;

    void doAccept()
    {
        acceptor_.async_accept(
            [this](const ErrorCode& ec, tcp::socket socket)
            {
                if (!ec)
                {
                    std::cout << "Client connected\n";

                    std::make_shared<Session>(
                        std::move(socket)
                    )->start();
                }
                else
                {
                    std::cerr << "Accept error: "
                              << ec.message() << '\n';
                }

                // Wait asynchronously for another client.
                doAccept();
            }
        );
    }

public:
    Server(asio::io_context& io, unsigned short port)
        : acceptor_(io, tcp::endpoint(tcp::v4(), port))
    {
        doAccept();
    }
};

int main()
{
    try
    {
        asio::io_context io;

        Server server(io, 8080);

        std::cout << "Async server listening on port 8080\n";

        io.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Server error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}

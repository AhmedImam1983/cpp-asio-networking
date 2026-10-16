
#include <boost/asio.hpp>

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;
using ErrorCode = boost::system::error_code;

class Session : public std::enable_shared_from_this<Session>
{
private:
    tcp::socket socket_;
    asio::steady_timer timer_;

    std::string welcome_ = "CONNECTED\n";
    std::string shutdownMessage_ = "SHUTDOWN\n";

    void sendShutdown()
    {
        auto self = shared_from_this();

        asio::async_write(
            socket_,
            asio::buffer(shutdownMessage_),
            [self](const ErrorCode& ec, std::size_t)
            {
                if (ec)
                {
                    std::cerr << "Send error: "
                              << ec.message() << '\n';
                    return;
                }

                std::cout << "SHUTDOWN sent\n";

                ErrorCode ignored;
                self->socket_.shutdown(
                    tcp::socket::shutdown_both, ignored);
                self->socket_.close(ignored);
            }
        );
    }

public:
    Session(tcp::socket socket, asio::io_context& io)
        : socket_(std::move(socket)), timer_(io)
    {
    }

    void start()
    {
        auto self = shared_from_this();

        asio::async_write(
            socket_,
            asio::buffer(welcome_),
            [self](const ErrorCode& ec, std::size_t)
            {
                if (ec)
                {
                    std::cerr << "Welcome error: "
                              << ec.message() << '\n';
                    return;
                }

                std::cout << "Client connected; waiting 5 seconds\n";

                self->timer_.expires_after(
                    std::chrono::seconds(5));

                self->timer_.async_wait(
                    [self](const ErrorCode& timerError)
                    {
                        if (!timerError)
                            self->sendShutdown();
                    }
                );
            }
        );
    }
};

class Server
{
private:
    asio::io_context& io_;
    tcp::acceptor acceptor_;

    void doAccept()
    {
        acceptor_.async_accept(
            [this](const ErrorCode& ec, tcp::socket socket)
            {
                if (!ec)
                {
                    std::make_shared<Session>(
                        std::move(socket), io_)->start();
                }
                else
                {
                    std::cerr << "Accept error: "
                              << ec.message() << '\n';
                }

                doAccept();
            }
        );
    }

public:
    explicit Server(asio::io_context& io)
        : io_(io),
          acceptor_(io, tcp::endpoint(tcp::v4(), 8081))
    {
        doAccept();
    }
};

int main()
{
    try
    {
        asio::io_context io;
        Server server(io);

        std::cout << "Push server listening on port 8081\n";
        io.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Server error: " << e.what() << '\n';
        return 1;
    }
}

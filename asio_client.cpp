
#include <boost/asio.hpp>

#include <chrono>
#include <iostream>
#include <string>

namespace asio = boost::asio;

using tcp = asio::ip::tcp;
using ErrorCode = boost::system::error_code;

int main()
{
    try
    {
        // 1. Create the I/O context and client socket.
        asio::io_context io;
        tcp::socket socket(io);

        // 2. Connect to the server.
        tcp::endpoint server(
            asio::ip::make_address("127.0.0.1"),
            8080
        );

        socket.connect(server);

        std::cout << "Connected to server\n";

        // 3. Send the request.
        // The newline marks the end of the message.
        std::string message = "Hello from C++ Client\n";

        asio::write(
            socket,
            asio::buffer(message)
        );

        std::cout << "Message sent. Waiting for ACK...\n";

        // 4. Prepare to receive the acknowledgment.
        asio::steady_timer timer(io);
        asio::streambuf responseBuffer;

        bool readFinished = false;
        bool timedOut = false;

        // 5. Start the 5-second timeout.
        timer.expires_after(std::chrono::seconds(5));

        timer.async_wait(
            [&](const ErrorCode& ec)
            {
                // Ignore a cancelled timer or a completed read.
                if (ec || readFinished)
                    return;

                timedOut = true;

                std::cerr << "Timeout: ACK not received\n";

                // Cancel pending asynchronous socket operations.
                ErrorCode ignored;
                socket.cancel(ignored);
            }
        );

        // 6. Wait asynchronously for a complete ACK line.
        asio::async_read_until(
            socket,
            responseBuffer,
            '\n',
            [&](const ErrorCode& ec, std::size_t)
            {
                readFinished = true;

                // Stop the timer because the read has finished.
                ErrorCode ignored;
                timer.cancel();

                if (ec)
                {
                    if (!timedOut)
                    {
                        std::cerr << "ACK read error: "
                                  << ec.message() << '\n';
                    }

                    return;
                }

                // Extract the response line.
                std::istream input(&responseBuffer);

                std::string response;
                std::getline(input, response);

                if (response != "ACK")
                {
                    std::cerr << "Invalid server acknowledgment: "
                              << response << '\n';
                    return;
                }

                std::cout << "Server acknowledged receipt: "
                          << response << '\n';

                // 7. Shut down sending only after receiving ACK.
                ErrorCode shutdownError;

                socket.shutdown(
                    tcp::socket::shutdown_send,
                    shutdownError
                );

                if (shutdownError)
                {
                    std::cerr << "Shutdown error: "
                              << shutdownError.message() << '\n';
                    return;
                }

                std::cout << "Send side shut down\n";
            }
        );

        // 8. Run the asynchronous operations.
        io.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Client error: "
                  << e.what() << '\n';

        return 1;
    }

    return 0;
}

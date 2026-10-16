
#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

// Server configuration
constexpr std::uint16_t PORT = 8080;
constexpr std::size_t HEADER_SIZE = sizeof(std::uint32_t);
constexpr std::size_t MAX_MESSAGE_SIZE = 1024 * 1024; // 1 MB
constexpr int MAX_EVENTS = 64;

enum class ReceiveStage
{
    ReadingHeader,
    ReadingPayload
};

enum class ReadStatus
{
    NeedMore,
    MessageReady,
    PeerClosed,
    ProtocolError,
    IoError,
    ResourceError
};

struct ClientState
{
    explicit ClientState(int fd)
        : socketFd(fd),
          stage(ReceiveStage::ReadingHeader),
          buffer(HEADER_SIZE),
          expectedSize(HEADER_SIZE),
          receivedSize(0)
    {
    }

    int socketFd;
    ReceiveStage stage;
    std::vector<char> buffer;

    // Bytes required in the current stage.
    std::size_t expectedSize;

    // Bytes received in the current stage.
    std::size_t receivedSize;
};

bool setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1)
        return false;

    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

ReadStatus receiveMessage(ClientState& client)
{
    while (true)
    {
        // Validate our internal state before doing pointer arithmetic.
        if (client.receivedSize > client.expectedSize ||
            client.buffer.size() != client.expectedSize)
        {
            return ReadStatus::ProtocolError;
        }

        // Have we completed the current stage?
        if (client.receivedSize == client.expectedSize)
        {
            if (client.stage == ReceiveStage::ReadingHeader)
            {
                // The complete header contains a 4-byte payload length.
                std::uint32_t networkLength = 0;

                std::memcpy(
                    &networkLength,
                    client.buffer.data(),
                    HEADER_SIZE
                );

                const std::uint32_t payloadSize =
                    ntohl(networkLength);

                // Never trust a length received from the network.
                if (payloadSize > MAX_MESSAGE_SIZE)
                    return ReadStatus::ProtocolError;

                // Switch to the payload stage.
                client.stage = ReceiveStage::ReadingPayload;
                client.expectedSize =
                    static_cast<std::size_t>(payloadSize);
                client.receivedSize = 0;

                try
                {
                    client.buffer.assign(
                        client.expectedSize,
                        '\0'
                    );
                }
                catch (const std::bad_alloc&)
                {
                    return ReadStatus::ResourceError;
                }

                // A zero-length payload is already complete.
                if (client.expectedSize == 0)
                    return ReadStatus::MessageReady;

                continue;
            }

            // The payload is complete.
            return ReadStatus::MessageReady;
        }

        const std::size_t remaining =
            client.expectedSize - client.receivedSize;

        // Append new bytes immediately after the bytes already received.
        ssize_t n = recv(
            client.socketFd,
            client.buffer.data() + client.receivedSize,
            remaining,
            0
        );

        if (n > 0)
        {
            client.receivedSize +=
                static_cast<std::size_t>(n);

            continue;
        }

        if (n == 0)
            return ReadStatus::PeerClosed;

        if (errno == EINTR)
            continue;

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return ReadStatus::NeedMore;

        return ReadStatus::IoError;
    }
}

void resetForNextMessage(ClientState& client)
{
    client.stage = ReceiveStage::ReadingHeader;
    client.expectedSize = HEADER_SIZE;
    client.receivedSize = 0;
    client.buffer.resize(HEADER_SIZE);
}

void closeClient(
    int epollFd,
    int clientFd,
    std::unordered_map<int, ClientState>& clients)
{
    epoll_ctl(epollFd, EPOLL_CTL_DEL, clientFd, nullptr);

    close(clientFd);
    clients.erase(clientFd);
}

int main()
{
    // Create the listening socket.
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);

    if (serverFd == -1)
    {
        perror("socket");
        return 1;
    }

    int reuseAddress = 1;

    if (setsockopt(
            serverFd,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuseAddress,
            sizeof(reuseAddress)) == -1)
    {
        perror("setsockopt");
        close(serverFd);
        return 1;
    }

    if (!setNonBlocking(serverFd))
    {
        perror("setNonBlocking server");
        close(serverFd);
        return 1;
    }

    // Bind to all local IPv4 interfaces on port 8080.
    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(PORT);

    if (bind(
            serverFd,
            reinterpret_cast<sockaddr*>(&serverAddress),
            sizeof(serverAddress)) == -1)
    {
        perror("bind");
        close(serverFd);
        return 1;
    }

    if (listen(serverFd, SOMAXCONN) == -1)
    {
        perror("listen");
        close(serverFd);
        return 1;
    }

    // Create the epoll instance.
    int epollFd = epoll_create1(EPOLL_CLOEXEC);

    if (epollFd == -1)
    {
        perror("epoll_create1");
        close(serverFd);
        return 1;
    }

    epoll_event serverEvent{};
    serverEvent.events = EPOLLIN;
    serverEvent.data.fd = serverFd;

    if (epoll_ctl(
            epollFd,
            EPOLL_CTL_ADD,
            serverFd,
            &serverEvent) == -1)
    {
        perror("epoll_ctl server");
        close(epollFd);
        close(serverFd);
        return 1;
    }

    // Each connected client owns its own receive state.
    std::unordered_map<int, ClientState> clients;

    epoll_event events[MAX_EVENTS];

    std::cout << "Server listening on port "
              << PORT << '\n';

    bool running = true;
    int exitCode = 0;

    while (running)
    {
        int eventCount = epoll_wait(
            epollFd,
            events,
            MAX_EVENTS,
            -1
        );

        if (eventCount == -1)
        {
            if (errno == EINTR)
                continue;

            perror("epoll_wait");
            exitCode = 1;
            break;
        }

        for (int i = 0; i < eventCount; ++i)
        {
            const int fd = events[i].data.fd;
            const std::uint32_t flags = events[i].events;

            // A listening-socket event means new connections may be waiting.
            if (fd == serverFd)
            {
                if (flags & (EPOLLERR | EPOLLHUP))
                {
                    std::cerr << "Listening socket failed\n";
                    running = false;
                    exitCode = 1;
                    break;
                }

                if (!(flags & EPOLLIN))
                    continue;

                // Accept all currently available connections.
                while (true)
                {
                    sockaddr_in clientAddress{};
                    socklen_t addressLength =
                        sizeof(clientAddress);

                    int clientFd = accept(
                        serverFd,
                        reinterpret_cast<sockaddr*>(&clientAddress),
                        &addressLength
                    );

                    if (clientFd == -1)
                    {
                        if (errno == EINTR)
                            continue;

                        if (errno == EAGAIN ||
                            errno == EWOULDBLOCK)
                        {
                            break;
                        }

                        perror("accept");
                        break;
                    }

                    if (!setNonBlocking(clientFd))
                    {
                        perror("setNonBlocking client");
                        close(clientFd);
                        continue;
                    }

                    auto [it, inserted] =
                        clients.try_emplace(clientFd, clientFd);

                    if (!inserted)
                    {
                        close(clientFd);
                        continue;
                    }

                    epoll_event clientEvent{};
                    clientEvent.events =
                        EPOLLIN | EPOLLRDHUP;
                    clientEvent.data.fd = clientFd;

                    if (epoll_ctl(
                            epollFd,
                            EPOLL_CTL_ADD,
                            clientFd,
                            &clientEvent) == -1)
                    {
                        perror("epoll_ctl client");
                        close(clientFd);
                        clients.erase(it);
                        continue;
                    }

                    char ip[INET_ADDRSTRLEN]{};

                    if (inet_ntop(
                            AF_INET,
                            &clientAddress.sin_addr,
                            ip,
                            sizeof(ip)))
                    {
                        std::cout << "Client connected: "
                                  << ip << ':'
                                  << ntohs(clientAddress.sin_port)
                                  << " (fd=" << clientFd << ")\n";
                    }
                }

                continue;
            }

            auto it = clients.find(fd);

            if (it == clients.end())
                continue;

            if (flags & EPOLLERR)
            {
                std::cerr << "Socket error for fd="
                          << fd << '\n';

                closeClient(epollFd, fd, clients);
                continue;
            }

            if (!(flags &
                  (EPOLLIN | EPOLLRDHUP | EPOLLHUP)))
            {
                continue;
            }

            ClientState& client = it->second;
            ReadStatus status = receiveMessage(client);

            switch (status)
            {
            case ReadStatus::MessageReady:
            {
                // This demo treats the payload as text.
                std::string message(
                    client.buffer.begin(),
                    client.buffer.end()
                );

                std::cout << "[fd=" << fd << "] Received "
                          << message.size() << " bytes: ";

                if (!message.empty())
                {
                    std::cout.write(
                        message.data(),
                        static_cast<std::streamsize>(
                            message.size())
                    );
                }

                std::cout << '\n';

                // Prepare this client for its next message.
                resetForNextMessage(client);
                break;
            }

            case ReadStatus::NeedMore:
                // Preserve the client's state and wait for another event.
                if (flags & EPOLLHUP)
                {
                    closeClient(epollFd, fd, clients);
                }
                break;

            case ReadStatus::PeerClosed:
            {
                const bool incompleteMessage =
                    (client.stage == ReceiveStage::ReadingHeader &&
                     client.receivedSize != 0) ||
                    (client.stage == ReceiveStage::ReadingPayload);

                if (incompleteMessage)
                {
                    std::cerr << "Client fd=" << fd
                              << " disconnected with an incomplete message\n";
                }
                else
                {
                    std::cout << "Client disconnected: fd="
                              << fd << '\n';
                }

                closeClient(epollFd, fd, clients);
                break;
            }

            case ReadStatus::ProtocolError:
                std::cerr << "Invalid protocol or oversized message: fd="
                          << fd << '\n';

                closeClient(epollFd, fd, clients);
                break;

            case ReadStatus::IoError:
                std::cerr << "recv failed: fd="
                          << fd << '\n';

                closeClient(epollFd, fd, clients);
                break;

            case ReadStatus::ResourceError:
                std::cerr << "Not enough memory for client message: fd="
                          << fd << '\n';

                closeClient(epollFd, fd, clients);
                break;
            }
        }
    }

    // Cleanup if the event loop exits because of an error.
    for (auto& [fd, client] : clients)
    {
        (void)client;
        epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, nullptr);
        close(fd);
    }

    close(epollFd);
    close(serverFd);

    return exitCode;
}

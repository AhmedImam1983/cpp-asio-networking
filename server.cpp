
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

void handleClient(int clientSocket)
{
    char buffer[1024];

    while (true)
    {
        ssize_t bytesReceived = recv(
            clientSocket,
            buffer,
            sizeof(buffer),
            0
        );

        if (bytesReceived > 0)
        {
            std::string message(buffer, bytesReceived);

            std::cout << "Received: "
                      << message << '\n';
        }
        else if (bytesReceived == 0)
        {
            std::cout << "Client disconnected\n";
            break;
        }
        else
        {
            std::cerr << "recv failed: "
                      << std::strerror(errno) << '\n';
            break;
        }
    }

    close(clientSocket);
}

int main()
{
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket == -1)
    {
        std::cerr << "socket failed\n";
        return 1;
    }

    sockaddr_in serverAddress{};

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(
            serverSocket,
            reinterpret_cast<sockaddr*>(&serverAddress),
            sizeof(serverAddress)) == -1)
    {
        std::cerr << "bind failed: "
                  << std::strerror(errno) << '\n';
        close(serverSocket);
        return 1;
    }

    if (listen(serverSocket, 5) == -1)
    {
        std::cerr << "listen failed\n";
        close(serverSocket);
        return 1;
    }

    std::cout << "Server listening on port 8080\n";

    while (true)
    {
        int clientSocket = accept(
            serverSocket, nullptr, nullptr
        );

        if (clientSocket == -1)
        {
            if (errno == EINTR)
                continue;

            std::cerr << "accept failed: "
                      << std::strerror(errno) << '\n';
            continue;
        }

        std::cout << "Client connected\n";

        std::thread clientThread(handleClient, clientSocket);
        clientThread.detach();
    }

    close(serverSocket);
    return 0;
}

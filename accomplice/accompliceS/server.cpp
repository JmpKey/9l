#include "server.h"

void Server::writeToFile(const std::string& _line, const std::string& _filename) {
    // Reading existing records into a file
    std::set<std::string> existingLines;
    std::ifstream infile(_filename);
    std::string line;

    while (std::getline(infile, line)) {
        existingLines.insert(line); // Add each row to the set
    }
    infile.close();

    // Checking if a record exists
    if (existingLines.find(_line) == existingLines.end()) {
        std::ofstream file(_filename, std::ios::app);
        if (file.is_open()) {
            file << _line << std::endl;
            file.close();
        } else {
            std::cerr << "No open file in write" << std::endl;
        }
    } else {
        std::cout << "Record already exists: " << _line << std::endl;
    }
}

#ifdef _WIN32
// WindowsNT zone
int Server::run(int _argc, char* _argv[]) {
    if (_argc != 3) {
        std::cerr << "Use: " << _argv[0] << " <file> <port>" << std::endl;
        return 1;
    }

    // Init Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }

    std::string filename = _argv[1];
    int port = std::stoi(_argv[2]);

    SOCKET sockfd;
    struct sockaddr_in6 server_addr, client_addr;
    int addr_len = sizeof(client_addr);
    char buffer[256];

    sockfd = socket(AF_INET6, SOCK_STREAM, 0);
    if (sockfd == INVALID_SOCKET) {
        std::cerr << "Error create socket" << std::endl;
        WSACleanup();
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_port = htons((unsigned short)port);
    server_addr.sin6_addr = in6addr_any;

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Socket linking error" << std::endl;
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }

    listen(sockfd, 5);
    std::cout << "Server start port " << port << std::endl;

    while (true) {
        SOCKET newsockfd = accept(sockfd, (struct sockaddr*)&client_addr, &addr_len);
        if (newsockfd == INVALID_SOCKET) {
            std::cerr << "Error accepting the connection" << std::endl;
            continue;
        }

        memset(buffer, 0, sizeof(buffer));

        int n = recv(newsockfd, buffer, sizeof(buffer) - 1, 0);
        if (n > 0) {
            buffer[n] = '\0';
            writeToFile(buffer, filename);
            std::cout << "Received: " << buffer << std::endl;
        }

        closesocket(newsockfd);
    }

    closesocket(sockfd);
    WSACleanup();
    return 0;
}

#else
// Unix-like zone
int Server::run(int _argc, char* _argv[]) {
    if (_argc != 3) {
        std::cerr << "Use: " << _argv[0] << " <file> <port>" << std::endl;
        return 1;
    }

    std::string filename = _argv[1];
    int port = std::stoi(_argv[2]);

    int sockfd;
    struct sockaddr_in6 server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[256];

    sockfd = socket(AF_INET6, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "Error create socket" << std::endl;
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_port = htons(port); // Use the specified port
    server_addr.sin6_addr = in6addr_any;

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Socket linking error" << std::endl;
        close(sockfd); // Close the socket before exiting
        return 1;
    }

    listen(sockfd, 5);
    std::cout << "Server start port " << port << std::endl;

    while (true) {
        int newsockfd = accept(sockfd, (struct sockaddr*)&client_addr, &addr_len);
        if (newsockfd < 0) {
            std::cerr << "Error accepting the connection" << std::endl;
            continue;
        }

        memset(buffer, 0, sizeof(buffer));
        ssize_t n = read(newsockfd, buffer, sizeof(buffer)-1);
        if (n > 0) {
            buffer[n] = '\0';
            writeToFile(buffer, filename);
            std::cout << "Received: " << buffer << std::endl;
        }

        close(newsockfd);
    }

    close(sockfd);
    return 0;
}
#endif

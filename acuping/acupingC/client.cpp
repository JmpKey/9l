#include "client.h"

#ifdef _WIN32
// WindowsNT zone
int Client::client_run(int _argc, char* _argv[]) {
    if (_argc < 2) {
        std::cerr << "Use: " << _argv[0] << " <ipv6_address>" << std::endl;
        return 1;
    }

    // 0. Initializing WinSock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return 1;
    }

    const char* target_ip = _argv[1];

    // 1. UDP Discovery
    SOCKET discovery_socket = socket(AF_INET6, SOCK_DGRAM, 0);
    if (discovery_socket == INVALID_SOCKET) {
        std::cerr << "Error socket: " << WSAGetLastError() << std::endl;
        return 1;
    }

    sockaddr_in6 server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_port = htons(DISCOVERY_PORT);

    if (inet_pton(AF_INET6, target_ip, &server_addr.sin6_addr) <= 0) {
        std::cerr << "Incorrect IPv6 address" << std::endl;
        closesocket(discovery_socket);
        return 1;
    }

    const char* msg = "DISCOVER";
    sendto(discovery_socket, msg, (int)strlen(msg), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // 2. Obtaining a TCP port
    char buffer[1024];
    int addr_len = sizeof(server_addr);
    int rec = recvfrom(discovery_socket, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&server_addr, &addr_len);

    if (rec == SOCKET_ERROR) {
        std::cerr << "Server not respond: " << WSAGetLastError() << std::endl;
        closesocket(discovery_socket);
        return 1;
    }
    buffer[rec] = '\0';
    int tcp_port = std::stoi(buffer);

    // 3. TCP Connection
    SOCKET tcp_socket = socket(AF_INET6, SOCK_STREAM, 0);
    server_addr.sin6_port = htons(tcp_port);

    if (connect(tcp_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Error TCP connect: " << WSAGetLastError() << std::endl;
        closesocket(discovery_socket);
        closesocket(tcp_socket);
        return 1;
    }

    // 4. Reading data
    std::string result;
    while (true) {
        int bytes = recv(tcp_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;
        buffer[bytes] = '\0';
        result += buffer;
    }

    std::cout << "Data from server:\n" << result << std::endl;

    closesocket(tcp_socket);
    closesocket(discovery_socket);
    WSACleanup();
    return 0;
}

#else
// Unix zone
int Client::client_run(int _argc, char* _argv[]) {
    if (_argc < 2) {
        std::cerr << "Use: " << _argv[0] << " <ipv6_address>" << std::endl;
        return 1;
    }

    const char* target_ip = _argv[1];

    // 1. UDP Discovery
    int discovery_socket = socket(AF_INET6, SOCK_DGRAM, 0);
    if (discovery_socket < 0) {
        perror("Error create socket");
        return 1;
    }

    sockaddr_in6 server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_port = htons(DISCOVERY_PORT);

    if (inet_pton(AF_INET6, target_ip, &server_addr.sin6_addr) <= 0) {
        std::cerr << "Incorrect IPv6 address" << std::endl;
        close(discovery_socket);
        return 1;
    }

    const char* msg = "DISCOVER";
    sendto(discovery_socket, msg, strlen(msg), 0, (struct sockaddr*)&server_addr, sizeof(server_addr));

    // 2. Obtaining a TCP port
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(server_addr);
    ssize_t rec = recvfrom(discovery_socket, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr*)&server_addr, &addr_len);

    if (rec < 0) {
        perror("Server not respond");
        close(discovery_socket);
        return 1;
    }
    buffer[rec] = '\0';
    int tcp_port = std::stoi(buffer);

    // 3. TCP Connection
    int tcp_socket = socket(AF_INET6, SOCK_STREAM, 0);
    server_addr.sin6_port = htons(tcp_port); // Change the port to the received one

    if (connect(tcp_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error TCP connect");
        close(discovery_socket);
        close(tcp_socket);
        return 1;
    }

    // 4. Reading data
    std::string result;
    while (true) {
        ssize_t bytes = recv(tcp_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes <= 0) break;
        buffer[bytes] = '\0';
        result += buffer;
    }

    std::cout << result << std::endl;

    close(tcp_socket);
    close(discovery_socket);
    return 0;
}
#endif

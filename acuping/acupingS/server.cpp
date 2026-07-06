#include <server.h>

#ifdef _WIN32
// WindowsNT zone
void Server::client_handler(SOCKET conn_socket) {
    std::cout << "[TCP] Flow for the client is running (socket: " << conn_socket << ")" << std::endl;

    std::ifstream file("my_contact");
    std::string content;

    if (file.is_open()) {
        content.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
    } else {
        content = "Error: File my_contact not found on the server";
    }

    // On Windows send returns the number of bytes sent or SOCKET_ERROR
    int bytes_sent = send(conn_socket, content.c_str(), content.size(), 0);
    if (bytes_sent == SOCKET_ERROR) {
        std::cerr << "Error sending data: " << WSAGetLastError() << std::endl;
    } else {
        std::cout << "[TCP] Sent " << bytes_sent << " bytes to client." << std::endl;
    }

    closesocket(conn_socket);
    std::cout << "[TCP] Client served and disconnected" << std::endl;
}

int Server::server_run() {
    // Init Winsock
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << std::endl;
        return 1;
    }

    // 1. Create a UDP socket for detection
    SOCKET discovery_socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if (discovery_socket == INVALID_SOCKET) {
        std::cerr << "Error UDP socket: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in6 discovery_addr{};
    discovery_addr.sin6_family = AF_INET6;
    discovery_addr.sin6_port = htons(DISCOVERY_PORT);
    // Use INADDR_ANY for IPv6
    discovery_addr.sin6_addr = in6addr_any;

    if (bind(discovery_socket, (struct sockaddr*)&discovery_addr, sizeof(discovery_addr)) == SOCKET_ERROR) {
        std::cerr << "Binding error UDP: " << WSAGetLastError() << std::endl;
        closesocket(discovery_socket);
        WSACleanup();
        return 1;
    }

    // 2. Create a persistent TCP socket to accept connections
    SOCKET tcp_listen_socket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (tcp_listen_socket == INVALID_SOCKET) {
        std::cerr << "Error TCP socket: " << WSAGetLastError() << std::endl;
        closesocket(discovery_socket);
        WSACleanup();
        return 1;
    }

    sockaddr_in6 tcp_addr{};
    tcp_addr.sin6_family = AF_INET6;
    tcp_addr.sin6_addr = in6addr_any;
    tcp_addr.sin6_port = 0; // Random free port

    if (bind(tcp_listen_socket, (struct sockaddr*)&tcp_addr, sizeof(tcp_addr)) == SOCKET_ERROR) {
        std::cerr << "Binding error TCP: " << WSAGetLastError() << std::endl;
        closesocket(tcp_listen_socket);
        closesocket(discovery_socket);
        WSACleanup();
        return 1;
    }

    // On Windows listen accepts backlog as int
    if (listen(tcp_listen_socket, 10) == SOCKET_ERROR) {
        std::cerr << "Listen error: " << WSAGetLastError() << std::endl;
        closesocket(tcp_listen_socket);
        closesocket(discovery_socket);
        WSACleanup();
        return 1;
    }

    // Getting the port number
    socklen_t len = sizeof(tcp_addr);
    if (getsockname(tcp_listen_socket, (struct sockaddr*)&tcp_addr, &len) == SOCKET_ERROR) {
        std::cerr << "getsockname error: " << WSAGetLastError() << std::endl;
        closesocket(tcp_listen_socket);
        closesocket(discovery_socket);
        WSACleanup();
        return 1;
    }
    int tcp_port = ntohs(tcp_addr.sin6_port);

    std::cout << "=== SERVER STARTED (NT) ===" << std::endl;
    std::cout << "UDP Discovery Port: " << DISCOVERY_PORT << std::endl;
    std::cout << "TCP Data Port: " << tcp_port << std::endl;
    std::cout << "Expectation clients..." << std::endl;

    // 3. We launch a separate thread to listen for UDP requests
    std::thread udp_thread([discovery_socket, tcp_port]() {
        while (true) {
            char buffer[BUFFER_SIZE];
            sockaddr_in6 client_addr{};
            int client_len = sizeof(client_addr);

            int n = recvfrom(discovery_socket, buffer, BUFFER_SIZE, 0,
                             (struct sockaddr*)&client_addr, &client_len);
            if (n > 0) {
                char client_ip[INET6_ADDRSTRLEN];
                if (inet_ntop(AF_INET6, &client_addr.sin6_addr, client_ip, INET6_ADDRSTRLEN) == NULL) {
                    std::cerr << "inet_ntop failed: " << WSAGetLastError() << std::endl;
                    continue;
                }
                std::cout << "[UDP] Inquiry DISCOVER from " << client_ip << std::endl;

                std::string port_str = std::to_string(tcp_port);
                int bytes_sent = sendto(discovery_socket, port_str.c_str(), port_str.size(), 0,
                                        (struct sockaddr*)&client_addr, client_len);
                if (bytes_sent == SOCKET_ERROR) {
                    std::cerr << "Error sending UDP response: " << WSAGetLastError() << std::endl;
                }
            } else if (n == SOCKET_ERROR) {
                std::cerr << "recvfrom error: " << WSAGetLastError() << std::endl;
            }
        }
    });
    udp_thread.detach(); // Detach the thread so it runs independently

    // 4. Main cycle: receiving TCP connections and their SEQUENTIAL processing
    while (true) {
        sockaddr_in6 client_tcp_addr{};
        int client_tcp_len = sizeof(client_tcp_addr);

        SOCKET conn_fd = accept(tcp_listen_socket, (struct sockaddr*)&client_tcp_addr, &client_tcp_len);
        if (conn_fd == INVALID_SOCKET) {
            std::cerr << "Error accept: " << WSAGetLastError() << std::endl;
            continue;
        }
        Server::client_handler(conn_fd);
    }

    closesocket(tcp_listen_socket);
    closesocket(discovery_socket);
    WSACleanup();
    return 0;
}

#else
// Unix zone
void Server::client_handler(int conn_fd) {
    std::cout << "[TCP] Flow for the client is running (fd: " << conn_fd << ")" << std::endl;

    std::ifstream file("my_contact");
    std::string content;

    if (file.is_open()) {
        content.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
    } else {
        content = "Error: File my_contact not found on the server";
    }

    send(conn_fd, content.c_str(), content.size(), 0);

    close(conn_fd);
    std::cout << "[TCP] Client served and disconnected" << std::endl;
}

int Server::server_run() {
    // 1. Create a UDP socket for detection
    int discovery_socket = socket(AF_INET6, SOCK_DGRAM, 0);
    if (discovery_socket < 0) {
        perror("Error UDP socket");
        return 1;
    }

    sockaddr_in6 discovery_addr{};
    discovery_addr.sin6_family = AF_INET6;
    discovery_addr.sin6_port = htons(DISCOVERY_PORT);
    discovery_addr.sin6_addr = in6addr_any;

    if (bind(discovery_socket, (struct sockaddr*)&discovery_addr, sizeof(discovery_addr)) < 0) {
        perror("Binding error UDP");
        close(discovery_socket);
        return 1;
    }

    // 2. Create a persistent TCP socket to accept connections
    int tcp_listen_socket = socket(AF_INET6, SOCK_STREAM, 0);
    sockaddr_in6 tcp_addr{};
    tcp_addr.sin6_family = AF_INET6;
    tcp_addr.sin6_addr = in6addr_any;
    tcp_addr.sin6_port = 0; // Random free port

    if (bind(tcp_listen_socket, (struct sockaddr*)&tcp_addr, sizeof(tcp_addr)) < 0) {
        perror("Binding error TCP");
        close(tcp_listen_socket);
        close(discovery_socket);
        return 1;
    }
    listen(tcp_listen_socket, 10);

    socklen_t len = sizeof(tcp_addr);
    getsockname(tcp_listen_socket, (struct sockaddr*)&tcp_addr, &len);
    int tcp_port = ntohs(tcp_addr.sin6_port);

    std::cout << "=== SERVER STARTED (POSIX) ===" << std::endl;
    std::cout << "UDP Discovery Port: " << DISCOVERY_PORT << std::endl;
    std::cout << "TCP Data Port: " << tcp_port << std::endl;
    std::cout << "Expectation clients..." << std::endl;

    // 3. We launch a separate thread to listen for UDP requests
    std::thread udp_thread([discovery_socket, tcp_port]() {
        while (true) {
            char buffer[BUFFER_SIZE];
            sockaddr_in6 client_addr;
            socklen_t client_len = sizeof(client_addr);

            ssize_t n = recvfrom(discovery_socket, buffer, BUFFER_SIZE, 0,
                                 (struct sockaddr*)&client_addr, &client_len);
            if (n > 0) {
                char client_ip[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &client_addr.sin6_addr, client_ip, INET6_ADDRSTRLEN);
                std::cout << "[UDP] Inquiry DISCOVER from " << client_ip << std::endl;

                std::string port_str = std::to_string(tcp_port);
                sendto(discovery_socket, port_str.c_str(), port_str.size(), 0,
                       (struct sockaddr*)&client_addr, client_len);
            }
        }
    });
    udp_thread.detach();

    // 4. Main cycle: receiving TCP connections and their SEQUENTIAL processing
    while (true) {
        sockaddr_in6 client_tcp_addr;
        socklen_t client_tcp_len = sizeof(client_tcp_addr);

        int conn_fd = accept(tcp_listen_socket, (struct sockaddr*)&client_tcp_addr, &client_tcp_len);
        if (conn_fd < 0) {
            perror("Error accept");
            continue;
        }

        Server::client_handler(conn_fd);
    }

    close(tcp_listen_socket);
    close(discovery_socket);
    return 0;
}
#endif

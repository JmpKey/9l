#include "client.h"

void Client::read_from_server(int sock) {
    char buffer[BUFFER_SIZE];
    int n;
    while ((n = read(sock, buffer, BUFFER_SIZE)) > 0) {
        std::cout.write(buffer, n);
        std::cout.flush();
    }
}

int Client::init_client(int _argc, char* _argv[]) {
    int sock = socket(AF_INET6, SOCK_STREAM, 0); // TCP
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    if (_argc < 2) {
        std::cerr << "Usage: " << _argv[0] << " <IPv6 address> <port>" << std::endl;
        return 1;
    }

    const char* ipv6_address = _argv[1];
    int port = std::stoi(_argv[2]);

    sockaddr_in6 server_addr{};
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_port = htons(port);
    inet_pton(AF_INET6, ipv6_address, &server_addr.sin6_addr);

    if (connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        return 1;
    }

    std::thread reader(&Client::read_from_server, this, sock);

    // We read keyboard input and send it to the server
    std::string line;
    while (std::getline(std::cin, line)) {
        line += "\n";
        write(sock, line.c_str(), line.size());
    }

    close(sock);
    reader.join();

    return 0;
}

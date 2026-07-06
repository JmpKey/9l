#include "client.h"

#ifdef _WIN32
Client::Client() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        exit(1);
    }
}

Client::~Client() { WSACleanup(); }
#endif

void Client::print_error(const std::string& msg) {
#ifdef _WIN32
    std::cerr << msg << " Error code: " << WSAGetLastError() << std::endl;
#else
    perror(msg.c_str());
#endif
}

int Client::init_client(int _argc, char* _argv[]) {
    if (_argc < 2) {
        std::cerr << "Usage: " << _argv[0] << " [<ipv6> <port>] [<ipv6> <port>] ..." << std::endl;
        return 1;
    }

    std::vector<ServerTarget> targets = parse_args(_argc, _argv);

    for (const auto& target : targets) {
        process_server(target);
    }

    return 0;
}

std::vector<ServerTarget> Client::parse_args(int _argc, char* _argv[]) {
    std::vector<ServerTarget> targets;
    std::string all_args;
    for (int i = 1; i < _argc; ++i) {
        all_args += _argv[i];
        all_args += " ";
    }

    std::stringstream ss(all_args);
    std::string part;
    while (std::getline(ss, part, '[')) {
        size_t end_pos = part.find(']');
        if (end_pos != std::string::npos) {
            std::string content = part.substr(0, end_pos);
            std::stringstream content_ss(content);
            ServerTarget st;
            if (content_ss >> st.ip >> st.port) {
                targets.push_back(st);
            }
        }
    }
    return targets;
}

void Client::process_server(const ServerTarget& _target) {
    SOCKET sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        print_error("socket");
        return;
    }

    sockaddr_in6 server_addr{};
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_port = htons(_target.port);

    if (inet_pton(AF_INET6, _target.ip.c_str(), &server_addr.sin6_addr) <= 0) {
        std::cerr << "Invalid address: " << _target.ip << std::endl;
#ifdef WIN32
        close_socket(sock);
#else
        close(sock);
#endif
        return;
    }

    std::cout << "Connecting to [" << _target.ip << " " << _target.port << "]..." << std::endl;

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        print_error("connect");
#ifdef WIN32
        close_socket(sock);
#else
        close(sock);
#endif
        return;
    }
#ifdef WIN32
    // Processing configuration via filesystem
    std::string configPath = "client.conf";
    if (!fs::exists(configPath)) {
        std::cerr << "Config file not found: " << configPath << std::endl;
        close_socket(sock);
        return;
    }

    std::string clientsNode = parse_config(configPath, "[nodes]");
    if (clientsNode.empty()) {
        std::cerr << "NodesMeditation->0xBADCAFE. Check [nodes] field in client.conf" << std::endl;
        close_socket(sock);
        return;
    }

    fs::path p(clientsNode);
    if (p.has_parent_path() && !fs::exists(p.parent_path())) {
        fs::create_directories(p.parent_path());
    }

    std::ofstream out_file(clientsNode, std::ios::app | std::ios::binary);
    if (!out_file.is_open()) {
        std::cerr << "Failed to open output file: " << clientsNode << std::endl;
        close_socket(sock);
        return;
    }

    char buffer[1024];
    int bytes_read;
    // On Windows read() does not work for sockets, use recv()
    while ((bytes_read = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        out_file.write(buffer, bytes_read);
    }

    out_file << "\n";
    out_file.close();

    std::cout << "Data received and saved to " << fs::absolute(p) << std::endl;
    close_socket(sock);

#else
    // Read data and write to a file
    std::string clientsNode = parse_config("client.conf", "[nodes]");
    if (!clientsNode.empty()) {
        std::cout << "Clients Path: " << clientsNode << std::endl;
    } else {
        std::cerr << "NodesMeditation->0xBADCAFE." << std::endl;
        /*
         * ?>...
         * Create a client.conf file with one fields: [nodes] = [path/to/file/your/nodes]
         * (Where your binar)
         */
        close(sock);
        std::exit(EXIT_FAILURE);
    }

    std::ofstream out_file(clientsNode, std::ios::app);
    char buffer[1024];
    int bytes_read;
    while ((bytes_read = read(sock, buffer, sizeof(buffer))) > 0) {
        out_file.write(buffer, bytes_read);
    }
    out_file << "\n"; // Separator between data of different servers
    out_file.close();

    std::cout << "Data received and saved." << std::endl;
    close(sock);
#endif
}

std::string Client::trim(const std::string& _str) {
#ifdef WIN32
    size_t first = _str.find_first_not_of(" \t\r\n");
    size_t last = _str.find_last_not_of(" \t\r\n");
#else
    size_t first = _str.find_first_not_of(" \t");
        size_t last = _str.find_last_not_of(" \t");
#endif
    return (first == std::string::npos) ? "" : _str.substr(first, (last - first + 1));
}

std::string Client::parse_config(const std::string& _filePath, const std::string& _key) {
    std::ifstream configFile(_filePath);
    if (!configFile.is_open()) {
        return "";
    }

    std::string line;
    while (std::getline(configFile, line)) {
        if (line.find(_key) != std::string::npos) {
            size_t equalPos = line.find('=');
            if (equalPos != std::string::npos) {
                std::string value = line.substr(equalPos + 1);
                value = trim(value);
                if (!value.empty() && value.front() == '[' && value.back() == ']') {
                    value = value.substr(1, value.size() - 2);
                    return trim(value);
                }
            }
        }
    }
    return "";
}















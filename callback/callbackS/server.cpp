#include "server.h"

#ifdef _WIN32
// WindowsNT zone
void close_socket(SOCKET sock) {
    closesocket(sock);
}

void Server::handle_client(SOCKET client_socket) {
    // 1. We accept the file name (256 bytes)
    char filename_buffer[FILENAME_BUFFER_SIZE];
    memset(filename_buffer, 0, FILENAME_BUFFER_SIZE);

    if (recv_all(client_socket, filename_buffer, FILENAME_BUFFER_SIZE) <= 0) {
        close_socket(client_socket);
        return;
    }

    // 2. We accept the offset (strictly 8 bytes)
    int64_t offset = 0;
    if (recv_all(client_socket, &offset, sizeof(int64_t)) <= 0) {
        close_socket(client_socket);
        return;
    }

    std::string requested_name(filename_buffer);
    std::string actual_filepath = "";
    int32_t file_status = -1; // 4 byte

    // Searching for a file in folders
    std::string path_files = "files/" + requested_name;
    size_t dot = requested_name.find('.');
    std::string path_manifest = "";
    // If a point is found, take the substring before it
    if (dot != std::string::npos) {
        path_manifest = "manifest/" + requested_name.substr(0, dot) + ".txt";
    }

    if (fs::exists(path_files) && !fs::is_directory(path_files)) {
        actual_filepath = path_files;
        file_status = 0;
    } else if (!path_manifest.empty() && fs::exists(path_manifest) && !fs::is_directory(path_manifest)) {
        actual_filepath = path_manifest;
        file_status = 1;
    }

    // 3. Send status (4 bytes)
    send_all(client_socket, &file_status, sizeof(int32_t));

    if (file_status == -1) {
        std::cerr << "[Server] File not found: " << requested_name << std::endl;
        close_socket(client_socket);
        return;
    }

    if (file_status == 1) { offset = 0; } // ADD!!!

    // 4. Open the file
    std::ifstream infile(actual_filepath, std::ios::binary | std::ios::ate);
    if (!infile) {
        int64_t error_size = -1;
        send_all(client_socket, &error_size, sizeof(int64_t));
        close_socket(client_socket);
        return;
    }

    int64_t total_file_size = static_cast<int64_t>(infile.tellg());

    // 5. Send the full size (strictly 8 bytes)
    if (send_all(client_socket, &total_file_size, sizeof(int64_t)) < 0) {
        close_socket(client_socket);
        return;
    }

    // 6. Data transfer
    if (offset < 0) offset = 0;
    if (offset >= total_file_size) {
        // Client has already downloaded everything
        std::cout << "[Server] Client already has full file: " << requested_name << std::endl;
    } else {
        infile.seekg(offset, std::ios::beg);
        char buffer[BUFFER_SIZE];
        int64_t bytes_to_send = total_file_size - offset;
        int64_t total_sent = 0;

        while (total_sent < bytes_to_send) {
            infile.read(buffer, BUFFER_SIZE);
            std::streamsize read_from_file = infile.gcount();
            if (read_from_file <= 0) break;

            if (send_all(client_socket, buffer, static_cast<size_t>(read_from_file)) < 0) {
                break;
            }
            total_sent += read_from_file;
        }
    }

    std::cout << "[Server] Finished " << requested_name
              << " (sent " << (total_file_size - offset) << " bytes starting from " << offset << ")" << std::endl;

    infile.close();
    close_socket(client_socket);
}

int Server::run(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: server <port>" << std::endl;
        return 1;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 1;

    if (!fs::exists("files")) fs::create_directory("files");
    if (!fs::exists("manifest")) fs::create_directory("manifest");

    int port = std::stoi(argv[1]);
    SOCKET server_socket = socket(AF_INET6, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) return 1;

    int no = 0;
    setsockopt(server_socket, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&no, sizeof(no));

    struct sockaddr_in6 server_addr{};
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_port = htons((unsigned short)port);
    server_addr.sin6_addr = in6addr_any;

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        perror("Bind failed");
        return 1;
    }

    listen(server_socket, SOMAXCONN);
    std::cout << "Server started on port " << port << ". Waiting for clients..." << std::endl;

    while (true) {
        struct sockaddr_in6 client_addr;
        int addr_len = sizeof(client_addr);
        SOCKET client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len);
        if (client_socket != INVALID_SOCKET) {
            // Create a thread for each client
            std::thread(&Server::handle_client, client_socket).detach();
        }
    }
    return 0;
}

ssize_t Server::recv_all(SOCKET sock, void* buffer, size_t len) {
    size_t total = 0;
    char* buf = (char*)buffer;
    while (total < len) {
        int n = recv(sock, buf + total, static_cast<int>(len - total), 0);
        if (n <= 0) return n;
        total += n;
    }
    return (ssize_t)total;
}

ssize_t Server::send_all(SOCKET sock, const void* buffer, size_t len) {
    size_t total = 0;
    const char* buf = (const char*)buffer;
    while (total < len) {
        int n = send(sock, buf + total, static_cast<int>(len - total), 0);
        if (n <= 0) return n;
        total += n;
    }
    return (ssize_t)total;
}

#else
// Unix-like zone
void Server::handle_client(int client_socket) {
    char filename_buffer[FILENAME_BUFFER_SIZE] = {0};

    // 1. Accept the file name
    if (recv_all(client_socket, filename_buffer, FILENAME_BUFFER_SIZE) <= 0) {
        close(client_socket);
        return;
    }

    // 2. We accept an offset for resuming
    long long offset = 0;
    if (recv_all(client_socket, &offset, sizeof(offset)) <= 0) {
        close(client_socket);
        return;
    }

    std::string requested_name(filename_buffer);
    std::string actual_filepath = "";
    int file_status = -1; // -1: не найден, 0: files, 1: manifest

    // Search logic: first in files, then in manifest
    std::string path_files = "files/" + requested_name;
    std::string path_manifest = "";
    size_t firstDot = requested_name.find('.');
    if (firstDot != std::string::npos) {
        path_manifest = "manifest/" + requested_name.substr(0, firstDot) + ".txt";
    }

    if (std::filesystem::exists(path_files) && !std::filesystem::is_directory(path_files)) {
        actual_filepath = path_files;
        file_status = 0;
    } else if (!path_manifest.empty() && std::filesystem::exists(path_manifest) && !std::filesystem::is_directory(path_manifest)) {
        actual_filepath = path_manifest;
        file_status = 1;
    }

    // 3. Send status (found or not)
    if (send_all(client_socket, &file_status, sizeof(file_status)) < 0) {
        close(client_socket);
        return;
    }

    if (file_status == -1) {
        std::cerr << "[Server] File not found: " << requested_name << std::endl;
        close(client_socket);
        return;
    }

    // 4. Open the file and get the FULL size
    std::ifstream infile(actual_filepath, std::ios::binary | std::ios::ate);
    if (!infile) {
        long long error_size = -1;
        send_all(client_socket, &error_size, sizeof(error_size));
        close(client_socket);
        return;
    }

    long long total_file_size = infile.tellg();
    
    // 5. Send the full file size to the client
    if (send_all(client_socket, &total_file_size, sizeof(total_file_size)) < 0) {
        close(client_socket);
        return;
    }

    // 6. We move to the desired offset and begin transmission
    if (offset > total_file_size) offset = total_file_size;
    if (file_status == 1) { offset = 0; } // ADD!!!!
    infile.seekg(offset, std::ios::beg);

    char buffer[BUFFER_SIZE];
    long long current_pos = offset;
    while (current_pos < total_file_size) {
	infile.read(buffer, BUFFER_SIZE);
        ssize_t read_from_file = infile.gcount();
        if (read_from_file <= 0) break;

        if (send_all(client_socket, buffer, read_from_file) < 0) {
            std::cerr << "[Server] Client disconnected during transfer" << std::endl;
            break;
        }
        current_pos += read_from_file;
    }

    std::cout << "[Server] Finished " << requested_name << " (sent from " << offset << " to " << current_pos << ")" << std::endl;
    infile.close();
    close(client_socket);
}

int Server::run(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        return 1;
    }

    if (!std::filesystem::exists("files")) std::filesystem::create_directory("files");
    if (!std::filesystem::exists("manifest")) std::filesystem::create_directory("manifest");

    int port = std::stoi(argv[1]);
    int server_socket = socket(AF_INET6, SOCK_STREAM, 0);
    if (server_socket < 0) return 1;

    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in6 server_addr{};
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_port = htons(port);
    server_addr.sin6_addr = in6addr_any;

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    listen(server_socket, 10);
    std::cout << "Server started on port " << port << " (IPv6). Waiting for clients..." << std::endl;

    while (true) {
        struct sockaddr_in6 client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_len);
        if (client_socket >= 0) {
            std::thread(&Server::handle_client, client_socket).detach();
        }
    }
    return 0;
}

ssize_t Server::recv_all(int sock, void* buffer, size_t len) {
    size_t total = 0;
    char* buf = (char*)buffer;
    while (total < len) {
        ssize_t n = recv(sock, buf + total, len - total, 0);
        if (n <= 0) return n;
        total += n;
    }
    return total;
}

ssize_t Server::send_all(int sock, const void* buffer, size_t len) {
    size_t total = 0;
    const char* buf = (const char*)buffer;
    while (total < len) {
        ssize_t n = send(sock, buf + total, len - total, MSG_NOSIGNAL);
        if (n <= 0) return n;
        total += n;
    }
    return total;
}
#endif

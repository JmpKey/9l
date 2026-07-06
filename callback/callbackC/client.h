#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <fstream>
#include <cstring>
#include <vector>
#include <string>
#include <set>
#include <deque>
#include <errno.h>

#ifdef _WIN32
#include <experimental/filesystem>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
typedef SSIZE_T ssize_t;
#define SOCKET_CLOSE closesocket
#define sleep(x) Sleep((x) * 1000)
namespace fs = std::experimental::filesystem; // recommendation use: std::filesystem
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <filesystem>
#include <sys/time.h>
#endif

#define BUFFER_SIZE 1024
#define FILENAME_BUFFER_SIZE 256

struct Peer {
    std::string ip;
    int port;

    // For use in std::set (to avoid visiting the same address twice)
    bool operator<(const Peer& other) const {
        if (ip != other.ip) return ip < other.ip;
        return port < other.port;
    }
};

class Client
{
public:
    Client(){};
    int cli_start(int argc, char* argv[]);

private:
#ifdef _WIN32
    ssize_t send_all(SOCKET sock, const void* buffer, size_t len);
    ssize_t recv_all(SOCKET sock, void* buffer, size_t len);
#else
    ssize_t send_all(int sock, const void* buffer, size_t len);
    ssize_t recv_all(int sock, void* buffer, size_t len);
#endif
    std::vector<Peer> parse_manifest(const std::string& filepath);
    bool download_file(const Peer& peer, const std::string& filename, int& out_file_type, std::string& out_path);
    std::string removeUntilLastSlash(const std::string& input);
};

#endif // CLIENT_H

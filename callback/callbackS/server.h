#ifndef SERVER_H
#define SERVER_H

#include <iostream>
#include <fstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <string>
#include <cstdint>
#include <experimental/filesystem>
#include <winsock2.h>
#include <ws2tcpip.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#pragma comment(lib, "ws2_32.lib") // linking by Winsock
typedef SSIZE_T ssize_t;
namespace fs = std::experimental::filesystem; // recommendation use: std::filesystem
#else
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <filesystem>
#endif

#define BUFFER_SIZE 1024
#define FILENAME_BUFFER_SIZE 256

class Server
{
public:
    Server(){};
    int run(int argc, char* argv[]);

private:
#ifdef _WIN32
    static void handle_client(SOCKET client_socket);
    static ssize_t recv_all(SOCKET sock, void* buffer, size_t len);
    static ssize_t send_all(SOCKET sock, const void* buffer, size_t len);
#else
    static void handle_client(int client_socket);
    static ssize_t recv_all(int sock, void* buffer, size_t len);
    static ssize_t send_all(int sock, const void* buffer, size_t len);
#endif
};

#endif // SERVER_H

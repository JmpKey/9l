#ifndef SERVER_H
#define SERVER_H

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <chrono>

// Windows specific includes
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <experimental/filesystem>
    #include <cstdlib>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
    namespace fs = std::experimental::filesystem;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    typedef int socket_t;
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

class Server
{
public:
#ifdef _WIN32
    Server():server_fd(INVALID_SOCKET){};
    ~Server();
#else
    Server(){};
#endif

    int init_server(int _argc, char* _argv[]);

private:
#ifdef WIN32
    SOCKET server_fd;
#endif
    int port = 1234;
    std::vector<std::string> ignor_addr;
    std::string line1 = "";

#ifdef _WIN32
    void handle_client(SOCKET _client_sock, sockaddr_in6 _client_addr);
    void runCommandAndWait(const std::string& command, const std::string& arg1, const std::string& arg2);
    bool removeStringFromFile(const std::string& filePath, const std::string& strToRemove);
#else
     void handle_client(int _client_sock, sockaddr_in6 client_addr);
     void removeStringFromFile(const std::string& _filename, const std::string& _strToRemove);
     void runCommandInThread(const std::string& _command, const std::string& _address, const std::string& _port);
#endif
    void print_error(const std::string& msg);
    std::string trim(const std::string& _str);
    std::string parse_config(const std::string& _filePath, const std::string& _key);
    void executeCommand(const std::string& _command);
    std::vector<std::string> readIPv6Addresses(const std::string& _filePath);
};

#endif // SERVER_H

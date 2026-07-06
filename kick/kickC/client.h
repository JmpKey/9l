#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdlib>

#ifdef _WIN32
    #define uint32_t unsigned int
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
    #define close_socket closesocket
    #include <experimental/filesystem>
    namespace fs = std::experimental::filesystem;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define close_socket close
    #define SOCKET int
    #define INVALID_SOCKET -1
#endif

struct ServerTarget {
    std::string ip;
    int port;
};

class Client
{
public:
#ifdef _WIN32
    Client();
    ~Client();
#else
    Client(){};
#endif

    int init_client(int _argc, char* _argv[]);

private:
    std::vector<ServerTarget> parse_args(int _argc, char* _argv[]);
    void process_server(const ServerTarget& _target);
    std::string trim(const std::string& _str);
    std::string parse_config(const std::string& _filePath, const std::string& _key);
    void print_error(const std::string& msg);
};

#endif // CLIENT_H

#ifndef SERVER_H
#define SERVER_H

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <set>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

class Server
{
public:
    Server(){};

    int run(int _argc, char* _argv[]);

private:
    void writeToFile(const std::string& _line, const std::string& _filename);
};

#endif // SERVER_H

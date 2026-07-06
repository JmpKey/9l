#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstring>
#include <algorithm>

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

class Client
{
public:
    Client(){};

    int start(int _argc, char* _argv[]);

private:
    void sendToAddress(const std::string& _address, int _port, const std::string& _message);
    std::string getLastLineFromFile(const std::string& _filename);
    std::vector<std::pair<std::string, int>> getIPv6Addresses(const std::string& _filename);
};

#endif // CLIENT_H

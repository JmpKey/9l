#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <string>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <cstring>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

#define DISCOVERY_PORT 12345
#define BUFFER_SIZE 1024

class Client
{
public:
    Client(){};
    int client_run(int _argc, char* _argv[]);
};

#endif // CLIENT_H

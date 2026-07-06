#ifndef SERVER_H
#define SERVER_H

#include <iostream>
#include <fstream>
#include <thread>

#ifdef _WIN32
    #include <string>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    //#include <vector>
    #include <cstring>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

#define DISCOVERY_PORT 12345
#define BUFFER_SIZE 1024

class Server
{
public:
    Server(){};
    int server_run();

private:
#ifdef _WIN32
    void client_handler(SOCKET conn_socket);
#else
    void client_handler(int conn_fd);
#endif
};

#endif // SERVER_H

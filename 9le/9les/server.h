#ifndef SERVER_H
#define SERVER_H

#include <arpa/inet.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <thread>
#include <netinet/in.h>
#include <netinet/ip6.h>

#define BUFFER_SIZE 1024

class Server
{
public:
    Server(){};

    int init_server(int _argc, char* _argv[]);
    void run_command(int client_sock);
};

#endif // SERVER_H

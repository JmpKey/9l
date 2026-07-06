#ifndef CLIENT_H
#define CLIENT_H

#include <arpa/inet.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <cstring>
#include <iostream>
#include <thread>

#define BUFFER_SIZE 1024

class Client
{
public:
    Client(){};

    void read_from_server(int sock);
    int init_client(int _argc, char* _argv[]);
};

#endif // CLIENT_H

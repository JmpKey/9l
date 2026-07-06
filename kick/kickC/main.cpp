#include "client.h"

int main(int argc, char *argv[])
{
    Client client;
    return client.init_client(argc, argv);
}

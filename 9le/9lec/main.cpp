#include "client.h"

int main(int argc, char *argv[]) {
    Client cli;
    return cli.init_client(argc, argv);
}

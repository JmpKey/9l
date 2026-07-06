#include "server.h"

int Server::init_server(int _argc, char* _argv[]) {
    if (_argc != 2) {
        std::cerr << "Usage: " << _argv[0] << " <port>" << std::endl;
        return 1;
    }

    int port = std::stoi(_argv[1]);

    int server_fd = socket(AF_INET6, SOCK_STREAM, 0); // TCP
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in6 addr{};
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = htons(port);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(server_fd, 5) < 0) { // Increasing the queue size
        perror("listen");
        return 1;
    }

    std::cout << "Server listening on port " << port << std::endl;

    while (true) {
        int client_sock = accept(server_fd, nullptr, nullptr);
        if (client_sock < 0) {
            perror("accept");
            continue;
        }

        std::cout << "Client connected, launching shell..." << std::endl;

        // Create a new thread to process the client
        std::thread client_thread(&Server::run_command, this, client_sock);
        client_thread.detach(); // Separate the thread so it runs independently
    }

    close(server_fd);
    return 0;
}

void Server::run_command(int client_sock) {
    int pipe_stdin[2], pipe_stdout[2];
    pipe(pipe_stdin);
    pipe(pipe_stdout);

    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        close(pipe_stdin[1]);
        close(pipe_stdout[0]);

        dup2(pipe_stdin[0], STDIN_FILENO);
        dup2(pipe_stdout[1], STDOUT_FILENO);
        dup2(pipe_stdout[1], STDERR_FILENO);

        close(pipe_stdin[0]);
        close(pipe_stdout[1]);

        // Run the command (for example, /bin/sh)
        execlp("/bin/sh", "sh", nullptr);
        _exit(1);
    } else if (pid > 0) {
        // Parent process
        close(pipe_stdin[0]);
        close(pipe_stdout[1]);

        fd_set readfds;
        char buffer[BUFFER_SIZE];
        int n;
        bool running = true;

        while(running) {
            FD_ZERO(&readfds);
            FD_SET(client_sock, &readfds);
            FD_SET(pipe_stdout[0], &readfds);

            int maxfd = (client_sock > pipe_stdout[0]) ? client_sock : pipe_stdout[0];
            int ret = select(maxfd+1, &readfds, nullptr, nullptr, nullptr);
            if (ret < 0) break;

            // Read data from the client and write to stdin of the process
            if (FD_ISSET(client_sock, &readfds)) {
                n = read(client_sock, buffer, BUFFER_SIZE);
                if (n <= 0) {
                    running = false;
                } else {
                    write(pipe_stdin[1], buffer, n);
                }
            }

            // Reading the output of the process and sending it to the client
            if (FD_ISSET(pipe_stdout[0], &readfds)) {
                n = read(pipe_stdout[0], buffer, BUFFER_SIZE);
                if (n <= 0) {
                    running = false;
                } else {
                    write(client_sock, buffer, n);
                }
            }
        }

        close(pipe_stdin[1]);
        close(pipe_stdout[0]);
        waitpid(pid, nullptr, 0);
    } else {
        perror("fork");
    }

    close(client_sock); // Closing the client socket after completion of work
}

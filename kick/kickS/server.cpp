#include "server.h"

#ifdef _WIN32
Server::~Server() {
    if (server_fd != INVALID_SOCKET) closesocket(server_fd);
    WSACleanup();
}
#endif

void Server::print_error(const std::string& msg) {
#ifdef _WIN32
    std::cerr << msg << " Error code: " << WSAGetLastError() << std::endl;
#else
    perror(msg.c_str());
#endif
}

int Server::init_server(int _argc, char* _argv[]) {
    if (_argc != 2) {
        std::cerr << "Usage: " << _argv[0] << " <port>" << std::endl;
        return 1;
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }
#endif

    port = std::stoi(_argv[1]);
    int server_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        std::cerr << "Error creating socket" << std::endl;
        return 1;
    }

    int opt = 1;
#ifdef WIN32
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    sockaddr_in6 addr{};
    addr.sin6_family = AF_INET6;
    addr.sin6_addr = in6addr_any;
    addr.sin6_port = htons(port);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed" << std::endl;
        return 1;
    }

    if (listen(server_fd, 10) == SOCKET_ERROR) {
        std::cerr << "Listen failed" << std::endl;
        return 1;
    }

    std::cout << "Server listening on port " << port << " (IPv6)" << std::endl;

    while (true) {
        sockaddr_in6 client_addr{};
#ifdef WIN32
        int client_len = sizeof(client_addr);
        SOCKET client_sock = accept(server_fd, (sockaddr*)&client_addr, &client_len);
#else
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_fd, (sockaddr*)&client_addr, &client_len);
#endif
        if (client_sock == INVALID_SOCKET) {
            continue;
        }

        std::thread(&Server::handle_client, this, client_sock, client_addr).detach();
    }

#ifdef _WIN32
    closesocket(server_fd);
#else
    close(server_fd);
#endif
    return 0;
}

#ifdef _WIN32
void Server::handle_client(SOCKET _client_sock, sockaddr_in6 _client_addr) {
    char addr_str[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &_client_addr.sin6_addr, addr_str, sizeof(addr_str));

    std::string clientsPath = parse_config("server.conf", "[clients]");
    if (clientsPath.empty()) clientsPath = "clients.txt";

    std::ifstream readFile(clientsPath);
    std::string line;
    bool exists = false;
    if (readFile.is_open()) {
        while (std::getline(readFile, line)) {
            if (trim(line) == addr_str) {
                exists = true;
                break;
            }
        }
        readFile.close();
    }

    if (!exists) {
        std::ofstream log_file(clientsPath, std::ios::app);
        if (log_file.is_open()) {
            log_file << addr_str << std::endl;
            log_file.close();
        } else {
            std::cerr << "Error: Failed to open " << clientsPath << " for appending client address." << std::endl;
        }
    }

    std::string nodesPath = parse_config("server.conf", "[nodes]");
    if (nodesPath.empty()) nodesPath = "nodes.txt";

    std::ifstream data_file(nodesPath);
    if (data_file.is_open()) {
        std::vector<std::string> lines;
        while (std::getline(data_file, line)) {
            lines.push_back(line);
        }
        data_file.close();

        for (size_t i = 0; i < lines.size(); ++i) {
            send(_client_sock, lines[i].c_str(), (int)lines[i].size(), 0);
            if (i < lines.size() - 1) {
                send(_client_sock, "\n", 1, 0);
            }
        }

        std::vector<std::string> addresses = readIPv6Addresses(clientsPath);
        for (const auto& address : addresses) {
        	if(ignor_addr.empty() || !(std::find(ignor_addr.begin(), ignor_addr.end(), address) != ignor_addr.end())) {
                	std::cout << "Target: " << address << std::endl;
                	// !!! WE USE A NEW runCommandAndWait, WHICH WAITS FOR THE CLIENT TO COMPLETE!!!
                	ignor_addr.push_back(address);
                	runCommandAndWait("kickC.exe", address, std::to_string(port));

                	if (!removeStringFromFile(clientsPath, address)) {
                    		std::cerr << "Failed to remove address '" << address << "' from " << clientsPath << std::endl;
                	}
                	std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }
    } else {
        std::string error_msg = "NodesMeditation->48454C50\n";
        send(_client_sock, error_msg.c_str(), (int)error_msg.size(), 0);
        std::cerr << "Error: Could not open " << nodesPath << " for sending data to client." << std::endl;
    }

    closesocket(_client_sock);
}
#else
void Server::handle_client(int _client_sock, sockaddr_in6 _client_addr) {
    // Write the client's address to file
    char addr_str[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &_client_addr.sin6_addr, addr_str, sizeof(addr_str));

    std::string clientsPath = parse_config("server.conf", "[clients]");
    if (!clientsPath.empty()) {
        std::cout << "Clients Path: " << clientsPath << std::endl;
    } else {
        std::cout << "Clients path not found." << std::endl;
        clientsPath = "clients";
    }

    std::ifstream readFile(clientsPath);
    std::string line;
    bool exists = false;

    while (std::getline(readFile, line1)) {
        if (line1 == addr_str) {
            exists = true; // Entry found
            break;
        }
    }
    readFile.close();

    if (!exists) {
        std::ofstream log_file(clientsPath, std::ios::app);
        if (log_file.is_open()) {
            log_file << addr_str << std::endl;
            log_file.close();
        } else {
            std::cerr << "Error clients read" << std::endl;
        }
    }

    // We read the data from the specified file and send it to the client
    std::string nodesPath = parse_config("server.conf", "[nodes]");
    if (!nodesPath.empty()) {
        std::cout << "Nodes Path: " << nodesPath << std::endl;
    } else {
        std::cout << "Nodes path not found." << std::endl;
        clientsPath = "nodes";
    }

    std::ifstream data_file(nodesPath);
    if (data_file.is_open()) {
        std::string line;
        std::vector<std::string> lines;

        // Read all the lines into a vector
        while (std::getline(data_file, line)) {
            lines.push_back(line);
        }
        data_file.close();

        // Sending strings over a socket
        for (size_t i = 0; i < lines.size(); ++i) {
            send(_client_sock, lines[i].c_str(), lines[i].size(), 0);
            // Line wrap other than the last line
            if (i < lines.size() - 1) {
                send(_client_sock, "\n", 1, 0);
            }
        }
        // {
        std::vector<std::string> addresses = readIPv6Addresses(clientsPath);
        for (const auto& address : addresses) {
        if(ignor_addr.empty() || !(std::find(ignor_addr.begin(), ignor_addr.end(), address) != ignor_addr.end())) {
                std::cout << "Target: " << address << std::endl;
		// !!! WE USE A NEW runCommandAndWait, WHICH WAITS FOR THE CLIENT TO COMPLETE!!!
                ignor_addr.push_back(address);
                runCommandInThread("./kickC", address, std::to_string(port));
                // The main thread continues to run
                removeStringFromFile(clientsPath, address);
                std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        }
        // }
    } else {
        std::string error_msg = "NodesMeditation->48454C50\n";
        /*
         * ?>...
         * Create a server.conf file with two fields:
         * [nodes] = [path/to/file/your/nodes]
         * [clients] = [path/to/file/your/clients]
         * (Where your binar)
         */
        send(_client_sock, error_msg.c_str(), error_msg.size(), 0);
    }

    close(_client_sock);
}
#endif

std::string Server::trim(const std::string& _str) {
#ifdef WIN32
    if (_str.empty()) return "";
    size_t first = _str.find_first_not_of(" \t\r\n");
    size_t last = _str.find_last_not_of(" \t\r\n");
    return (first == std::string::npos) ? "" : _str.substr(first, (last - first + 1));
#else
    size_t first = _str.find_first_not_of(" \t");
    size_t last = _str.find_last_not_of(" \t");
    return (first == std::string::npos) ? "" : _str.substr(first, (last - first + 1));

#endif
}

void Server::executeCommand(const std::string& _command) {
    int result = std::system(_command.c_str());
    std::cout << (result != 0 ? "1" : "0") << std::endl;
}

#ifdef WIN32
void Server::runCommandAndWait(const std::string& command, const std::string& arg1, const std::string& arg2) {
    std::string fullCommand = command + " [" + arg1 + " " + arg2 + "]";

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    std::vector<char> cmdBuffer(fullCommand.begin(), fullCommand.end());
    cmdBuffer.push_back('\0');

    std::cout << "Launching client: " << cmdBuffer.data() << std::endl;

    if (!CreateProcessA(NULL,            // Module name (NULL - command line is used)
                       cmdBuffer.data(), // Command line
                       NULL,             // Process handle is not inherited
                       NULL,             // Thread handle is not inherited
                       FALSE,            // Setting handle inheritance to FALSE
                       0,                // Flags creating
                       NULL,             // Using a parent's environment block
                       NULL,             // Using the parent's start directory
                       &si,              // Pointer to the STARTUPINFO structure
                       &pi)              // Pointer to a PROCESS_INFORMATION structure
    ) {
        std::cerr << "CreateProcess failed (" << GetLastError() << ").\n";
        return;
    }

    // We are waiting for the child process to complete
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Closing process and thread handles
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}
#else
void Server::runCommandInThread(const std::string& _command, const std::string& _address, const std::string& _port) {
    // On Windows, IPv6 often requires quotes on the command line if there are parentheses
    std::string fullCommand = _command + " " + "[" + _address + " " + _port + "]";

    std::thread([this, fullCommand]() {
        executeCommand(fullCommand);
    }).detach();
}
#endif

#ifdef WIN32
bool Server::removeStringFromFile(const std::string& filePath, const std::string& strToRemove) {
    fs::path p(filePath);
    fs::path temp_p = p.string() + ".tmp"; // Temporary file

    std::ifstream inFile(filePath);
    if (!inFile.is_open()) {
        std::cerr << "Error: Could not open file for reading: " << filePath << std::endl;
        return false;
    }

    std::ofstream outFile(temp_p.string());
    if (!outFile.is_open()) {
        std::cerr << "Error: Could not open temporary file for writing: " << temp_p << std::endl;
        inFile.close();
        return false;
    }

    std::string line;
    bool removed_item = false;
    while (std::getline(inFile, line)) {
        if (trim(line) == strToRemove) {
            removed_item = true; // A line to be deleted has been found, skip it
        } else {
            outFile << line << std::endl;
        }
    }

    inFile.close();
    outFile.close();

    if (!removed_item) {
        // The line was not found, no need to replace the file
        try {
            if (fs::exists(temp_p)) {
                fs::remove(temp_p); // Deleting a temporary file
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Warning: Failed to remove temporary file " << temp_p << ": " << e.what() << std::endl;
        }
        return true;
    }

    try {
        if (fs::exists(p)) {
            fs::remove(p);
        }
        fs::rename(temp_p, p); // Rename the temporary file to the original one
        return true;
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error during file replacement for " << filePath << ": " << e.what() << std::endl;
        // Trying to clear a temporary file when renaming fails
        try {
            if (fs::exists(temp_p)) {
                fs::remove(temp_p);
            }
        } catch (...) { /* ignore */ }
        return false;
    }
}
#else
void Server::removeStringFromFile(const std::string& _filename, const std::string& _strToRemove) {
    std::ifstream inputFile(_filename);
    std::ofstream tempFile("temp.txt");
    std::string line;

    if (inputFile.is_open() && tempFile.is_open()) {
        bool isFirstLine = true; // Flag to track first line

        while (std::getline(inputFile, line)) {
            if (isFirstLine) {
                // Проверяем первую строку
                if (line.find(_strToRemove) == std::string::npos) {
                    // If the first line does not contain the desired substring, write it to a temporary file
                    tempFile << line << '\n';
                }
                isFirstLine = false; // Set the flag for subsequent lines
            } else {
                // Write the remaining lines unchanged
                tempFile << line << '\n';
            }
        }

        inputFile.close();
        tempFile.close();

        // Deleting the original file
        std::remove(_filename.c_str());
        // Rename the temporary file to its original name
        std::rename("temp.txt", _filename.c_str());
    } else {
        std::cerr << "Unable to open file." << std::endl;
    }
}
#endif

std::string Server::parse_config(const std::string& _filePath, const std::string& _key) {
    std::ifstream configFile(_filePath);
    if (!configFile.is_open()) return "";

    std::string line;
    while (std::getline(configFile, line)) {
        if (line.find(_key) != std::string::npos) {
            size_t equalPos = line.find('=');
            if (equalPos != std::string::npos) {
                std::string value = line.substr(equalPos + 1);
                value = trim(value);
                if (!value.empty() && value.front() == '[' && value.back() == ']') {
                    return trim(value.substr(1, value.size() - 2));
                }
            }
        }
    }
    return "";
}

std::vector<std::string> Server::readIPv6Addresses(const std::string& _filePath) {
    std::vector<std::string> ipv6Addresses;
    std::ifstream file(_filePath);
    if (!file.is_open()) return ipv6Addresses;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (!line.empty()) ipv6Addresses.push_back(line);
    }
    file.close();

    std::sort(ipv6Addresses.begin(), ipv6Addresses.end());
    ipv6Addresses.erase(std::unique(ipv6Addresses.begin(), ipv6Addresses.end()), ipv6Addresses.end());

    return ipv6Addresses;
}

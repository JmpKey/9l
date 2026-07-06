#include "client.h"

#ifdef _WIN32
// WindowsNT zone
std::vector<std::pair<std::string, int>> Client::getIPv6Addresses(const std::string& _filename) {
    std::vector<std::pair<std::string, int>> addresses;
    std::ifstream file(_filename);
    std::string line;
    bool foundAddressesSection = false;

    while (std::getline(file, line)) {
        line.erase(0, line.find_first_not_of(" \n\r\t"));
        line.erase(line.find_last_not_of(" \n\r\t") + 1);

        if (line == "- List of IPv6 Addresses of Seeders:") {
            foundAddressesSection = true;
            continue;
        }

        if (foundAddressesSection) {
            if (line.rfind("-", 0) == 0) {
                size_t pos = line.find('#');
                if (pos != std::string::npos && pos + 1 < line.size()) {
                    std::string address = line.substr(1, pos - 1);
                    address.erase(std::remove(address.begin(), address.end(), ' '), address.end());
                    address.erase(std::remove(address.begin(), address.end(), '"'), address.end());
                    try {
                        int port = std::stoi(line.substr(pos + 1));
                        addresses.emplace_back(address, port + 1); // This is necessary so that the notification does not conflict with the distribution, so reserve the next port  [Problem nport+1]
                    } catch (...) {
                        std::cerr << "Ошибка в порте: " << address << std::endl;
                    }
                }
            }
        }
    }
    return addresses;
}

std::string Client::getLastLineFromFile(const std::string& _filename) {
    std::ifstream file(_filename);
    std::string line;
    std::string lastLine;
    while (std::getline(file, line)) {
        if(!line.empty()) lastLine = line;
    }
    return lastLine;
}

void Client::sendToAddress(const std::string& _address, int port, const std::string& _message) {
    SOCKET sockfd = socket(AF_INET6, SOCK_STREAM, 0);
    if (sockfd == INVALID_SOCKET) {
        std::cerr << "Error creating socket." << std::endl;
        return;
    }

    struct sockaddr_in6 server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_port = htons((unsigned short)port);

    if (inet_pton(AF_INET6, _address.c_str(), &server_addr.sin6_addr) <= 0) {
        std::cerr << "Invalid IPv6 address: " << _address << std::endl;
        closesocket(sockfd);
        return;
    }

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Error connecting to " << _address << ":" << port << std::endl;
        closesocket(sockfd);
        return;
    }

    send(sockfd, _message.c_str(), (int)_message.length(), 0);
    closesocket(sockfd);
}

int Client::start(int _argc, char* _argv[]) {
    if (_argc != 2) {
        std::cerr << "Use: " << _argv[0] << " <file>" << std::endl;
        return 1;
    }

    // Init Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return 1;
    }

    std::string filename = _argv[1];
    auto addresses = getIPv6Addresses(filename);
    std::string message = getLastLineFromFile(filename);

    if (addresses.empty()) {
        std::cerr << "No addresses available." << std::endl;
        WSACleanup();
        return 1;
    }

    for (size_t i = 0; i < addresses.size() - 1; ++i) {
        sendToAddress(addresses[i].first, addresses[i].second, message);
    }

    WSACleanup();
    return 0;
}

#else
// Unix-like zone
std::vector<std::pair<std::string, int>> Client::getIPv6Addresses(const std::string& _filename) {
    std::vector<std::pair<std::string, int>> addresses;
    std::ifstream file(_filename);
    std::string line;
    bool foundAddressesSection = false;

    while (std::getline(file, line)) {
        // Remove spaces at the beginning and end of a line
        line.erase(0, line.find_first_not_of(" \n\r\t"));
        line.erase(line.find_last_not_of(" \n\r\t") + 1);

        // Checking if we have found the section with addresses
        if (line == "- List of IPv6 Addresses of Seeders:") {
            foundAddressesSection = true;
            continue; // next string
        }

        // If we are in the addresses section
        if (foundAddressesSection) {
            // Checking that the line starts with '-'
            if (line.rfind("-", 0) == 0) {
                // Extracting the address and port
                size_t pos = line.find('#');
                if (pos != std::string::npos && pos + 1 < line.size()) {
                    std::string address = line.substr(1, pos - 1); // Del '- ' before the address
                    address.erase(std::remove(address.begin(), address.end(), ' '), address.end());
                    address.erase(std::remove(address.begin(), address.end(), '"'), address.end());
                    try {
                        int port = std::stoi(line.substr(pos + 1));
                        addresses.emplace_back(address, port + 1); // This is necessary so that the notification does not conflict with the distribution, so reserve the next port  [Problem nport+1]
                    } catch (const std::invalid_argument& e) {
                        std::cerr << "Error converting port to address " << address << ": " << e.what() << std::endl;
                    } catch (const std::out_of_range& e) {
                        std::cerr << "Port out of range for address " << address << ": " << e.what() << std::endl;
                    }
                }
            }
        }
    }

    return addresses;
}

std::string Client::getLastLineFromFile(const std::string& _filename) {
    std::ifstream file(_filename);
    std::string line;
    std::string lastLine;

    while (std::getline(file, line)) {
        lastLine = line; // Save the last line
    }

    return lastLine;
}

void Client::sendToAddress(const std::string& _address, int port, const std::string& _message) {
    int sockfd;
    struct sockaddr_in6 server_addr;

    sockfd = socket(AF_INET6, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cerr << "Error creating socket." << std::endl;
        return;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_port = htons(port);

    inet_pton(AF_INET6, _address.c_str(), &server_addr.sin6_addr);

    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Error connecting to " << _address << ":" << port << std::endl;
        close(sockfd);
        return;
    }

    send(sockfd, _message.c_str(), _message.length(), 0);

    close(sockfd);
}

int Client::start(int _argc, char* _argv[]) {
    if (_argc != 2) {
        std::cerr << "Use: " << _argv[0] << " <file>" << std::endl;
        return 1;
    }

    std::string filename = _argv[1];
    
    auto addresses = getIPv6Addresses(filename);
    std::string message = getLastLineFromFile(filename);

    // Checking if there are addresses for connection
    if (addresses.empty()) {
        std::cerr << "There are no available addresses to connect to." << std::endl;
        return 1;
    }

    // We go to all addresses except the last one
    for (size_t i = 0; i < addresses.size() - 1; ++i) {
        const auto& [address, port] = addresses[i];
        sendToAddress(address, port, message);
    }

    return 0;
}
#endif

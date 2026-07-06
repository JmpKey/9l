#include "client.h"

#ifdef _WIN32
// WindowsNT zone
long long getFileSize(const std::string& fileName) {
    HANDLE hFile = CreateFileA(
        fileName.c_str(),
        GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return 0; // Error open file
    }

    LARGE_INTEGER fileSize;
    if (GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        return fileSize.QuadPart; // Return size file
    } else {
        CloseHandle(hFile);
        return 0; // Error get size file
    }
}

std::string getWinError() {
    return std::to_string(WSAGetLastError());
}

std::vector<Peer> Client::parse_manifest(const std::string& filepath) {
    std::vector<Peer> found_peers;
    std::ifstream file(filepath);
    std::string line;
    bool in_seeder_list = false;

    if (!file.is_open()) return found_peers;

    while (std::getline(file, line)) {
        if (line.find("List of IPv6 Addresses of Seeders:") != std::string::npos) {
            in_seeder_list = true;
            continue;
        }
        if (in_seeder_list) {
            size_t start = line.find('\"');
            size_t end = line.find_last_of('\"');
            if (start != std::string::npos && end != std::string::npos && end > start) {
                std::string content = line.substr(start + 1, end - start - 1);
                size_t hash_pos = content.find('#');
                if (hash_pos != std::string::npos) {
                    Peer p;
                    p.ip = content.substr(0, hash_pos);
                    try {
                        p.port = std::stoi(content.substr(hash_pos + 1));
                        found_peers.push_back(p);
                    } catch (...) {}
                }
            }
        }
    }
    return found_peers;
}

bool Client::download_file(const Peer& peer, const std::string& filename, int& out_file_type, std::string& out_path) {
    std::string base_filename = removeUntilLastSlash(filename);

    // Trying to download (5 attempts)
    for (int attempt = 0; attempt < 5; ++attempt) {
        SOCKET sock = socket(AF_INET6, SOCK_STREAM, 0);
        if (sock == -1) {
            sleep(1);
            continue;
        }

        // Install timeout
        DWORD timeout = 5000;
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

        struct sockaddr_in6 server_addr{};
        server_addr.sin6_family = AF_INET6;
        server_addr.sin6_port = htons(peer.port);
        if (inet_pton(AF_INET6, peer.ip.c_str(), &server_addr.sin6_addr) <= 0) {
            SOCKET_CLOSE(sock); return false;
        }

        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            SOCKET_CLOSE(sock); sleep(1); continue;
        }

        // 1. DETERMINING THE PATH AND OFFSET (Offset)
        // First - find out the file type to understand where to download
        // But the protocol requires you to send the offset first
        // Therefore, first we make a "prediction" of the path
        std::string pred_path = "files/" + base_filename;
        if (base_filename.find(".txt") != std::string::npos) pred_path = "manifest/" + base_filename;

        int64_t offset_to_send = 0; // use int64_t
        if (fs::exists(pred_path) && !fs::is_directory(pred_path)) {
            std::error_code ec;
            offset_to_send = static_cast<int64_t>(getFileSize(pred_path));
            std::cout << "File size: " << offset_to_send << std::endl;
        }

        // 2. NETWORK EXCHANGE
        char filename_buffer[FILENAME_BUFFER_SIZE] = {0};
        strncpy(filename_buffer, base_filename.c_str(), FILENAME_BUFFER_SIZE - 1);

        // Helmet name
        if (send_all(sock, filename_buffer, FILENAME_BUFFER_SIZE) <= 0) {
            SOCKET_CLOSE(sock); continue;
        }
        // Helm offset (strictly 8 bytes)
        if (send_all(sock, &offset_to_send, sizeof(offset_to_send)) <= 0) {
            SOCKET_CLOSE(sock); continue;
        }
        // Getting the type
        if (recv_all(sock, &out_file_type, sizeof(out_file_type)) <= 0) {
            SOCKET_CLOSE(sock); continue;
        }
        if (out_file_type == -1) {
            SOCKET_CLOSE(sock); return false;
        }
        // We get the full size (strictly 8 bytes)
        int64_t remote_file_size = -1;
        if (recv_all(sock, &remote_file_size, sizeof(remote_file_size)) <= 0) {
            SOCKET_CLOSE(sock); continue;
        }

        // 3. SPECIFYING THE PATH TO SAVE
        std::string actual_save_path;
        if (out_file_type == 1) {
            if (!fs::exists("manifest")) fs::create_directory("manifest");
            size_t dot = base_filename.find('.');
            std::string name = (dot != std::string::npos) ? base_filename.substr(0, dot) : base_filename;
            actual_save_path = "manifest/" + name + ".txt";
            offset_to_send = 0; // We download manifestos from scratch
        } else {
            if (!fs::exists("files")) fs::create_directory("files");
            actual_save_path = "files/" + base_filename;
        }
        out_path = actual_save_path;

        // If the file is already ready
        int64_t current_on_disk = 0;
        if (fs::exists(actual_save_path)) current_on_disk = static_cast<int64_t>(getFileSize(actual_save_path));

        if (current_on_disk >= remote_file_size && remote_file_size > 0) {
            SOCKET_CLOSE(sock); return true;
        }

        // 4. WRITE IN FILE
        // IMPORTANT: On Windows we DO NOT use ios::app to resume downloading!
        // Using binary | in | out for seekp capability
        std::ofstream outfile;
        if (!fs::exists(actual_save_path)) {
            outfile.open(actual_save_path, std::ios::binary | std::ios::out);
            outfile.close();
        }
        outfile.open(actual_save_path, std::ios::binary | std::ios::in | std::ios::out);
        if (!outfile.is_open()) {
            SOCKET_CLOSE(sock); return false;
        }

        // We get into the resuming position
        outfile.seekp(offset_to_send, std::ios::beg);

        char buffer[BUFFER_SIZE];
        int64_t current_received = offset_to_send;

        while (current_received < remote_file_size) {
            // recv on Windows returns int, convert to ssize_t
            int bytes = recv(sock, buffer, BUFFER_SIZE, 0);
            if (bytes <= 0) break;

            outfile.write(buffer, bytes);
            current_received += bytes;
        }

        outfile.flush();
        outfile.close();
        SOCKET_CLOSE(sock);

        if (current_received >= remote_file_size) return true;
        std::cout << "[Client] Retry... (" << current_received << "/" << remote_file_size << ")" << std::endl;
    }
    return false;
}

int Client::cli_start(int argc, char* argv[]) {
    if (argc != 2) {
	std::cerr << "Usage: " << argv[0] << " <initial_manifest>" << std::endl;
        return 1;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 1;

    std::string initial_manifest = argv[1];
    std::string target_file_gz = initial_manifest;
    size_t lastDotPos = target_file_gz.find_last_of('.');
    if(lastDotPos != std::string::npos) target_file_gz = target_file_gz.substr(0, lastDotPos);
    target_file_gz += ".tar.gz";

    for (int global_attempt = 0; global_attempt < 10; ++global_attempt) {
        std::deque<Peer> queue;
        std::set<Peer> visited;
        bool file_found_somewhere = false;
        bool had_network_errors = false;

        auto initial_peers = parse_manifest(initial_manifest);
        for (const auto& p : initial_peers) queue.push_back(p);

        while (!queue.empty()) {
            Peer current_peer = queue.front();
            queue.pop_front();

            if (visited.count(current_peer)) continue;
            visited.insert(current_peer);

            int file_type = -1;
            std::string saved_path;

            if (download_file(current_peer, target_file_gz, file_type, saved_path)) {
                if (file_type == 0) {
                    WSACleanup();
                    return 0;
                } else {
                    auto new_peers = parse_manifest(saved_path);
                    for (const auto& p : new_peers) queue.push_back(p);
                    file_found_somewhere = true;
                }
            } else {
                if (file_type != -1) file_found_somewhere = true;
                had_network_errors = true;
            }
        }

        if (file_found_somewhere || had_network_errors) sleep(5);
        else break;
    }

    WSACleanup();
    return 1;
}

std::string Client::removeUntilLastSlash(const std::string& input) {
    size_t pos = input.find_last_of("/\\");
    if (pos != std::string::npos) return input.substr(pos + 1);
    return input;
}

ssize_t Client::send_all(SOCKET sock, const void* buffer, size_t len) {
    size_t total = 0;
    const char* buf = (const char*)buffer;
    while (total < len) {
        int n = send(sock, buf + total, (int)(len - total), 0);
        if (n <= 0) return n;
        total += n;
    }
    return (ssize_t)total;
}

ssize_t Client::recv_all(SOCKET sock, void* buffer, size_t len) {
    size_t total = 0;
    char* buf = (char*)buffer;
    while (total < len) {
        int n = recv(sock, buf + total, (int)(len - total), 0);
        if (n <= 0) return n;
        total += n;
    }
    return (ssize_t)total;
}


#else
// Unix-like zone
std::vector<Peer> Client::parse_manifest(const std::string& filepath) {
    std::vector<Peer> found_peers;
    std::ifstream file(filepath);
    std::string line;
    bool in_seeder_list = false;

    if (!file.is_open()) {
        std::cerr << "[Client] Error: Could not open manifest file: " << filepath << std::endl;
        return found_peers;
    }

    while (std::getline(file, line)) {
        if (line.find("List of IPv6 Addresses of Seeders:") != std::string::npos) {
            in_seeder_list = true;
            continue;
        }
        if (in_seeder_list) {
            // We are looking for a line like - "address#port"
            size_t start = line.find('\"');
            size_t end = line.find_last_of('\"');
            if (start != std::string::npos && end != std::string::npos && end > start) {
                std::string content = line.substr(start + 1, end - start - 1);
                size_t hash_pos = content.find('#');
                if (hash_pos != std::string::npos) {
                    Peer p;
                    p.ip = content.substr(0, hash_pos);
                    try {
                        p.port = std::stoi(content.substr(hash_pos + 1));
                        found_peers.push_back(p);
                    } catch (const std::out_of_range& oor) {
                        std::cerr << "[Client] Port out of range in manifest: " << content.substr(hash_pos + 1) << std::endl;
                    } catch (const std::invalid_argument& ia) {
                        std::cerr << "[Client] Invalid port in manifest: " << content.substr(hash_pos + 1) << std::endl;
                    }
                }
            }
        }
    }
    return found_peers;
}

bool Client::download_file(const Peer& peer, const std::string& filename, int& out_file_type, std::string& out_path) {
    std::string local_path_for_offset_calc = filename; 
    
    long long offset_to_send_to_peer = 0; // offset we will send to the peer
    long long remote_file_size = -1;
    std::ios_base::openmode mode = std::ios::binary; // ADD

    // We are trying to download/renew the file (max. 5 reconnection attempts)
    for (int attempt = 0; attempt < 5; ++attempt) {
        int sock = socket(AF_INET6, SOCK_STREAM, 0);
        if (sock < 0) {
            std::cerr << "[Client] Error creating socket (Attempt " << attempt + 1 << "/5): " << strerror(errno) << std::endl;
            sleep(1); 
            continue; 
        }

        // Install timeout for socket
        struct timeval timeout;
        timeout.tv_sec = 5; // 5s
        timeout.tv_usec = 0;

        if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout)) < 0) {
            std::cerr << "[Client] setsockopt SO_SNDTIMEO failed (Attempt " << attempt + 1 << "/5): " << strerror(errno) << std::endl;
            close(sock);
            sleep(1);
            continue;
        }

        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) < 0) {
            std::cerr << "[Client] setsockopt SO_RCVTIMEO failed (Attempt " << attempt + 1 << "/5): " << strerror(errno) << std::endl;
            close(sock);
            sleep(1);
            continue;
        }
        // End install timeout

        struct sockaddr_in6 server_addr{};
        server_addr.sin6_family = AF_INET6;
        server_addr.sin6_port = htons(peer.port);
        if (inet_pton(AF_INET6, peer.ip.c_str(), &server_addr.sin6_addr) <= 0) {
            std::cerr << "[Client] Invalid IP address: " << peer.ip << std::endl;
            close(sock); 
            return false;
        }

        if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "[Client] Failed to connect to [" << peer.ip << "]:" << peer.port
                      << " (Attempt " << attempt + 1 << "/5) - " << strerror(errno) << std::endl;
            close(sock);
            sleep(1); 
            continue; 
        }
        std::cout << "[Client] Successfully connected to [" << peer.ip << "]:" << peer.port << " on attempt " << attempt + 1 << std::endl;

        // Calculate offset based on local file
        //if (std::filesystem::exists("files/" + removeUntilLastSlash(filename))) { local_path_for_offset_calc = "files/" + removeUntilLastSlash(filename); }
        if (std::filesystem::exists("files/" + removeUntilLastSlash(filename))) { offset_to_send_to_peer = std::filesystem::file_size("files/" + removeUntilLastSlash(filename)); }
	else {
        if (std::filesystem::exists(local_path_for_offset_calc) && !std::filesystem::is_directory(local_path_for_offset_calc)) {
            try {
                offset_to_send_to_peer = std::filesystem::file_size(local_path_for_offset_calc);
            } catch (const std::filesystem::filesystem_error& e) {
                std::cerr << "[Client] Error getting file size for " << local_path_for_offset_calc << ": " << e.what() << std::endl;
                close(sock);
                continue; // Try again, the problem may be temporary
            }
        }
        }

        // 1. Send name (we send only the base file name to the server)
        char filename_buffer[FILENAME_BUFFER_SIZE] = {0};
        strncpy(filename_buffer, removeUntilLastSlash(filename).c_str(), FILENAME_BUFFER_SIZE - 1);
        if (send_all(sock, filename_buffer, FILENAME_BUFFER_SIZE) <= 0) {
            std::cerr << "[Client] Error sending filename to [" << peer.ip << "]:" << peer.port
                      << " (Attempt " << attempt + 1 << "/5) - " << strerror(errno) << ". Retrying..." << std::endl;
            close(sock); continue;
        }

        // 2. Helmet offset
        if (send_all(sock, &offset_to_send_to_peer, sizeof(offset_to_send_to_peer)) <= 0) {
            std::cerr << "[Client] Error sending offset to [" << peer.ip << "]:" << peer.port
                      << " (Attempt " << attempt + 1 << "/5) - " << strerror(errno) << ". Retrying..." << std::endl;
            close(sock); continue;
        }

        // 3. We get the status (out_file_type: 0 - target file, 1 - manifest, -1 - not found)
        if (recv_all(sock, &out_file_type, sizeof(out_file_type)) <= 0) { 
            std::cerr << "[Client] Error receiving file type from [" << peer.ip << "]:" << peer.port
                      << " (Attempt " << attempt + 1 << "/5) - " << strerror(errno) << ". Retrying..." << std::endl;
            close(sock); continue; 
        }
        if (out_file_type == -1) { 
            std::cout << "[Client] File '" << filename << "' not found on [" << peer.ip << "]:" << peer.port << std::endl;
            close(sock); 
            return false; // File not found on this peer, no need to repeat
        }

        // 4. Get size
        if (recv_all(sock, &remote_file_size, sizeof(remote_file_size)) <= 0) { 
            std::cerr << "[Client] Error receiving file size from [" << peer.ip << "]:" << peer.port
                      << " (Attempt " << attempt + 1 << "/5) - " << strerror(errno) << ". Retrying..." << std::endl;
            close(sock); continue; 
        }

        // This is where the modification begins: determining the real save path
        std::string actual_save_path;
        long long current_local_bytes_on_disk = 0; // Actual size of the file on disk in the save location

        if (out_file_type == 1) { // it manifest
            mode |= std::ios::trunc; // ADD
            if (!std::filesystem::exists("manifest")) {
                std::error_code ec;
                std::filesystem::create_directory("manifest", ec);
                if (ec) {
                    std::cerr << "[Client] Error creating directory 'manifest': " << ec.message() << std::endl;
                    close(sock);
                    return false;
                }
            }
            std::string base_filename = removeUntilLastSlash(filename);
            size_t firstDotPos = base_filename.find('.');
            std::string tmp_manifest_name = base_filename;
            if(firstDotPos != std::string::npos) { tmp_manifest_name.erase(firstDotPos); }

            actual_save_path = "manifest/" + tmp_manifest_name + ".txt";
            current_local_bytes_on_disk = 0; // For manifests, we start from scratch (rewrite/download again)
        } else if (out_file_type == 0) { // This is the target file (not the manifest)
            mode |= std::ios::app; // ADD
            if (!std::filesystem::exists("files")) {
                std::error_code ec;
                std::filesystem::create_directory("files", ec);
                if (ec) {
                    std::cerr << "[Client] Error creating directory 'files': " << ec.message() << std::endl;
                    close(sock);
                    return false;
                }
            }
            actual_save_path = "files/" + removeUntilLastSlash(filename);
            
            // For target files we download, so we calculate the actual size on disk
            if (std::filesystem::exists(actual_save_path) && !std::filesystem::is_directory(actual_save_path)) {
                try {
                    current_local_bytes_on_disk = std::filesystem::file_size(actual_save_path);
                } catch (const std::filesystem::filesystem_error& e) {
                    std::cerr << "[Client] Error getting file size for " << actual_save_path << ": " << e.what() << std::endl;
                    close(sock);
                    return false; 
                }
            }
        } else { // Unknown file type
            std::cerr << "[Client] Unknown file type received from peer (" << out_file_type << ") for file: " << filename << std::endl;
            close(sock);
            return false;
        }
        
        out_path = actual_save_path; // Update the output parameter out_path

        // If the file has already been downloaded completely
        if (current_local_bytes_on_disk >= remote_file_size && remote_file_size > 0) {
            std::cout << "[Client] File " << removeUntilLastSlash(filename) << " already fully downloaded to " << actual_save_path << std::endl;
            close(sock); return true;
        }

        // 5. Download in add-on mode (app)
        std::ofstream outfile(actual_save_path, mode); // ADD
        if (!outfile.is_open()) {
            std::cerr << "[Client] Error opening local file for writing: " << actual_save_path << std::endl;
            close(sock);
            return false; 
        }

        char buffer[BUFFER_SIZE];
	
        long long current_received = (out_file_type == 1) ? 0 : current_local_bytes_on_disk; // ADD

        while (current_received < remote_file_size) {
            ssize_t bytes = recv(sock, buffer, BUFFER_SIZE, 0);
            if (bytes < 0) { 
                std::cerr << "[Client] Error during download from [" << peer.ip << "]:" << peer.port
                          << ": " << strerror(errno) << ". Breaking for retry..." << std::endl;
                break; 
            }
            if (bytes == 0) { 
                std::cerr << "[Client] Peer closed connection during download from [" << peer.ip << "]:" << peer.port
                          << ". Breaking for retry..." << std::endl;
                break;
            }
            outfile.write(buffer, bytes);
            current_received += bytes;
        }
        outfile.close();
        close(sock);

        if (current_received >= remote_file_size) {
            std::cout << "[Client] Successfully downloaded " << removeUntilLastSlash(filename) << " from [" << peer.ip << "]:" << peer.port << std::endl;
            return true;
        }
        std::cout << "[Client] Connection dropped or timed out during download. Retrying from " << current_received << " bytes for [" << peer.ip << "]:" << peer.port << "..." << std::endl;
    }
    // If all 5 attempts fail
    std::cerr << "[Client] Failed to download " << filename << " from [" << peer.ip << "]:" << peer.port << " after 5 attempts." << std::endl;
    return false;
}

int Client::cli_start(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <initial_manifest>" << std::endl;
        return 1;
    }

    std::string initial_manifest = argv[1];
    std::string target_file_gz = initial_manifest;
    size_t lastDotPos = target_file_gz.find_last_of('.');
    if(lastDotPos != std::string::npos) { target_file_gz.erase(lastDotPos); }
    target_file_gz += ".tar.gz";

    const int MAX_GLOBAL_RETRIES = 10; // You can increase the number of laps

    for (int global_attempt = 0; global_attempt < MAX_GLOBAL_RETRIES; ++global_attempt) {
        std::cout << "\n--- GLOBAL SCAN ATTEMPT " << global_attempt + 1 << " ---" << std::endl;
        
        std::deque<Peer> queue;
        std::set<Peer> visited;
        
        bool file_found_somewhere = false; // At least one peer confirmed the presence of the file
        bool had_network_errors = false;   // Were there any peers that simply didn't respond?

        auto initial_peers = parse_manifest(initial_manifest);
        for (const auto& p : initial_peers) queue.push_back(p);

        while (!queue.empty()) {
            Peer current_peer = queue.front();
            queue.pop_front();

            if (visited.count(current_peer)) continue;
            visited.insert(current_peer);

            int file_type = -1;
            std::string saved_path;
            
            if (download_file(current_peer, target_file_gz, file_type, saved_path)) {
                // If the function returns true, the file (or manifest) has been downloaded completely
                if (file_type == 0) {
                    std::cout << "[Client] SUCCESS! File fully downloaded." << std::endl;
                    return 0; 
                } else {
                    std::cout << "[Client] Found manifest, expanding peer list..." << std::endl;
                    auto new_peers = parse_manifest(saved_path);
                    for (const auto& p : new_peers) {
                        if (visited.find(p) == visited.end()) queue.push_back(p);
                    }
                    file_found_somewhere = true; // A manifesto is also a confirmation of life
                }
            } else {
                // If download_file returned false, let's see why
                if (file_type != -1) {
                    // The peer said "Yes, the file is there", but the connection was lost during the download
                    file_found_somewhere = true;
                    had_network_errors = true;
                } else {
                    had_network_errors = true; // if we have not received a clear answer, we consider this a reason for a retray
                }
            }
        }
        
        if (file_found_somewhere || had_network_errors) {
            // If the file flashed somewhere OR someone simply didn’t respond, try again
            std::cout << "[Client] Scan finished. File incomplete, but some peers were unreachable or confirmed file. "
                      << "Restarting global scan in 5 seconds..." << std::endl;
            sleep(5);
        } else {
            // If we polled absolutely everyone, and everyone answered “No file” (file_type -1)
            // and there was no timeout (had_network_errors = false)
            std::cerr << "[Client] All peers explicitly said they don't have the file. Giving up." << std::endl;
            return 1;
        }
    }

    std::cerr << "[Client] Reached maximum global retries. File incomplete." << std::endl;
    return 1;
}

std::string Client::removeUntilLastSlash(const std::string& input) {
    // Finding the last occurrence of a symbol '/'
    size_t pos = input.find_last_of('/');
    
    // If '/' найден, found, return the substring after it
    if (pos != std::string::npos) {
        return input.substr(pos + 1);
    }
    
    // If '/' not found, return the original string
    return input;
}


ssize_t Client::send_all(int sock, const void* buffer, size_t len) {
    size_t total = 0;
    const char* buf = (const char*)buffer;
    while (total < len) {
        ssize_t n = send(sock, buf + total, len - total, 0);
        if (n <= 0) { // Error or connection closed. Timeout will return -1
            return n;
        }
        total += n;
    }
    return total;
}

ssize_t Client::recv_all(int sock, void* buffer, size_t len) {
    size_t total = 0;
    char* buf = (char*)buffer;
    while (total < len) {
        ssize_t n = recv(sock, buf + total, len - total, 0);
        if (n <= 0) { // Error or connection closed. Timeout will return -1
            return n;
        }
        total += n;
    }
    return total;
}
#endif

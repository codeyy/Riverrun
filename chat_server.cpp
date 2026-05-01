#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <unordered_map>
#include <atomic>
#include <functional>

#pragma comment(lib, "ws2_32.lib")


using std::cout;
using std::string;



class Server {
private:
    SOCKET server_fd;

    std::unordered_map<SOCKET, std::string> socket_to_user;
    std::unordered_map<std::string, SOCKET> user_to_socket;

    std::mutex client_mutex;

    const std::string DELIM = "<MESSAGE_DELIMITER>"; // Unique delimiter to separate messages in the stream Notice: this should be updated in both client and server to match
    
    std::atomic<bool> isRunning{true};
    std::atomic<int> active_threads{0};
    
public:

    Server() {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);

        server_fd = socket(AF_INET, SOCK_STREAM, 0);

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(8080);
        server_addr.sin_addr.s_addr = INADDR_ANY;

        bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr));
        listen(server_fd, SOMAXCONN);
    }
    void start(){
        cout << "Running: "<< (isRunning == true ? "true" : "false") <<"\n";
        accept_loop();
        cleanup();
    };
    // 1. Add this to your class members

void cleanup() {
    isRunning = false;

    {
        std::lock_guard<std::mutex> lock(client_mutex);
        for (const auto& [_, Socket] : user_to_socket) {
            shutdown(Socket, SD_BOTH);
            closesocket(Socket);
        }
    }

    closesocket(server_fd);

    // 2. Wait for the COUNTER, not the map size
    while (active_threads > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    WSACleanup();
}

private:

    void accept_loop() {
        while (isRunning) {
            SOCKET client = accept(server_fd, nullptr, nullptr);
            
            if (client == INVALID_SOCKET) {
                break; // Exit loop if socket was closed by cleanup()
            }

            active_threads++; // Increment BEFORE detaching
            try {
                std::thread(&Server::handle_client, this, client).detach();
            } catch (...) {
                active_threads--; // Decrement if thread fails to start
                closesocket(client);
            }
        }
    }


    void handle_client(SOCKET client){
        std::string buffer_accum;
        while (isRunning) {
            char buffer[1024];
            int bytes = recv(client, buffer, sizeof(buffer), 0);
            if ((bytes <= 0) || (bytes == SOCKET_ERROR)) {
                remove_client(client);
                break;
            }

            buffer_accum.append(buffer, bytes);

            size_t pos;
            while ((pos = buffer_accum.find(DELIM)) != std::string::npos) {
                std::string msg = buffer_accum.substr(0, pos);
                buffer_accum.clear();
                
                message_processor(client, msg);
            }
        }
        active_threads--;
        return;
    }

    void message_processor(SOCKET client, const std::string& msg) {
        
        std::string user;
        {
            std::lock_guard<std::mutex> lock(client_mutex);
            user = socket_to_user[client];
        }
        
           // Example protocol:
          // LOGIN:username
         // BROADCAST:hello
        // DM:Harry:hello
        
        if (msg.rfind("LOGIN:", 0) == 0) {
            std::string username = msg.substr(6);
            std::lock_guard<std::mutex> lock(client_mutex);
            if (user_to_socket.find(username) != user_to_socket.end()) {
                send_to(client, "Username already taken, try another one.");
                return;
            }

            socket_to_user[client] = username;
            user_to_socket[username] = client;
            send_to(client, "Welcome " + username + "!");
            broadcast(username + " joined", username);

            return;
        }
        else if (msg.rfind("BROADCAST:", 0) == 0) {
            broadcast("[" + user + "] " + msg.substr(10), user);
            return;
        }
        else if (msg.rfind("DM:", 0) == 0) { 
            // Format: "DM:recipient:message"
            size_t first_colon = 2;
            size_t second_colon = msg.find(':', first_colon + 1);
                
            if (second_colon != std::string::npos) {
                std::string recipient = msg.substr(first_colon + 1, second_colon - first_colon - 1);
                std::string message = msg.substr(second_colon + 1);
            
                std::lock_guard<std::mutex> lock(client_mutex);
        
                auto it = user_to_socket.find(recipient);
                if (it != user_to_socket.end()) {
                    SOCKET recipient_socket = it->second;
                    send_to(recipient_socket, "[" + user + "] " + message);
                } else {
                    send_to(client, "User " + recipient + " not found");
                }
                } else {
                    send_to(client, "Invalid DM format. Use DM:recipient:message");
            }
        }
        return;
    }

    void broadcast(string msg, const string& sender_username = "") {
        msg+=DELIM;

        for (auto const& [_, Socket] : user_to_socket){

            if (socket_to_user[Socket] != sender_username) {
                send_to(Socket, msg);
            }
        }
    }

    void send_to(SOCKET recipient_socket, const std::string& msg) {
        std::string full = msg + DELIM;
        send(recipient_socket, full.c_str(), full.size(), 0);
        return;
    }
    
    void remove_client(SOCKET client) {
        std::lock_guard<std::mutex> lock(client_mutex);
        
        std::string user = socket_to_user[client];
        
        socket_to_user.erase(client);
        user_to_socket.erase(user);
        
        if (user != "") broadcast(user + " left");
        closesocket(client);

        return;
    }
};




void command_processor(Server& server);

int main() {
    Server server;
    
    std::cout << "Setup Complete.\n";
    std::thread(&command_processor, std::ref(server)).detach();

    server.start();

    return 0;
}



void command_processor(Server& server) {
    std::unordered_map<std::string, std::function<void()>> actions;

    actions["/terminate"] = [&server]() {
        std::cout << "Shutting down...\n";
        server.cleanup();
        exit(0);
    };
    // add more commands here as needed......


    while (true) {
        std::string command;
        std::getline(std::cin, command);

        actions[command]();
    }
}
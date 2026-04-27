// server.cpp
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <ranges>
#include <unordered_map>

#pragma comment(lib, "ws2_32.lib")

std::unordered_map<SOCKET, std::string> socket_to_user;
std::unordered_map<std::string, SOCKET> user_to_socket;

int socket_per_user(SOCKET client_socket);
int broadcast(std::string msg);

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_fd, (sockaddr*)&server_addr, sizeof(server_addr));    

    SOCKET client_socket;
    listen(server_fd, SOMAXCONN);

    std::cout << "Server listening on port 8080...\n";

    
    while(true){
        client_socket = accept(server_fd, nullptr, nullptr);
        std::thread th(socket_per_user, client_socket);
        th.detach();
    }

    closesocket(client_socket);
    closesocket(server_fd);
    WSACleanup();
}



int socket_per_user(SOCKET client_socket){
    int BytesReceived;
    char buffer[512];
    std::string username;
    while(true){
        int BytesReceived = recv(client_socket, buffer, sizeof(buffer), 0);
        if (BytesReceived > 0) {
            buffer[BytesReceived] = '\0'; // Manually add the terminator
        }else{
            return 0;
        }

        bool found;
        username = std::string(buffer, BytesReceived);
        for (auto const& [key, value] : user_to_socket){
            if (key == username){
                found = true;
                break;
            }
        }
        if (!found){break;}
    }
    broadcast("\n"+username+" Joined");

    user_to_socket[username] = client_socket;
    socket_to_user[client_socket] = username;

    send(client_socket, ("Wellcom "+username).c_str(), strlen(("Wellcom "+username).c_str()), 0);
    std::cout << "\n"+username+" Joined";// removable

    while(true){
        char buffer[1024];
        int BytesReceived = recv(client_socket, buffer, sizeof(buffer), 0);
        if (BytesReceived > 0) {
            buffer[BytesReceived] = '\0'; // Manually add the terminator
        }else{
            user_to_socket.erase(username);
            socket_to_user.erase(client_socket);
            broadcast("\n"+username+" Left");
            break;
        }

        std::string str = std::string(buffer, BytesReceived);
        broadcast("["+username+"]: "+str);
    }

    return 0;
}


int broadcast(std::string msg){
    for (auto const& [key, value] : user_to_socket){
        send(value, msg.c_str(), strlen(msg.c_str()), 0);
    }
    return 0;
}
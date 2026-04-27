// client.cpp
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <algorithm>
#include <string>
#include <thread>

#pragma comment(lib, "ws2_32.lib")

void receiver(SOCKET sock);

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    auto result = connect(sock, (sockaddr*)&server_addr, sizeof(server_addr));
    if (result == 0){
        std::cout << "Connection Established.\n\n";
    }else{
        std::cout << "Failed to Connect.\n\n" << "Terminating...\n";
        exit(0);
    }
    std::string name;
    std::cout << "Username: ";
    std::getline(std::cin, name);
    
    send(sock, name.c_str(), strlen(name.c_str()), 0);

    std::thread recv_th(receiver, sock);
    
    while(true){
        std::string message;
        std::getline(std::cin, message);
        
        send(sock, message.c_str(), strlen(message.c_str()), 0);
    }

    recv_th.join();
    closesocket(sock);
    WSACleanup();
}


void receiver(SOCKET sock){
    while(true){
        char buffer[1024];
        int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0'; // Manually add the terminator
        }else{
            break;
        }
        
        //std::cout << "\r\033[K"; //if the message bit is bothering in terminal

        std::cout << "\n\n   " << buffer << "\n";
        std::cout << "\n message: ";
    }
}
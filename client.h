#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <string>
#include <atomic>

#pragma comment(lib, "ws2_32.lib")





class Client {
private:
    SOCKET sock;
    std::string buffer_accum;
    std::atomic<bool> bRunning = true;
    std::atomic<bool> recv_loop_Finished = false;
    std::atomic<bool> send_loop_Finished = false;

    const std::string DELIM = "<MESSAGE_DELIMITER>";

    void process_message(const std::string& msg) {
        std::cout << "\r\033[K   " << msg << "\n> ";
    }
    
    void recv_loop() {
        char buffer[1024];
    
        while (bRunning) {
            int bytes = recv(sock, buffer, sizeof(buffer), 0);
            if ((bytes <= 0) || (bytes == SOCKET_ERROR)) {
                std::cout << "Disconnected from server.\n";
                break;
            }
    
            buffer_accum.append(buffer, bytes);
    
            size_t pos;
            while ((pos = buffer_accum.find(DELIM)) != std::string::npos) {
                std::string message = buffer_accum.substr(0, pos);
                buffer_accum.clear();
                if(message == "STATUS?"){
                    send(sock, ("ONLINE"+DELIM).c_str(), strlen(("ONLINE" + DELIM).c_str()), 0);
                    continue;
                }

                process_message(message);
            }
        }
        recv_loop_Finished = true;
        cleanup();
    }

    void send_loop() {
        std::string input;
        

        while (bRunning) {
            std::cout << "> ";
            std::getline(std::cin, input);

            if (input == "/exit") {
                std::cout << "Exiting...\n";
                break;
            }

            if (input.empty()) continue;

            std::string final_msg;

            if (input.rfind("/dm", 0) == 0) {
                // format: /dm user message
                size_t first_space = input.find(' ', 4);
                if (first_space != std::string::npos) {
                    std::string user = input.substr(4, first_space - 4);
                    std::string msg = input.substr(first_space + 1);
                    final_msg = "DM:" + user + ":" + msg;
                }
            } else {
                final_msg = "BROADCAST:" + input;
            }

            final_msg += DELIM;

            send(sock, final_msg.c_str(), final_msg.size(), 0);
        }
        send_loop_Finished = true;
        cleanup();
    }

    void get_username() {
        while(true){
        std::string username;
        std::cout << "Enter username: ";
        std::getline(std::cin, username);
        if (username.empty()) continue;
        if (username == "/exit") {
            std::cout << "Exiting...\n";
            shutdown(sock, SD_SEND);
            closesocket(sock);
            WSACleanup();
            exit(0);
        }

        send(sock, ("LOGIN:" + username + DELIM).c_str(), 
             strlen(("LOGIN:" + username + DELIM).c_str()), 0);
        
        char buffer[1024];
            int bytes = recv(sock, buffer, sizeof(buffer), 0);
            if ((bytes <= 0) || (bytes == SOCKET_ERROR)) {
                std::cout <<"\vServer busy, please try again later....\n";
                shutdown(sock, SD_SEND);
                closesocket(sock);
                WSACleanup();
                exit(1);
            }
    
            buffer_accum.append(buffer, bytes);
    
            size_t pos;
            while ((pos = buffer_accum.find(DELIM)) != std::string::npos) {
                std::string message = buffer_accum.substr(0, pos);
                buffer_accum.clear();
                if (message.rfind("Welcome", 0) == 0) {
                    std::cout << message << "\n";
                    return;
                } else {
                    std::cout << message << "\n";
                }
            }
        }       
    }

public:
    Client() {
        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);
        sock = socket(AF_INET, SOCK_STREAM, 0);
    }
    bool connect_to_server() {
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(8080);
        inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

        return connect(sock, (sockaddr*)&server_addr, sizeof(server_addr)) == 0;
    }


    void start() {
        get_username();
        std::thread recv_thread(&Client::recv_loop, this);
        send_loop();

        recv_thread.join();
    }
    void cleanup() {
        bRunning = false;

        // 1. Initiate graceful teardown: Stop sending
        shutdown(sock, SD_SEND);
        closesocket(sock);

        while (!recv_loop_Finished || !send_loop_Finished) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        WSACleanup();
    }
};
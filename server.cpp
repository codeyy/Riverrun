#include <iostream>
#include <thread>
#include "Server.h" // Include the header file for the Server class


using std::cout;
using std::string;


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
    actions["/usercount"] = [&server]() {
        std::cout << "Current user count: " << server.user_count() << std::endl;
    };
    // add more commands here as needed.....


    while (true) {
        std::string command;
        std::getline(std::cin, command);

        if(command.empty()) continue;
        if(actions.find(command) == actions.end()) {
            std::cout << "Unknown command:  " << command << std::endl;
        } else {
            actions[command]();
        }
    }
}
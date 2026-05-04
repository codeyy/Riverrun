#include <iostream>
#include <thread>
#include "client.h" // Include the header file for the Client class


int main() {
    Client client;

    if (!client.connect_to_server()) {
        std::cout << "Failed to connect.\n";
        return 1;
    }

    std::cout << "Connected!\n";

    client.start();
    client.cleanup();
}
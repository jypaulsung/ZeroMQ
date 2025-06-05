// Hello World server in C++
// Binds REP socket to tcp://*:5555
// Expects "Hello" from client, replies with "World"
//
// Reference: https://zeromq.org/languages/cplusplus/

#include <zmq.hpp>
#include <string>
#include <iostream>
#include <unistd.h>

int main() {
    // Prepare context and socket
    static const int kNumberofThreads = 2;
    zmq::context_t context(kNumberofThreads);
    zmq::socket_t socket(context, zmq::socket_type::rep);
    socket.bind("tcp://*:5555");

    while (true) {
        zmq::message_t request;

        // Wait for next request from client
        auto result = socket.recv(request, zmq::recv_flags::none);
        
        if (result) {
            // access the data in the request and convert it to a string
            std::string received_message(static_cast<char*>(request.data()), request.size()); 
            std::cout << "Received request: " << received_message << std::endl;
        }
        else {
            std::cerr << "Failed to receive any request." << std::endl;
        }

        // Do some 'work'
        sleep (1); 

        // Send reply back to client
        constexpr std::string_view kReplyString = "World";
        zmq::message_t reply(kReplyString.length());
        memcpy(reply.data(), kReplyString.data(), kReplyString.length());
        socket.send(reply, zmq::send_flags::none);
    }

    return 0;
}
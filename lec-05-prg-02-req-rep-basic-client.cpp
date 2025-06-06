// Hellow World client in C++
// Connects REQ socket to tcp://localhost:5555
// Sends "Hello" to server, expects "World" back
//
// Reference: https://zeromq.org/languages/cplusplus/

#include <zmq.hpp>
#include <string>
#include <iostream>

int main () {
    // Prepare context and socket
    zmq::context_t context (1);
    zmq::socket_t socket (context, zmq::socket_type::req);

    std::cout << "Connecting to hello world server..." << std::endl;
    socket.connect ("tcp://localhost:5555");

    // Do 10 requests, waiting each time for a response
    for (int request_nbr = 0; request_nbr != 10; request_nbr++) {
        zmq::message_t request (5);
        memcpy(request.data(), "Hello", 5);
        std::cout << "Sending request " << request_nbr << " ..." << std::endl;
        socket.send(request, zmq::send_flags::none);
        
        // Get the reply
        zmq::message_t reply;
        socket.recv(reply, zmq::recv_flags::none);
        std::cout << "Received reply " << request_nbr << " [ "
                  << std::string(static_cast<char*>(reply.data()), reply.size()) << " ]" << std::endl;
    }

    return 0;
}
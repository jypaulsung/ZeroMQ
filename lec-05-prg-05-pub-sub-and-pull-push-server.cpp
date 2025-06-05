// Reference: https://stackoverflow.com/questions/49289072/zmq-dealer-router-communication

#include <zmq.hpp>
#include <string>
#include <iostream>

int main() {
    // Prepare context and sockets
    zmq::context_t context(1);
    zmq::socket_t publisher(context, zmq::socket_type::pub);
    publisher.bind("tcp://*:5557");
    zmq::socket_t collector(context, zmq::socket_type::pull);
    collector.bind("tcp://*:5558");

    while (true) {
        zmq::message_t update;
        if (collector.recv(update, zmq::recv_flags::none)) {
            std::cout << "I: publishing update: " 
                      << std::string(static_cast<char*>(update.data()), update.size()) 
                      << std::endl;
            // Publish the update to all subscribers
            publisher.send(update, zmq::send_flags::none);
        }
    }


}
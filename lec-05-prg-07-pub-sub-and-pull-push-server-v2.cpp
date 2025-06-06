// Reference: https://stackoverflow.com/questions/49289072/zmq-dealer-router-communication

#include <zmq.hpp>
#include <string>
#include <iostream>

#include <unistd.h> // for sleep function

int main() {
    // Prepare context and socket
    zmq::context_t context(1);
    zmq::socket_t publisher(context, zmq::socket_type::pub);
    publisher.set(zmq::sockopt::linger, 0); // Set linger option to 0 to avoid blocking on close
    publisher.bind("tcp://*:5557");
    zmq::socket_t collector(context, zmq::socket_type::pull);
    collector.set(zmq::sockopt::linger, 0); // Set linger option to 0 to avoid blocking on close
    collector.bind("tcp://*:5558");

    while (true) {
        zmq::message_t message;
        // Receive a message from the collector
        if (collector.recv(message, zmq::recv_flags::none)) {
            // Print the received message
            std::string msg_str(static_cast<char*>(message.data()), message.size());
            std::cout << "server: publishing update => " << msg_str << std::endl;
            // Publish the update to all subscribers
            publisher.send(message, zmq::send_flags::none);
        }
        else {
            std::cerr << "server: no update message received" << std::endl;
            sleep(1); // Sleep for a second before retrying
        }
    }

    // Clean up
    publisher.close();
    collector.close();
    context.shutdown();
    context.close();

    return 0;
}
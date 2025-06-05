// Reference: https://stackoverflow.com/questions/49289072/zmq-dealer-router-communication

#include <zmq.hpp>
#include <string>
#include <iostream>
#include <random>
#include <ctime>

int main() {
    // Prepare context and subscriber
    zmq::context_t context(1);
    zmq::socket_t subscriber(context, zmq::socket_type::sub);
    subscriber.connect("tcp://localhost:5557");
    subscriber.set(zmq::sockopt::subscribe, ""); // Subscribe to all messages

    zmq::socket_t publisher(context, zmq::socket_type::push);
    publisher.connect("tcp://localhost:5558");

    // Seed the random number generator with the current time
    std::mt19937 rng(static_cast<unsigned int>(time(nullptr)));

    // Create a uniform distribution [1, 100]
    std::uniform_int_distribution<int> dist(1, 100);

    // Generate a random number
    int rand_num = dist(rng);

    // Initialize poll set
    zmq::pollitem_t items[] = {
        { subscriber, 0, ZMQ_POLLIN, 0 }
    };
    
    while (true) {
        // Poll with 100ms timeout
        zmq::poll(items, 1, 100);

        if (items[0].revents & ZMQ_POLLIN) {
            zmq::message_t msg_received;
            subscriber.recv(msg_received);
            std::string msg_str(static_cast<char*>(msg_received.data()), msg_received.size());
            std::cout << "I: received message " << msg_str << std::endl;
        } else {
            int rand_val = dist(rng);
            if (rand_val < 10) {
                std::string msg_str = std::to_string(rand_val);
                zmq::message_t msg_sent(msg_str.begin(), msg_str.end());
                publisher.send(msg_sent, zmq::send_flags::none);
                std::cout << "I: sending message " << rand_val << std::endl;
            }
        }
    }

    return 0;

}
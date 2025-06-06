// Reference: https://stackoverflow.com/questions/49289072/zmq-dealer-router-communication

#include <zmq.hpp>
#include <string>
#include <iostream>

#include <unistd.h> // for sleep function
#include <random>
#include <ctime>

int main(int argc, char *argv[]) {
    // Prepare context and socket
    zmq::context_t context(1);
    zmq::socket_t subscriber(context, zmq::socket_type::sub);
    subscriber.set(zmq::sockopt::linger, 0); // Set linger option to 0 to avoid blocking on close
    subscriber.connect("tcp://localhost:5557");
    // Subscribe to all messages
    subscriber.set(zmq::sockopt::subscribe, "");
    zmq::socket_t publisher(context, zmq::socket_type::push);
    publisher.set(zmq::sockopt::linger, 0); // Set linger option to 0 to avoid blocking on close
    publisher.connect("tcp://localhost:5558");

    const char *clientID = (argc > 1) ? argv[1] : "0";

    // Seed the random number generator with the current time
    std::mt19937 rng(static_cast<unsigned int>(time(nullptr)));
    // Create a uniform distribution [1, 100]
    std::uniform_int_distribution<int> dist(1, 100);

    // Initialize poll set
    zmq::pollitem_t items[] = {
        { subscriber, 0, ZMQ_POLLIN, 0 }
    };

    while (true) {
        // Poll with 100ms timeout
        zmq::poll(items, 1, std::chrono::milliseconds(100));

        if (items[0].revents & ZMQ_POLLIN) {
            zmq::message_t msg_received;
            subscriber.recv(msg_received);
            std::string msg_str(static_cast<char*>(msg_received.data()), msg_received.size());
            std::cout << clientID << ": receive status => " << msg_str << std::endl;
        }
        else {
            int rand = dist(rng);
            if (rand < 10) {
                sleep(1);
                std::string msg1 = "(" + std::string(clientID) + ":ON)";
                zmq::message_t msg_sent1(msg1.begin(), msg1.end());
                publisher.send(msg_sent1, zmq::send_flags::none);
                std::cout << clientID << ": send status - activated" << std::endl;
            }
            else if (rand > 90) {
                sleep(1);
                std::string msg2 = "(" + std::string(clientID) + ":OFF)";
                zmq::message_t msg_sent2(msg2.begin(), msg2.end());
                publisher.send(msg_sent2, zmq::send_flags::none);
                std::cout << clientID << ": send status - deactivated" << std::endl;
            }
        }
    }

    // Clean up
    subscriber.close();
    publisher.close();
    context.shutdown();
    context.close();

    return 0;

}
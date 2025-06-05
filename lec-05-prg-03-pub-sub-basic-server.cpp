// Weather update server
// Binds PUB socket to tcp://*:5556
// Publishes random weather updates
//
// Reference: https://zguide.zeromq.org/docs/chapter1/#Getting-the-Message-Out

#include <zmq.hpp>
#include <string>
#include <iostream>
#include <time.h>

#define within(num) (int) ((float)num * random () / (RAND_MAX + 1.0))

int main() {
    std::cout << "Publishing updates at weather server..." << std::endl;

    // Prepare context and publisher
    zmq::context_t context (1);
    zmq::socket_t publisher (context, zmq::socket_type::pub);
    publisher.bind("tcp://*:5556");

    // Initialize random number generator
    while(true) {
        int zipcode, temperature, relhumidity;

        // Get arbitrary weather data
        zipcode = within(100000);
        temperature = within(215) - 80;
        relhumidity = within(50) + 10;

        // Send message to all subscribers
        zmq::message_t message(20);
        snprintf((char *)message.data(), 20, "%05d %d %d", zipcode, temperature, relhumidity);
        publisher.send(message, zmq::send_flags::none);
    }

    return 0;

}

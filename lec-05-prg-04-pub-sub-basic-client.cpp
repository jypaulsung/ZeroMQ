// Weather update client
// Connects SUB socket to tcp://localhost:5556
// Collects weather updates and finds avg temp in zipcode
//
// Reference: https://zguide.zeromq.org/docs/chapter1/#Getting-the-Message-Out

#include <zmq.hpp>
#include <iostream>
#include <sstream>
#include <cmath>

// Function to dynamically adjust how the average temperature is printed
std::string format_average_temperature(double value) {
       std::ostringstream out;

       if (std::floor(value) == value) {
              // Integer: print without decimal places
              out << static_cast<int>(value);
       }
       else if (std::floor(value * 10) == value * 10) {
              // One decimal float: print with one decimal place
              out << std::fixed << std::setprecision(1) << value;
       }
       else {
              out << std::fixed << std::setprecision(2) << value;
       }

       return out.str();
}

int main(int argc, char *argv[]) {
       zmq::context_t context(1);

       // Socket to talk to server
       zmq::socket_t subscriber(context, zmq::socket_type::sub);
       
       std::cout << "Collecting updates from weather server..." << std::endl;
       subscriber.set(zmq::sockopt::linger, 0); // Set linger option to 0 to avoid blocking on close
       subscriber.connect("tcp://localhost:5556");

       // Subscribe to zipcode, default is NYC, 10001
       const char *filter = (argc > 1) ? argv[1] : "10001";
       subscriber.set(zmq::sockopt::subscribe, filter);

       // Process 20 updates
       int update_nbr;
       long total_temp = 0;
       for (update_nbr = 0; update_nbr < 20; update_nbr++) {
              zmq::message_t update;
              int zipcode, temperature, relhumidity;

              subscriber.recv(update, zmq::recv_flags::none);

              std::istringstream iss(static_cast<char*>(update.data()));
              iss >> zipcode >> temperature >> relhumidity;

              std::cout << "Receive temperature for zipcode '" << filter << "' was "
                        << temperature << " F" << std::endl;

              total_temp += temperature;
       }

       double average_temp = static_cast<double>(total_temp) / update_nbr;

       std::cout << "Average temperature for zipcode '" << filter << "' was "
                 << format_average_temperature(average_temp) << " F" << std::endl;


       // Clean up
       subscriber.close();
       context.shutdown();
       context.close();

       return 0;
}

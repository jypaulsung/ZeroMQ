// Asynchronous Client/Server Pattern
// Reference: https://zguide.zeromq.org/docs/chapter3/#The-Asynchronous-Client-Server-Pattern

#include <string>
#include <iostream>
#include <thread>
#include <chrono>

#include <zmq.hpp>

class ClientTask {
    public:
        ClientTask(const std::string& client_id)
            : client_id_(client_id), context_(1), socket_(context_, zmq::socket_type::dealer) {}
        
        void run() {
            socket_.set(zmq::sockopt::routing_id, client_id_);
            socket_.set(zmq::sockopt::linger, 0); // Set linger option to 0 to avoid blocking on close
            socket_.connect("tcp://localhost:5570");

            std::cout << "Client " << client_id_ << " started" << std::endl;

            zmq::pollitem_t items[] = {
                { socket_, 0, ZMQ_POLLIN, 0 }
            };

            int reqs = 0;

            while (true) {
                reqs++;
                std::string request = "request #" + std::to_string(reqs);
                std::cout << "Req #" << reqs << " sent.." << std::endl;
                socket_.send(zmq::buffer(request), zmq::send_flags::none);

                std::this_thread::sleep_for(std::chrono::seconds(1));

                zmq::poll(items, 1, std::chrono::milliseconds(1000));
                if (items[0].revents & ZMQ_POLLIN) {
                    zmq::message_t msg_received;
                    socket_.recv(msg_received, zmq::recv_flags::none);
                    std::string msg_str(static_cast<char*>(msg_received.data()), msg_received.size());
                    std::cout << client_id_ << " received: " << msg_str << std::endl;
                }
            }

            // Clean up
            socket_.close();
            context_.shutdown();
            context_.close();
        }

    private:
        std::string client_id_;
        zmq::context_t context_;
        zmq::socket_t socket_;
};

int main(int argc, char* argv[]) {
    if (argc >1) {
        std::string client_id = argv[1];
        ClientTask client(client_id);
        
        std::thread client_thread(&ClientTask::run, &client);

        client_thread.detach(); // Detach the thread to run independently

        // Prevent the main thread from exiting immediately
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    else {
        std::cerr << "Usage: " << argv[0] << " <client_id>" << std::endl;
        return 1;
    }

    return 0;
}
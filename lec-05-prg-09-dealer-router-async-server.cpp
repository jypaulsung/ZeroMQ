// Asynchronous Client/Server Pattern
// Reference: https://zguide.zeromq.org/docs/chapter3/#The-Asynchronous-Client-Server-Pattern

#include <vector>
#include <thread>
#include <memory>
#include <functional>

#include <zmq.hpp>
#include "zhelpers.hpp"

class server_worker {
    public:
        server_worker(zmq::context_t &context, int worker_id)
            : context_(context), worker_id_(worker_id), worker_(context_, zmq::socket_type::dealer) 
        {}
        
        void work() {
            worker_.set(zmq::sockopt::linger, 0); // Set linger option to 0 to avoid blocking on close
            worker_.connect("inproc://backend");
            std::cout << "Worker#" << worker_id_ << " started" << std::endl;
            try {
                while (true) {
                    zmq::message_t identity;
                    zmq::message_t msg;
                    zmq::message_t copied_id;
                    zmq::message_t copied_msg;
                    worker_.recv(&identity);
                    worker_.recv(&msg);

                    std::string identity_str(static_cast<char*>(identity.data()), identity.size());
                    std::string msg_str(static_cast<char*>(msg.data()), msg.size());

                    std::cout << "Worker#" << worker_id_ << " received " << msg_str << " from " << identity_str << std::endl;
                    
                    worker_.send(identity, zmq::send_flags::sndmore);
                    worker_.send(msg, zmq::send_flags::none);
                }
            }
            catch (std::exception &e) {
                std::cerr << "Worker#" << worker_id_ << " exception: " << e.what() << std::endl;
            }
        }
    
    private:
        zmq::context_t &context_;
        zmq::socket_t worker_;
        const int worker_id_;
};

class server_task {
    public:
        server_task(int num_server)
            : context_(1), frontend_(context_, zmq::socket_type::router), backend_(context_, zmq::socket_type::dealer),
              num_server_(num_server)
        {}
    
    void run() {
        frontend_.set(zmq::sockopt::linger, 0); // Set linger option to 0 to avoid blocking on close
        frontend_.bind("tcp://*:5570");
        backend_.set(zmq::sockopt::linger, 0);
        backend_.bind("inproc://backend");

        // std::vector<server_worker*> worker;
        // std::vector<std::thread*> worker_thread;
        // for (int i = 0; i < num_server; i++) {
        //     worker.push_back(new server_worker(context_, i));
        //     worker_thread.push_back(new std::thread(std::bind(&server_worker::work, worker[i])));
        //     worker_thread[i]->detach();
        // }

        std::vector<std::thread> worker_threads;
        for (int i = 0; i < num_server_; i++) {
            worker_threads.emplace_back([this, i]() {
                server_worker worker(context_, i);
                worker.work();
            });
        }

        try {
            zmq::proxy(static_cast<void*>(frontend_),
                       static_cast<void*>(backend_),
                       nullptr);
        }
        catch (std::exception &e) {}

        frontend_.close();
        backend_.close();
        context_.shutdown();
        context_.close();
    }

    private:
        zmq::context_t context_;
        zmq::socket_t frontend_;
        zmq::socket_t backend_;
        const int num_server_;
};

int main(int argc, char *argv[]) {
    if (argc > 1) {
        server_task *server = new server_task(std::stoi(argv[1]));
        server->run();
        delete server;
    }

    return 0;
}
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <sstream>
#include <chrono>
#include <random>
#include <zmq.hpp>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <csignal>
#include <future>

// Determine the local IP address of the machine
std::string get_local_ip() {
    try {
        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            throw std::runtime_error("Failed to create socket");
        }
        const char* kGoogleDNSip = "8.8.8.8";
        int dns_port = 80;

        sockaddr_in serv;
        memset(&serv, 0, sizeof(serv));
        serv.sin_family = AF_INET;
        serv.sin_addr.s_addr = inet_addr(kGoogleDNSip);
        serv.sin_port = htons(dns_port);

        if (connect(sockfd, (const sockaddr*)&serv, sizeof(serv)) < 0) {
            close(sockfd);
            throw std::runtime_error("Failed to connect to DNS server");
        }
        else {
            sockaddr_in name;
            socklen_t namelen = sizeof(name);
            if (getsockname(sockfd, (sockaddr*)&name, &namelen) < 0) {
                close(sockfd);
                throw std::runtime_error("Failed to get socket name");
            }
            else {
                char buffer[INET_ADDRSTRLEN];
                const char* p = inet_ntop(AF_INET, &name.sin_addr, buffer, INET_ADDRSTRLEN);
                if (p == nullptr) {
                    close(sockfd);
                    throw std::runtime_error("Failed to convert IP address to string");
                }
                else {
                    close(sockfd);
                    return std::string(buffer);
                }
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error getting local IP address: " << e.what() << std::endl;
        return "";
    }
}

// Search for P2P name server in the local network
std::string search_nameserver(zmq::context_t& context, std::string& ip_mask, const std::string& local_ip_addr, int port_nameserver) {
    zmq::socket_t req(context, ZMQ_SUB);

    for (int last = 1; last < 255; ++last) {
        std::ostringstream oss;
        oss << "tcp://" << ip_mask << "." << last << ":" << port_nameserver;
        std::string target_ip_addr = oss.str();
        req.set(zmq::sockopt::linger, 0); // Set linger option to 0 to avoid blocking on close
        req.connect(target_ip_addr);
        req.set(zmq::sockopt::rcvtimeo, 2000); // Set receive timeout for 2 seconds
        req.set(zmq::sockopt::subscribe, "NAMESERVER"); // Subscribe to NAMESERVER messages
    }

    try {
        zmq::message_t message;
        if (req.recv(message, zmq::recv_flags::none)) {
            std::string res(static_cast<char*>(message.data()), message.size());
            auto pos = res.find(':');
            if (res.substr(0, pos) == "NAMESERVER") {
                req.close();
                return res.substr(pos + 1);
            }
        }
    } catch (...) {}

    req.close();
    
    return "";
}

bool global_flag_shutdown = false;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        global_flag_shutdown = true;
    }
}

// Generate periodic (1 second) beacon message
void beacon_nameserver(zmq::context_t& context, const std::string& local_ip_addr, int port_nameserver, std::promise<void>& beacon_ready) {
    zmq::socket_t publisher(context, ZMQ_PUB);
    
    std::string bind_address = "tcp://" + local_ip_addr + ":" + std::to_string(port_nameserver);
    publisher.set(zmq::sockopt::linger, 0); // Set linger option to 0 to avoid blocking on close
    publisher.bind(bind_address);
    std::cout << "local p2p name server bind to " << bind_address << "." << std::endl;
    beacon_ready.set_value(); // Indicate that the beacon server is ready

    signal(SIGINT, signal_handler);  // Handle Ctrl+C

    while (!global_flag_shutdown) {
        try {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::string msg = "NAMESERVER:" + local_ip_addr;
            zmq::message_t message(msg.begin(), msg.end());
            publisher.send(message, zmq::send_flags::none);
        } catch (const zmq::error_t& e) {
            std::cerr << "ZMQ error: " << e.what() << std::endl;
            global_flag_shutdown = true; // Set shutdown flag on error

            publisher.close();

            break;
        } catch (const std::exception& e) {
            std::cerr << "General error: " << e.what() << std::endl;
            global_flag_shutdown = true; // Set shutdown flag on error

            publisher.close();

            break;
        }
    }

    publisher.close();

}

// User subscription manager {ip address and user id}
void user_manager_nameserver(zmq::context_t& context, const std::string& local_ip_addr, int port_subscribe, std::promise<void>& user_manager_ready) {
    zmq::socket_t socket(context, ZMQ_REP);
    std::ostringstream addr;
    addr << "tcp://" << local_ip_addr << ":" << port_subscribe;
    socket.set(zmq::sockopt::linger, 0); // Set linger option to 0 to avoid blocking on close
    socket.bind(addr.str());
    socket.set(zmq::sockopt::rcvtimeo, 2000); // Set receive timeout for 2 seconds

    std::vector<std::string> user_db;
    std::cout << "local p2p db server activated at " << addr.str() << std::endl;
    user_manager_ready.set_value(); // Indicate that the user manager server is ready

    signal(SIGINT, signal_handler);  // Handle Ctrl+C

    while (!global_flag_shutdown) {
        try {
            zmq::message_t request;
            if (socket.recv(request, zmq::recv_flags::none)) {
                std::string user_req(static_cast<char*>(request.data()), request.size());
                auto pos = user_req.find(':');
                if (pos == std::string::npos) {
                    std::cerr << "Invalid user request format: " << user_req << std::endl;
                    global_flag_shutdown = true; // Set shutdown flag on error

                    socket.close();

                    break;
                } else {
                    std::string user_ip = user_req.substr(0, pos);
                    std::string user_id = user_req.substr(pos + 1);
                    user_db.push_back(user_id);
                    user_db.push_back(user_ip);
                    std::cout << "user registration '" << user_id << "' from '" << user_ip << "'." << std::endl;
                    std::string ok = "ok";
                    socket.send(zmq::buffer(ok), zmq::send_flags::none);
                }
            }
            else if (errno == EAGAIN) {
                continue; // Timeout occurred, just check the shutdown flag
            }
            else {
                std::cerr << "Failed to receive user request." << std::endl;
                global_flag_shutdown = true; // Set shutdown flag on error

                socket.close();

                break;
            }
        } catch (const zmq::error_t& e) {
            std::cerr << "ZMQ error: " << e.what() << std::endl;
            global_flag_shutdown = true; // Set shutdown flag on error

            socket.close();

            break;
        } catch (const std::exception& e) {
            std::cerr << "General error: " << e.what() << std::endl;
            global_flag_shutdown = true; // Set shutdown flag on error

            socket.close();

            break;
        }
    }

    socket.close();

}

// Relay message between p2p users
void relay_server_nameserver(zmq::context_t& context, const std::string& local_ip_addr, int port_chat_publisher, int port_chat_collector, std::promise<void>& relay_server_ready) {
    zmq::socket_t publisher(context, ZMQ_PUB);
    zmq::socket_t collector(context, ZMQ_PULL);

    std::ostringstream pub_addr, coll_addr;
    pub_addr << "tcp://" << local_ip_addr << ":" << port_chat_publisher;
    coll_addr << "tcp://" << local_ip_addr << ":" << port_chat_collector;

    publisher.set(zmq::sockopt::linger, 0); // Set linger option to 0 to avoid blocking on close
    collector.set(zmq::sockopt::linger, 0); 
    publisher.bind(pub_addr.str());
    collector.bind(coll_addr.str());
    collector.set(zmq::sockopt::rcvtimeo, 2000); // Set receive timeout for 2 seconds

    std::cout << "local p2p relay server activated at " << pub_addr.str() << " & " << port_chat_collector << std::endl;
    relay_server_ready.set_value(); // Indicate that the relay server is ready

    signal(SIGINT, signal_handler);  // Handle Ctrl+C

    while (!global_flag_shutdown) {
        try {
            zmq::message_t message;
            if (collector.recv(message, zmq::recv_flags::none)) {
                std::string msg(static_cast<char*>(message.data()), message.size());
                std::cout << "p2p-relay:<==> " << msg << std::endl;
                std::string relay_msg = "RELAY:" + msg;
                publisher.send(zmq::buffer(relay_msg), zmq::send_flags::none);
            }
            else if (errno == EAGAIN) {
                continue; // Timeout occurred, just check the shutdown flag
            }
            else {
                std::cerr << "ZMQ error: " << zmq_strerror(errno) << std::endl;
                global_flag_shutdown = true; // Set shutdown flag on error

                publisher.close();
                collector.close();

                break;
            }
        } catch (const zmq::error_t& e) {
            std::cerr << "ZMQ error: " << e.what() << std::endl;
            global_flag_shutdown = true; // Set shutdown flag on error

            publisher.close();
            collector.close();

            break;
        } catch (const std::exception& e) {
            std::cerr << "General error: " << e.what() << std::endl;
            global_flag_shutdown = true; // Set shutdown flag on error

            publisher.close();
            collector.close();

            break;
        }
    }

    publisher.close();
    collector.close();

}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "usage is './p2p_chat _user-name_'" << std::endl;
        return 0;
    }
    else {
        std::cout << "starting p2p chatting program." << std::endl;
    }

    int port_nameserver = 9001;
    int port_chat_publisher = 9002;
    int port_chat_collector = 9003;
    int port_subscribe = 9004;

    std::string user_name = argv[1];
    std::string ip_addr = get_local_ip();
    std::string ip_mask = ip_addr.substr(0, ip_addr.find_last_of('.'));

    std::cout << "searching for p2p server." << std::endl;

    zmq::context_t server_context(1);

    std::string name_server_ip_addr = search_nameserver(std::ref(server_context), ip_mask, ip_addr, port_nameserver);
    std::string ip_addr_p2p_server;
    

    std::thread beacon_thread, user_manager_thread, relay_server_thread;
    bool is_server = false;
    std::promise<void> beacon_promise, user_manager_promise, relay_server_promise;
    std::future<void> beacon_future = beacon_promise.get_future();
    std::future<void> user_manager_future = user_manager_promise.get_future();
    std::future<void> relay_server_future = relay_server_promise.get_future();
    if (name_server_ip_addr.empty()) {
        is_server = true; // If no server found, this instance will act as a server
        ip_addr_p2p_server = ip_addr;
        std::cout << "p2p server is not found, and p2p server mode is activated." << std::endl;

        beacon_thread = std::thread(beacon_nameserver, std::ref(server_context), ip_addr, port_nameserver, std::ref(beacon_promise));
        beacon_future.get(); // Wait for the beacon server to be ready
        std::cout << "p2p beacon server is activated." << std::endl;

        user_manager_thread = std::thread(user_manager_nameserver, std::ref(server_context), ip_addr, port_subscribe, std::ref(user_manager_promise));
        user_manager_future.get(); // Wait for the user manager server to be ready
        std::cout << "p2p subscriber database server is activated." << std::endl;

        relay_server_thread = std::thread(relay_server_nameserver, std::ref(server_context), ip_addr, port_chat_publisher, port_chat_collector, std::ref(relay_server_promise));
        relay_server_future.get(); // Wait for the relay server to be ready
        std::cout << "p2p message relay server is activated." << std::endl;
    } else {
        ip_addr_p2p_server = name_server_ip_addr;
        std::cout << "p2p server found at " << ip_addr_p2p_server << ", and p2p client mode is activated." << std::endl;
    }

    std::cout << "starting user registration procedure." << std::endl;

    zmq::context_t db_client_context(1);
    zmq::socket_t db_client_socket(db_client_context, ZMQ_REQ);
    std::ostringstream sub_addr;
    sub_addr << "tcp://" << ip_addr_p2p_server << ":" << port_subscribe;
    db_client_socket.set(zmq::sockopt::linger, 0); // Set linger option to 0 to avoid blocking on close
    db_client_socket.connect(sub_addr.str());

    std::string reg_msg = ip_addr + ":" + user_name;
    db_client_socket.send(zmq::buffer(reg_msg), zmq::send_flags::none);
    zmq::message_t ack;
    (void)db_client_socket.recv(ack, zmq::recv_flags::none);
    std::string ack_str(static_cast<char*>(ack.data()), ack.size());
    if (ack_str == "ok") {
        std::cout << "user registration to p2p server completed." << std::endl;
    } else {
        std::cout << "user registration to p2p server failed." << std::endl;
    }

    std::cout << "starting message transfer procedure." << std::endl;
    zmq::context_t relay_client(1);
    zmq::socket_t p2p_rx(relay_client, ZMQ_SUB);
    zmq::socket_t p2p_tx(relay_client, ZMQ_PUSH);

    p2p_rx.set(zmq::sockopt::subscribe, "RELAY"); // Subscribe to RELAY messages
    p2p_rx.set(zmq::sockopt::linger, 0); // Set linger option to 0 to avoid blocking on close
    p2p_tx.set(zmq::sockopt::linger, 0);
    p2p_rx.connect("tcp://" + ip_addr_p2p_server + ":" + std::to_string(port_chat_publisher));
    p2p_tx.connect("tcp://" + ip_addr_p2p_server + ":" + std::to_string(port_chat_collector));

    std::cout << "starting autonomous message transmit and receive scenario." << std::endl;

    std::mt19937 gen(std::random_device{}());
    std::uniform_int_distribution<> dist(1, 100);

    zmq::pollitem_t items[] = {
        {static_cast<void*>(p2p_rx), 0, ZMQ_POLLIN, 0}
    };

    signal(SIGINT, signal_handler);  // Handle Ctrl+C

    while (!global_flag_shutdown) {
        // zmq::poll(&items[0], 1, 100);
        zmq::poll(items, 1, std::chrono::milliseconds(100));

        if (items[0].revents & ZMQ_POLLIN) {
            zmq::message_t msg;
            (void)p2p_rx.recv(msg, zmq::recv_flags::none);
            std::string message(static_cast<char*>(msg.data()), msg.size());
            size_t first = message.find(":");
            size_t second = message.find(":", first + 1);
            std::cout << "p2p-recv::<<== " << message.substr(first + 1, second - first - 1)
                      << ":" << message.substr(second + 1) << std::endl;
        } else {
            int rand = dist(gen);
            if (rand < 10 || rand > 90) {
                std::this_thread::sleep_for(std::chrono::seconds(3));
                std::string msg = "(" + user_name + "," + ip_addr + ((rand < 10) ? ":ON)" : ":OFF)");
                p2p_tx.send(zmq::buffer(msg), zmq::send_flags::none);
                std::cout << "p2p-send::==>> " << msg << std::endl;
            }
        }
    }

    db_client_socket.close();
    db_client_context.shutdown();
    db_client_context.close();

    p2p_rx.close();
    p2p_tx.close();
    relay_client.shutdown();
    relay_client.close();

    if (is_server) {
        beacon_thread.join();
        user_manager_thread.join();
        relay_server_thread.join();
    }

    server_context.shutdown();
    server_context.close();

    return 0;
}
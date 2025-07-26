#include <algorithm>
#include <array>
#include <iostream>
#include <string>
#include <thread>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class chat_client {
public:
    chat_client(boost::asio::io_context& io_context,
        const tcp::resolver::results_type& endpoints) : 
        io_context_{io_context}, 
        socket_{io_context} {
            do_connect(endpoints);
        };

    void write(const std::array<char, 1024>& msg) {
        boost::asio::post(io_context_,
            [this, msg] () {
                // TODO: Handle multiple messages written at once with queue
                write_msg_ = msg;
                do_write();
            }
        );
    }
    
private:
    void do_connect(const tcp::resolver::results_type& endpoints) {
        boost::asio::async_connect(socket_, endpoints,
            [this] (boost::system::error_code ec, tcp::endpoint) {
                if(!ec) {
                    
                    std::cout << "Local Endpoint: " << socket_.local_endpoint() << std::endl;
                    std::cout << "Remote Endpoint: " << socket_.remote_endpoint() << std::endl;
                    do_read();
                }
            }
        );
    }
    
    void do_read() {
        socket_.async_read_some(
            boost::asio::buffer(read_msg_),
            [this] (boost::system::error_code ec, std::size_t len) {
                if(!ec) {
                    std::cout << read_msg_.data() << std::endl;
                    do_read();
                } else {
                    socket_.close();
                }
            }
        );
    }

    void do_write() {
        boost::asio::async_write(socket_,
            boost::asio::buffer(write_msg_),
            [this] (boost::system::error_code ec, std::size_t) {
                if(!ec) {
                } else {
                    socket_.close();
                }
            }
        );
    }

    boost::asio::io_context& io_context_;
    tcp::socket socket_;
    std::array<char, 1024> read_msg_;
    std::array<char, 1024> write_msg_;
};

int main(int argc, char* argv[]) {
    try {
        if(argc != 3) {
            std::cerr << "Usage: client <host> <port>" << std::endl;
            return 1;
        }

        boost::asio::io_context io_context;
        tcp::resolver resolver{io_context};
        
        // TODO: Sanitize user input
        tcp::resolver::results_type endpoints = resolver.resolve(argv[1], argv[2]);

        chat_client client{io_context, endpoints};

        std::thread reader_thread([&io_context](){ io_context.run(); });

        while(true) {
            std::string line;
            std::array<char, 1024> msg{};
            std::getline(std::cin, line);

            // TODO: More robust exit
            if(line == "exit")
                break;
            
            std::size_t copy_len = std::min(line.size(), msg.size());
            std::copy_n(line.begin(), copy_len, msg.begin());
            client.write(msg);
        }
    } catch(std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

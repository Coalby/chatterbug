#include <array>
#include <iostream>
#include <memory>
#include <set>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class chat_participant {
public:
    virtual ~chat_participant() {}
    virtual void deliver(const std::array<char, 1024>& msg) = 0;
};

typedef std::shared_ptr<chat_participant> chat_participant_ptr;

class chat_room {
public:
    void join(chat_participant_ptr participant) {
        participants_.insert(participant);
    }

    void leave(chat_participant_ptr participant) {
        participants_.erase(participant); 
    }

    void deliver(const chat_participant_ptr calling_participant, const std::array<char, 1024>& msg) {
        for(auto& participant : participants_) {
            if(participant.get() != calling_participant.get())
                participant->deliver(msg);
        }
    }

private:
    std::set<chat_participant_ptr> participants_;
};

class chat_session :
    public chat_participant,
    public std::enable_shared_from_this<chat_session>
{
public:
    chat_session(tcp::socket socket, chat_room& room) :
        socket_(std::move(socket)),
        room_(room) {}

    void deliver(const std::array<char, 1024>& msg) {
        write_buf_ = msg;

        // TODO: Add queue for messages sent if in progress
        do_write();
    }

    void start() {
        room_.join(shared_from_this());
        do_read();
    }

private:
    void do_read() {
        auto self(shared_from_this());

        socket_.async_read_some(
            boost::asio::buffer(read_buf_),
            [this, self] (boost::system::error_code ec, std::size_t length) {
                if(!ec) {
                    room_.deliver(shared_from_this(), read_buf_); 
                    do_read();
                } else {
                    room_.leave(shared_from_this());
                }
            }
        );
    }

    void do_write() {
        auto self(shared_from_this());

        boost::asio::async_write(socket_,
                boost::asio::buffer(write_buf_, 1024),
                [this, self] (boost::system::error_code ec, std::size_t /*length*/) {
                    if(!ec) {

                    } else {
                        room_.leave(shared_from_this());
                    }
                }
        );
    }
    
    chat_room& room_;
    std::array<char, 1024> read_buf_;
    std::array<char, 1024> write_buf_;
    tcp::socket socket_;
};

class chat_server {
public:
    chat_server(boost::asio::io_context& io_context,
                const tcp::endpoint& endpoint) :
        acceptor_(io_context, endpoint) {
            do_accept();
        };

private:
    void do_accept() {
        acceptor_.async_accept(
            [this] (boost::system::error_code ec, tcp::socket socket) {
                if(!ec) {
                    std::make_shared<chat_session>(std::move(socket), room_)->start();
                }

                do_accept();
            });
    }

    tcp::acceptor acceptor_;
    chat_room room_;
};

int main(int argc, char* argv[]) {
    try {
        if(argc < 2) {
            std::cerr << "Usage: chat_server <port>\n";
            return 1;
        }

        boost::asio::io_context io_context_;
        tcp::endpoint endpoint(tcp::v4(), std::atoi(argv[1]));

        chat_server server_(io_context_, endpoint);

        io_context_.run();
    } catch(std::exception& e) {
        std::cout << "Exception: " << e.what() << std::endl;
    }

    return 0;
}

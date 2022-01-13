#ifndef _JRUDP_H
#define _JRUDP_H

#include "recver.hpp"
#include "sender.hpp"
#include <cstring>
#include <stdexcept>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>

namespace jrReliableUDP {
    class Socket {
    private:
        enum ConnectionState {CLOSED, SYN_SENT, LISTEN, SYN_RCVD, ESTABLISHED,
                              FIN_WAIT, TIME_WAIT, CLOSE_WAIT, LAST_ACK};

    private:
        int sockfd;
        uint port;
        bool is_passive_end;
        sockaddr_in addr;
        RTO rto;    // Timeout retransmit parameters
        ConnectionState cur_state;
        Sender sender;
        Recver recver;

    private:
        Socket(int fd, bool is_passive_end, sockaddr_in addr, RTO rto,
               const Sender& s, const Recver& r, ConnectionState cs);
        [[noreturn]] void disconnect_exception(std::string msg);
        void set_local_address(uint16_t port);
        void set_peer_address(std::string ip, uint16_t port);

    public:
        Socket();
        Socket(const Socket&) = default;
        Socket(Socket&&) = default;
        ~Socket();
        Socket& operator=(const Socket&) = default;
        Socket& operator=(Socket&&) = default;
        void bind(uint16_t port);   // Bind a local port
        void connect(std::string peer_ip, uint16_t peer_port);  // Actively open, Send SYN and ISN to peer, CLOSED->SYN_SENT
        void listen();  // Passively open, wait SYN and ISN,CLOSED->SYN_RCVD
        Socket accept();   // send SYN and ACK to peer, SYN_RCVD->ESTABLISHED
        void disconnect();  // ESTABLISHED->FIN_WAIT_1,FIN_WAIT_2,CLOSE_WAIT,LAST_ACK,TIME_WAIT->CLOSE
        std::string recv_pkg();
        void send_pkg(const std::string& data);
    };
}

#endif

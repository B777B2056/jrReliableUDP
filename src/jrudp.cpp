#include "jrudp.hpp"

jrReliableUDP::Socket::Socket()
    : sockfd(::socket(AF_INET, SOCK_DGRAM, 0)), rto(RTO_INIT, -1, -1), cur_state(CLOSED),
      sender(sockfd, addr, rto), recver(sockfd, addr, rto) {
    if(-1 == sockfd) {
        throw std::runtime_error(error_msg("Socket create failed"));
    }
}

jrReliableUDP::Socket::Socket(int fd, bool is_passive_end, sockaddr_in peer_addr, RTO rto,
                              const Sender& s, const Recver& r, ConnectionState cs)
    : sockfd(fd), is_passive_end(is_passive_end), addr(peer_addr), rto(rto), cur_state(cs),
      sender(sockfd, addr, this->rto, s), recver(sockfd, addr, this->rto, r) {
//    struct sigaction act;
//    act.sa_handler = Socket::keep_alive_timeout;
//    ::sigemptyset(&act.sa_mask);
//    act.sa_flags = 0;
//    ::sigaction(SIGALRM, &act, nullptr);
}

jrReliableUDP::Socket::~Socket() {
    if(cur_state == LISTEN) {
        ::close(sockfd);
    }
}

//void jrReliableUDP::Socket::keep_alive_timeout(int sig) {
//    if(sig == SIGALRM) {
//        // Start send keep alive probe packet

//    }
//}

void jrReliableUDP::Socket::disconnect_exception(std::string msg) {
    ::close(sockfd);
    cur_state = CLOSED;
    throw std::runtime_error(msg);
}

void jrReliableUDP::Socket::set_local_address(uint16_t port) {
    ::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
}

void jrReliableUDP::Socket::set_peer_address(std::string ip, uint16_t port) {
    // find host ip by name through DNS service
    auto hpk = ::gethostbyname(ip.c_str());
    if(!hpk) {
        throw std::runtime_error(error_msg("IP address parsing failed"));
    }
    // init struct
    ::memset(&addr, 0, sizeof(addr));
    // fill the struct
    addr.sin_family = AF_INET;		// protocol
    addr.sin_addr.s_addr = inet_addr(inet_ntoa(*reinterpret_cast<in_addr*>(hpk->h_addr_list[0])));
    addr.sin_port = htons(port);		// target process port number
}

void jrReliableUDP::Socket::bind(uint16_t port) {
    set_local_address(port);
    if(-1 == ::bind(sockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_in))) {
        disconnect_exception(error_msg("Bind failed"));
    }
    this->port = port;
}

void jrReliableUDP::Socket::connect(std::string peer_ip, uint16_t peer_port) {
    is_passive_end = false;
    while(true) {
#ifdef DEBUG
    std::cout << states[cur_state] << ":";
#endif
        switch(cur_state) {
        case CLOSED:
            // Set peer ip and port
            set_peer_address(peer_ip, peer_port);
            // Send SYN and ISN(CLOSED->SYN_SENT)
            sender.send_SYN();
            cur_state = SYN_SENT;
            break;
        case SYN_SENT:
            // wait for peer's SYN
        {
            // Received correct SYN(SYN_SENT->ESTABLISHED)
            if(IS_SYN(recver.recv_raw_packet().type)) {
                cur_state = ESTABLISHED;
            }
        }
            break;
        case ESTABLISHED:
            sender.set_WND();
            recver.set_WND();
            return ;
        default:
            return ;
        }
    }
}

void jrReliableUDP::Socket::listen() {
    is_passive_end = true;
#ifdef DEBUG
    std::cout << states[cur_state] << ":";
#endif
    switch (cur_state) {
    case CLOSED:
        cur_state = LISTEN;
        break;
    case LISTEN:
        break;
    default:
        break;
    }
#ifdef DEBUG
    std::cout << states[cur_state] << ":";
#endif
}

jrReliableUDP::Socket jrReliableUDP::Socket::accept() {
    bool stop = false;
    while(!stop) {
#ifdef DEBUG
    std::cout << states[cur_state] << ":";
#endif
        switch(cur_state) {
        case CLOSED:
            throw std::runtime_error("Not listening");
        case LISTEN:
        {
            // wait for peer's SYN(LISTEN->SYN_RCVD)
            if(IS_SYN(recver.recv_raw_packet().type)) {
                cur_state = SYN_RCVD;
            } else {
                // Received other type packet, send RST
                sender.send_RST();
                disconnect_exception("Connection invalid");
            }
        }
            break;
        case SYN_RCVD:
            // Send SYN and wait for peer's ACK
            sender.send_SYN();
            // SYN_RCVD->ESTABLISHED
            cur_state = ESTABLISHED;
            break;
        case ESTABLISHED:
            cur_state = LISTEN;
            stop = true;
            break;
        default:
            break;
        }
    }
    return Socket(::dup(sockfd), true, addr, rto, sender, recver, ESTABLISHED);
}

void jrReliableUDP::Socket::disconnect() {
    if(cur_state == ESTABLISHED) {
        // Send all pkg in send buffer
        sender.send_all_in_buf();
    }
    sender.reset_WND();
    recver.reset_WND();
    if(is_passive_end) {
        // Server
        while(true) {
#ifdef DEBUG
    std::cout << states[cur_state] << ":";
#endif
            switch(cur_state) {
            case ESTABLISHED:
                // ESTABLISHED->CLOSE_WAIT
                cur_state = CLOSE_WAIT;
                break;
            case CLOSE_WAIT:
            {
                // wait peer's FIN
                if(IS_FIN(recver.recv_raw_packet().type)) {
                    // Recv the FIN, CLOSE_WAIT->LAST_ACK
                    cur_state = LAST_ACK;
                }
            }
                break;
            case LAST_ACK:
                // Send FIN to peer, wait peer's ACK, LAST_ACK->CLOSED
                sender.send_FIN();
                cur_state = CLOSED;
                break;
            case CLOSED:
                ::close(sockfd);
#ifdef DEBUG
                std::cout << states[cur_state] << std::endl;
#endif
                return ;
            default:
                return ;
            }
        }
    } else {
        // Client
        while(true) {
#ifdef DEBUG
    std::cout << states[cur_state] << ":";
#endif
            switch(cur_state) {
            case ESTABLISHED:
                // ESTABLISHED->FIN_WAIT_1
                cur_state = FIN_WAIT;
                break;
            case FIN_WAIT:
                // Send FIN to peer(FIN_WAIT_1) and wait for peer's ACK(FIN_WAIT_1->FIN_WAIT_2)
                sender.send_FIN();
                // FIN_WAIT_2->TIME_WAIT
                cur_state = TIME_WAIT;
                break;
            case TIME_WAIT:
            {
                // wait peer's FIN
                RawPacket fin = recver.recv_raw_packet();
                if(IS_FIN(fin.type)) {
                    // TIME_WAIT->CLOSED
                    cur_state = CLOSED;
                }
            }
                break;
            case CLOSED:
                ::close(sockfd);
#ifdef DEBUG
                std::cout << states[cur_state] << std::endl;
#endif
                return ;
            default:
                return ;
            }
        }
    }
}

std::string jrReliableUDP::Socket::recv_pkg() {
    if(cur_state != ESTABLISHED) {
        return "";
    } else {
        RawPacket pkg = recver.recv_raw_packet();
        if(IS_FIN(pkg.type)) {
            cur_state = CLOSE_WAIT;
        }
        return std::string(pkg.data);
    }
}

void jrReliableUDP::Socket::send_pkg(const std::string& data) {
    if(cur_state != ESTABLISHED) {
        disconnect_exception("Connection is not ESTABLISHED");
    }
    if(data.size() > MAX_SIZE) {
        throw std::runtime_error("Package too large");
    }
    sender.send_DATA(data);
}

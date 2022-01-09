#include "jrudp.hpp"

std::string jrReliableUDP::error_msg(std::string msg) {
    return msg + ":" + strerror(errno);
}

jrReliableUDP::Socket::Socket() : cur_seq_num(init_seq_num()), cur_ack_num(0),
                                  cur_win_size(flow_win_size()), cur_state(CLOSED), dupack_cnt(0) {
    sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if(-1 == sockfd) {
        throw std::runtime_error(error_msg("Socket create failed"));
    }
}

jrReliableUDP::Socket::Socket(int fd, bool is_passive_end, sockaddr_in peer_addr,
                              uint32_t csn, uint32_t can, ConnectionState cs)
    : sockfd(fd), is_passive_end(is_passive_end), addr(peer_addr),
      cur_seq_num(csn), cur_ack_num(can), cur_win_size(flow_win_size()), cur_state(cs), dupack_cnt(0) {}

jrReliableUDP::Socket::~Socket() {
    if(cur_state == LISTEN) {
        ::close(sockfd);
    }
}

uint32_t jrReliableUDP::Socket::init_seq_num() const {
    return 0;
}

uint16_t jrReliableUDP::Socket::flow_win_size() const {
    return 8;
}

void jrReliableUDP::Socket::set_timeout(uint ms) {
  timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = ms*1000;
  ::setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
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

void jrReliableUDP::Socket::send_raw_packet(const RawPacket& pkg) {
    char buf[sizeof(RawPacket)];
    ::memmove(buf, &pkg, sizeof(RawPacket));
    if(-1 == ::sendto(sockfd, buf, sizeof(RawPacket), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        throw std::runtime_error(error_msg("Send error:"));
    }
    if(sent_pkgs.find(pkg.seq_num) == sent_pkgs.end()) {
        sent_pkgs[pkg.seq_num] = pkg;
    }
    if(pkg.type == ACK) { return ; }  // ACK doesn't need retransmit
    // Wait ACK and Retransmit function
    socklen_t len = sizeof(sockaddr_in);
    RawPacket ack_pkg;
    ::memset(&addr, 0, len);
    ++cur_seq_num;
    if(::recvfrom(sockfd, buf, sizeof(RawPacket), 0, reinterpret_cast<sockaddr*>(&addr), &len) > 0) {
        ::memmove(&ack_pkg, buf, sizeof(RawPacket));
        if(IS_ACK(ack_pkg.type)) {
            // ACK arrived
            if(ack_pkg.ack_num >= cur_seq_num) {
                // Correct ACK
                dupack_cnt = 0; // Reset counter
                cur_seq_num = ack_pkg.ack_num;
                sent_pkgs.erase(sent_pkgs.begin(), sent_pkgs.find(ack_pkg.ack_num));  // Update sent buffer
            } else {
                // Duplicate ACK
                if(dupack_cnt == DUPTHRESH) {
                    // Reset counter
                    dupack_cnt = 0;
                    // Retransmit all
                    uint32_t right = cur_seq_num;
                    cur_seq_num = ack_pkg.ack_num;
                    for(uint32_t i = ack_pkg.ack_num; i <= right; ++i) {
                        send_raw_packet(sent_pkgs[i]);
                    }
                } else {
                    ++dupack_cnt;
                }
            }
        } else {
            // RST arrived
            ::close(sockfd);
            throw std::runtime_error("Connection closed by peer.");
        }
    } else {
        if(errno == EAGAIN) {
            // Recv ACK timeout, retransmit current pkg
            send_raw_packet(pkg);
        } else {
            ::close(sockfd);
            throw std::runtime_error(error_msg("Send failed"));
        }
    }
}

void jrReliableUDP::Socket::send_raw_packet(uint type) {
    RawPacket pkg;
    pkg.seq_num = cur_seq_num;
    pkg.ack_num = cur_ack_num;
    pkg.win_size = cur_win_size;
    pkg.type = type;
    pkg.mss = DEFAULT_MSS;
    send_raw_packet(pkg);
}

jrReliableUDP::RawPacket jrReliableUDP::Socket::wait_raw_packet() {
    socklen_t len = sizeof(addr);
    RawPacket pkg;
    char buf[sizeof(RawPacket)];
PKG_MISSING:
    ::memset(&addr, 0, len);
    if(::recvfrom(sockfd, buf, sizeof(RawPacket), 0, reinterpret_cast<sockaddr*>(&addr), &len) > 0) {
        ::memmove(&pkg, buf, sizeof(RawPacket));
        if(cur_ack_num == pkg.seq_num) {
            cur_ack_num = pkg.seq_num + 1;
            rcvd_pkgs[pkg.seq_num] = pkg;
            send_raw_packet(ACK);
        } else if(cur_ack_num < pkg.seq_num) {
            send_raw_packet(ACK);
            goto PKG_MISSING;
        }
        RawPacket ret = rcvd_pkgs.begin()->second;
        rcvd_pkgs.erase(rcvd_pkgs.begin());
        return ret;
    } else {
        ::close(sockfd);
        throw std::runtime_error(error_msg("Recv failed"));
    }
}

void jrReliableUDP::Socket::bind(uint16_t port) {
    set_local_address(port);
    if(-1 == ::bind(sockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_in))) {
        ::close(sockfd);
        throw std::runtime_error(error_msg("Bind failed"));
    }
    this->port = port;
}

void jrReliableUDP::Socket::connect(std::string peer_ip, uint16_t peer_port) {
    set_timeout(RTT_INIT);
    is_passive_end = false;
    while(true) {
        switch(cur_state) {
        case CLOSED:
            // Set peer ip and port
            set_peer_address(peer_ip, peer_port);
            // Send SYN and ISN(CLOSED->SYN_SENT)
            send_raw_packet(SYN);
            cur_state = SYN_SENT;
            break;
        case SYN_SENT:
            // wait for peer's SYN
        {
            RawPacket peer_syn = wait_raw_packet();
            // Received correct SYN(SYN_SENT->ESTABLISHED)
            if(IS_SYN(peer_syn.type)) {
                cur_state = ESTABLISHED;
            }
        }
            break;
        case ESTABLISHED:
            return ;
        default:
            throw std::runtime_error("Connection established");
        }
    }
}

void jrReliableUDP::Socket::listen() {
    is_passive_end = true;
    switch (cur_state) {
    case CLOSED:
        cur_state = LISTEN;
        break;
    case LISTEN:
        throw std::runtime_error("Already listening");
    default:
        break;
    }
}

jrReliableUDP::Socket jrReliableUDP::Socket::accept() {
    bool stop = false;
    while(!stop) {
        switch(cur_state) {
        case CLOSED:
            throw std::runtime_error("Not listening");
        case LISTEN:
        {
            // wait for peer's SYN(LISTEN->SYN_RCVD)
            RawPacket syn = wait_raw_packet();
            if(IS_SYN(syn.type)) {
                cur_state = SYN_RCVD;
            } else {
                // Received other type packet, send RST
                send_raw_packet(RST);
                throw std::runtime_error("Connection invalid");
            }
        }
            break;
        case SYN_RCVD:
            // Send SYN and wait for peer's ACK
            send_raw_packet(SYN);
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
    return Socket(::dup(sockfd), true, addr, cur_seq_num, cur_ack_num, ESTABLISHED);
}

void jrReliableUDP::Socket::disconnect() {
    if(is_passive_end) {
        // Server
        while(true) {
            switch(cur_state) {
            case ESTABLISHED:
                // ESTABLISHED->CLOSE_WAIT
                cur_state = CLOSE_WAIT;
                break;
            case CLOSE_WAIT:
            {
                // wait peer's FIN
                RawPacket fin = wait_raw_packet();
                if(IS_FIN(fin.type)) {
                    // Recv the FIN, CLOSE_WAIT->LAST_ACK
                    cur_state = LAST_ACK;
                }
            }
                break;
            case LAST_ACK:
                // Send FIN to peer, wait peer's ACK, LAST_ACK->CLOSED
                send_raw_packet(FIN);
                cur_state = CLOSED;
                break;
            case CLOSED:
                ::close(sockfd);
                return ;
            default:
                throw std::runtime_error("Not connecting");
            }
        }
    } else {
        // Client
        while(true) {
            switch(cur_state) {
            case ESTABLISHED:
                // ESTABLISHED->FIN_WAIT_1
                cur_state = FIN_WAIT;
                break;
            case FIN_WAIT:
                // Send FIN to peer(FIN_WAIT_1) and wait for peer's ACK(FIN_WAIT_1->FIN_WAIT_2)
                send_raw_packet(FIN);
                // FIN_WAIT_2->TIME_WAIT
                cur_state = TIME_WAIT;
                break;
            case TIME_WAIT:
            {
                // wait peer's FIN
                RawPacket fin = wait_raw_packet();
                if(IS_FIN(fin.type)) {
                    // TIME_WAIT->CLOSED
                    cur_state = CLOSED;
                }
            }
                break;
            case CLOSED:
                ::close(sockfd);
                return ;
            default:
                throw std::runtime_error("Not connecting");
            }
        }
    }
}

std::string jrReliableUDP::Socket::recv_pkg() {
    if(cur_state != ESTABLISHED) {
        throw std::runtime_error("Connection is not ESTABLISHED");
    }
    return std::string(wait_raw_packet().data);
}

void jrReliableUDP::Socket::send_pkg(const std::string& data) {
    if(cur_state != ESTABLISHED) {
        throw std::runtime_error("Connection is not ESTABLISHED");
    }
    if(data.size() > MAX_SIZE) {
        throw std::runtime_error("Package too large");
    }
    RawPacket pkg;
    pkg.seq_num = cur_seq_num;
    pkg.ack_num = cur_ack_num;
    pkg.win_size = cur_win_size;
    pkg.type = DATA;
    pkg.mss = DEFAULT_MSS;
    ::strcpy(pkg.data, data.data());
    send_raw_packet(pkg);
}

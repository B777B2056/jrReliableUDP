#include "jrudp.hpp"

std::string jrReliableUDP::error_msg(std::string msg) {
    return msg + ":" + strerror(errno);
}

jrReliableUDP::Socket::Socket() : current_seq_num(init_seq_num()), current_ack_num(0),
                                  current_win_size(flow_win_size()), current_state(CLOSED) {
    sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if(-1 == sockfd) {
        throw std::runtime_error(error_msg("Socket create failed"));
    }
}

jrReliableUDP::Socket::Socket(int fd, bool is_passive_end, sockaddr_in peer_addr,
                              uint32_t csn, uint32_t can, ConnectionState cs)
    : sockfd(fd), is_passive_end(is_passive_end), addr(peer_addr),
      current_seq_num(csn), current_ack_num(can), current_win_size(flow_win_size()), current_state(cs) {}

uint32_t jrReliableUDP::Socket::init_seq_num() const {
    return 0;
}

uint16_t jrReliableUDP::Socket::flow_win_size() const {
    return 8;
}

void jrReliableUDP::Socket::set_local_address(uint port) {
    ::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
}

void jrReliableUDP::Socket::set_peer_address(std::string ip, uint port) {
    // find host ip by name through DNS service
    auto hpk = ::gethostbyname(ip.c_str());
    if(!hpk) {
        throw std::runtime_error(error_msg("IP address parsing failed"));
    }
    // init struct
    ::memset(&addr, 0, sizeof(addr));
    // fill the struct
    addr.sin_family = AF_INET;		// protocol
    addr.sin_addr.s_addr = inet_addr(inet_ntoa(*(in_addr*)(hpk->h_addr_list[0])));
    addr.sin_port = htons(port);		// target process port number
}

void jrReliableUDP::Socket::send_raw_packet(uint type) {
    RawPacket p1;
    p1.seq_num = current_seq_num;
    p1.ack_num = current_ack_num;
    p1.win_size = current_win_size;
    p1.type = type;
    p1.mss = DEFAULT_MSS;
    char buf[sizeof(RawPacket)];
    ::memmove(buf, &p1, sizeof(RawPacket));
    ::sendto(sockfd, buf, sizeof(RawPacket), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    sent_packets[p1.seq_num] = p1;
    if(type == ACK) {
        return ;
    }
    socklen_t len = sizeof(addr);
    RawPacket p2;
    ::memset(&addr, 0, len);
    ++current_seq_num;
    if(::recvfrom(sockfd, buf, sizeof(RawPacket), 0, reinterpret_cast<sockaddr*>(&addr), &len) > 0) {
        ::memmove(&p2, buf, sizeof(RawPacket));
        if(IS_ACK(p2.type)) {
            if(p2.ack_num == current_seq_num) {
                // Correct ACK
                current_ack_num = p2.seq_num + 1;
            } else {
                // Wrong ACK, need retransmit
                RawPacket p = sent_packets[p2.ack_num];
                ::memmove(buf, &p, sizeof(RawPacket));
                ::sendto(sockfd, buf, sizeof(RawPacket), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
            }
        }
    } else {
        throw std::runtime_error(error_msg("Send failed"));
    }
}

jrReliableUDP::RawPacket jrReliableUDP::Socket::wait_raw_packet() {
    socklen_t len = sizeof(addr);
    RawPacket packet;
    char packet_buf[sizeof(RawPacket)];
    ::memset(&addr, 0, len);
    if(::recvfrom(sockfd, packet_buf, sizeof(RawPacket), 0, reinterpret_cast<sockaddr*>(&addr), &len) > 0) {
        ::memmove(&packet, packet_buf, sizeof(RawPacket));
        current_ack_num = packet.seq_num + 1;
        return packet;
    } else {
        throw std::runtime_error(error_msg("Recv failed"));
    }
}

void jrReliableUDP::Socket::bind(uint port) {
    set_local_address(port);
    if(-1 == ::bind(sockfd, reinterpret_cast<sockaddr*>(&addr), sizeof(sockaddr_in))) {
        ::close(sockfd);
        throw std::runtime_error(error_msg("Bind failed"));
    }
    this->port = port;
}

void jrReliableUDP::Socket::connect(std::string peer_ip, uint peer_port) {
    is_passive_end = false;
    while(true) {
        switch(current_state) {
        case CLOSED:
            // Set peer ip and port
            set_peer_address(peer_ip, peer_port);
            //    ::alarm(OVERTIME_SEC);
            // Send SYN and ISN(CLOSED->SYN_SENT)
            send_raw_packet(SYN);
            current_state = SYN_SENT;
            break;
        case SYN_SENT:
            // wait for peer's SYN
        {
            RawPacket peer_syn = wait_raw_packet();
            // Received correct SYN(SYN_SENT->ESTABLISHED)
            if(IS_SYN(peer_syn.type)) {
                current_state = ESTABLISHED;
            }
        }
            break;
        case ESTABLISHED:
            // Send ACK(ESTABLISHED)
            send_raw_packet(ACK);
            return ;
        default:
            throw std::runtime_error("Connection established");
        }
    }
}

void jrReliableUDP::Socket::listen() {
    is_passive_end = true;
    switch (current_state) {
    case CLOSED:
        current_state = LISTEN;
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
        switch(current_state) {
        case CLOSED:
            throw std::runtime_error("Not listening");
        case LISTEN:
        {
            // wait for peer's SYN(LISTEN->SYN_RCVD)
            RawPacket syn = wait_raw_packet();
            if(IS_SYN(syn.type)) {
                current_state = SYN_RCVD;
            } else {
                // Received other type packet, send RST
                send_raw_packet(RST);
                throw std::runtime_error("Connection invalid");
            }
        }
            break;
        case SYN_RCVD:
            send_raw_packet(ACK);
            // Send SYN and wait for peer's ACK
            send_raw_packet(SYN);
            // SYN_RCVD->ESTABLISHED
            current_state = ESTABLISHED;
            break;
        case ESTABLISHED:
            current_state = LISTEN;
            stop = true;
            break;
        default:
            break;
        }
    }
    return Socket(::dup(sockfd), true, addr, current_seq_num, current_ack_num, ESTABLISHED);
}

void jrReliableUDP::Socket::disconnect() {
    if(is_passive_end) {
        // Server
        while(true) {
            switch(current_state) {
            case ESTABLISHED:
                // ESTABLISHED->CLOSE_WAIT
                current_state = CLOSE_WAIT;
                break;
            case CLOSE_WAIT:
            {
                // wait peer's FIN
                RawPacket fin = wait_raw_packet();
                if(IS_FIN(fin.type)) {
                    // Recv the FIN, CLOSE_WAIT->LAST_ACK
                    current_state = LAST_ACK;
                }
            }
                break;
            case LAST_ACK:
                // Send FIN to peer, wait peer's ACK, LAST_ACK->CLOSED
                send_raw_packet(ACK);
                send_raw_packet(FIN);
                current_state = CLOSED;
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
            switch(current_state) {
            case ESTABLISHED:
                // ESTABLISHED->FIN_WAIT_1
                current_state = FIN_WAIT;
                break;
            case FIN_WAIT:
                // Send FIN to peer(FIN_WAIT_1) and wait for peer's ACK(FIN_WAIT_1->FIN_WAIT_2)
                send_raw_packet(FIN);
                // FIN_WAIT_2->TIME_WAIT
                current_state = TIME_WAIT;
                break;
            case TIME_WAIT:
            {
                // wait peer's FIN
                RawPacket fin = wait_raw_packet();
                if(IS_FIN(fin.type)) {
                    send_raw_packet(ACK);
                    // TIME_WAIT->CLOSED
                    current_state = CLOSED;
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

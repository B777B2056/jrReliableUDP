#include "sender.hpp"

namespace jrReliableUDP {
    Sender::Sender(int sockfd, sockaddr_in& addr, RTO& rto)
        : sockfd(sockfd), addr(addr), rto(rto), cur_seq_num(0), dupack_cnt(1),
        SND_WND(1), CONG_WND(1), ssthresh(init_ssthresh()), is_fast_recover(false) {

    }

    Sender::Sender(int sockfd, sockaddr_in& addr, RTO& rto, const Sender& s)
        : sockfd(sockfd), addr(addr), rto(rto), cur_seq_num(s.cur_seq_num), dupack_cnt(1),
        SND_WND(init_WND()), CONG_WND(1), ssthresh(init_ssthresh()), is_fast_recover(false) {

    }

    Sender::Sender(int sockfd, sockaddr_in& addr, RTO& rto, const Sender& s, uint16_t SND_WND)
        : sockfd(sockfd), addr(addr), rto(rto), cur_seq_num(s.cur_seq_num), dupack_cnt(1),
        SND_WND(SND_WND), CONG_WND(1), ssthresh(init_ssthresh()), is_fast_recover(false) {

    }

    uint32_t Sender::init_seq_num() const {
        return 0;
    }

    uint16_t Sender::init_WND() const {
        return 8;
    }

    uint16_t Sender::init_ssthresh() const {
        return 8;
    }

    void Sender::set_timeout() {
      timeval tv;
      tv.tv_sec = 0;
      tv.tv_usec = rto.backoff_factor*rto.RTO_ms*1000;
      ::setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    void Sender::send_pkgs_in_buf() {
        char buf[sizeof(RawPacket)];
        for(auto& pkgs : swnd) {
            RawPacket p = pkgs.second;
            ::memmove(buf, &p, sizeof(RawPacket));
            if(-1 == ::sendto(sockfd, buf, sizeof(RawPacket), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
                throw std::runtime_error(jrReliableUDP::error_msg("Send error:"));
            }
        }
    #ifdef DEBUG
        std::cout << "Sent SEQ:" << cur_seq_num << ",";
    #endif
        wait_ack();
    }

    void Sender::wait_ack() {
        char buf[sizeof(RawPacket)];
        // Wait ACK and Retransmit function
        socklen_t len = sizeof(sockaddr_in);
        RawPacket ack_pkg;
        ::memset(&addr, 0, len);
        for(auto it = swnd.begin(); it != swnd.end(); ) {
            set_timeout();
            if(::recvfrom(sockfd, buf, sizeof(RawPacket), 0, reinterpret_cast<sockaddr*>(&addr), &len) > 0) {
                ::memmove(&ack_pkg, buf, sizeof(RawPacket));
                if(IS_ACK(ack_pkg.type)) {
                    int64_t rtt = get_time_diff_from_now_ms(ack_pkg.timestamp);
                    rto.update_RTO_ms(rtt);
                    // ACK arrived
                    SND_WND = std::min(ack_pkg.win_size, CONG_WND); // update SND.WND by RCV.WND
                    uint32_t last_seq = it->first;
    #ifdef DEBUG
                    std::cout << "Received ACK:" << ack_pkg.ack_num << ",";
                    std::cout << "Last SEQ:" << last_seq << ",";
                    std::cout << "RTT=" << rtt << "ms" << ",";
                    std::cout << "SRTT=" << rto.srtt << ",";
                    std::cout << "RTTVAR=" << rto.rttvar << ",";
                    std::cout << "RTO=" << rto.RTO_ms << "ms" << std::endl;
    #endif
                    if(ack_pkg.ack_num >= last_seq + 1) {
                        // Correct ACK
                        it = swnd.erase(swnd.begin(), swnd.find(ack_pkg.ack_num));  // Update sent buffer
                        dupack_cnt = 1; // Reset counter
                        // Slow start, congestion window size index inc
                        if(CONG_WND < ssthresh) {
                            ++CONG_WND;
                        }
                        // Cancel fast recover
                        if(is_fast_recover) {
                            is_fast_recover = false;
                        }
                    } else {
                        // Duplicate ACK
                        if(dupack_cnt == DUPTHRESH) {
                            // Update sent buffer
                            swnd.erase(swnd.begin(), swnd.find(ack_pkg.ack_num));
                            // Reset counter
                            dupack_cnt = 1;
                            // Retransmition
                            // Fast retransmition's congestion occurs
                            CONG_WND = CONG_WND / 2;
                            ssthresh = CONG_WND;
                            // Fast recover
                            if(!is_fast_recover) {
                                is_fast_recover = true;
                                CONG_WND = ssthresh + DUPTHRESH;
                            }
                            break;
                        } else {
                            ++dupack_cnt;
                            ++it;
                        }
                        if(is_fast_recover) {
                            ++CONG_WND;
                        }
                    }
                } else if(IS_RST(ack_pkg.type)) {
                    // RST arrived
                    throw std::runtime_error("Connection closed by peer.");
                }
            } else {
                if(errno == EAGAIN) {
                    // If the waiting time exceeds the upper limit of the timeout, the current end considers that the peer end is closed
                    if(rto.RTO_ms > MAX_WAIT_TIME) {
                        throw std::runtime_error("Connection closed by peer.");
                    }
                    // Backoff
                    rto.backoff_factor *= 2;
                    // Recv ACK timeout, retransmit, DO NOT slide the send window
                    // Timeout retransmition's congestion occurs
                    ssthresh = CONG_WND / 2;
                    CONG_WND = 1;
                } else {
                    throw std::runtime_error(jrReliableUDP::error_msg("Send failed"));
                }
            }
        }
        // Congestion avoidance, congestion window size linear inc
        if(CONG_WND >= ssthresh) {
            ++CONG_WND;
        }
    }

    void Sender::send_raw_packet(const RawPacket& pkg) {
        // Probe peer's RCV.WND
        while(SND_WND == 0) {
            SND_WND = 1;
            RawPacket probe(cur_seq_num, 0, 0, DATA);
            swnd[probe.seq_num] = probe;
            ++cur_seq_num;
            send_pkgs_in_buf();
        }
        // Add into SND window
        if(swnd.find(pkg.seq_num) == swnd.end()) {
            swnd[pkg.seq_num] = pkg;
            ++cur_seq_num;
        }
        // SND size not reach SND_WND
        if(swnd.size() < SND_WND) {
            return ;
        }
        // Send all pkg in SND window
        send_pkgs_in_buf();
    }

    void Sender::send_SYN() {
        send_raw_packet(RawPacket(cur_seq_num, 0, 0, SYN));
    }

    void Sender::send_FIN() {
        send_raw_packet(RawPacket(cur_seq_num, 0, 0, FIN));
    }

    void Sender::send_RST() {
        send_raw_packet(RawPacket(cur_seq_num, 0, 0, RST));
    }

    void Sender::send_DATA(const std::string& data) {
        send_raw_packet(RawPacket(cur_seq_num, 0, 0, DATA, data));
    }

//    void Sender::send_keepalive_probe() {
//        send_raw_packet(RawPacket(swnd.begin()->first-1, 0, 0, DATA));
//    }

    void Sender::send_all_in_buf() {
        while(!swnd.empty()) {
            send_pkgs_in_buf();
        }
    }
}

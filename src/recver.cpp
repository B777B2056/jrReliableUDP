#include "recver.hpp"

namespace jrReliableUDP {
    Recver::Recver(int sockfd, sockaddr_in& addr, RTO& rto)
        : sockfd(sockfd), addr(addr), rto(rto), cur_ack_num(0), RCV_NXT(0), RCV_WND(1) {

    }

    Recver::Recver(int sockfd, sockaddr_in& addr, RTO& rto, const Recver& r)
        : sockfd(sockfd), addr(addr), rto(rto), cur_ack_num(r.cur_ack_num), RCV_NXT(0), RCV_WND(init_WND()) {

    }

    Recver::Recver(int sockfd, sockaddr_in& addr, RTO& rto, const Recver& r, uint16_t RCV_WND)
        : sockfd(sockfd), addr(addr), rto(rto), cur_ack_num(r.cur_ack_num), RCV_NXT(0), RCV_WND(RCV_WND) {

    }

    uint16_t Recver::init_WND() const {
        return 8;
    }

    void Recver::cancel_timeout() {
      timeval tv;
      tv.tv_sec = 0;
      tv.tv_usec = 0;
      ::setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    }

    void Recver::send_ACK() {
        RawPacket pkg(0, cur_ack_num, RCV_WND - RCV_NXT, ACK);
        char buf[sizeof(RawPacket)];
        // ACK doesn't need retransmit and flow control
        ::memmove(buf, &pkg, sizeof(RawPacket));
        if(-1 == ::sendto(sockfd, buf, sizeof(RawPacket), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
            throw std::runtime_error(jrReliableUDP::error_msg("Send error:"));
        }
    }

    RawPacket Recver::recv_raw_packet() {
        socklen_t len = sizeof(sockaddr_in);
        RawPacket pkg;
        char buf[sizeof(RawPacket)];
    #if (defined (FAST_TRANSMIT_DEBUG)) || (defined (TIMEOUT_TRANSMIT_DEBUG))
        int drop_cnt = 0;
    #endif
        int dup_cnt = 0;
        ::memset(&addr, 0, len);
        cancel_timeout();
        for(uint16_t i = RCV_NXT; i < RCV_WND; ) {
            if(::recvfrom(sockfd, buf, sizeof(RawPacket), 0, reinterpret_cast<sockaddr*>(&addr), &len) > 0) {
                ::memmove(&pkg, buf, sizeof(RawPacket));
    #ifdef FAST_TRANSMIT_DEBUG
                if(pkg.seq_num == 2) {
                    ++drop_cnt;
                    if(drop_cnt == 1) {
                        goto RETRANSMIT;
                    }
                }
    #endif
    #ifdef TIMEOUT_TRANSMIT_DEBUG
                if(pkg.seq_num == 2) {
                    ++drop_cnt;
                    if(drop_cnt == 1) {
                        ::sleep(5);
                    }
                }
    #endif
    #ifdef DEBUG
                std::cout << "Received SEQ:" << pkg.seq_num << ",";
    #endif
                if(cur_ack_num == pkg.seq_num) {
                    dup_cnt = 0;
                    ++cur_ack_num;
                    rwnd[pkg.seq_num] = pkg;
                    send_ACK();
    #ifdef DEBUG
                    std::cout << "Sent ACK:" << cur_ack_num << std::endl;
    #endif
                    if(IS_FIN(pkg.type)) {
                        return pkg;
                    }
                    ++i;
                    ++RCV_NXT;
                } else if(cur_ack_num < pkg.seq_num) {
    #ifdef FAST_TRANSMIT_DEBUG
    RETRANSMIT:
    #endif
                    if(dup_cnt < DUPTHRESH) {
                        send_ACK();
    #ifdef DEBUG
                        std::cout << "Sent ACK:" << cur_ack_num << std::endl;
    #endif
                    } else {
    #ifdef DEBUG
                        std::cout << "Droped." << std::endl;
    #endif
                    }
                    ++dup_cnt;
                }
    #ifdef DEBUG
                else {
                        std::cout << "Droped." << std::endl;

                }
     #endif
            } else {
                throw std::runtime_error(error_msg("Recv failed"));
            }
        }
        RawPacket ret = rwnd.begin()->second;
        rwnd.erase(rwnd.begin());
        --RCV_NXT;
        return ret;
    }
}

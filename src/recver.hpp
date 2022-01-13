#ifndef RECVER_H
#define RECVER_H

#include "defs.hpp"

namespace jrReliableUDP {
    class Recver {
    private:
        int sockfd;
        sockaddr_in& addr;
        RTO& rto;
        uint32_t cur_ack_num;
        uint16_t RCV_NXT;
        uint16_t RCV_WND;
        std::map<uint32_t, RawPacket> rwnd;

    private:
        uint16_t init_WND() const;
        void cancel_timeout();
        void send_ACK();

    public:
        Recver(int sockfd, sockaddr_in& addr, RTO& rto);
        Recver(int sockfd, sockaddr_in& addr, RTO& rto, const Recver& r);
        Recver(int sockfd, sockaddr_in& addr, RTO& rto, const Recver& r, uint16_t RCV_WND);
        void set_WND() { RCV_WND = init_WND(); }
        void reset_WND() { RCV_WND = 1; }
        RawPacket recv_raw_packet();
    };
}

#endif

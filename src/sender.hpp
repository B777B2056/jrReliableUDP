#ifndef SENDER_H
#define SENDER_H

#include "defs.hpp"

namespace jrReliableUDP {
    class Sender {
    private:
        int sockfd;
        sockaddr_in& addr;
        RTO& rto;
        uint32_t cur_seq_num;
        int dupack_cnt; // Duplicate ACK counter
        uint16_t SND_WND;
        std::map<uint32_t, RawPacket> swnd;
        // Congress arguments
        uint16_t CONG_WND;
        uint16_t ssthresh;
        bool is_fast_recover;
        const int64_t MAX_WAIT_TIME = 10000;

    private:
        uint32_t init_seq_num() const;
        uint16_t init_WND() const;
        uint16_t init_ssthresh() const;
        void set_timeout();
        void send_pkgs_in_buf();
        void wait_ack();
        void send_raw_packet(const RawPacket& pkg);

    public:
        Sender(int sockfd, sockaddr_in& addr, RTO& rto);
        Sender(int sockfd, sockaddr_in& addr, RTO& rto, const Sender& s);
        Sender(int sockfd, sockaddr_in& addr, RTO& rto, const Sender& s, uint16_t SND_WND);
        void set_WND() { SND_WND = init_WND(); }
        void reset_WND() { SND_WND = 1; }
        void send_SYN();
        void send_FIN();
        void send_RST();
        void send_DATA(const std::string& data);
//        void send_keepalive_probe();
        void send_all_in_buf();
    };
}

#endif

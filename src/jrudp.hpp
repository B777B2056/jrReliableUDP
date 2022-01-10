#ifndef _JRUDP_H
#define _JRUDP_H

#define DEBUG
#ifdef DEBUG
#include <vector>
#include <iostream>
#endif

#include <map>
#include <queue>
#include <string>
#include <cstring>
#include <stdexcept>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define DATA (0)
#define RST (1)
#define SYN (4)
#define FIN (2)
#define ACK (8)
#define DEFAULT_MSS (1460)
#define OVERTIME_SEC (1)
#define DUPTHRESH (3)
#define RTT_INIT (1000)
#define MAX_SIZE (512)

#define IS_ACK(type) ((type&ACK) == ACK)
#define IS_SYN(type) ((type&SYN) == SYN)
#define IS_FIN(type) ((type&FIN) == FIN)
#define IS_RST(type) ((type&RST) == RST)

namespace jrReliableUDP {
    struct RawPacket {
        uint32_t seq_num;
        uint32_t ack_num;
        uint16_t win_size;  // flow control sliding window size
        uint type:4;    // 4 bit flag: ACK, SYN, FIN, RST
        uint mss:12;
        char data[MAX_SIZE];
    };

    std::string error_msg(std::string msg);

    class Socket {
    private:
        enum ConnectionState {CLOSED, SYN_SENT, LISTEN, SYN_RCVD, ESTABLISHED,
                              FIN_WAIT, TIME_WAIT, CLOSE_WAIT, LAST_ACK};
#ifdef DEBUG
        std::vector<std::string> states = {"CLOSED", "SYN_SENT", "LISTEN", "SYN_RCVD", "ESTABLISHED",
                                           "FIN_WAIT", "TIME_WAIT", "CLOSE_WAIT", "LAST_ACK"};
#endif

    private:
        int sockfd;
        uint port;
        bool is_passive_end;
        sockaddr_in addr;
        uint32_t cur_seq_num;
        uint32_t cur_ack_num;
        uint16_t cur_win_size;
        ConnectionState cur_state;
        int dupack_cnt; // Duplicate ACK counter
        std::map<uint32_t, RawPacket> sent_pkgs, rcvd_pkgs;

    private:
        Socket(int fd, bool is_passive_end, sockaddr_in addr, uint32_t csn, uint32_t can, ConnectionState cs);
        uint32_t init_seq_num() const;
        uint16_t flow_win_size() const;
        void set_timeout(uint ms);
        void set_local_address(uint16_t port);
        void set_peer_address(std::string ip, uint16_t port);
        void send_raw_packet(const RawPacket& pkg);
        void send_raw_packet(uint type);
        RawPacket wait_raw_packet();

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

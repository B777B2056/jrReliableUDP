#ifndef DEFS_H
#define DEFS_H

#include <map>
#include <string>
#include <chrono>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <netdb.h>
#include <sys/socket.h>

#define DATA (0)
#define RST (1)
#define SYN (4)
#define FIN (2)
#define ACK (8)
#define DEFAULT_MSS (1460)
#define DUPTHRESH (3)
#define MAX_SIZE (512)
#define RTO_INIT (1)

#define IS_ACK(type) ((type&ACK) == ACK)
#define IS_SYN(type) ((type&SYN) == SYN)
#define IS_FIN(type) ((type&FIN) == FIN)
#define IS_RST(type) ((type&RST) == RST)

#define DEBUG
//#define TIMEOUT_TRANSMIT_DEBUG
//#define FAST_TRANSMIT_DEBUG
#ifdef DEBUG
#include <unistd.h>
#include <vector>
#include <iostream>
#endif

namespace jrReliableUDP {
    using uint = unsigned int;

#ifdef DEBUG
        static std::vector<std::string> states = {"CLOSED", "SYN_SENT", "LISTEN", "SYN_RCVD", "ESTABLISHED",
                                           "FIN_WAIT", "TIME_WAIT", "CLOSE_WAIT", "LAST_ACK"};
#endif

    struct RTO {
      int64_t RTO_ms;
      int64_t srtt;
      int64_t rttvar;
      int64_t backoff_factor;
      const int64_t RTO_G_INDEX = 3;
      const int64_t RTO_H_INDEX = 2;

      RTO(int64_t a, int64_t b, int64_t c) : RTO_ms(a), srtt(b), rttvar(c), backoff_factor(1) {}

      void update_RTO_ms(int64_t rtts) {
          this->backoff_factor = 1;
          if(this->srtt == -1) {
              this->srtt = rtts;
              this->rttvar = rtts/2;
          } else {
              int64_t g = (2 >> RTO_G_INDEX);
              int64_t h = (2 >> RTO_H_INDEX);
              this->srtt = (1 - g) * this->srtt + g * rtts;
              this->rttvar = (1 - h) * this->rttvar + h * std::abs(rtts - this->srtt);
          }
          this->RTO_ms = this->srtt + 4 * this->rttvar;
      }   // Calc RTO in ms
    };

    struct RawPacket {
        uint32_t seq_num;
        uint32_t ack_num;
        uint16_t win_size;  // flow control sliding window size
        uint type:4;    // 4 bit flag: ACK, SYN, FIN, RST
        uint mss:12;
        int64_t timestamp;
        char data[MAX_SIZE];

        RawPacket() {}

        RawPacket(uint32_t seq_num, uint32_t ack_num, uint16_t win_size, uint type, const std::string& data="") {
            this->seq_num = seq_num;
            this->ack_num = ack_num;
            this->win_size = win_size;
            this->type = type;
            this->mss = DEFAULT_MSS;
            this->timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
            ::strcpy(this->data, data.data());
        }

        RawPacket(const RawPacket& pkg)
            : seq_num(pkg.seq_num), ack_num(pkg.ack_num),
              win_size(pkg.win_size), type(pkg.type), mss(pkg.mss), timestamp(pkg.timestamp) {
            ::memmove(data, pkg.data, MAX_SIZE);
        }

        RawPacket& operator=(const RawPacket& pkg) {
            seq_num = pkg.seq_num;
            ack_num = pkg.ack_num;
            win_size = pkg.win_size;
            type = pkg.type;
            mss = pkg.mss;
            timestamp = pkg.timestamp;
            ::memmove(data, pkg.data, MAX_SIZE);
            return *this;
        }
    };

    std::string error_msg(std::string msg);

    int64_t get_time_diff_from_now_ms(int64_t start);
}

#endif

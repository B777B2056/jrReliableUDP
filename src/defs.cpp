#include "defs.hpp"

namespace jrReliableUDP {
    std::string error_msg(std::string msg) {
        return msg + ":" + strerror(errno);
    }

    int64_t get_time_diff_from_now_ms(int64_t start) {
        auto end = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - std::chrono::duration<int64_t, std::milli>(start)).count();
    }
}

#pragma once
#include <atomic>

namespace features {
    struct stealer_stats {
        std::atomic<int> password_count{0};
        std::atomic<int> cookie_count{0};
        std::atomic<int> card_count{0};
        std::atomic<int> wallet_count{0};
    };

    extern stealer_stats stats;
}

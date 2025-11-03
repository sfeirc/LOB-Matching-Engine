#pragma once

#include <cstdint>
#include <chrono>

struct Trade {
    uint64_t buy_id;
    uint64_t sell_id;
    int64_t  price;
    int64_t  qty;
    std::chrono::steady_clock::time_point ts;
};


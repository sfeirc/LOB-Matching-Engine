#pragma once

#include <cstdint>
#include <chrono>

enum class MsgType {
    NewLimit,
    NewMarket,
    Cancel
};

enum class Side {
    Buy,
    Sell
};

struct Msg {
    MsgType type;
    Side    side;     // ignored for Cancel
    uint64_t id;      // unique order id
    int64_t price;    // ticks (ignored for NewMarket/Cancel)
    int64_t qty;      // lots (0 for Cancel)
    std::chrono::steady_clock::time_point ts;
};


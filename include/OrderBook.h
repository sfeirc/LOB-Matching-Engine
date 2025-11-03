#pragma once

#include <map>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include "Message.h"
#include "Trade.h"

// Compiler hints for maximum optimization
#ifdef __GNUC__
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define HOT         __attribute__((hot))
#define ALWAYS_INLINE __attribute__((always_inline)) inline
#define PREFETCH(addr) __builtin_prefetch(addr, 0, 3)
#else
#define LIKELY(x)   (x)
#define UNLIKELY(x) (x)
#define HOT
#define ALWAYS_INLINE inline
#define PREFETCH(addr)
#endif

// Configuration: Enable/disable trade recording
static constexpr bool ENABLE_TRADE_RECORDING = true;

using Price = int64_t;
using OrderId = uint64_t;
using Quantity = int64_t;

// Compact Order with intrusive list pointers for O(1) cancel
struct alignas(32) Order {
    OrderId id;
    Price price;
    Quantity qty;
    Side side;
    
    // Intrusive list for O(1) remove
    Order* next_in_level;
    Order* prev_in_level;
    
    Order() noexcept : next_in_level(nullptr), prev_in_level(nullptr) {}
    
    Order(OrderId id, Side side, Price price, Quantity qty) noexcept
        : id(id), side(side), price(price), qty(qty),
          next_in_level(nullptr), prev_in_level(nullptr) {}
    
    Order(Order&&) noexcept = default;
    Order& operator=(Order&&) noexcept = default;
    Order(const Order&) = delete;
    Order& operator=(const Order&) = delete;
};

// Fast price level using intrusive doubly-linked list
class PriceLevel {
private:
    Order* head_;
    Order* tail_;
    size_t count_;
    Quantity cached_qty_;  // Cache total quantity (always accurate)
    
public:
    PriceLevel() noexcept : head_(nullptr), tail_(nullptr), count_(0), cached_qty_(0) {}
    
    bool empty() const noexcept { return head_ == nullptr; }
    size_t size() const noexcept { return count_; }
    
    Order* get_front() const noexcept { return head_; }
    
    void add_order(Order* order) {
        order->next_in_level = nullptr;
        order->prev_in_level = tail_;
        
        if (UNLIKELY(head_ == nullptr)) {
            head_ = tail_ = order;
        } else {
            tail_->next_in_level = order;
            tail_ = order;
        }
        
        count_++;
        cached_qty_ += order->qty;
    }
    
    void remove_order(Order* order) {
        if (UNLIKELY(order == nullptr)) return;
        
        cached_qty_ -= order->qty;
        count_--;
        
        if (order->prev_in_level) {
            order->prev_in_level->next_in_level = order->next_in_level;
        } else {
            head_ = order->next_in_level;
        }
        
        if (order->next_in_level) {
            order->next_in_level->prev_in_level = order->prev_in_level;
        } else {
            tail_ = order->prev_in_level;
        }
        
        order->next_in_level = nullptr;
        order->prev_in_level = nullptr;
    }
    
    // Update cache when order quantity changes (partial fill)
    void update_qty(Quantity old_qty, Quantity new_qty) {
        cached_qty_ += (new_qty - old_qty);
    }
    
    void remove_front() {
        if (LIKELY(head_ != nullptr)) {
            remove_order(head_);
        }
    }
    
    Quantity total_qty() const noexcept {
        return cached_qty_;  // O(1) - always accurate with incremental updates
    }
};

class OrderBook {
private:
    // Object pool for zero-allocation hot path
    static constexpr size_t POOL_SIZE = 2 * 1024 * 1024;
    std::vector<Order> order_pool_;
    size_t pool_next_;
    
    // Use std::map (fast enough for ~500 price levels)
    std::map<Price, PriceLevel, std::greater<Price>> bids_;
    std::map<Price, PriceLevel, std::less<Price>> asks_;
    
    // Fast cancel: direct pointer to Order (O(1) cancel)
    std::unordered_map<OrderId, Order*> order_pointers_;
    
    std::vector<Trade> trades_;
    uint64_t total_messages_;
    uint64_t total_trades_;
    std::chrono::steady_clock::time_point current_match_ts_;
    
    ALWAYS_INLINE Order* allocate_order() {
        // Object pool: pre-allocated to avoid hot-path allocations
        if (UNLIKELY(pool_next_ >= order_pool_.size())) {
            // Grow pool by POOL_SIZE (should rarely happen after warm-up)
            size_t old_size = order_pool_.size();
            order_pool_.resize(old_size + POOL_SIZE);
            // Initialize new Orders (they'll be overwritten but ensures valid state)
            for (size_t i = old_size; i < order_pool_.size(); ++i) {
                order_pool_[i] = Order();
            }
        }
        Order* order = &order_pool_[pool_next_++];
        // Reset intrusive list pointers (in case reused)
        order->next_in_level = nullptr;
        order->prev_in_level = nullptr;
        return order;
    }
    
    ALWAYS_INLINE HOT void match_orders_fast(Order* incoming, Order* resting);
    ALWAYS_INLINE HOT void match_limit_buy_fast(Order* order);
    ALWAYS_INLINE HOT void match_limit_sell_fast(Order* order);
    ALWAYS_INLINE HOT void insert_limit_order_fast(Order* order);
    
public:
    OrderBook() : pool_next_(0), total_messages_(0), total_trades_(0) {
        order_pool_.reserve(POOL_SIZE);
        order_pool_.resize(POOL_SIZE);
        trades_.reserve(10 * 1024 * 1024);
    }
    
    HOT void process_message(const Msg& msg);
    
    Price best_bid() const noexcept;
    Price best_ask() const noexcept;
    Quantity best_bid_qty() const noexcept;
    Quantity best_ask_qty() const noexcept;
    Quantity total_bid_qty() const;
    Quantity total_ask_qty() const;
    
    std::vector<Trade> get_trades() const { return trades_; }
    uint64_t get_total_messages() const { return total_messages_; }
    uint64_t get_total_trades() const { return total_trades_; }
    void clear_trades() { trades_.clear(); }
};

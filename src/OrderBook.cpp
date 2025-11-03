#include "OrderBook.h"

// Ultra-fast matching with minimal overhead
inline void OrderBook::match_orders_fast(Order* incoming, Order* resting) {
    // Fast path: calculate match quantity (branchless min)
    Quantity match_qty = (incoming->qty < resting->qty) ? incoming->qty : resting->qty;
    
    // Update quantities first (critical path) - ensures progress
    incoming->qty -= match_qty;
    resting->qty -= match_qty;
    
    // Trade recording (compile-time optional)
    if constexpr (ENABLE_TRADE_RECORDING) {
        Trade& trade = trades_.emplace_back();
        trade.buy_id = (incoming->side == Side::Buy) ? incoming->id : resting->id;
        trade.sell_id = (incoming->side == Side::Buy) ? resting->id : incoming->id;
        trade.price = resting->price;
        trade.qty = match_qty;
        trade.ts = current_match_ts_;
    }
    
    total_trades_++;
}

inline void OrderBook::match_limit_buy_fast(Order* order) {
    while (LIKELY(order->qty > 0 && !asks_.empty())) {
        auto best_ask_it = asks_.begin();
        Price best_ask_price = best_ask_it->first;
        
        if (UNLIKELY(best_ask_price > order->price)) {
            break;
        }
        
        PriceLevel& level = best_ask_it->second;
        
        // Hot matching loop with prefetch
        while (LIKELY(order->qty > 0 && !level.empty())) {
            Order* resting = level.get_front();
            if (UNLIKELY(resting == nullptr || resting->qty <= 0)) {
                level.remove_front();
                continue;
            }
            
            // Prefetch next order
            if (LIKELY(resting->next_in_level)) {
                PREFETCH(resting->next_in_level);
            }
            
            Quantity resting_qty_before = resting->qty;
            match_orders_fast(order, resting);
            
            // Update cache for partial/full fill
            if (resting_qty_before != resting->qty) {
                level.update_qty(resting_qty_before, resting->qty);
            }
            
            if (UNLIKELY(resting->qty <= 0)) {
                order_pointers_.erase(resting->id);
                level.remove_order(resting);
                
                if (UNLIKELY(level.empty())) {
                    asks_.erase(best_ask_it);
                }
            }
            
            // Break if incoming order fully filled
            if (UNLIKELY(order->qty <= 0)) {
                break;
            }
            
            // If resting order partially filled, it stays at front - continue matching against it
            // If fully filled, it's removed above, so continue to next order
        }
        
        // Remove empty level
        if (UNLIKELY(level.empty())) {
            asks_.erase(best_ask_it);
        }
        
        // Exit outer loop if incoming fully filled or no more asks
        if (UNLIKELY(order->qty <= 0 || asks_.empty())) {
            break;
        }
    }
    
    if (LIKELY(order->qty > 0)) {
        insert_limit_order_fast(order);
    }
}

inline void OrderBook::match_limit_sell_fast(Order* order) {
    while (LIKELY(order->qty > 0 && !bids_.empty())) {
        auto best_bid_it = bids_.begin();
        Price best_bid_price = best_bid_it->first;
        
        if (UNLIKELY(best_bid_price < order->price)) {
            break;
        }
        
        PriceLevel& level = best_bid_it->second;
        
        while (LIKELY(order->qty > 0 && !level.empty())) {
            Order* resting = level.get_front();
            if (UNLIKELY(resting == nullptr || resting->qty <= 0)) {
                level.remove_front();
                continue;
            }
            
            if (LIKELY(resting->next_in_level)) {
                PREFETCH(resting->next_in_level);
            }
            
            Quantity resting_qty_before = resting->qty;
            match_orders_fast(order, resting);
            
            // Update cache for partial/full fill
            if (resting_qty_before != resting->qty) {
                level.update_qty(resting_qty_before, resting->qty);
            }
            
            if (UNLIKELY(resting->qty <= 0)) {
                order_pointers_.erase(resting->id);
                level.remove_order(resting);
                
                if (UNLIKELY(level.empty())) {
                    bids_.erase(best_bid_it);
                }
            }
            
            // Break if incoming order fully filled
            if (UNLIKELY(order->qty <= 0)) {
                break;
            }
            
            // If resting order partially filled, it stays at front - continue matching against it
            // If fully filled, it's removed above, so continue to next order
        }
        
        // Remove empty level
        if (UNLIKELY(level.empty())) {
            bids_.erase(best_bid_it);
        }
        
        // Exit outer loop if incoming fully filled or no more bids
        if (UNLIKELY(order->qty <= 0 || bids_.empty())) {
            break;
        }
    }
    
    if (LIKELY(order->qty > 0)) {
        insert_limit_order_fast(order);
    }
}

inline void OrderBook::insert_limit_order_fast(Order* order) {
    Price price = order->price;
    OrderId order_id = order->id;
    
    if (LIKELY(order->side == Side::Buy)) {
        PriceLevel& level = bids_[price];
        level.add_order(order);
        order_pointers_[order_id] = order;
    } else {
        PriceLevel& level = asks_[price];
        level.add_order(order);
        order_pointers_[order_id] = order;
    }
}

void OrderBook::process_message(const Msg& msg) {
    // Set timestamp once per message
    current_match_ts_ = std::chrono::steady_clock::now();
    total_messages_++;
    
    switch (msg.type) {
        case MsgType::NewLimit: {
            Order* order = allocate_order();
            *order = Order(msg.id, msg.side, msg.price, msg.qty);
            
            if (LIKELY(order->side == Side::Buy)) {
                match_limit_buy_fast(order);
            } else {
                match_limit_sell_fast(order);
            }
            break;
        }
        
        case MsgType::NewMarket: {
            Side side = msg.side;
            OrderId id = msg.id;
            Quantity qty = msg.qty;
            
            if (LIKELY(side == Side::Buy)) {
                while (LIKELY(qty > 0 && !asks_.empty())) {
                    auto best_ask_it = asks_.begin();
                    PriceLevel& level = best_ask_it->second;
                    
                    while (LIKELY(qty > 0 && !level.empty())) {
                        Order* resting = level.get_front();
                        if (UNLIKELY(resting == nullptr || resting->qty <= 0)) {
                            level.remove_front();
                            continue;
                        }
                        
                        Quantity resting_qty_before = resting->qty;
                        Order market_order(id, Side::Buy, 0, qty);
                        match_orders_fast(&market_order, resting);
                        qty = market_order.qty;
                        
                        if (UNLIKELY(resting->qty <= 0)) {
                            order_pointers_.erase(resting->id);
                            level.remove_order(resting);
                            
                            if (UNLIKELY(level.empty())) {
                                asks_.erase(best_ask_it);
                                break;
                            }
                        } else {
                            level.update_qty(resting_qty_before, resting->qty);
                            break;
                        }
                    }
                    
                    if (UNLIKELY(qty <= 0 || asks_.empty())) {
                        break;
                    }
                }
            } else {
                while (LIKELY(qty > 0 && !bids_.empty())) {
                    auto best_bid_it = bids_.begin();
                    PriceLevel& level = best_bid_it->second;
                    
                    while (LIKELY(qty > 0 && !level.empty())) {
                        Order* resting = level.get_front();
                        if (UNLIKELY(resting == nullptr || resting->qty <= 0)) {
                            level.remove_front();
                            continue;
                        }
                        
                        Quantity resting_qty_before = resting->qty;
                        Order market_order(id, Side::Sell, 0, qty);
                        match_orders_fast(&market_order, resting);
                        qty = market_order.qty;
                        
                        if (UNLIKELY(resting->qty <= 0)) {
                            order_pointers_.erase(resting->id);
                            level.remove_order(resting);
                            
                            if (UNLIKELY(level.empty())) {
                                bids_.erase(best_bid_it);
                                break;
                            }
                        } else {
                            level.update_qty(resting_qty_before, resting->qty);
                            break;
                        }
                    }
                    
                    if (UNLIKELY(qty <= 0 || bids_.empty())) {
                        break;
                    }
                }
            }
            break;
        }
        
        case MsgType::Cancel: {
            auto it = order_pointers_.find(msg.id);
            if (LIKELY(it != order_pointers_.end())) {
                Order* order = it->second;
                if (LIKELY(order != nullptr)) {
                    Price price = order->price;
                    
                    if (LIKELY(order->side == Side::Buy)) {
                        auto bid_it = bids_.find(price);
                        if (LIKELY(bid_it != bids_.end())) {
                            bid_it->second.remove_order(order);
                            if (UNLIKELY(bid_it->second.empty())) {
                                bids_.erase(bid_it);
                            }
                        }
                    } else {
                        auto ask_it = asks_.find(price);
                        if (LIKELY(ask_it != asks_.end())) {
                            ask_it->second.remove_order(order);
                            if (UNLIKELY(ask_it->second.empty())) {
                                asks_.erase(ask_it);
                            }
                        }
                    }
                }
                
                order_pointers_.erase(it);
            }
            break;
        }
    }
}

Price OrderBook::best_bid() const noexcept {
    return bids_.empty() ? 0 : bids_.begin()->first;
}

Price OrderBook::best_ask() const noexcept {
    return asks_.empty() ? 0 : asks_.begin()->first;
}

Quantity OrderBook::best_bid_qty() const noexcept {
    return bids_.empty() ? 0 : bids_.begin()->second.total_qty();
}

Quantity OrderBook::best_ask_qty() const noexcept {
    return asks_.empty() ? 0 : asks_.begin()->second.total_qty();
}

Quantity OrderBook::total_bid_qty() const {
    Quantity total = 0;
    for (const auto& [price, level] : bids_) {
        total += level.total_qty();
    }
    return total;
}

Quantity OrderBook::total_ask_qty() const {
    Quantity total = 0;
    for (const auto& [price, level] : asks_) {
        total += level.total_qty();
    }
    return total;
}

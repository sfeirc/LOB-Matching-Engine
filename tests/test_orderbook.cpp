#include "../include/OrderBook.h"
#include "../include/Message.h"
#include <cassert>
#include <iostream>
#include <chrono>

// Helper to create Msg with timestamp
Msg make_msg(MsgType type, Side side, uint64_t id, int64_t price, int64_t qty) {
    Msg msg;
    msg.type = type;
    msg.side = side;
    msg.id = id;
    msg.price = price;
    msg.qty = qty;
    msg.ts = std::chrono::steady_clock::now();
    return msg;
}

// Test 1: Basic limit order matching
void test_basic_matching() {
    OrderBook book;
    
    // Add buy order
    book.process_message(make_msg(MsgType::NewLimit, Side::Buy, 1, 100, 10));
    
    // Add sell order at same price - should match
    book.process_message(make_msg(MsgType::NewLimit, Side::Sell, 2, 100, 10));
    
    assert(book.get_total_trades() == 1);
    
    std::cout << "✓ test_basic_matching passed" << std::endl;
}

// Test 2: Partial fills across multiple levels
void test_partial_fills_multi_level() {
    OrderBook book;
    
    // Add multiple sell orders at different prices
    book.process_message(make_msg(MsgType::NewLimit, Side::Sell, 1, 100, 5));
    book.process_message(make_msg(MsgType::NewLimit, Side::Sell, 2, 101, 5));
    book.process_message(make_msg(MsgType::NewLimit, Side::Sell, 3, 102, 5));
    
    // Add large buy order that sweeps all three levels
    book.process_message(make_msg(MsgType::NewLimit, Side::Buy, 4, 105, 12));
    
    assert(book.get_total_trades() >= 3);
    
    std::cout << "✓ test_partial_fills_multi_level passed" << std::endl;
}

// Test 3: Cancel first order in FIFO queue
void test_cancel_first_fifo() {
    OrderBook book;
    
    book.process_message(make_msg(MsgType::NewLimit, Side::Buy, 1, 100, 10));
    book.process_message(make_msg(MsgType::NewLimit, Side::Buy, 2, 100, 10));
    book.process_message(make_msg(MsgType::NewLimit, Side::Buy, 3, 100, 10));
    
    assert(book.best_bid_qty() == 30);
    
    // Cancel first order
    book.process_message(make_msg(MsgType::Cancel, Side::Buy, 1, 0, 0));
    
    assert(book.best_bid_qty() == 20);
    
    std::cout << "✓ test_cancel_first_fifo passed" << std::endl;
}

// Test 4: Cancel middle order
void test_cancel_middle_fifo() {
    OrderBook book;
    
    book.process_message(make_msg(MsgType::NewLimit, Side::Buy, 1, 100, 10));
    book.process_message(make_msg(MsgType::NewLimit, Side::Buy, 2, 100, 10));
    book.process_message(make_msg(MsgType::NewLimit, Side::Buy, 3, 100, 10));
    
    book.process_message(make_msg(MsgType::Cancel, Side::Buy, 2, 0, 0));
    
    assert(book.best_bid_qty() == 20);
    
    std::cout << "✓ test_cancel_middle_fifo passed" << std::endl;
}

// Test 5: Cancel last order
void test_cancel_last_fifo() {
    OrderBook book;
    
    book.process_message(make_msg(MsgType::NewLimit, Side::Buy, 1, 100, 10));
    book.process_message(make_msg(MsgType::NewLimit, Side::Buy, 2, 100, 10));
    book.process_message(make_msg(MsgType::NewLimit, Side::Buy, 3, 100, 10));
    
    book.process_message(make_msg(MsgType::Cancel, Side::Buy, 3, 0, 0));
    
    assert(book.best_bid_qty() == 20);
    
    std::cout << "✓ test_cancel_last_fifo passed" << std::endl;
}

// Test 6: Insert at best price (no immediate cross)
void test_insert_at_best_price() {
    OrderBook book;
    
    book.process_message(make_msg(MsgType::NewLimit, Side::Buy, 1, 100, 10));
    book.process_message(make_msg(MsgType::NewLimit, Side::Buy, 2, 100, 10));
    
    assert(book.get_total_trades() == 0);
    assert(book.best_bid_qty() == 20);
    
    std::cout << "✓ test_insert_at_best_price passed" << std::endl;
}

// Test 7: Immediate cross
void test_immediate_cross() {
    OrderBook book;
    
    book.process_message(make_msg(MsgType::NewLimit, Side::Sell, 1, 100, 10));
    book.process_message(make_msg(MsgType::NewLimit, Side::Buy, 2, 105, 10));
    
    assert(book.get_total_trades() == 1);
    
    std::cout << "✓ test_immediate_cross passed" << std::endl;
}

// Test 8: Empty book market order
void test_empty_book_market_order() {
    OrderBook book;
    
    book.process_message(make_msg(MsgType::NewMarket, Side::Buy, 1, 0, 10));
    
    assert(book.get_total_trades() == 0);
    
    std::cout << "✓ test_empty_book_market_order passed" << std::endl;
}

int main() {
    std::cout << "Running OrderBook unit tests...\n" << std::endl;
    
    try {
        test_basic_matching();
        test_partial_fills_multi_level();
        test_cancel_first_fifo();
        test_cancel_middle_fifo();
        test_cancel_last_fifo();
        test_insert_at_best_price();
        test_immediate_cross();
        test_empty_book_market_order();
        
        std::cout << "\n✓ All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Test failed: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n✗ Test failed with unknown exception" << std::endl;
        return 1;
    }
}

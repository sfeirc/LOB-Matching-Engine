#include <iostream>
#include <fstream>
#include <random>
#include <chrono>
#include <iomanip>
#include <filesystem>
#include <cstring>
#include <cinttypes>

// Ultra-fast integer to string conversion (no allocations)
inline char* fast_itoa(uint64_t n, char* end) {
    char* start = end;
    do {
        *--start = '0' + (n % 10);
        n /= 10;
    } while (n > 0);
    return start;
}

inline char* fast_itoa_signed(int64_t n, char* end) {
    if (n < 0) {
        *--end = '-';
        n = -n;
    }
    return fast_itoa(static_cast<uint64_t>(n), end);
}

int main(int argc, char* argv[]) {
    // Configuration
    int64_t num_messages = 10'000'000;
    if (argc > 1) {
        num_messages = std::strtoll(argv[1], nullptr, 10);
        if (num_messages <= 0) num_messages = 10'000'000;
    }
    
    // Fast random number generator (xoshiro-style)
    uint64_t rng_state[4] = {42, 0x1234567890ABCDEFULL, 0xFEDCBA0987654321ULL, 0xABCDEF0123456789ULL};
    
    auto xoshiro_next = [&]() -> uint64_t {
        uint64_t result = rng_state[0] + rng_state[3];
        uint64_t t = rng_state[1] << 17;
        rng_state[2] ^= rng_state[0];
        rng_state[3] ^= rng_state[1];
        rng_state[1] ^= rng_state[2];
        rng_state[0] ^= rng_state[3];
        rng_state[2] ^= t;
        rng_state[3] = (rng_state[3] << 45) | (rng_state[3] >> 19);
        return result;
    };
    
    auto fast_random = [&](uint64_t max) -> uint64_t {
        return xoshiro_next() % max;
    };
    
    // Configuration
    const int64_t base_price = 100000;
    const int64_t start_ts = 1693526400000000000LL;
    
    // Pre-allocated order tracking (fixed-size array for speed)
    const size_t MAX_ORDERS = 100000;
    struct OrderSlot {
        uint64_t id;
        int8_t side;  // 0=Buy, 1=Sell
        int64_t price;
        bool valid;
    };
    OrderSlot* orders = new OrderSlot[MAX_ORDERS];
    size_t num_active_orders = 0;
    uint64_t next_order_id = 1;
    
    // Create output filename
    std::filesystem::create_directories("data");
    std::string filename = "data/large_dataset_" + std::to_string(num_messages / 1000) + "k.csv";
    
    std::cout << "Generating " << num_messages << " messages..." << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Open file with large buffer
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return 1;
    }
    
    // Write header
    file.write("# ts_ns,MsgType,Side,OrderId,Price,Qty\n", 38);
    
    // Huge buffer for maximum performance (128MB)
    const size_t buffer_size = 128 * 1024 * 1024;
    char* buffer = new char[buffer_size];
    size_t buffer_pos = 0;
    
    // Pre-allocated scratch space for number conversion (separate buffers for each use)
    char num_buf_ts[32];
    char num_buf_id[32];
    char num_buf_qty[32];
    char num_buf_price[32];
    const char* buy_str = "Buy";
    const char* sell_str = "Sell";
    const char* newlimit_str = "NewLimit";
    const char* newmarket_str = "NewMarket";
    const char* cancel_str = "Cancel";
    
    int64_t current_ts = start_ts;
    int64_t messages_generated = 0;
    
    // Pre-generate random values for batch processing
    const size_t BATCH_SIZE = 10000;
    uint64_t* rnd_batch = new uint64_t[BATCH_SIZE * 5];  // 5 values per message
    
    for (int64_t batch_start = 0; batch_start < num_messages; batch_start += BATCH_SIZE) {
        size_t batch_count = (batch_start + BATCH_SIZE < num_messages) ? BATCH_SIZE : (num_messages - batch_start);
        
        // Pre-generate all random numbers for this batch
        for (size_t i = 0; i < batch_count * 5; ++i) {
            rnd_batch[i] = xoshiro_next();
        }
        
        size_t rnd_idx = 0;
        
        for (size_t b = 0; b < batch_count; ++b) {
            // Increment timestamp
            current_ts += 1000 + (rnd_batch[rnd_idx++] % 1000000);
            
            // Message type: 70% NewLimit, 20% NewMarket, 10% Cancel
            uint64_t msg_type_roll = rnd_batch[rnd_idx++] % 100;
            uint8_t msg_type = (msg_type_roll < 70) ? 0 : (msg_type_roll < 90) ? 1 : 2;
            
            // Side
            uint8_t side_val = rnd_batch[rnd_idx++] % 2;
            const char* side_str = side_val ? sell_str : buy_str;
            size_t side_len = side_val ? 4 : 3;
            
            if (msg_type == 2 && num_active_orders > 0) {
                // Cancel
                uint64_t cancel_idx = rnd_batch[rnd_idx++] % num_active_orders;
                size_t found = 0;
                for (size_t i = 0; i < MAX_ORDERS; ++i) {
                    if (orders[i].valid) {
                        if (found == cancel_idx) {
                            uint64_t order_id = orders[i].id;
                            int8_t cancel_side = orders[i].side;
                            orders[i].valid = false;
                            num_active_orders--;
                            
                            // Format: timestamp,Cancel,side,order_id,0,0\n
                            char* p = buffer + buffer_pos;
                            
                            // Timestamp
                            char* ts_end = num_buf_ts + sizeof(num_buf_ts);
                            char* ts_start = fast_itoa(current_ts, ts_end);
                            size_t ts_len = ts_end - ts_start;
                            std::memcpy(p, ts_start, ts_len);
                            p += ts_len;
                            *p++ = ',';
                            
                            // "Cancel,"
                            std::memcpy(p, cancel_str, 6);
                            p += 6;
                            *p++ = ',';
                            
                            // Side
                            const char* cancel_side_str = cancel_side ? sell_str : buy_str;
                            size_t cancel_side_len = cancel_side ? 4 : 3;
                            std::memcpy(p, cancel_side_str, cancel_side_len);
                            p += cancel_side_len;
                            *p++ = ',';
                            
                            // Order ID
                            char* id_end = num_buf_id + sizeof(num_buf_id);
                            char* id_start = fast_itoa(order_id, id_end);
                            size_t id_len = id_end - id_start;
                            std::memcpy(p, id_start, id_len);
                            p += id_len;
                            *p++ = ',';
                            
                            // "0,0\n"
                            *p++ = '0';
                            *p++ = ',';
                            *p++ = '0';
                            *p++ = '\n';
                            
                            buffer_pos = p - buffer;
                            messages_generated++;
                            break;
                        }
                        found++;
                    }
                }
            } else if (msg_type == 1) {
                // NewMarket
                uint64_t order_id = next_order_id++;
                uint64_t qty = 1 + (rnd_batch[rnd_idx++] % 1000);
                
                // Format: timestamp,NewMarket,side,order_id,0,qty\n
                char* p = buffer + buffer_pos;
                
                // Timestamp
                char* ts_end = num_buf_ts + sizeof(num_buf_ts);
                char* ts_start = fast_itoa(current_ts, ts_end);
                size_t ts_len = ts_end - ts_start;
                std::memcpy(p, ts_start, ts_len);
                p += ts_len;
                *p++ = ',';
                
                // "NewMarket,"
                std::memcpy(p, newmarket_str, 9);
                p += 9;
                *p++ = ',';
                
                // Side
                std::memcpy(p, side_str, side_len);
                p += side_len;
                *p++ = ',';
                
                // Order ID
                char* id_end = num_buf_id + sizeof(num_buf_id);
                char* id_start = fast_itoa(order_id, id_end);
                size_t id_len = id_end - id_start;
                std::memcpy(p, id_start, id_len);
                p += id_len;
                *p++ = ',';
                
                // "0,"
                *p++ = '0';
                *p++ = ',';
                
                // Qty
                char* qty_end = num_buf_qty + sizeof(num_buf_qty);
                char* qty_start = fast_itoa(qty, qty_end);
                size_t qty_len = qty_end - qty_start;
                std::memcpy(p, qty_start, qty_len);
                p += qty_len;
                *p++ = '\n';
                
                buffer_pos = p - buffer;
                messages_generated++;
            } else {
                // NewLimit
                uint64_t order_id = next_order_id++;
                int64_t price = base_price + (rnd_batch[rnd_idx++] % 501);
                uint64_t qty = 1 + (rnd_batch[rnd_idx++] % 1000);
                
                // Store order (if space available)
                if (num_active_orders < MAX_ORDERS) {
                    for (size_t i = 0; i < MAX_ORDERS; ++i) {
                        if (!orders[i].valid) {
                            orders[i].id = order_id;
                            orders[i].side = side_val;
                            orders[i].price = price;
                            orders[i].valid = true;
                            num_active_orders++;
                            break;
                        }
                    }
                } else {
                    // Remove oldest 10%
                    size_t to_remove = num_active_orders / 10;
                    for (size_t i = 0, removed = 0; i < MAX_ORDERS && removed < to_remove; ++i) {
                        if (orders[i].valid) {
                            orders[i].valid = false;
                            num_active_orders--;
                            removed++;
                        }
                    }
                    // Add new order
                    for (size_t i = 0; i < MAX_ORDERS; ++i) {
                        if (!orders[i].valid) {
                            orders[i].id = order_id;
                            orders[i].side = side_val;
                            orders[i].price = price;
                            orders[i].valid = true;
                            num_active_orders++;
                            break;
                        }
                    }
                }
                
                // Format: timestamp,NewLimit,side,order_id,price,qty\n
                char* p = buffer + buffer_pos;
                
                // Timestamp
                char* ts_end = num_buf_ts + sizeof(num_buf_ts);
                char* ts_start = fast_itoa(current_ts, ts_end);
                size_t ts_len = ts_end - ts_start;
                std::memcpy(p, ts_start, ts_len);
                p += ts_len;
                *p++ = ',';
                
                // "NewLimit,"
                std::memcpy(p, newlimit_str, 8);
                p += 8;
                *p++ = ',';
                
                // Side
                std::memcpy(p, side_str, side_len);
                p += side_len;
                *p++ = ',';
                
                // Order ID
                char* id_end = num_buf_id + sizeof(num_buf_id);
                char* id_start = fast_itoa(order_id, id_end);
                size_t id_len = id_end - id_start;
                std::memcpy(p, id_start, id_len);
                p += id_len;
                *p++ = ',';
                
                // Price
                char* price_end = num_buf_price + sizeof(num_buf_price);
                char* price_start = fast_itoa(price, price_end);
                size_t price_len = price_end - price_start;
                std::memcpy(p, price_start, price_len);
                p += price_len;
                *p++ = ',';
                
                // Qty
                char* qty_end = num_buf_qty + sizeof(num_buf_qty);
                char* qty_start = fast_itoa(qty, qty_end);
                size_t qty_len = qty_end - qty_start;
                std::memcpy(p, qty_start, qty_len);
                p += qty_len;
                *p++ = '\n';
                
                buffer_pos = p - buffer;
                messages_generated++;
            }
            
            // Flush buffer when nearly full
            if (buffer_pos > buffer_size - 512) {
                file.write(buffer, buffer_pos);
                buffer_pos = 0;
            }
        }
    }
    
    // Flush remaining buffer
    if (buffer_pos > 0) {
        file.write(buffer, buffer_pos);
    }
    
    delete[] buffer;
    delete[] rnd_batch;
    delete[] orders;
    
    file.close();
    
    auto end_time = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    double elapsed_sec = elapsed / 1000.0;
    
    // Get file size
    std::ifstream size_check(filename, std::ios::binary | std::ios::ate);
    size_t file_size = size_check.tellg();
    
    double throughput = messages_generated / elapsed_sec;
    
    std::cout << "\nGenerated " << messages_generated << " messages in " 
              << elapsed << " ms (" << std::fixed << std::setprecision(2) << elapsed_sec << " s)" << std::endl;
    std::cout << "Throughput: " << std::fixed << std::setprecision(0) 
              << throughput << " messages/second" << std::endl;
    std::cout << "Output file: " << filename << std::endl;
    std::cout << "File size: " << std::fixed << std::setprecision(2) 
              << (file_size / (1024.0 * 1024.0)) << " MB" << std::endl;
    
    return 0;
}

#include "OrderBook.h"
#include "CSVReader.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <sstream>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/utsname.h>
#endif

// Get system info for benchmarking
std::string get_cpu_info() {
#ifdef _WIN32
    // Windows: Try to get CPU name from registry
    HKEY hKey;
    char cpuName[512] = "Unknown CPU";
    DWORD size = sizeof(cpuName);
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 
                      0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExA(hKey, "ProcessorNameString", nullptr, nullptr, (LPBYTE)cpuName, &size);
        RegCloseKey(hKey);
    }
    return std::string(cpuName);
#else
    FILE* file = fopen("/proc/cpuinfo", "r");
    if (!file) return "Unknown CPU";
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "model name", 10) == 0) {
            char* colon = strchr(line, ':');
            if (colon) {
                fclose(file);
                std::string name = colon + 1;
                // Trim whitespace
                while (!name.empty() && (name[0] == ' ' || name[0] == '\t')) name.erase(0, 1);
                while (!name.empty() && name.back() == '\n') name.pop_back();
                return name;
            }
        }
    }
    fclose(file);
    return "Unknown CPU";
#endif
}

std::string get_compiler_info() {
#ifdef __GNUC__
    return "gcc " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__) + 
           " -O3 -march=native -flto";
#elif defined(_MSC_VER)
    return "MSVC " + std::to_string(_MSC_VER);
#elif defined(__clang__)
    return "clang " + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__);
#else
    return "Unknown compiler";
#endif
}

struct Metrics {
    uint64_t events;
    double engine_time_ms;
    double throughput_mps;
    struct {
        double p50_us;
        double p95_us;
        double p99_us;
        double p999_us;
        double min_us;
        double max_us;
        double avg_us;
    } latency_us;
    std::string cpu;
    std::string compiler;
    std::string commit;
    double csv_read_ms;
};

void write_metrics_json(const Metrics& metrics, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Warning: Could not write metrics to " << filename << std::endl;
        return;
    }
    
    file << std::fixed << std::setprecision(2);
    file << "{\n";
    file << "  \"events\": " << metrics.events << ",\n";
    file << "  \"engine_time_ms\": " << metrics.engine_time_ms << ",\n";
    file << "  \"throughput_mps\": " << metrics.throughput_mps << ",\n";
    file << "  \"csv_read_ms\": " << metrics.csv_read_ms << ",\n";
    file << "  \"latency_us\": {\n";
    file << "    \"p50\": " << metrics.latency_us.p50_us << ",\n";
    file << "    \"p95\": " << metrics.latency_us.p95_us << ",\n";
    file << "    \"p99\": " << metrics.latency_us.p99_us << ",\n";
    file << "    \"p99.9\": " << metrics.latency_us.p999_us << ",\n";
    file << "    \"min\": " << metrics.latency_us.min_us << ",\n";
    file << "    \"max\": " << metrics.latency_us.max_us << ",\n";
    file << "    \"avg\": " << metrics.latency_us.avg_us << "\n";
    file << "  },\n";
    file << "  \"cpu\": \"" << metrics.cpu << "\",\n";
    file << "  \"compiler\": \"" << metrics.compiler << "\",\n";
    file << "  \"commit\": \"" << metrics.commit << "\",\n";
    file << "  \"single_threaded\": true\n";
    file << "}\n";
    file.close();
}

int main(int argc, char* argv[]) {
    std::string csv_file;
    std::string metrics_file;
    bool sample_latency = true;  // Sample 1/1000 messages for latency tracking
    
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--metrics") == 0 && i + 1 < argc) {
            metrics_file = argv[++i];
        } else if (strcmp(argv[i], "--no-latency") == 0) {
            sample_latency = false;
        } else if (csv_file.empty()) {
            csv_file = argv[i];
        }
    }
    
    if (csv_file.empty()) {
        std::cerr << "Usage: " << argv[0] << " <csv_file> [--metrics <json_file>] [--no-latency]" << std::endl;
        return 1;
    }
    
    // Read messages from CSV
    std::cout << "Reading messages from " << csv_file << "..." << std::endl;
    auto csv_start = std::chrono::steady_clock::now();
    auto messages = CSVReader::read_messages(csv_file);
    auto csv_end = std::chrono::steady_clock::now();
    auto csv_elapsed = std::chrono::duration_cast<std::chrono::microseconds>(csv_end - csv_start);
    double csv_read_ms = csv_elapsed.count() / 1000.0;
    
    if (messages.empty()) {
        std::cerr << "No messages loaded. Exiting." << std::endl;
        return 1;
    }
    
    std::cout << "Loaded " << messages.size() << " messages in " << csv_elapsed.count() 
              << " microseconds (" << std::fixed << std::setprecision(2) << csv_read_ms << " ms)." << std::endl;
    
    // Create order book
    OrderBook book;
    
    // Latency tracking: sample every Nth message for large datasets
    const size_t LATENCY_SAMPLE_RATE = 1000;  // Sample 1 in 1000 messages
    std::vector<uint64_t> latencies;
    bool track_latency = sample_latency;
    
    if (track_latency) {
        size_t expected_samples = (messages.size() < 1'000'000) 
            ? messages.size() 
            : messages.size() / LATENCY_SAMPLE_RATE + 1;
        latencies.reserve(expected_samples);
    }
    
    // ENGINE-ONLY TIMING: Time only the matching loop (separate from CSV I/O)
    auto engine_start = std::chrono::steady_clock::now();
    
    for (size_t i = 0; i < messages.size(); ++i) {
        const auto& msg = messages[i];
        
        // Sample latency for every message (small datasets) or every Nth (large datasets)
        bool should_sample = track_latency && (
            messages.size() <= 1'000'000 || (i % LATENCY_SAMPLE_RATE == 0)
        );
        
        auto msg_start = should_sample ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point();
        book.process_message(msg);
        auto msg_end = should_sample ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point();
        
        if (should_sample) {
            auto msg_latency = std::chrono::duration_cast<std::chrono::nanoseconds>(
                msg_end - msg_start).count();
            latencies.push_back(msg_latency);
        }
    }
    
    auto engine_end = std::chrono::steady_clock::now();
    auto engine_elapsed = std::chrono::duration_cast<std::chrono::microseconds>(engine_end - engine_start);
    double engine_time_ms = engine_elapsed.count() / 1000.0;
    
    // Collect trades
    auto trades = book.get_trades();
    
    // Calculate ENGINE-ONLY throughput (excluding CSV I/O)
    double engine_time_seconds = engine_time_ms / 1000.0;
    double throughput_mps = messages.size() / engine_time_seconds;
    
    // Get system info
    std::string cpu_info = get_cpu_info();
    std::string compiler_info = get_compiler_info();
    std::string commit_hash = "unknown";  // Could use git rev-parse
    
    // Print summary
    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total messages: " << book.get_total_messages() << std::endl;
    std::cout << "Total trades: " << book.get_total_trades() << std::endl;
    std::cout << "Best bid: " << book.best_bid() << " (qty: " << book.best_bid_qty() << ")" << std::endl;
    std::cout << "Best ask: " << book.best_ask() << " (qty: " << book.best_ask_qty() << ")" << std::endl;
    std::cout << "Total bid quantity: " << book.total_bid_qty() << std::endl;
    std::cout << "Total ask quantity: " << book.total_ask_qty() << std::endl;
    
    std::cout << "\n=== Performance (Engine-Only) ===" << std::endl;
    std::cout << "CSV Read time: " << std::fixed << std::setprecision(2) << csv_read_ms << " ms" << std::endl;
    std::cout << "Engine time: " << engine_time_ms << " ms" << std::endl;
    std::cout << "Throughput: " << std::setprecision(2) << throughput_mps << " messages/second" << std::endl;
    
    std::cout << "\n=== System Info ===" << std::endl;
    std::cout << "CPU: " << cpu_info << std::endl;
    std::cout << "Compiler: " << compiler_info << std::endl;
    std::cout << "Single-threaded: Yes" << std::endl;
    
    // Latency statistics
    Metrics metrics;
    metrics.events = messages.size();
    metrics.engine_time_ms = engine_time_ms;
    metrics.throughput_mps = throughput_mps;
    metrics.csv_read_ms = csv_read_ms;
    metrics.cpu = cpu_info;
    metrics.compiler = compiler_info;
    metrics.commit = commit_hash;
    
    if (track_latency && !latencies.empty()) {
        std::sort(latencies.begin(), latencies.end());
        
        size_t n = latencies.size();
        metrics.latency_us.p50_us = latencies[n * 50 / 100] / 1000.0;
        metrics.latency_us.p95_us = latencies[n * 95 / 100] / 1000.0;
        metrics.latency_us.p99_us = latencies[n * 99 / 100] / 1000.0;
        metrics.latency_us.p999_us = (n > 1000) ? latencies[n * 999 / 1000] / 1000.0 : latencies.back() / 1000.0;
        metrics.latency_us.min_us = latencies.front() / 1000.0;
        metrics.latency_us.max_us = latencies.back() / 1000.0;
        
        uint64_t sum = std::accumulate(latencies.begin(), latencies.end(), 0ULL);
        metrics.latency_us.avg_us = (sum / n) / 1000.0;
        
        std::cout << "\n=== Latency Statistics (microseconds) ===" << std::endl;
        std::cout << "Min:    " << std::fixed << std::setprecision(2) << metrics.latency_us.min_us << " µs" << std::endl;
        std::cout << "Avg:    " << metrics.latency_us.avg_us << " µs" << std::endl;
        std::cout << "P50:    " << metrics.latency_us.p50_us << " µs" << std::endl;
        std::cout << "P95:    " << metrics.latency_us.p95_us << " µs" << std::endl;
        std::cout << "P99:    " << metrics.latency_us.p99_us << " µs" << std::endl;
        if (n > 1000) {
            std::cout << "P99.9:  " << metrics.latency_us.p999_us << " µs" << std::endl;
        }
        std::cout << "Max:    " << metrics.latency_us.max_us << " µs" << std::endl;
        
        if (messages.size() > 1'000'000) {
            std::cout << "\nNote: Latency sampled at 1/" << LATENCY_SAMPLE_RATE 
                      << " rate (" << n << " samples)" << std::endl;
        }
    } else if (!track_latency) {
        std::cout << "\nNote: Latency tracking disabled (use --no-latency to suppress this message)" << std::endl;
        // Set default values for metrics
        metrics.latency_us.p50_us = metrics.latency_us.p95_us = metrics.latency_us.p99_us = 0.0;
        metrics.latency_us.p999_us = metrics.latency_us.min_us = metrics.latency_us.max_us = metrics.latency_us.avg_us = 0.0;
    }
    
    // Write metrics JSON if requested
    if (!metrics_file.empty()) {
        // Create results directory if needed
        size_t last_slash = metrics_file.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            std::string dir = metrics_file.substr(0, last_slash);
            #ifdef _WIN32
            system(("mkdir \"" + dir + "\" 2>nul").c_str());
            #else
            system(("mkdir -p \"" + dir + "\" 2>/dev/null").c_str());
            #endif
        }
        write_metrics_json(metrics, metrics_file);
        std::cout << "\nMetrics written to: " << metrics_file << std::endl;
    }
    
    // Optional: print first few trades
    if (!trades.empty()) {
        std::cout << "\n=== Sample Trades (first 10) ===" << std::endl;
        size_t count = std::min(static_cast<size_t>(10), trades.size());
        for (size_t i = 0; i < count; ++i) {
            const auto& trade = trades[i];
            auto ts_ms = std::chrono::duration_cast<std::chrono::microseconds>(
                trade.ts.time_since_epoch()).count();
            std::cout << "Trade: buy_id=" << trade.buy_id 
                      << ", sell_id=" << trade.sell_id
                      << ", price=" << trade.price
                      << ", qty=" << trade.qty
                      << ", ts=" << ts_ms << "us" << std::endl;
        }
        if (trades.size() > count) {
            std::cout << "... (" << (trades.size() - count) << " more trades)" << std::endl;
        }
    }
    
    return 0;
}

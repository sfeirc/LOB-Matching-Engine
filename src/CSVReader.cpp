#include "CSVReader.h"
#include <sstream>
#include <iostream>
#include <algorithm>

MsgType CSVReader::parse_msg_type(const std::string& s) {
    if (s == "NewLimit") return MsgType::NewLimit;
    if (s == "NewMarket") return MsgType::NewMarket;
    if (s == "Cancel") return MsgType::Cancel;
    return MsgType::NewLimit;  // default
}

Side CSVReader::parse_side(const std::string& s) {
    if (s == "Buy") return Side::Buy;
    if (s == "Sell") return Side::Sell;
    return Side::Buy;  // default
}

std::vector<Msg> CSVReader::read_messages(const std::string& filename) {
    std::vector<Msg> messages;
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return messages;
    }
    
    std::string line;
    bool first_line = true;
    
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Skip header line
        if (first_line && (line.find("ts_ns") != std::string::npos || 
                           line.find("MsgType") != std::string::npos)) {
            first_line = false;
            continue;
        }
        first_line = false;
        
        std::istringstream iss(line);
        std::string token;
        std::vector<std::string> tokens;
        
        while (std::getline(iss, token, ',')) {
            // Trim whitespace
            size_t start = token.find_first_not_of(" \t\n\r");
            if (start != std::string::npos) {
                size_t end = token.find_last_not_of(" \t\n\r");
                token = token.substr(start, end - start + 1);
            } else {
                token.clear();
            }
            tokens.push_back(token);
        }
        
        if (tokens.size() < 6) {
            continue;  // Skip malformed lines
        }
        
        Msg msg;
        try {
            // ts_ns,MsgType,Side,OrderId,Price,Qty
            // We ignore ts_ns for MVP logic (just process in order)
            msg.type = parse_msg_type(tokens[1]);
            msg.side = parse_side(tokens[2]);
            msg.id = std::stoull(tokens[3]);
            msg.price = std::stoll(tokens[4]);
            msg.qty = std::stoll(tokens[5]);
            msg.ts = std::chrono::steady_clock::now();
            
            messages.push_back(msg);
        } catch (const std::exception& e) {
            std::cerr << "Error parsing line: " << line << " - " << e.what() << std::endl;
            continue;
        }
    }
    
    file.close();
    return messages;
}


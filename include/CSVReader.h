#pragma once

#include <string>
#include <vector>
#include <fstream>
#include "Message.h"

class CSVReader {
public:
    static std::vector<Msg> read_messages(const std::string& filename);
    
private:
    static MsgType parse_msg_type(const std::string& s);
    static Side parse_side(const std::string& s);
};


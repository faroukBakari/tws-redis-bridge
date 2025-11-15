#pragma once

#include "MarketData.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <ctime>

/**
 * @brief Format Unix timestamp (ms) to ISO 8601 string
 * 
 * [PERFORMANCE] Simple implementation for MVP. For production, consider
 * C++20 std::format or Howard Hinnant's date library.
 */
inline std::string formatTimestamp(long timestampMs) {
    std::time_t seconds = timestampMs / 1000;
    int milliseconds = timestampMs % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&seconds), "%Y-%m-%dT%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << milliseconds << 'Z';
    
    return ss.str();
}

/**
 * @brief Serialize InstrumentState to JSON using RapidJSON Writer API
 * 
 * [PERFORMANCE] Uses SAX-style Writer (not DOM) for minimal allocations.
 * Target: 10-50μs per message.
 * 
 * @param state Complete instrument state snapshot
 * @return JSON string matching schema in docs/PROJECT-SPECIFICATION.md §3.4.2
 */
inline std::string serializeState(const InstrumentState& state) {
    using namespace rapidjson;
    
    StringBuffer buffer;
    Writer<StringBuffer> writer(buffer);
    
    writer.StartObject();
    
    // Metadata
    writer.Key("instrument");
    writer.String(state.symbol.c_str());
    
    writer.Key("conId");
    writer.Int(state.conId);
    
    // Use most recent timestamp (quote or trade)
    long latestTimestamp = std::max(state.quoteTimestamp, state.tradeTimestamp);
    writer.Key("timestamp");
    writer.Int64(latestTimestamp);
    
    // Price nested object
    writer.Key("price");
    writer.StartObject();
    writer.Key("bid"); writer.Double(state.bidPrice);
    writer.Key("ask"); writer.Double(state.askPrice);
    writer.Key("last"); writer.Double(state.lastPrice);
    writer.EndObject();
    
    // Size nested object
    writer.Key("size");
    writer.StartObject();
    writer.Key("bid"); writer.Int(state.bidSize);
    writer.Key("ask"); writer.Int(state.askSize);
    writer.Key("last"); writer.Int(state.lastSize);
    writer.EndObject();
    
    // Separate timestamps for quote vs trade
    writer.Key("timestamps");
    writer.StartObject();
    writer.Key("quote"); writer.Int64(state.quoteTimestamp);
    writer.Key("trade"); writer.Int64(state.tradeTimestamp);
    writer.EndObject();
    
    // Exchange
    writer.Key("exchange");
    writer.String(state.exchange.c_str());
    
    // Tick attributes
    writer.Key("tickAttrib");
    writer.StartObject();
    writer.Key("pastLimit"); writer.Bool(state.pastLimit);
    writer.EndObject();
    
    writer.EndObject();
    
    return buffer.GetString();
}

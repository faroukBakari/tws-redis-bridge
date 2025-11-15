#pragma once

#include <string>
#include <chrono>

/**
 * @brief Tick update types from TWS API callbacks
 */
enum class TickUpdateType {
    BidAsk,   // tickByTickBidAsk callback
    AllLast   // tickByTickAllLast callback
};

/**
 * @brief Normalized tick update structure
 * 
 * This struct is enqueued from EWrapper callbacks (Thread 2) to 
 * the lock-free queue for processing by the Redis Worker (Thread 3).
 * 
 * [PERFORMANCE] Keep this lightweight - only essential fields.
 */
struct TickUpdate {
    int tickerId = 0;
    TickUpdateType type = TickUpdateType::BidAsk;
    long timestamp = 0;  // Unix timestamp (ms) from TWS
    
    // BidAsk fields
    double bidPrice = 0.0;
    double askPrice = 0.0;
    int bidSize = 0;
    int askSize = 0;
    
    // AllLast fields
    double lastPrice = 0.0;
    int lastSize = 0;
    bool pastLimit = false;
};

/**
 * @brief Complete instrument state (aggregated from partial updates)
 * 
 * This is maintained by the Redis Worker thread and published as complete
 * JSON snapshots to Redis.
 * 
 * [ARCHITECTURE] Complete snapshots simplify downstream consumers (no merge logic).
 */
struct InstrumentState {
    std::string symbol;
    int conId = 0;
    int tickerId = 0;
    
    // Quote data (from tickByTickBidAsk)
    double bidPrice = 0.0;
    double askPrice = 0.0;
    int bidSize = 0;
    int askSize = 0;
    long quoteTimestamp = 0;
    bool hasQuote = false;
    
    // Trade data (from tickByTickAllLast)
    double lastPrice = 0.0;
    int lastSize = 0;
    long tradeTimestamp = 0;
    bool hasTrade = false;
    
    // Attributes
    std::string exchange;
    bool pastLimit = false;
};

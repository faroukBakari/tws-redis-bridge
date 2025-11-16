// main.cpp - Entry point for TWS-Redis Bridge
// ARCHITECTURE: Producer-Consumer pattern with lock-free queue

#include "TwsClient.h"
#include "RedisPublisher.h"
#include "Serialization.h"
#include "MarketData.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include <chrono>
#include <concurrentqueue.h>

using namespace tws_bridge;

// REASON: Global flag for graceful shutdown on SIGINT/SIGTERM
static std::atomic<bool> g_running{true};

void signalHandler(int signal) {
    std::cout << "\n[MAIN] Received signal " << signal << ", shutting down...\n";
    g_running.store(false);
}

// REASON: Consumer thread - dequeues TickUpdates, aggregates state, publishes to Redis
void redisWorkerLoop(moodycamel::ConcurrentQueue<TickUpdate>& queue, 
                     RedisPublisher& redis,
                     std::atomic<bool>& running) {
    std::cout << "[WORKER] Redis worker thread started\n";
    
    // REASON: InstrumentState map for aggregating BidAsk + AllLast updates
    std::unordered_map<std::string, InstrumentState> stateMap;
    
    TickUpdate update;
    while (running.load()) {
        // REASON: Non-blocking dequeue with short timeout for responsive shutdown
        if (queue.try_dequeue(update)) {
            // REASON: Get symbol from tickerId map (need to pass from TwsClient)
            // For now, derive symbol from tickerId (temporary hack)
            std::string symbol = "UNKNOWN";
            if (update.tickerId == 1001 || update.tickerId == 11001) symbol = "AAPL";
            else if (update.tickerId == 1002 || update.tickerId == 11002) symbol = "SPY";
            else if (update.tickerId == 1003 || update.tickerId == 11003) symbol = "TSLA";
            else if (update.tickerId == 2001) symbol = "SPY";  // Historical bars
            else if (update.tickerId == 3001) symbol = "SPY";  // Real-time bars
            
            // PERFORMANCE: State aggregation logic (merge BidAsk + AllLast)
            auto& state = stateMap[symbol];
            state.symbol = symbol;
            state.tickerId = update.tickerId;
            
            if (update.type == TickUpdateType::BidAsk) {
                state.bidPrice = update.bidPrice;
                state.askPrice = update.askPrice;
                state.bidSize = update.bidSize;
                state.askSize = update.askSize;
                state.quoteTimestamp = update.timestamp;
                state.hasQuote = true;
            } else if (update.type == TickUpdateType::AllLast) {
                state.lastPrice = update.lastPrice;
                state.lastSize = update.lastSize;
                state.tradeTimestamp = update.timestamp;
                state.hasTrade = true;
                state.pastLimit = update.pastLimit;
            } else if (update.type == TickUpdateType::Bar) {
                // PIVOT: Bar data handling for Gate 2a-2c testing
                std::cout << "[WORKER] Bar: " << symbol 
                          << " | O: " << update.open << " H: " << update.high
                          << " L: " << update.low << " C: " << update.close
                          << " V: " << update.volume << "\n";
                
                // Publish bar data immediately (no aggregation needed)
                try {
                    std::string json = serializeBarData(symbol, update);
                    
                    std::string channel = "TWS:BARS:" + symbol;
                    redis.publish(channel, json);
                    std::cout << "[WORKER] Published bar to " << channel << "\n";
                } catch (const std::exception& e) {
                    std::cerr << "[WORKER] Redis publish error: " << e.what() << "\n";
                }
                continue;  // Skip tick aggregation logic
            }
            
            // REASON: Only publish when we have both BidAsk AND AllLast
            if (state.hasQuote && state.hasTrade) {
                try {
                    std::string json = serializeState(state);
                    std::string channel = "TWS:TICKS:" + symbol;
                    redis.publish(channel, json);
                    
                    // PERFORMANCE: Minimal logging (can be removed in production)
                    std::cout << "[WORKER] Published: " << symbol 
                              << " | Bid: " << state.bidPrice 
                              << " | Ask: " << state.askPrice 
                              << " | Last: " << state.lastPrice << "\n";
                } catch (const std::exception& e) {
                    std::cerr << "[WORKER] Redis publish error: " << e.what() << "\n";
                }
            }
        } else {
            // REASON: Yield CPU when queue is empty (avoid busy-wait)
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
    
    std::cout << "[WORKER] Redis worker thread stopped\n";
}

int main(int argc, char* argv[]) {
    (void)argc;  // REASON: Unused parameters
    (void)argv;
    // REASON: Suppress unused parameter warnings for future CLI argument support
    (void)argc;
    (void)argv;
    
    std::cout << "=== TWS-Redis Bridge v0.1.0 ===\n";
    
    // Signal handlers for graceful shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Configuration (TODO: Move to config file)
    const std::string TWS_HOST = "127.0.0.1";
    const unsigned int TWS_PORT = 7497;  // Paper trading port
    const int CLIENT_ID = 1;
    const std::string REDIS_URI = "tcp://127.0.0.1:6379";
    
    try {
        // REASON: Lock-free queue for producer (TWS callbacks) → consumer (Redis worker)
        moodycamel::ConcurrentQueue<TickUpdate> queue(10000);  // Pre-allocate 10K slots
        
        // Initialize Redis publisher
        std::cout << "[MAIN] Connecting to Redis at " << REDIS_URI << "\n";
        RedisPublisher redis(REDIS_URI);
        
        if (!redis.isConnected()) {
            std::cerr << "[MAIN] Failed to connect to Redis\n";
            return 1;
        }
        std::cout << "[MAIN] Redis connected\n";
        
        // ========== THREAD 2: Start Redis Worker Thread ==========
        // Consumes from lock-free queue, publishes to Redis
        std::thread workerThread(redisWorkerLoop, std::ref(queue), std::ref(redis), std::ref(g_running));
        
        // ========== THREAD 3: Connect to TWS (starts EReader thread) ==========
        std::cout << "[MAIN] Connecting to TWS Gateway at " << TWS_HOST << ":" << TWS_PORT << "\n";
        TwsClient client(queue);
        
        // NOTE: client.connect() internally calls m_reader->start() which spawns Thread 3 (EReader)
        if (!client.connect(TWS_HOST, TWS_PORT, CLIENT_ID)) {
            std::cerr << "[MAIN] Failed to connect to TWS Gateway\n";
            g_running.store(false);
            workerThread.join();
            return 1;
        }
        std::cout << "[MAIN] TWS connected (EReader thread now running)\n";
        
        // Wait for nextValidId callback (confirms connection)
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // ========== PIVOT: Use historical bars instead of tick-by-tick (markets closed) ==========
        std::cout << "[MAIN] Subscribing to historical bar data (markets closed)...\n";
        std::cout << "[MAIN] Requesting 5-minute bars for last 1 hour\n";
        client.subscribeHistoricalBars("SPY", 2001, "3600 S", "5 mins");  // 1 hour of 5-min bars
        
        // Wait for historical data to complete
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        // ========== DAY 1 EVENING: Real-Time Bars (Gate 3a) ==========
        std::cout << "[MAIN] Subscribing to real-time bars (5-second updates)...\n";
        client.subscribeRealTimeBars("SPY", 3001, 5, "TRADES");  // 5-second bars
        
        // ========== THREAD 1: Main Thread Message Loop ==========
        // Thread 3 (EReader) reads socket → signals Thread 1 → callbacks enqueue to Thread 2
        std::cout << "[MAIN] Entering message processing loop...\n";
        std::cout << "[MAIN] Thread architecture:\n";
        std::cout << "  Thread 1 (Main):    Processes TWS messages, calls EWrapper callbacks\n";
        std::cout << "  Thread 2 (Worker):  Dequeues updates, publishes to Redis\n";
        std::cout << "  Thread 3 (EReader): TWS API internal socket reader\n\n";
        
        // REASON: Use separate thread for TWS message processing to avoid blocking on waitForSignal()
        // This allows main thread to respond to signals immediately
        std::thread msgThread([&client]() {
            while (g_running.load() && client.isConnected()) {
                client.processMessages();
            }
            std::cout << "[MSG] Message processing thread stopped\n";
        });
        
        // REASON: Main thread just monitors shutdown flag
        while (g_running.load() && client.isConnected()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // REASON: Clean shutdown sequence
        std::cout << "[MAIN] Disconnecting from TWS...\n";
        client.disconnect();
        
        std::cout << "[MAIN] Waiting for message thread...\n";
        if (msgThread.joinable()) {
            msgThread.join();
        }
        
        std::cout << "[MAIN] Waiting for worker thread...\n";
        workerThread.join();
        
        std::cout << "[MAIN] Shutdown complete\n";
        
    } catch (const std::exception& e) {
        std::cerr << "[MAIN] Fatal error: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}

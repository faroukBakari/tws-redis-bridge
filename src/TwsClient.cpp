// TwsClient.cpp - TWS API bidirectional adapter implementation
// Implements both sides of TWS communication:
// 1. Outbound: Send commands to TWS (via EClientSocket)
// 2. Inbound: Receive callbacks from TWS (via EWrapper interface)

#include "TwsClient.h"
#include "EClientSocket.h"
#include "Contract.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace tws_bridge {

TwsClient::TwsClient(moodycamel::ConcurrentQueue<TickUpdate>& queue)
    : m_queue(queue)
    , m_signal(std::make_unique<EReaderOSSignal>())
    , m_client(std::make_unique<EClientSocket>(this, m_signal.get())) {
    // NOTE: "this" pointer passed to EClientSocket - TWS stores it and calls our callbacks
}

TwsClient::~TwsClient() {
    disconnect();
}

// ========== Outbound API: Commands we send TO TWS ==========

bool TwsClient::connect(const std::string& host, unsigned int port, int clientId) {
    std::cout << "[TWS] Attempting connection to " << host << ":" << port << "\n";
    
    bool success = m_client->eConnect(host.c_str(), port, clientId, false);
    if (!success) {
        std::cerr << "[TWS] eConnect failed\n";
        return false;
    }
    
    // ========== START THREAD 3: EReader Socket Reader ==========
    // This spawns a new thread that reads from TWS socket and signals main thread
    m_reader = std::make_unique<EReader>(m_client.get(), m_signal.get());
    m_reader->start();  // <-- Internal std::thread created here by TWS API
    m_connected.store(true);
    
    std::cout << "[TWS] Connection established, EReader thread started\n";
    return true;
}

void TwsClient::disconnect() {
    if (m_connected.load()) {
        std::cout << "[TWS] Disconnecting...\n";
        m_connected.store(false);
        m_client->eDisconnect();
    }
}

bool TwsClient::isConnected() const {
    return m_connected.load() && m_client->isConnected();
}

void TwsClient::subscribeTickByTick(const std::string& symbol, int tickerId) {
    std::cout << "[TWS] Subscribing to tick-by-tick for " << symbol << " (tickerId=" << tickerId << ")\n";
    
    // Store tickerId → symbol mapping for callback routing
    m_tickerToSymbol[tickerId] = symbol;
    
    // Create stock contract for US equities
    Contract contract;
    contract.symbol = symbol;
    contract.secType = "STK";
    contract.exchange = "SMART";
    contract.currency = "USD";
    
    // Subscribe to both BidAsk and AllLast tick types
    // Convention: BidAsk uses base tickerId, AllLast uses tickerId + 10000
    m_client->reqTickByTickData(tickerId, contract, "BidAsk", 0, true);
    m_client->reqTickByTickData(tickerId + 10000, contract, "AllLast", 0, true);
    
    // Store second tickerId mapping for AllLast
    m_tickerToSymbol[tickerId + 10000] = symbol;
}

void TwsClient::subscribeHistoricalBars(const std::string& symbol, int tickerId,
                                         const std::string& duration,
                                         const std::string& barSize) {
    std::cout << "[TWS] Subscribing to historical bars for " << symbol 
              << " (tickerId=" << tickerId << ", duration=" << duration 
              << ", barSize=" << barSize << ")\n";
    
    // Store tickerId → symbol mapping
    m_tickerToSymbol[tickerId] = symbol;
    
    // Create stock contract
    Contract contract;
    contract.symbol = symbol;
    contract.secType = "STK";
    contract.exchange = "SMART";
    contract.currency = "USD";
    
    // Request historical data
    // Parameters: tickerId, contract, endDateTime (empty=now), duration, barSize, 
    //             whatToShow, useRTH, formatDate, keepUpToDate, chartOptions
    m_client->reqHistoricalData(tickerId, contract, "", duration, barSize, 
                                 "TRADES", 1, 1, false, TagValueListSPtr());
}

void TwsClient::subscribeRealTimeBars(const std::string& symbol, int tickerId,
                                       int barSize, const std::string& whatToShow) {
    std::cout << "[TWS] Subscribing to real-time bars for " << symbol 
              << " (tickerId=" << tickerId << ", barSize=" << barSize 
              << "s, whatToShow=" << whatToShow << ")\n";
    
    // Store tickerId → symbol mapping
    m_tickerToSymbol[tickerId] = symbol;
    
    // Create stock contract
    Contract contract;
    contract.symbol = symbol;
    contract.secType = "STK";
    contract.exchange = "SMART";
    contract.currency = "USD";
    
    // Request real-time bars
    // Parameters: tickerId, contract, barSize (seconds: 5 only), whatToShow, useRTH, realTimeBarsOptions
    // NOTE: TWS only supports 5-second bars for real-time
    m_client->reqRealTimeBars(tickerId, contract, barSize, whatToShow, true, TagValueListSPtr());
}

void TwsClient::processMessages() {
    if (!isConnected() || !m_reader) {
        return;
    }
    
    // REASON: Check if messages are available, process them
    // waitForSignal() blocks, but we'll handle timeout at main loop level
    m_signal->waitForSignal();
    
    if (isConnected() && m_reader) {
        m_reader->processMsgs();
    }
}

// ========== Inbound API: Callbacks TWS invokes ON us ==========

void TwsClient::connectAck() {
    std::cout << "[TWS] Connection acknowledged\n";
}

// ========== Critical Callbacks: Market Data ==========

void TwsClient::nextValidId(OrderId orderId) {
    std::cout << "[TWS] nextValidId: " << orderId << " (connection confirmed)\n";
    m_nextValidOrderId.store(orderId);
}

void TwsClient::connectionClosed() {
    std::cout << "[TWS] Connection closed by server\n";
    m_connected.store(false);
}

void TwsClient::error(int id, time_t errorTime, int errorCode, const std::string& errorString, 
                      const std::string& advancedOrderRejectJson) {
    (void)errorTime;  // Unused in MVP
    (void)advancedOrderRejectJson;
    
    // Filter informational messages (TWS connection status codes)
    if (errorCode == 2104 || errorCode == 2106 || errorCode == 2158) {
        std::cout << "[TWS] Info [" << errorCode << "]: " << errorString << "\n";
    } else {
        std::cerr << "[TWS] Error [" << errorCode << "] (id=" << id << "): " << errorString << "\n";
    }
}

void TwsClient::tickByTickBidAsk(int reqId, time_t time, double bidPrice, double askPrice,
                                 Decimal bidSize, Decimal askSize, 
                                 const TickAttribBidAsk& tickAttribBidAsk) {
    (void)tickAttribBidAsk;  // MVP doesn't use bid/ask attributes
    
    // Look up symbol from tickerId
    auto it = m_tickerToSymbol.find(reqId);
    if (it == m_tickerToSymbol.end()) {
        std::cerr << "[TWS] Unknown tickerId: " << reqId << "\n";
        return;
    }
    
    // CRITICAL PATH: Construct update on stack, enqueue without heap allocation
    TickUpdate update;
    update.tickerId = reqId;
    update.type = TickUpdateType::BidAsk;
    update.timestamp = time * 1000;  // Convert to milliseconds
    update.bidPrice = bidPrice;
    update.askPrice = askPrice;
    update.bidSize = static_cast<int>(bidSize);
    update.askSize = static_cast<int>(askSize);
    
    // CRITICAL: Non-blocking enqueue, target < 1μs
    if (!m_queue.try_enqueue(update)) {
        std::cerr << "[TWS] Queue full! Dropping BidAsk update\n";
    }
}

void TwsClient::tickByTickAllLast(int reqId, int tickType, time_t time, double price,
                                  Decimal size, const TickAttribLast& tickAttribLast,
                                  const std::string& exchange, const std::string& specialConditions) {
    (void)tickType;
    (void)exchange;
    (void)specialConditions;
    auto it = m_tickerToSymbol.find(reqId);
    if (it == m_tickerToSymbol.end()) {
        std::cerr << "[TWS] Unknown tickerId: " << reqId << "\n";
        return;
    }
    
    TickUpdate update;
    update.tickerId = reqId;
    update.type = TickUpdateType::AllLast;
    update.timestamp = time * 1000;  // Convert to milliseconds
    update.lastPrice = price;
    update.lastSize = static_cast<int>(size);
    update.pastLimit = tickAttribLast.pastLimit;
    
    if (!m_queue.try_enqueue(update)) {
        std::cerr << "[TWS] Queue full! Dropping AllLast update\n";
    }
}

// ========== Unused Callbacks (stub implementations) ==========

void TwsClient::tickPrice(TickerId tickerId, TickType field, double price, const TickAttrib& attribs) {
    (void)tickerId; (void)field; (void)price; (void)attribs;
    // Not used in tick-by-tick mode
}

void TwsClient::tickSize(TickerId tickerId, TickType field, Decimal size) {
    (void)tickerId; (void)field; (void)size;
    // Not used in tick-by-tick mode
}

void TwsClient::tickString(TickerId tickerId, TickType tickType, const std::string& value) {
    (void)tickerId; (void)tickType; (void)value;
    // Not used in tick-by-tick mode
}

void TwsClient::historicalData(TickerId reqId, const Bar& bar) {
    // Look up symbol from tickerId
    auto it = m_tickerToSymbol.find(reqId);
    if (it == m_tickerToSymbol.end()) {
        std::cerr << "[TWS] Unknown tickerId in historicalData: " << reqId << "\n";
        return;
    }
    
    // Parse bar timestamp from TWS format (YYYYMMDD HH:MM:SS)
    // For MVP, use current time (TODO: parse bar.time string properly)
    long timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    // Construct bar update
    TickUpdate update;
    update.tickerId = reqId;
    update.type = TickUpdateType::Bar;
    update.timestamp = timestamp;
    update.open = bar.open;
    update.high = bar.high;
    update.low = bar.low;
    update.close = bar.close;
    update.volume = bar.volume;
    update.wap = bar.wap;
    update.barCount = bar.count;
    
    // Enqueue bar data
    if (!m_queue.try_enqueue(update)) {
        std::cerr << "[TWS] Queue full! Dropping bar update\n";
    } else {
        std::cout << "[TWS] Historical bar: " << it->second 
                  << " | O: " << bar.open << " H: " << bar.high 
                  << " L: " << bar.low << " C: " << bar.close 
                  << " V: " << bar.volume << "\n";
    }
}

void TwsClient::realtimeBar(TickerId reqId, long time, double open, double high, double low, 
                             double close, Decimal volume, Decimal wap, int count) {
    // Look up symbol from tickerId
    auto it = m_tickerToSymbol.find(reqId);
    if (it == m_tickerToSymbol.end()) {
        std::cerr << "[TWS] Unknown tickerId in realtimeBar: " << reqId << "\n";
        return;
    }
    
    // Convert TWS timestamp (Unix epoch seconds) to milliseconds
    long timestamp = time * 1000;
    
    // Construct bar update
    TickUpdate update;
    update.tickerId = reqId;
    update.type = TickUpdateType::Bar;
    update.timestamp = timestamp;
    update.open = open;
    update.high = high;
    update.low = low;
    update.close = close;
    update.volume = volume;
    update.wap = wap;
    update.barCount = count;
    
    // Enqueue real-time bar data
    if (!m_queue.try_enqueue(update)) {
        std::cerr << "[TWS] Queue full! Dropping real-time bar update\n";
    } else {
        std::cout << "[TWS] Real-time bar: " << it->second 
                  << " | O: " << open << " H: " << high 
                  << " L: " << low << " C: " << close 
                  << " V: " << volume << "\n";
    }
}

void TwsClient::historicalDataEnd(int reqId, const std::string& startDateStr, 
                                   const std::string& endDateStr) {
    std::cout << "[TWS] Historical data complete for reqId=" << reqId 
              << " (start=" << startDateStr << ", end=" << endDateStr << ")\n";
}

} // namespace tws_bridge

// TwsClient.cpp - Implementation of TWS API wrapper

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
}

TwsClient::~TwsClient() {
    disconnect();
}

bool TwsClient::connect(const std::string& host, unsigned int port, int clientId) {
    std::cout << "[TWS] Attempting connection to " << host << ":" << port << "\n";
    
    bool success = m_client->eConnect(host.c_str(), port, clientId, false);
    if (!success) {
        std::cerr << "[TWS] eConnect failed\n";
        return false;
    }
    
    // REASON: TWS API requires EReader thread for message processing
    m_reader = std::make_unique<EReader>(m_client.get(), m_signal.get());
    m_reader->start();
    m_connected.store(true);
    
    std::cout << "[TWS] Connection established, EReader started\n";
    return true;
}

void TwsClient::disconnect() {
    if (m_connected.load()) {
        std::cout << "[TWS] Disconnecting...\n";
        m_client->eDisconnect();
        m_connected.store(false);
    }
}

bool TwsClient::isConnected() const {
    return m_connected.load() && m_client->isConnected();
}

void TwsClient::subscribeTickByTick(const std::string& symbol, int tickerId) {
    std::cout << "[TWS] Subscribing to tick-by-tick for " << symbol << " (tickerId=" << tickerId << ")\n";
    
    // REASON: Store tickerId → symbol mapping for callback routing
    m_tickerToSymbol[tickerId] = symbol;
    
    // REASON: Create stock contract for US equities
    Contract contract;
    contract.symbol = symbol;
    contract.secType = "STK";
    contract.exchange = "SMART";
    contract.currency = "USD";
    
    // REASON: Subscribe to both BidAsk and AllLast tick types
    // BidAsk uses base tickerId, AllLast uses tickerId + 10000
    m_client->reqTickByTickData(tickerId, contract, "BidAsk", 0, true);
    m_client->reqTickByTickData(tickerId + 10000, contract, "AllLast", 0, true);
    
    // REASON: Store second tickerId mapping for AllLast
    m_tickerToSymbol[tickerId + 10000] = symbol;
}

void TwsClient::processMessages() {
    // REASON: EReader signals when messages are available
    m_signal->waitForSignal();
    m_reader->processMsgs();
}

void TwsClient::connectAck() {
    std::cout << "[TWS] Connection acknowledged\n";
}

// ========== EWrapper Callback Implementations ==========

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
    (void)errorTime;  // REASON: Unused parameter, keep for API compatibility
    (void)advancedOrderRejectJson;  // REASON: Not used in this implementation
    // REASON: Filter informational messages (code 2104, 2106, 2158)
    if (errorCode == 2104 || errorCode == 2106 || errorCode == 2158) {
        std::cout << "[TWS] Info [" << errorCode << "]: " << errorString << "\n";
    } else {
        std::cerr << "[TWS] Error [" << errorCode << "] (id=" << id << "): " << errorString << "\n";
    }
}

void TwsClient::tickByTickBidAsk(int reqId, time_t time, double bidPrice, double askPrice,
                                 Decimal bidSize, Decimal askSize, 
                                 const TickAttribBidAsk& tickAttribBidAsk) {
    (void)tickAttribBidAsk;  // REASON: Not used in this implementation
    // REASON: Look up symbol from tickerId
    auto it = m_tickerToSymbol.find(reqId);
    if (it == m_tickerToSymbol.end()) {
        std::cerr << "[TWS] Unknown tickerId: " << reqId << "\n";
        return;
    }
    
    // PERFORMANCE: Construct TickUpdate on stack, then enqueue (zero-copy)
    TickUpdate update;
    update.tickerId = reqId;
    update.type = TickUpdateType::BidAsk;
    update.timestamp = time * 1000;  // Convert to milliseconds
    update.bidPrice = bidPrice;
    update.askPrice = askPrice;
    update.bidSize = static_cast<int>(bidSize);
    update.askSize = static_cast<int>(askSize);
    
    // CRITICAL: Enqueue must be < 1μs (non-blocking)
    if (!m_queue.try_enqueue(update)) {
        std::cerr << "[TWS] Queue full! Dropping BidAsk update\n";
    }
}

void TwsClient::tickByTickAllLast(int reqId, int tickType, time_t time, double price,
                                  Decimal size, const TickAttribLast& tickAttribLast,
                                  const std::string& exchange, const std::string& specialConditions) {
    (void)tickType;  // REASON: Not used in this implementation
    (void)exchange;  // REASON: Not used in this implementation  
    (void)specialConditions;  // REASON: Not used in this implementation
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

// REASON: Unused callbacks for standard market data (not tick-by-tick)
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

} // namespace tws_bridge

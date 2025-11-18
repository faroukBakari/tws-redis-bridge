// TwsClient.h - TWS API bidirectional adapter
// SCOPE: Manages TWS connection, subscriptions, and callbacks
//
// ARCHITECTURE CLARIFICATION:
// - Inherits EWrapper (callback interface) - TWS pushes data TO us via callbacks
// - Wraps EClientSocket (command interface) - WE send requests TO TWS
// - Despite the name "EWrapper", it's a CALLBACK HANDLER, not a wrapper
// - This class is the bridge between TWS API threading model and our lock-free queue

#pragma once

#include "EWrapper.h"
#include "EReader.h"
#include "IErrorHandler.h"
#include "EReaderOSSignal.h"
#include "MarketData.h"
#include <memory>
#include <string>
#include <atomic>
#include <unordered_map>
#include <concurrentqueue.h>

class EClientSocket;

namespace tws_bridge {

// Bidirectional TWS adapter: implements callbacks (EWrapper) + manages connection (EClientSocket)
class TwsClient : public IErrorHandler {
public:
    explicit TwsClient(moodycamel::ConcurrentQueue<TickUpdate>& queue);
    ~TwsClient();

    // ========== Outbound API: Commands WE send TO TWS ==========
    bool createConnection(const std::string& host, unsigned int port, int clientId);
    void disconnect();
    bool isConnected() const;
    void subscribeTickByTick(const std::string& symbol, int tickerId);
    void subscribeHistoricalBars(const std::string& symbol, int tickerId, 
                                  const std::string& duration = "1 D", 
                                  const std::string& barSize = "5 mins");
    void subscribeRealTimeBars(const std::string& symbol, int tickerId, 
                                int barSize = 5, 
                                const std::string& whatToShow = "TRADES");
    
    // Message processing loop (dispatches callbacks from EReader thread)
    void processMessages();

    // ========== Inbound API: Callbacks TWS invokes ON us (EWrapper interface) ==========
    // These are called by EReader thread when TWS sends us data
    void tickPrice(TickerId tickerId, TickType field, double price, const TickAttrib& attribs);
    void tickSize(TickerId tickerId, TickType field, Decimal size);
    void tickString(TickerId tickerId, TickType tickType, const std::string& value);
    void tickByTickBidAsk(int reqId, time_t time, double bidPrice, double askPrice, 
                          Decimal bidSize, Decimal askSize, const TickAttribBidAsk& tickAttribBidAsk);
    void tickByTickAllLast(int reqId, int tickType, time_t time, double price, 
                           Decimal size, const TickAttribLast& tickAttribLast, 
                           const std::string& exchange, const std::string& specialConditions);
    
    void nextValidId(OrderId orderId);
    void connectionClosed();
    void error(int id, time_t errorTime, int errorCode, const std::string& errorString, const std::string& advancedOrderRejectJson);
    void connectAck();
    
    // ========== Unused EWrapper callbacks (stub implementations) ==========
    // TWS API requires implementing 90+ callbacks, most unused for tick-by-tick
    void tickOptionComputation(TickerId /*tickerId*/, TickType /*tickType*/, int /*tickAttrib*/, double /*impliedVol*/, 
                               double /*delta*/, double /*optPrice*/, double /*pvDividend*/, double /*gamma*/, 
                               double /*vega*/, double /*theta*/, double /*undPrice*/) {}
    void tickGeneric(TickerId /*tickerId*/, TickType /*tickType*/, double /*value*/) {}
    void tickEFP(TickerId /*tickerId*/, TickType /*tickType*/, double /*basisPoints*/, const std::string& /*formattedBasisPoints*/,
                 double /*totalDividends*/, int /*holdDays*/, const std::string& /*futureLastTradeDate*/, 
                 double /*dividendImpact*/, double /*dividendsToLastTradeDate*/) {}
    void orderStatus(OrderId /*orderId*/, const std::string& /*status*/, Decimal /*filled*/, Decimal /*remaining*/, 
                     double /*avgFillPrice*/, long long /*permId*/, int /*parentId*/, double /*lastFillPrice*/, 
                     int /*clientId*/, const std::string& /*whyHeld*/, double /*mktCapPrice*/) {}
    void openOrder(OrderId /*orderId*/, const Contract&, const Order&, const OrderState&) {}
    void openOrderEnd() {}
    void winError(const std::string& /*str*/, int /*lastError*/) {}
    void updateAccountValue(const std::string& /*key*/, const std::string& /*val*/, const std::string& /*currency*/, 
                            const std::string& /*accountName*/) {}
    void updatePortfolio(const Contract& /*contract*/, Decimal /*position*/, double /*marketPrice*/, double /*marketValue*/,
                         double /*averageCost*/, double /*unrealizedPNL*/, double /*realizedPNL*/, 
                         const std::string& /*accountName*/) {}
    void updateAccountTime(const std::string& /*timeStamp*/) {}
    void accountDownloadEnd(const std::string& /*accountName*/) {}
    void contractDetails(int /*reqId*/, const ContractDetails& /*contractDetails*/) {}
    void bondContractDetails(int /*reqId*/, const ContractDetails& /*contractDetails*/) {}
    void contractDetailsEnd(int /*reqId*/) {}
    void execDetails(int /*reqId*/, const Contract& /*contract*/, const Execution& /*execution*/) {}
    void execDetailsEnd(int /*reqId*/) {}
    void updateMktDepth(TickerId /*id*/, int /*position*/, int /*operation*/, int /*side*/, double /*price*/, Decimal /*size*/) {}
    void updateMktDepthL2(TickerId /*id*/, int /*position*/, const std::string& /*marketMaker*/, int /*operation*/, 
                          int /*side*/, double /*price*/, Decimal /*size*/, bool /*isSmartDepth*/) {}
    void updateNewsBulletin(int /*msgId*/, int /*msgType*/, const std::string& /*newsMessage*/, 
                            const std::string& /*originExch*/) {}
    void managedAccounts(const std::string& /*accountsList*/) {}
    void receiveFA(faDataType /*pFaDataType*/, const std::string& /*cxml*/) {}
    void historicalData(TickerId reqId, const Bar& bar);
    void historicalDataEnd(int reqId, const std::string& startDateStr, const std::string& endDateStr);
    void scannerParameters(const std::string& /*xml*/) {}
    void scannerData(int /*reqId*/, int /*rank*/, const ContractDetails& /*contractDetails*/, const std::string& /*distance*/,
                     const std::string& /*benchmark*/, const std::string& /*projection*/, const std::string& /*legsStr*/) {}
    void scannerDataEnd(int /*reqId*/) {}
    void realtimeBar(TickerId reqId, long time, double open, double high, double low, double close,
                     Decimal volume, Decimal wap, int count);
    void currentTime(long /*time*/) {}
    void fundamentalData(TickerId /*reqId*/, const std::string& /*data*/) {}
    void deltaNeutralValidation(int /*reqId*/, const DeltaNeutralContract& /*deltaNeutralContract*/) {}
    void tickSnapshotEnd(int /*reqId*/) {}
    void marketDataType(TickerId /*reqId*/, int /*marketDataType*/) {}
    void commissionAndFeesReport(const CommissionAndFeesReport& /*commissionAndFeesReport*/) {}
    void position(const std::string& /*account*/, const Contract& /*contract*/, Decimal /*position*/, double /*avgCost*/) {}
    void positionEnd() {}
    void accountSummary(int /*reqId*/, const std::string& /*account*/, const std::string& /*tag*/, const std::string& /*value*/, 
                        const std::string& /*currency*/) {}
    void accountSummaryEnd(int /*reqId*/) {}
    void verifyMessageAPI(const std::string& /*apiData*/) {}
    void verifyCompleted(bool /*isSuccessful*/, const std::string& /*errorText*/) {}
    void displayGroupList(int /*reqId*/, const std::string& /*groups*/) {}
    void displayGroupUpdated(int /*reqId*/, const std::string& /*contractInfo*/) {}
    void verifyAndAuthMessageAPI(const std::string& /*apiData*/, const std::string& /*xyzChallange*/) {}
    void verifyAndAuthCompleted(bool /*isSuccessful*/, const std::string& /*errorText*/) {}
    void positionMulti(int /*reqId*/, const std::string& /*account*/, const std::string& /*modelCode*/, 
                       const Contract& /*contract*/, Decimal /*pos*/, double /*avgCost*/) {}
    void positionMultiEnd(int /*reqId*/) {}
    void accountUpdateMulti(int /*reqId*/, const std::string& /*account*/, const std::string& /*modelCode*/, 
                            const std::string& /*key*/, const std::string& /*value*/, const std::string& /*currency*/) {}
    void accountUpdateMultiEnd(int /*reqId*/) {}
    void securityDefinitionOptionalParameter(int /*reqId*/, const std::string& /*exchange*/, int /*underlyingConId*/,
                                             const std::string& /*tradingClass*/, const std::string& /*multiplier*/,
                                             const std::set<std::string>& /*expirations*/, const std::set<double>& /*strikes*/) {}
    void securityDefinitionOptionalParameterEnd(int /*reqId*/) {}
    void softDollarTiers(int /*reqId*/, const std::vector<SoftDollarTier>& /*tiers*/) {}
    void familyCodes(const std::vector<FamilyCode>& /*familyCodes*/) {}
    void symbolSamples(int /*reqId*/, const std::vector<ContractDescription>& /*contractDescriptions*/) {}
    void mktDepthExchanges(const std::vector<DepthMktDataDescription>& /*depthMktDataDescriptions*/) {}
    void tickNews(int /*tickerId*/, time_t /*timeStamp*/, const std::string& /*providerCode*/, const std::string& /*articleId*/, 
                  const std::string& /*headline*/, const std::string& /*extraData*/) {}
    void smartComponents(int /*reqId*/, const SmartComponentsMap& /*theMap*/) {}
    void tickReqParams(int /*tickerId*/, double /*minTick*/, const std::string& /*bboExchange*/, int /*snapshotPermissions*/) {}
    void newsProviders(const std::vector<NewsProvider>& /*newsProviders*/) {}
    void newsArticle(int /*requestId*/, int /*articleType*/, const std::string& /*articleText*/) {}
    void historicalNews(int /*requestId*/, const std::string& /*time*/, const std::string& /*providerCode*/, 
                        const std::string& /*articleId*/, const std::string& /*headline*/) {}
    void historicalNewsEnd(int /*requestId*/, bool /*hasMore*/) {}
    void headTimestamp(int /*reqId*/, const std::string& /*headTimestamp*/) {}
    void histogramData(int /*reqId*/, const HistogramDataVector& /*data*/) {}
    void historicalDataUpdate(TickerId /*reqId*/, const Bar& /*bar*/) {}
    void rerouteMktDataReq(int /*reqId*/, int /*conid*/, const std::string& /*exchange*/) {}
    void rerouteMktDepthReq(int /*reqId*/, int /*conid*/, const std::string& /*exchange*/) {}
    void marketRule(int /*marketRuleId*/, const std::vector<PriceIncrement>& /*priceIncrements*/) {}
    void pnl(int /*reqId*/, double /*dailyPnL*/, double /*unrealizedPnL*/, double /*realizedPnL*/) {}
    void pnlSingle(int /*reqId*/, Decimal /*pos*/, double /*dailyPnL*/, double /*unrealizedPnL*/, double /*realizedPnL*/, double /*value*/) {}
    void historicalTicks(int /*reqId*/, const std::vector<HistoricalTick>& /*ticks*/, bool /*done*/) {}
    void historicalTicksBidAsk(int /*reqId*/, const std::vector<HistoricalTickBidAsk>& /*ticks*/, bool /*done*/) {}
    void historicalTicksLast(int /*reqId*/, const std::vector<HistoricalTickLast>& /*ticks*/, bool /*done*/) {}
    void tickByTickMidPoint(int /*reqId*/, time_t /*time*/, double /*midPoint*/) {}
    void orderBound(long long /*permId*/, int /*clientId*/, int /*orderId*/) {}
    void completedOrder(const Contract& /*contract*/, const Order& /*order*/, const OrderState& /*orderState*/) {}
    void completedOrdersEnd() {}
    void replaceFAEnd(int /*reqId*/, const std::string& /*text*/) {}
    void wshMetaData(int /*reqId*/, const std::string& /*dataJson*/) {}
    void wshEventData(int /*reqId*/, const std::string& /*dataJson*/) {}
    void historicalSchedule(int /*reqId*/, const std::string& /*startDateTime*/, const std::string& /*endDateTime*/, 
                            const std::string& /*timeZone*/, const std::vector<HistoricalSession>& /*sessions*/) {}
    void userInfo(int /*reqId*/, const std::string& /*whiteBrandingId*/) {}
    void currentTimeInMillis(time_t /*timeInMillis*/) {}

    // ========== Protobuf callbacks (unused) ==========
    void execDetailsProtoBuf(const protobuf::ExecutionDetails& /*executionDetailsProto*/) {}
    void execDetailsEndProtoBuf(const protobuf::ExecutionDetailsEnd& /*executionDetailsEndProto*/) {}
    void orderStatusProtoBuf(const protobuf::OrderStatus& /*orderStatusProto*/) {}
    void openOrderProtoBuf(const protobuf::OpenOrder& /*openOrderProto*/) {}
    void openOrdersEndProtoBuf(const protobuf::OpenOrdersEnd& /*openOrderEndProto*/) {}
    void errorProtoBuf(const protobuf::ErrorMessage& /*errorProto*/) {}

private:
    // ========== Data Flow: Callbacks → Queue → Redis Worker ==========
    moodycamel::ConcurrentQueue<TickUpdate>& m_queue;  // Zero-copy enqueue from callbacks
    
    // ========== TWS API Components ==========
    std::unique_ptr<EReaderOSSignal> m_signal;   // Thread synchronization primitive
    std::unique_ptr<EClientSocket> m_client;     // Command interface (sends to TWS)
    std::unique_ptr<EReader> m_reader;           // Socket reader thread (receives from TWS)
    
    // ========== Connection State ==========
    std::atomic<bool> m_connected{false};
    std::atomic<OrderId> m_nextValidOrderId{0};
    
    // ========== Symbol Routing ==========
    std::unordered_map<int, std::string> m_tickerToSymbol;  // tickerId → symbol lookup
};

} // namespace tws_bridge

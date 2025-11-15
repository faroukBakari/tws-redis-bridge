// TwsClient.h - TWS API EWrapper implementation
// SCOPE: Producer thread - handles TWS callbacks, enqueues tick updates

#pragma once

#include "EWrapper.h"
#include "EReaderOSSignal.h"
#include "EReader.h"
#include "MarketData.h"
#include <memory>
#include <string>
#include <atomic>
#include <unordered_map>
#include <concurrentqueue.h>

class EClientSocket;

namespace tws_bridge {

// REASON: Inherits from TWS EWrapper to receive market data callbacks
class TwsClient : public EWrapper {
public:
    explicit TwsClient(moodycamel::ConcurrentQueue<TickUpdate>& queue);
    ~TwsClient() override;

    // Connection management
    bool connect(const std::string& host, unsigned int port, int clientId);
    void disconnect();
    bool isConnected() const;
    
    // Market data subscription
    void subscribeTickByTick(const std::string& symbol, int tickerId);
    
    // Message processing loop (must be called from main thread)
    void processMessages();

    // EWrapper interface - callbacks from TWS
    void tickPrice(TickerId tickerId, TickType field, double price, const TickAttrib& attribs) override;
    void tickSize(TickerId tickerId, TickType field, Decimal size) override;
    void tickString(TickerId tickerId, TickType tickType, const std::string& value) override;
    void tickByTickBidAsk(int reqId, time_t time, double bidPrice, double askPrice, 
                          Decimal bidSize, Decimal askSize, const TickAttribBidAsk& tickAttribBidAsk) override;
    void tickByTickAllLast(int reqId, int tickType, time_t time, double price, 
                           Decimal size, const TickAttribLast& tickAttribLast, 
                           const std::string& exchange, const std::string& specialConditions) override;
    
    void nextValidId(OrderId orderId) override;
    void connectionClosed() override;
    void error(int id, time_t errorTime, int errorCode, const std::string& errorString, const std::string& advancedOrderRejectJson) override;
    void connectAck() override;
    
    // REASON: Must implement all pure virtual methods from EWrapper
    // These are minimally implemented for compilation
    void tickOptionComputation(TickerId /*tickerId*/, TickType /*tickType*/, int /*tickAttrib*/, double /*impliedVol*/, 
                               double /*delta*/, double /*optPrice*/, double /*pvDividend*/, double /*gamma*/, 
                               double /*vega*/, double /*theta*/, double /*undPrice*/) override {}
    void tickGeneric(TickerId /*tickerId*/, TickType /*tickType*/, double /*value*/) override {}
    void tickEFP(TickerId /*tickerId*/, TickType /*tickType*/, double /*basisPoints*/, const std::string& /*formattedBasisPoints*/,
                 double /*totalDividends*/, int /*holdDays*/, const std::string& /*futureLastTradeDate*/, 
                 double /*dividendImpact*/, double /*dividendsToLastTradeDate*/) override {}
    void orderStatus(OrderId /*orderId*/, const std::string& /*status*/, Decimal /*filled*/, Decimal /*remaining*/, 
                     double /*avgFillPrice*/, long long /*permId*/, int /*parentId*/, double /*lastFillPrice*/, 
                     int /*clientId*/, const std::string& /*whyHeld*/, double /*mktCapPrice*/) override {}
    void openOrder(OrderId /*orderId*/, const Contract&, const Order&, const OrderState&) override {}
    void openOrderEnd() override {}
    void winError(const std::string& /*str*/, int /*lastError*/) override {}
    void updateAccountValue(const std::string& /*key*/, const std::string& /*val*/, const std::string& /*currency*/, 
                            const std::string& /*accountName*/) override {}
    void updatePortfolio(const Contract& /*contract*/, Decimal /*position*/, double /*marketPrice*/, double /*marketValue*/,
                         double /*averageCost*/, double /*unrealizedPNL*/, double /*realizedPNL*/, 
                         const std::string& /*accountName*/) override {}
    void updateAccountTime(const std::string& /*timeStamp*/) override {}
    void accountDownloadEnd(const std::string& /*accountName*/) override {}
    void contractDetails(int /*reqId*/, const ContractDetails& /*contractDetails*/) override {}
    void bondContractDetails(int /*reqId*/, const ContractDetails& /*contractDetails*/) override {}
    void contractDetailsEnd(int /*reqId*/) override {}
    void execDetails(int /*reqId*/, const Contract& /*contract*/, const Execution& /*execution*/) override {}
    void execDetailsEnd(int /*reqId*/) override {}
    void updateMktDepth(TickerId /*id*/, int /*position*/, int /*operation*/, int /*side*/, double /*price*/, Decimal /*size*/) override {}
    void updateMktDepthL2(TickerId /*id*/, int /*position*/, const std::string& /*marketMaker*/, int /*operation*/, 
                          int /*side*/, double /*price*/, Decimal /*size*/, bool /*isSmartDepth*/) override {}
    void updateNewsBulletin(int /*msgId*/, int /*msgType*/, const std::string& /*newsMessage*/, 
                            const std::string& /*originExch*/) override {}
    void managedAccounts(const std::string& /*accountsList*/) override {}
    void receiveFA(faDataType /*pFaDataType*/, const std::string& /*cxml*/) override {}
    void historicalData(TickerId /*reqId*/, const Bar& /*bar*/) override {}
    void historicalDataEnd(int /*reqId*/, const std::string& /*startDateStr*/, const std::string& /*endDateStr*/) override {}
    void scannerParameters(const std::string& /*xml*/) override {}
    void scannerData(int /*reqId*/, int /*rank*/, const ContractDetails& /*contractDetails*/, const std::string& /*distance*/,
                     const std::string& /*benchmark*/, const std::string& /*projection*/, const std::string& /*legsStr*/) override {}
    void scannerDataEnd(int /*reqId*/) override {}
    void realtimeBar(TickerId /*reqId*/, long /*time*/, double /*open*/, double /*high*/, double /*low*/, double /*close*/,
                     Decimal /*volume*/, Decimal /*wap*/, int /*count*/) override {}
    void currentTime(long /*time*/) override {}
    void fundamentalData(TickerId /*reqId*/, const std::string& /*data*/) override {}
    void deltaNeutralValidation(int /*reqId*/, const DeltaNeutralContract& /*deltaNeutralContract*/) override {}
    void tickSnapshotEnd(int /*reqId*/) override {}
    void marketDataType(TickerId /*reqId*/, int /*marketDataType*/) override {}
    void commissionAndFeesReport(const CommissionAndFeesReport& /*commissionAndFeesReport*/) override {}
    void position(const std::string& /*account*/, const Contract& /*contract*/, Decimal /*position*/, double /*avgCost*/) override {}
    void positionEnd() override {}
    void accountSummary(int /*reqId*/, const std::string& /*account*/, const std::string& /*tag*/, const std::string& /*value*/, 
                        const std::string& /*currency*/) override {}
    void accountSummaryEnd(int /*reqId*/) override {}
    void verifyMessageAPI(const std::string& /*apiData*/) override {}
    void verifyCompleted(bool /*isSuccessful*/, const std::string& /*errorText*/) override {}
    void displayGroupList(int /*reqId*/, const std::string& /*groups*/) override {}
    void displayGroupUpdated(int /*reqId*/, const std::string& /*contractInfo*/) override {}
    void verifyAndAuthMessageAPI(const std::string& /*apiData*/, const std::string& /*xyzChallange*/) override {}
    void verifyAndAuthCompleted(bool /*isSuccessful*/, const std::string& /*errorText*/) override {}
    void positionMulti(int /*reqId*/, const std::string& /*account*/, const std::string& /*modelCode*/, 
                       const Contract& /*contract*/, Decimal /*pos*/, double /*avgCost*/) override {}
    void positionMultiEnd(int /*reqId*/) override {}
    void accountUpdateMulti(int /*reqId*/, const std::string& /*account*/, const std::string& /*modelCode*/, 
                            const std::string& /*key*/, const std::string& /*value*/, const std::string& /*currency*/) override {}
    void accountUpdateMultiEnd(int /*reqId*/) override {}
    void securityDefinitionOptionalParameter(int /*reqId*/, const std::string& /*exchange*/, int /*underlyingConId*/,
                                             const std::string& /*tradingClass*/, const std::string& /*multiplier*/,
                                             const std::set<std::string>& /*expirations*/, const std::set<double>& /*strikes*/) override {}
    void securityDefinitionOptionalParameterEnd(int /*reqId*/) override {}
    void softDollarTiers(int /*reqId*/, const std::vector<SoftDollarTier>& /*tiers*/) override {}
    void familyCodes(const std::vector<FamilyCode>& /*familyCodes*/) override {}
    void symbolSamples(int /*reqId*/, const std::vector<ContractDescription>& /*contractDescriptions*/) override {}
    void mktDepthExchanges(const std::vector<DepthMktDataDescription>& /*depthMktDataDescriptions*/) override {}
    void tickNews(int /*tickerId*/, time_t /*timeStamp*/, const std::string& /*providerCode*/, const std::string& /*articleId*/, 
                  const std::string& /*headline*/, const std::string& /*extraData*/) override {}
    void smartComponents(int /*reqId*/, const SmartComponentsMap& /*theMap*/) override {}
    void tickReqParams(int /*tickerId*/, double /*minTick*/, const std::string& /*bboExchange*/, int /*snapshotPermissions*/) override {}
    void newsProviders(const std::vector<NewsProvider>& /*newsProviders*/) override {}
    void newsArticle(int /*requestId*/, int /*articleType*/, const std::string& /*articleText*/) override {}
    void historicalNews(int /*requestId*/, const std::string& /*time*/, const std::string& /*providerCode*/, 
                        const std::string& /*articleId*/, const std::string& /*headline*/) override {}
    void historicalNewsEnd(int /*requestId*/, bool /*hasMore*/) override {}
    void headTimestamp(int /*reqId*/, const std::string& /*headTimestamp*/) override {}
    void histogramData(int /*reqId*/, const HistogramDataVector& /*data*/) override {}
    void historicalDataUpdate(TickerId /*reqId*/, const Bar& /*bar*/) override {}
    void rerouteMktDataReq(int /*reqId*/, int /*conid*/, const std::string& /*exchange*/) override {}
    void rerouteMktDepthReq(int /*reqId*/, int /*conid*/, const std::string& /*exchange*/) override {}
    void marketRule(int /*marketRuleId*/, const std::vector<PriceIncrement>& /*priceIncrements*/) override {}
    void pnl(int /*reqId*/, double /*dailyPnL*/, double /*unrealizedPnL*/, double /*realizedPnL*/) override {}
    void pnlSingle(int /*reqId*/, Decimal /*pos*/, double /*dailyPnL*/, double /*unrealizedPnL*/, double /*realizedPnL*/, double /*value*/) override {}
    void historicalTicks(int /*reqId*/, const std::vector<HistoricalTick>& /*ticks*/, bool /*done*/) override {}
    void historicalTicksBidAsk(int /*reqId*/, const std::vector<HistoricalTickBidAsk>& /*ticks*/, bool /*done*/) override {}
    void historicalTicksLast(int /*reqId*/, const std::vector<HistoricalTickLast>& /*ticks*/, bool /*done*/) override {}
    void tickByTickMidPoint(int /*reqId*/, time_t /*time*/, double /*midPoint*/) override {}
    void orderBound(long long /*permId*/, int /*clientId*/, int /*orderId*/) override {}
    void completedOrder(const Contract& /*contract*/, const Order& /*order*/, const OrderState& /*orderState*/) override {}
    void completedOrdersEnd() override {}
    void replaceFAEnd(int /*reqId*/, const std::string& /*text*/) override {}
    void wshMetaData(int /*reqId*/, const std::string& /*dataJson*/) override {}
    void wshEventData(int /*reqId*/, const std::string& /*dataJson*/) override {}
    void historicalSchedule(int /*reqId*/, const std::string& /*startDateTime*/, const std::string& /*endDateTime*/, 
                            const std::string& /*timeZone*/, const std::vector<HistoricalSession>& /*sessions*/) override {}
    void userInfo(int /*reqId*/, const std::string& /*whiteBrandingId*/) override {}
    void currentTimeInMillis(time_t /*timeInMillis*/) override {}

    // REASON: Protobuf methods (not used, but required by EWrapper)
    void execDetailsProtoBuf(const protobuf::ExecutionDetails& /*executionDetailsProto*/) override {}
    void execDetailsEndProtoBuf(const protobuf::ExecutionDetailsEnd& /*executionDetailsEndProto*/) override {}
    void orderStatusProtoBuf(const protobuf::OrderStatus& /*orderStatusProto*/) override {}
    void openOrderProtoBuf(const protobuf::OpenOrder& /*openOrderProto*/) override {}
    void openOrdersEndProtoBuf(const protobuf::OpenOrdersEnd& /*openOrderEndProto*/) override {}
    void errorProtoBuf(const protobuf::ErrorMessage& /*errorProto*/) override {}

private:
    // REASON: Lock-free queue for zero-copy message passing to consumer thread
    moodycamel::ConcurrentQueue<TickUpdate>& m_queue;
    
    std::unique_ptr<EReaderOSSignal> m_signal;
    std::unique_ptr<EClientSocket> m_client;
    std::unique_ptr<EReader> m_reader;
    
    std::atomic<bool> m_connected{false};
    std::atomic<OrderId> m_nextValidOrderId{0};
    
    // REASON: Map tickerId to instrument symbol for routing
    std::unordered_map<int, std::string> m_tickerToSymbol;
};

} // namespace tws_bridge

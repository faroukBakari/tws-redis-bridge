// test_serialization.cpp - Unit tests for JSON serialization

#include <catch2/catch_test_macros.hpp>
#include "Serialization.h"
#include "MarketData.h"

TEST_CASE("InstrumentState serialization", "[serialization]") {
    InstrumentState state;
    state.symbol = "AAPL";
    state.conId = 265598;
    state.tickerId = 1001;
    state.bidPrice = 171.55;
    state.askPrice = 171.57;
    state.lastPrice = 171.56;
    state.bidSize = 100;
    state.askSize = 200;
    state.lastSize = 50;
    state.quoteTimestamp = 1700000000000;
    state.tradeTimestamp = 1700000000500;
    state.hasQuote = true;
    state.hasTrade = true;
    state.exchange = "NASDAQ";
    state.pastLimit = false;
    
    SECTION("Serialize to JSON") {
        std::string json = serializeState(state);
        
        REQUIRE(!json.empty());
        REQUIRE(json.find("\"instrument\":\"AAPL\"") != std::string::npos);
        REQUIRE(json.find("\"bid\":171.55") != std::string::npos);
        REQUIRE(json.find("\"ask\":171.57") != std::string::npos);
        REQUIRE(json.find("\"last\":171.56") != std::string::npos);
    }
}

TEST_CASE("Empty InstrumentState", "[serialization]") {
    InstrumentState state;
    state.symbol = "TEST";
    
    std::string json = serializeState(state);
    REQUIRE(!json.empty());
    REQUIRE(json.find("\"instrument\":\"TEST\"") != std::string::npos);
}

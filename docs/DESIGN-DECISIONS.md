# TWS-Redis Bridge: Design Decisions

## Project Overview

This document outlines the design decisions for building a C++ microservice that bridges Interactive Brokers' Trader Workstation (TWS) API with Redis Pub/Sub. This component will enhance the existing Vue.js + FastAPI trading web app (https://github.com/faroukBakari/trader-pro) by providing low-latency market data ingestion and distribution.

### Context

The existing project currently serves randomly generated data for testing. This TWS-Redis bridge will initiate the Interactive Brokers integration, providing real market data to the application stack.

## Project Objectives

### Primary Goal
Consume tick and Level 1 data from TWS Gateway, normalize it, and publish real-time updates to Redis Pub/Sub, enabling FastAPI/Vue to consume data via WebSocket. This architecture decouples the stack and reduces computational load on Python/JavaScript components.

### MVP Timeline
**Achievable in 7 days (minimum)**

**Critical Note**: The 3-day timeline was too ambitious. The TWS API's threading model, error handling, and reconnection logic have a steep learning curve. A full week allows for proper implementation of the producer-consumer architecture and robust reliability features without cutting corners.

### Key Deliverables
1. Minimal C++ client for TWS Gateway (socket/TCP wrapper)
2. Parser for price/size/instrument messages with compact JSON schema mapping
3. Redis Pub/Sub publisher integration (using redis-plus-plus)
4. CLI utility for replay mode from tick files (testing purposes)

### Tests and Acceptance Criteria
- **End-to-end validation**: Run TWS simulator or replay mode → verify messages on Redis
- **Performance target**: Median latency < 50ms on LAN
- **Observability**: Clear, structured logs for debugging and monitoring

### Risks and Mitigations
- **Risk**: IB protocol complexity
  - **Mitigation**: Limit MVP scope to essential Level 1/order-book messages; implement basic fuzz tests for parser robustness

## Key Implementation Challenges

### 1. TWS API Integration (Primary Challenge)
Requires deep research into:
- IB Campus documentation: https://ibkrcampus.com/campus/ibkr-api-page/twsapi-doc/#api-introduction
- External resources with integration examples
- Understanding C++ API patterns and best practices

### 2. Redis Pub/Sub Integration
- Implement redis-plus-plus library
- Run local Docker Redis instance for testing

---

## Design Decisions

### 1. TWS API Client Implementation

**Decision**: Use IB's provided C++ API (not custom socket implementation)

**Rationale**: 
- Official support and documentation
- Proven stability and compatibility
- Handles protocol complexity and versioning

#### Understanding the TWS API Architecture

The TWS C++ API uses a **bidirectional communication pattern** that can be confusing due to misleading naming:

##### The Components

**EWrapper (Inbound API - Misleadingly Named)**
- **What it is**: A callback interface (Observer pattern)
- **What it's NOT**: Despite the name, it's **not** a wrapper
- **Better name would be**: `ITwsCallbacks` or `IEventHandler`
- **Role**: Defines callbacks that **TWS invokes on your application**
- **Direction**: TWS → Application (inbound data)
- **Example callbacks**: `tickByTickBidAsk()`, `error()`, `nextValidId()`

**EClient/EClientSocket (Outbound API)**
- **What it is**: Command interface
- **Role**: Defines methods **your application calls to send requests to TWS**
- **Direction**: Application → TWS (outbound commands)
- **Example methods**: `reqTickByTickData()`, `eConnect()`, `placeOrder()`

**EReader (Internal Thread Manager)**
- Spawns Thread 3 (socket reader) when `start()` is called
- Reads TCP socket, parses TWS protocol, enqueues messages
- See `src/TwsClient.cpp` line 39 for creation point

##### TwsClient: A Bidirectional Adapter

Our `TwsClient` class serves dual roles:

```
Application (TwsClient)
      ↑ callbacks      ↓ commands
  EWrapper         EClientSocket
      ↑                   ↓
           TWS Gateway
```

**Implementation Pattern**:
- **Inherits** `EWrapper` to implement callbacks (inbound)
- **Wraps** `EClientSocket` member to send commands (outbound)

**Code Reference**: See `include/TwsClient.h`:
- Lines 1-8: Architecture clarification comments
- Lines 30-36: Explicit separation of Outbound API (commands) vs Inbound API (callbacks)
- Lines 171-177: Member variables showing the wrapping of `EClientSocket`

**Key Insight**: This bidirectional pattern is fundamental to TWS integration. Understanding that `EWrapper` is a callback interface (not a wrapper) eliminates the primary source of confusion when working with the TWS API.

### 2. Threading Model (CRITICAL)

**Problem**: EWrapper callbacks are executed on Thread 1 (Main) after `processMsgs()` dispatches them. Blocking this thread with I/O operations (like Redis publishing) will cause catastrophic message backlogs and latency spikes in the TWS message processing loop.

**Decision**: Implement a Producer-Consumer architecture with lock-free queue

#### Architecture (Matches Implementation)

**Thread 1: Main Thread (Message Processing - Producer)**
- **Created**: Implicit (program entry point)
- **Location**: `src/main.cpp` lines 149-159
- **Responsibility**: Process incoming TWS messages at maximum speed
- **Critical Rule**: NEVER block this thread
- **Implementation**:
  - Runs message processing loop: `client.processMessages()` → `processMsgs()` → callbacks
  - When a callback fires (e.g., `tickByTickBidAsk`), perform minimal work:
    - Copy the data into a lock-free queue (`TickUpdate` struct)
    - Return immediately (< 1μs target)
  - No JSON serialization, no state aggregation, no Redis I/O on this thread

**Thread 2: Redis Worker Thread (Consumer)**
- **Created**: `std::thread workerThread(...)` at `main.cpp` line 121
- **Location**: `redisWorkerLoop()` function at `main.cpp` lines 24-83
- **Responsibility**: Process queued market data and publish to Redis
- **Implementation Loop**:
  1. Dequeue `TickUpdate` from the lock-free queue
  2. Aggregate BidAsk + AllLast updates into `InstrumentState`
  3. Serialize complete state to JSON using RapidJSON
  4. Publish JSON to Redis via `redis-plus-plus`
  5. Repeat

**Thread 3: EReader Thread (TWS API Internal)**
- **Created**: `m_reader->start()` at `src/TwsClient.cpp` line 39
- **Hidden**: Managed by TWS library (not visible in application code)
- **Role**: Socket I/O, TWS protocol decoding, signaling Thread 1

**Code Reference**: See `src/main.cpp` lines 123-125 and 149-155 for explicit thread creation points and architecture documentation.

#### Queue Selection

**Decision**: Use `moodycamel::ConcurrentQueue` (lock-free, high-performance)

**Rationale**:
- Lock-free design (no mutex contention)
- Optimized for SPSC (single-producer, single-consumer) scenarios
- Significantly faster than `std::queue` with `std::mutex`
- Battle-tested in high-frequency trading systems

**Alternative**: `std::queue` with `std::mutex` and `std::condition_variable` (simpler, but slower)

#### Implementation Pattern

```cpp
#include <moodycamel/concurrentqueue.h>

struct TickUpdate {
    int tickerId;
    long timestamp;
    // ... other fields
};

moodycamel::ConcurrentQueue<TickUpdate> tickQueue;

// In EWrapper callback (Producer - Thread 1)
void tickByTickAllLast(int reqId, int tickType, long time, double price, int size, ...) {
    TickUpdate update;
    update.tickerId = reqId;
    update.timestamp = time;
    // ... copy other fields
    tickQueue.enqueue(update); // Lock-free, non-blocking
}

// In Redis Worker (Consumer - Thread 2)
void redisWorkerLoop() {
    TickUpdate update;
    while (running) {
        if (tickQueue.try_dequeue(update)) {
            // Update state, serialize, publish (heavy work here is OK)
            updateInstrumentState(update);
            std::string json = serializeToJson(update.tickerId);
            redis.publish("market_ticks", json);
        }
    }
}
```

#### Performance Target
- EReader thread latency: < 1μs per callback (just enqueue)
- Redis Worker throughput: > 10,000 messages/second
- End-to-end latency: < 50ms (TWS → Redis)

### 2.1. Market Hours Pivot Strategy (ADR-009)

**[DECISION]** Start with bar data (`reqHistoricalData` + `reqRealTimeBars`), transition to tick-by-tick when markets open **[Date: 2025-11-16]**

**Context:** Development sprint began during market closure (holiday/weekend). Tick-by-tick data (`reqTickByTickData`) requires live market hours (9:30 AM - 4:00 PM ET). Need to validate architecture without waiting for market open.

**Rationale:**
- **Bar data available 24/7:** Historical bars (`reqHistoricalData`) and real-time bars (`reqRealTimeBars`) work outside market hours
- **Architecture-agnostic:** Producer-consumer threading model, lock-free queue, state aggregation, and Redis publishing work identically regardless of data source
- **Risk mitigation:** Validates critical path (TWS connection, EReader threading, queue performance) before attempting tick-by-tick
- **Continuous development:** Enables Day 1-2 progress without market dependency
- **Fallback testing:** Historical ticks (`reqHistoricalTicks`) provide tick-level data offline (1000 tick limit)

**Implementation Timeline:**
- **Day 1 Morning (Gate 1b):** Dev environment setup (Docker Compose, Redis, Makefile targets)
- **Day 1 Afternoon (Gates 2a-2c):** Historical bar data end-to-end flow
  - `reqHistoricalData("SPY", "1 D", "5 mins")` → 5-minute OHLCV bars
  - Validate: TWS → Queue → Console → Redis
- **Day 1 Evening (Gates 3a-3b):** Real-time bars + queue performance
  - `reqRealTimeBars("SPY", 5, "TRADES")` → 5-second updates during off-hours
  - Benchmark lock-free queue enqueue < 1μs
- **Day 2 Morning (Gate 4):** Historical tick data testing
  - `reqHistoricalTicks()` for BidAsk and Last trade data (if markets remain closed)
  - Test state aggregation with tick-level granularity offline
- **Day 3+ (Markets Open):** Transition to tick-by-tick
  - Switch to `reqTickByTickData()` with live market data
  - Validate tick-by-tick callbacks (`tickByTickBidAsk`, `tickByTickAllLast`)

**Trade-offs:**
- **Bar granularity vs tick precision:** 5-second bars sufficient for architecture validation, not true real-time
- **Schema compatibility:** Bar data uses subset of tick schema (OHLCV fields, single timestamp) - JSON schema remains compatible
- **State structure:** `InstrumentState` simplified for bars (no bid/ask/last separation), same struct with fields set to 0 when unavailable

**Alternatives Rejected:**
- **Wait for market hours:** Blocks Day 1-2 progress, violates fail-fast philosophy
- **Mock data generator:** Doesn't validate TWS API integration (primary risk)
- **Use sampled `reqMktData()`:** Even slower than bars (250ms sampling), doesn't test real-time pipeline

**Consequences:**
- ✅ Continuous development during market closure
- ✅ Fail-fast validation of architecture before tick-by-tick complexity
- ✅ Three fallback strategies: bars (24/7) → historical ticks (offline) → tick-by-tick (live)
- ❌ Delayed tick-by-tick validation (acceptable: architecture proven first)
- ❌ Bar data testing doesn't validate quote/trade merge logic (mitigated: Day 2 historical ticks)

**Status:** **ACTIVE** (Day 1-2)
- Gate 1a: ✅ PASSED (compilation successful)
- Gate 1b: ⏳ IN PROGRESS (Redis container setup)
- Gates 2a-2c: 🔴 BLOCKED (waiting for Gate 1b)

**Reference:** See `docs/FAIL-FAST-PLAN.md` § 2.1 (Day 1), § 2.2 (Day 2), § 4.1 (Contingency Plans) for detailed implementation and fallback strategies.

---

### 3. Market Data Subscription Strategy

#### Tick-by-Tick Data (Primary Choice)

**Decision**: Use `reqTickByTickData` for true tick-level granularity

**CRITICAL**: The reference implementation at the end of this document is **WRONG** and uses `reqMktData`. This section defines the **CORRECT** approach.

**API Method**:
```cpp
client.reqTickByTickData(int reqId, const Contract& contract, 
                         const std::string& tickType, int numberOfTicks, bool ignoreSize);
```

**API Callbacks** (must implement in `EWrapper`):
- `tickByTickAllLast(int reqId, int tickType, long time, double price, int size, ...)`: Last trade information
- `tickByTickBidAsk(int reqId, long time, double bidPrice, double askPrice, int bidSize, int askSize, ...)`: Best bid/ask updates
- `tickByTickMidPoint(int reqId, long time, double midPoint)`: Mid-point price

**Rationale**: 
- **DO NOT** use standard `reqMktData` subscription (which invokes `tickPrice`, `tickSize` callbacks)
- `reqMktData` provides throttled/sampled data (not true tick-level)
- `reqTickByTickData` provides genuine tick-level updates
- Essential for low-latency trading applications

**Example Subscription**:
```cpp
Contract contract;
contract.symbol = "AAPL";
contract.secType = "STK";
contract.exchange = "SMART";
contract.currency = "USD";

// Subscribe to All Last ticks (trades)
client.reqTickByTickData(1, contract, "AllLast", 0, false);

// Subscribe to BidAsk ticks (quotes)
client.reqTickByTickData(2, contract, "BidAsk", 0, false);
```

#### Real-Time Bars (Supplementary)

**Decision**: Use `reqRealTimeBars` for 5-second OHLCV bars

**Use Cases**:
- Chart updates and visualization
- Inter-bar analytics
- Aggregated data views

### 4. State Management (CRITICAL)

**Problem**: The JSON schema shows a consolidated object with `bid`, `ask`, and `last` prices together. However, TWS sends discrete, partial updates:
- `tickByTickBidAsk`: Only provides bid/ask (no last price)
- `tickByTickAllLast`: Only provides last trade (no bid/ask)

**Decision**: Maintain internal state per instrument and publish complete snapshots

#### Architecture

**State Storage**:
```cpp
struct InstrumentState {
    std::string symbol;
    int conId;
    
    // Quote data (from tickByTickBidAsk)
    double bidPrice = 0.0;
    int bidSize = 0;
    double askPrice = 0.0;
    int askSize = 0;
    long quoteTimestamp = 0;
    
    // Trade data (from tickByTickAllLast)
    double lastPrice = 0.0;
    int lastSize = 0;
    long tradeTimestamp = 0;
    
    std::string exchange;
    bool canAutoExecute = false;
    bool pastLimit = false;
};

// State map: tickerId -> InstrumentState
std::unordered_map<int, InstrumentState> instrumentStates;
std::mutex stateMutex; // Protect map during updates
```

#### Update Logic (in Redis Worker Thread)

```cpp
void processTickUpdate(const TickUpdate& update) {
    std::lock_guard<std::mutex> lock(stateMutex);
    
    InstrumentState& state = instrumentStates[update.tickerId];
    
    if (update.type == TickUpdateType::BidAsk) {
        // Update quote data
        state.bidPrice = update.bidPrice;
        state.bidSize = update.bidSize;
        state.askPrice = update.askPrice;
        state.askSize = update.askSize;
        state.quoteTimestamp = update.timestamp;
    }
    else if (update.type == TickUpdateType::AllLast) {
        // Update trade data
        state.lastPrice = update.price;
        state.lastSize = update.size;
        state.tradeTimestamp = update.timestamp;
    }
    
    // Serialize COMPLETE state (not just the update)
    std::string json = serializeInstrumentState(state);
    redis.publish("market_ticks", json);
}
```

#### Timestamp Handling

**CRITICAL**: Do NOT use `std::time(nullptr)` for timestamps. This is the server's clock time, not the market event time.

**Correct Approach**: Use the `time` parameter from TWS callbacks:
- `tickByTickBidAsk(... long time ...)`: This is the market timestamp
- `tickByTickAllLast(... long time ...)`: This is the market timestamp

**Time Format**: TWS provides Unix timestamp in **seconds** or **milliseconds** (check API version documentation). Use the provided value directly:

```cpp
writer.Key("timestamp"); 
writer.Int64(time); // Use TWS-provided timestamp, not std::time(nullptr)
```

**Precision**: For low-latency applications, use millisecond precision if available.

#### Benefits
- Downstream consumers (FastAPI/Vue) always receive complete, up-to-date instrument snapshots
- Simplifies client logic (no need to merge partial updates)
- Ensures consistency even when bid/ask and last trade updates arrive at different times

#### Bar Data Handling (Market Hours Pivot)

**Context:** When using historical/real-time bars instead of tick-by-tick data (see § 2.1 Market Hours Pivot Strategy), state structure is simplified.

**Bar Data State Fields:**
- **OHLCV fields:** `open`, `high`, `low`, `close`, `volume` (from `historicalData()` or `realtimeBar()` callbacks)
- **Single timestamp:** Bar start time (not separate quote/trade timestamps)
- **No bid/ask/last separation:** Bars are trade-based aggregates, use `close` as last price

**Transition Strategy:**
- Same `InstrumentState` struct used for both bars and ticks
- Bar mode: Set `lastPrice = close`, leave `bidPrice`/`askPrice` at 0.0 (or use `close` for all three)
- JSON schema remains compatible: Consumers ignore missing/zero bid/ask fields when processing bar data
- State aggregation logic unchanged: Update fields, serialize complete state, publish

**Implementation Reference:**
- See `src/Serialization.cpp` for `serializeBarData()` vs `serializeTickData()` implementations
- Both use same Redis channel pattern: `TWS:TICKS:{SYMBOL}` (or `TWS:BARS:{SYMBOL}` for explicit bar channel)
- Schema evolution: Bar data is subset of tick data, backward compatible

**Rationale:** Maintaining schema compatibility enables seamless transition from bar testing to tick-by-tick production without downstream consumer changes.

---

### 5. Data Format and Serialization

#### JSON Schema for Redis Messages

**Schema Design**:
```json
{
  "instrument": "AAPL",
  "conId": 265598,
  "timestamp": 1700000000,
  "price": {
    "bid": 171.55,
    "ask": 171.57,
    "last": 171.56
  },
  "size": {
    "bid": 100,
    "ask": 200,
    "last": 50
  },
  "exchange": "SMART",
  "tickAttrib": {
    "canAutoExecute": true,
    "pastLimit": false,
    "preOpen": false
  }
}
```

#### Type Mapping (C++ to JSON)
- **IDs**: `int` → JSON integer
- **Prices**: `double` → JSON number
- **Sizes**: `int` → JSON integer
- **Symbols/Exchanges**: `string` → JSON string

#### JSON Library Selection

**Decision**: Use `RapidJSON`

**Rationale**:
- Designed explicitly for high performance and low memory footprint
- One of the fastest JSON libraries available for C++
- Header-only library (easy integration)
- Low-latency optimized for high-throughput scenarios
- Ideal for critical data path where serialization performance matters

**Best Practice**: 
- Use Writer (SAX) API directly for maximum performance (avoids DOM allocations)
- Start with JSON for development and debugging
- Consider migrating to Protobuf if profiling reveals bandwidth constraints

#### Implementation Example
```cpp
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

using namespace rapidjson;

StringBuffer s;
Writer<StringBuffer> writer(s);

writer.StartObject();
writer.Key("instrument"); writer.String("AAPL");
writer.Key("conId"); writer.Int(265598);
writer.Key("timestamp"); writer.Int64(marketTimestamp); // Use TWS callback time parameter
writer.Key("price");
writer.StartObject();
writer.Key("bid"); writer.Double(171.55);
writer.Key("ask"); writer.Double(171.57);
writer.Key("last"); writer.Double(171.56);
writer.EndObject();
writer.Key("size");
writer.StartObject();
writer.Key("bid"); writer.Int(100);
writer.Key("ask"); writer.Int(200);
writer.Key("last"); writer.Int(50);
writer.EndObject();
writer.Key("exchange"); writer.String("SMART");
writer.EndObject();

std::string message = s.GetString();
```

### 6. Redis Integration

#### Library Selection

**Decision**: Use `redis-plus-plus` exclusively

**Rationale**:
- Modern C++11/14/17 support
- Clean, intuitive API
- Excellent performance
- Active maintenance and community

#### Publishing Pattern
```cpp
#include <sw/redis++/redis++.h>

sw::redis::Redis redis("tcp://127.0.0.1:6379");
redis.publish("market_ticks", message);
```

#### Subscription Pattern (for testing/monitoring)
```cpp
#include <sw/redis++/redis++.h>

sw::redis::Redis redis("tcp://127.0.0.1:6379");
auto sub = redis.subscriber();
sub.subscribe("market_ticks");
sub.on_message([](std::string channel, std::string msg) {
    // Process message
});

while (true) {
    sub.consume();
}
```

#### Local Redis Setup

**Docker Command**:
```bash
docker run -d --name redis -p 6379:6379 redis:latest
```

**Docker Compose Configuration**:
```yaml
version: "3"
services:
  redis:
    image: redis:7.0
    ports:
      - "6379:6379"
    volumes:
      - ./redis_data:/data
```

### 7. Error Handling and Reliability

#### Error Handling Strategy

**TWS API Error Delivery**: Via `EWrapper::error` method

**Common Error Codes**:
- `502`: Socket connection failed
- `10089`: Market data subscription required
- `2104/2106/2158`: Connection notifications

**Best Practices**:
- Handle all error codes explicitly
- Log detailed diagnostics with context
- Implement error code to human-readable message mapping

#### Reconnection Logic

**TWS Reconnection**:
- Detect connection loss via `isConnected()` method
- Gracefully disconnect on TWS restart (e.g., daily maintenance)
- Wait for TWS availability with exponential backoff
- Reconnect and re-establish all subscriptions
- Synchronize state after reconnection

**Redis Reconnection**:
- Implement automatic reconnection with backoff
- Queue messages during disconnection (with size limits)
- Resume publishing when connection restored

#### Rate Limit Handling

**Monitoring**:
- Watch for pacing violations
- Track request rates per second

**Mitigation**:
- Implement exponential backoff on rate limit errors
- Request throttling/queue mechanism
- Log rate limit occurrences for capacity planning

**Consequences of Exceeding Limits**:
- Connection termination
- Delayed data delivery
- Temporary subscription suspension

#### Reliability Checklist
- ✅ Use `isConnected()` to verify connection status
- ✅ Handle all error codes explicitly
- ✅ Log detailed diagnostics
- ✅ Test reconnection logic using TWS paper trading
- ✅ Simulate failures in testing environment

---

## System Architecture

### Data Flow Diagram
```
[TWS/IB Gateway] 
    ↕ TCP
[C++ Microservice]
    ↓ JSON Pub/Sub
[Redis]
    ↓ Pub/Sub
[FastAPI]
    ↓ WebSocket
[Vue.js]
```

### Component Responsibilities

1. **TWS/IB Gateway**: Market data source
2. **C++ Microservice**: Data normalization and distribution bridge
3. **Redis**: High-performance message broker
4. **FastAPI**: WebSocket server and business logic
5. **Vue.js**: User interface and visualization

---

## Reference Implementation

### ⚠️ CORRECTED: Producer-Consumer Architecture with Tick-by-Tick

**Note**: This replaces the previous incorrect example that used `reqMktData` and blocked the EReader thread.

```cpp
#include "EWrapper.h"
#include "EClientSocket.h"
#include "EReader.h"
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <sw/redis++/redis++.h>
#include <moodycamel/concurrentqueue.h>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <mutex>

// Tick update message for queue
enum class TickUpdateType { BidAsk, AllLast };

struct TickUpdate {
    int tickerId;
    TickUpdateType type;
    long timestamp;
    
    // BidAsk fields
    double bidPrice = 0.0;
    int bidSize = 0;
    double askPrice = 0.0;
    int askSize = 0;
    
    // AllLast fields
    double lastPrice = 0.0;
    int lastSize = 0;
    bool pastLimit = false;
};

// Instrument state
struct InstrumentState {
    std::string symbol;
    int conId;
    double bidPrice = 0.0;
    int bidSize = 0;
    double askPrice = 0.0;
    int askSize = 0;
    double lastPrice = 0.0;
    int lastSize = 0;
    long timestamp = 0;
    std::string exchange;
    bool pastLimit = false;
};

class MyWrapper : public EWrapper {
public:
    MyWrapper(moodycamel::ConcurrentQueue<TickUpdate>& queue) 
        : tickQueue(queue) {}
    
    // CORRECT: Use tickByTickBidAsk (not tickPrice)
    void tickByTickBidAsk(int reqId, long time, double bidPrice, double askPrice,
                          int bidSize, int askSize, const TickAttribBidAsk& attribs) override {
        TickUpdate update;
        update.tickerId = reqId;
        update.type = TickUpdateType::BidAsk;
        update.timestamp = time;
        update.bidPrice = bidPrice;
        update.askPrice = askPrice;
        update.bidSize = bidSize;
        update.askSize = askSize;
        
        // CRITICAL: Just enqueue, do NOT serialize or publish here
        tickQueue.enqueue(update);
    }
    
    // CORRECT: Use tickByTickAllLast (not tickPrice)
    void tickByTickAllLast(int reqId, int tickType, long time, double price,
                           int size, const TickAttribLast& attribs, 
                           const std::string& exchange, const std::string& specialConditions) override {
        TickUpdate update;
        update.tickerId = reqId;
        update.type = TickUpdateType::AllLast;
        update.timestamp = time;
        update.lastPrice = price;
        update.lastSize = size;
        update.pastLimit = attribs.pastLimit;
        
        // CRITICAL: Just enqueue, do NOT serialize or publish here
        tickQueue.enqueue(update);
    }
    
    // Implement other required EWrapper methods...
    void error(int id, int errorCode, const std::string& errorString) override {
        // Error handling logic
    }
    
private:
    moodycamel::ConcurrentQueue<TickUpdate>& tickQueue;
};

// Redis Worker Thread (Consumer)
class RedisWorker {
public:
    RedisWorker(moodycamel::ConcurrentQueue<TickUpdate>& queue)
        : tickQueue(queue), redis("tcp://127.0.0.1:6379"), running(true) {}
    
    void start() {
        workerThread = std::thread(&RedisWorker::workerLoop, this);
    }
    
    void stop() {
        running = false;
        if (workerThread.joinable()) {
            workerThread.join();
        }
    }
    
private:
    void workerLoop() {
        TickUpdate update;
        while (running) {
            if (tickQueue.try_dequeue(update)) {
                processUpdate(update);
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    }
    
    void processUpdate(const TickUpdate& update) {
        std::lock_guard<std::mutex> lock(stateMutex);
        
        // Update instrument state
        InstrumentState& state = instrumentStates[update.tickerId];
        
        if (update.type == TickUpdateType::BidAsk) {
            state.bidPrice = update.bidPrice;
            state.askPrice = update.askPrice;
            state.bidSize = update.bidSize;
            state.askSize = update.askSize;
        } else if (update.type == TickUpdateType::AllLast) {
            state.lastPrice = update.lastPrice;
            state.lastSize = update.lastSize;
            state.pastLimit = update.pastLimit;
        }
        
        state.timestamp = update.timestamp;
        
        // Serialize complete state to JSON
        std::string json = serializeState(state);
        
        // Publish to Redis (blocking I/O is OK on this thread)
        redis.publish("market_ticks", json);
    }
    
    std::string serializeState(const InstrumentState& state) {
        using namespace rapidjson;
        StringBuffer s;
        Writer<StringBuffer> writer(s);
        
        writer.StartObject();
        writer.Key("instrument"); writer.String(state.symbol.c_str());
        writer.Key("conId"); writer.Int(state.conId);
        writer.Key("timestamp"); writer.Int64(state.timestamp); // Use market timestamp
        
        writer.Key("price");
        writer.StartObject();
        writer.Key("bid"); writer.Double(state.bidPrice);
        writer.Key("ask"); writer.Double(state.askPrice);
        writer.Key("last"); writer.Double(state.lastPrice);
        writer.EndObject();
        
        writer.Key("size");
        writer.StartObject();
        writer.Key("bid"); writer.Int(state.bidSize);
        writer.Key("ask"); writer.Int(state.askSize);
        writer.Key("last"); writer.Int(state.lastSize);
        writer.EndObject();
        
        writer.Key("exchange"); writer.String(state.exchange.c_str());
        writer.EndObject();
        
        return s.GetString();
    }
    
    moodycamel::ConcurrentQueue<TickUpdate>& tickQueue;
    sw::redis::Redis redis;
    std::atomic<bool> running;
    std::thread workerThread;
    
    std::unordered_map<int, InstrumentState> instrumentStates;
    std::mutex stateMutex;
};

int main() {
    // Create lock-free queue
    moodycamel::ConcurrentQueue<TickUpdate> tickQueue;
    
    // Create and start Redis worker (Consumer)
    RedisWorker redisWorker(tickQueue);
    redisWorker.start();
    
    // Create TWS wrapper (Producer)
    MyWrapper wrapper(tickQueue);
    EClientSocket client(&wrapper, nullptr);
    
    // Connect to TWS
    client.eConnect("127.0.0.1", 7497, 0);
    
    // Create and start EReader thread
    auto reader = std::make_unique<EReader>(&client, &wrapper.signal);
    reader->start();
    
    // Subscribe to AAPL tick-by-tick data (CORRECT)
    Contract contract;
    contract.symbol = "AAPL";
    contract.secType = "STK";
    contract.exchange = "SMART";
    contract.currency = "USD";
    
    // CORRECT: Use reqTickByTickData (not reqMktData)
    client.reqTickByTickData(1, contract, "BidAsk", 0, false);
    client.reqTickByTickData(2, contract, "AllLast", 0, false);
    
    // Process messages in main thread
    while (client.isConnected()) {
        wrapper.signal.waitForSignal();
        reader->processMsgs();
    }
    
    // Cleanup
    redisWorker.stop();
    
    return 0;
}
```

### Key Improvements in Corrected Implementation

1. **✅ Threading Model**: Producer-consumer architecture with lock-free queue
2. **✅ Subscription Strategy**: Uses `reqTickByTickData` and implements `tickByTickBidAsk`/`tickByTickAllLast` callbacks
3. **✅ State Management**: Maintains complete instrument state and publishes full snapshots
4. **✅ Timestamp Handling**: Uses market timestamps from TWS (not server clock)
5. **✅ Non-blocking EReader**: Callbacks only enqueue data (< 1μs)
6. **✅ Separated I/O**: Redis publishing happens on dedicated worker thread

### Production Requirements

This corrected example still requires:
- Comprehensive error handling and logging
- Reconnection logic with exponential backoff
- Rate limit tracking and throttling
- Graceful shutdown with proper resource cleanup
- Configuration management (connection params, subscriptions)
- State synchronization on reconnection
- Memory management for queue overflow scenarios

---

## Next Steps

1. **Environment Setup**
   - Install TWS API C++ dependencies
   - Set up Redis Docker container
   - Configure build system (CMake)

2. **Development Phases** (7-day timeline)
   - Days 1-2: TWS connection, EReader threading, and tick-by-tick subscription
   - Days 3-4: Producer-consumer queue architecture and state management
   - Days 5-6: JSON serialization, Redis publishing, and error handling
   - Day 7: Reconnection logic, rate limiting, and CLI replay mode testing

3. **Testing Strategy**
   - Unit tests for JSON serialization
   - Integration tests with TWS paper trading
   - End-to-end latency measurements
   - Failure scenario simulations

4. **Documentation**
   - API documentation
   - Deployment guide
   - Troubleshooting guide

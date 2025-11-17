# TWS-Redis Bridge: Testability Analysis & Testing Strategy

**Date:** November 17, 2025  
**Status:** 🔴 **POOR TESTABILITY** - Major Refactoring Required

---

## Executive Summary

**Current Test Coverage:** ~2% (2 basic serialization tests only)  
**Testable Components:** ~20% (only pure functions testable)  
**Untestable Components:** ~80% (tight coupling, no mocks possible)

**Verdict: Your implementation is largely UNTESTABLE in its current form.**

### Why Current Code is Untestable

| Component | Testability | Reason | Can Mock? |
|-----------|-------------|--------|-----------|
| `TwsClient` callbacks | ❌ **0%** | Directly enqueues to concrete queue | No |
| `redisWorkerLoop()` | ❌ **0%** | Hardcoded Redis dependency | No |
| `RedisPublisher` | ❌ **0%** | Direct redis-plus-plus coupling | No |
| Symbol resolution | ❌ **0%** | Hardcoded if-else in worker loop | No |
| State aggregation | ❌ **0%** | Embedded in worker loop | No |
| Signal handling | ❌ **0%** | Global state, OS signals | No |
| Serialization | ✅ **100%** | Pure functions, no dependencies | N/A |
| Thread coordination | ❌ **0%** | Raw threads, no abstraction | No |

**Critical Issues:**
1. **No Dependency Injection** - All dependencies hardcoded
2. **Tight Coupling** - Every class directly instantiates its dependencies
3. **No Interfaces** - Cannot substitute implementations
4. **Global State** - Shared mutable state prevents isolated testing
5. **Concrete Types** - No abstraction layers

---

## Table of Contents

1. [Component-by-Component Testability Analysis](#1-component-by-component-testability-analysis)
2. [Why Your Code Cannot Be Tested](#2-why-your-code-cannot-be-tested)
3. [Testing Hierarchy & Strategy](#3-testing-hierarchy--strategy)
4. [Required Refactoring for Testability](#4-required-refactoring-for-testability)
5. [Comprehensive Test Plan](#5-comprehensive-test-plan)
6. [Testing Tools & Frameworks](#6-testing-tools--frameworks)
7. [Implementation Roadmap](#7-implementation-roadmap)

---

## 1. Component-by-Component Testability Analysis

### 1.1. TwsClient Class

#### Current Implementation

```cpp
class TwsClient : public EWrapper {
    moodycamel::ConcurrentQueue<TickUpdate>& m_queue;  // Concrete dependency!
    
    void tickByTickBidAsk(int reqId, ...) override {
        TickUpdate update;
        // ... populate update
        m_queue.try_enqueue(update);  // Direct call to concrete queue!
    }
};
```

#### Testability Score: 🔴 **0/10**

**Problems:**

1. **Cannot test callbacks in isolation**
   ```cpp
   // IMPOSSIBLE: Cannot verify callback logic without real queue
   TEST_CASE("tickByTickBidAsk creates correct TickUpdate") {
       moodycamel::ConcurrentQueue<TickUpdate> queue(100);
       TwsClient client(queue);  // Requires real queue
       
       client.tickByTickBidAsk(1001, 1234567890, 100.5, 100.6, 10, 20, ...);
       
       // Cannot inspect what was enqueued without consuming from queue
       TickUpdate update;
       queue.try_dequeue(update);  // Side effect - mutates queue state
       REQUIRE(update.bidPrice == 100.5);  // Fragile
   }
   ```

2. **Cannot mock TWS API**
   - Callbacks are invoked by TWS library, not your code
   - Cannot simulate TWS connection failures
   - Cannot test error scenarios

3. **Symbol mapping race condition untestable**
   ```cpp
   std::unordered_map<int, std::string> m_tickerToSymbol;  // Unprotected!
   // Cannot inject mock map, cannot verify thread-safety
   ```

4. **Cannot test connection lifecycle**
   - `connect()` requires real TWS Gateway running
   - `disconnect()` has side effects on TWS state
   - No way to simulate connection failures

**What You CANNOT Test:**
- ❌ Callback data transformation logic
- ❌ Error handling in callbacks
- ❌ Symbol mapping correctness
- ❌ Thread-safety of shared state
- ❌ Connection state transitions
- ❌ Subscription management

**What You CAN Test (with difficulty):**
- ⚠️ End-to-end integration (requires TWS + Redis)
- ⚠️ Queue receives data (requires real queue instance)

---

### 1.2. RedisPublisher Class

#### Current Implementation

```cpp
class RedisPublisher {
    std::unique_ptr<sw::redis::Redis> m_redis;  // Concrete dependency!
    
public:
    void publish(const std::string& channel, const std::string& message) {
        m_redis->publish(channel, message);  // Direct call to Redis!
    }
};
```

#### Testability Score: 🔴 **1/10**

**Problems:**

1. **Cannot test without Redis running**
   ```cpp
   // IMPOSSIBLE: Requires Redis container
   TEST_CASE("RedisPublisher publishes to correct channel") {
       RedisPublisher redis("tcp://127.0.0.1:6379");  // Needs real Redis!
       
       redis.publish("TWS:TICKS:AAPL", "{\"price\": 100}");
       
       // How do we verify without subscribing from another process?
   }
   ```

2. **Cannot simulate network failures**
   - Connection timeout
   - Redis crash mid-publish
   - Network partition

3. **Cannot verify retry logic**
   - No retry mechanism implemented
   - No way to inject failure scenarios

4. **Cannot measure performance in tests**
   - Actual publish latency depends on network
   - Cannot isolate logic performance

**What You CANNOT Test:**
- ❌ Publishing logic without Redis
- ❌ Error handling (connection failures)
- ❌ Channel naming correctness
- ❌ Message format validation
- ❌ Performance characteristics
- ❌ Reconnection behavior

**What You CAN Test:**
- ⚠️ Integration test with real Redis (slow, brittle)

---

### 1.3. Worker Loop (redisWorkerLoop)

#### Current Implementation

```cpp
void redisWorkerLoop(moodycamel::ConcurrentQueue<TickUpdate>& queue,
                     RedisPublisher& redis,  // Concrete class!
                     std::atomic<bool>& running) {
    
    // HARDCODED symbol resolution
    std::string symbol = "UNKNOWN";
    if (update.tickerId == 1001) symbol = "AAPL";
    // ... 7 more hardcoded mappings
    
    // State aggregation logic
    auto& state = stateMap[symbol];
    if (update.type == TickUpdateType::BidAsk) {
        state.bidPrice = update.bidPrice;
        // ...
    }
    
    // Publish
    redis.publish(channel, json);
}
```

#### Testability Score: 🔴 **0/10**

**Problems:**

1. **Cannot test state aggregation logic**
   ```cpp
   // IMPOSSIBLE: Worker loop has no return value, side effects only
   TEST_CASE("Worker aggregates BidAsk and AllLast correctly") {
       // How do we verify internal stateMap without publishing?
       // Cannot access stateMap - it's local to worker loop!
   }
   ```

2. **Cannot test symbol resolution**
   - Hardcoded if-else statements
   - Cannot inject different mappings for testing
   - Cannot verify behavior for unknown tickerId

3. **Cannot test without real Redis**
   - Publishes to concrete RedisPublisher
   - Cannot mock/spy on publish calls
   - Cannot verify publish wasn't called (negative test)

4. **Cannot test threading behavior**
   - Function signature requires actual thread
   - Cannot simulate race conditions
   - Cannot verify shutdown behavior

5. **Cannot test individual message types**
   - Bar vs BidAsk vs AllLast logic all interleaved
   - Cannot test one path without triggering others

**What You CANNOT Test:**
- ❌ State aggregation logic
- ❌ Symbol resolution correctness
- ❌ Message routing (Bar vs Tick)
- ❌ Publishing decision logic
- ❌ Error handling
- ❌ Performance characteristics
- ❌ Shutdown behavior

**What You CAN Test:**
- ⚠️ End-to-end with real queue + real Redis (integration only)

---

### 1.4. Serialization Functions

#### Current Implementation

```cpp
std::string serializeState(const InstrumentState& state);
std::string serializeBarData(const std::string& symbol, const TickUpdate& update);
```

#### Testability Score: ✅ **10/10**

**Why This Works:**

```cpp
// PERFECT: Pure function, no dependencies, easy to test
TEST_CASE("serializeState produces valid JSON") {
    InstrumentState state;
    state.symbol = "AAPL";
    state.bidPrice = 100.5;
    state.askPrice = 100.6;
    
    std::string json = serializeState(state);  // Pure function!
    
    REQUIRE(!json.empty());
    REQUIRE(json.find("\"instrument\":\"AAPL\"") != std::string::npos);
    REQUIRE(json.find("\"bid\":100.5") != std::string::npos);
}
```

**What You CAN Test:**
- ✅ JSON schema correctness
- ✅ Edge cases (empty state, max values, special chars)
- ✅ Performance (serialization latency)
- ✅ Error handling (NaN, infinity)

**This is the ONLY testable component in your codebase.**

---

### 1.5. Main Function & Application Lifecycle

#### Current Implementation

```cpp
int main() {
    std::signal(SIGINT, signalHandler);  // Global state!
    
    ConcurrentQueue<TickUpdate> queue(10000);
    RedisPublisher redis(REDIS_URI);  // Direct instantiation!
    TwsClient client(queue);  // Direct instantiation!
    
    std::thread workerThread(redisWorkerLoop, ...);  // Raw thread!
    
    client.subscribeHistoricalBars("SPY", 2001);  // Hardcoded!
    
    while (g_running.load() && client.isConnected()) { ... }
}
```

#### Testability Score: 🔴 **0/10**

**Problems:**

1. **Cannot instantiate Application**
   - `main()` is entry point, not a class
   - Cannot call `main()` from tests
   - All logic is procedural

2. **Global state prevents multiple instances**
   ```cpp
   static std::atomic<bool> g_running{true};  // Global!
   
   // IMPOSSIBLE: Cannot run two tests in parallel
   TEST_CASE("Test scenario A") {
       // main() modifies g_running
   }
   
   TEST_CASE("Test scenario B") {
       // g_running already modified by previous test!
   }
   ```

3. **Cannot inject configuration**
   - Host, port, symbols all hardcoded
   - Cannot test different configurations
   - Cannot test error scenarios (invalid port)

4. **Cannot test shutdown behavior**
   - Requires sending OS signal to process
   - Cannot verify clean shutdown in unit test
   - Cannot test shutdown timeout

**What You CANNOT Test:**
- ❌ Application startup
- ❌ Component initialization
- ❌ Configuration loading
- ❌ Thread coordination
- ❌ Shutdown sequence
- ❌ Signal handling
- ❌ Error recovery

---

## 2. Why Your Code Cannot Be Tested

### 2.1. Fundamental Issue: No Dependency Injection

**Current Pattern (UNTESTABLE):**

```cpp
class TwsClient {
    // Concrete dependency injected via constructor reference
    moodycamel::ConcurrentQueue<TickUpdate>& m_queue;
    
public:
    TwsClient(moodycamel::ConcurrentQueue<TickUpdate>& queue)  // Concrete type!
        : m_queue(queue) {}
    
    void tickByTickBidAsk(...) {
        m_queue.try_enqueue(update);  // Calls concrete implementation
    }
};
```

**Why This Fails:**
- Cannot substitute mock queue for testing
- Cannot spy on enqueue calls
- Cannot inject fake queue that records calls
- Must use real queue with actual memory allocation

**Correct Pattern (TESTABLE):**

```cpp
// 1. Define interface (abstract base class)
class ITickSink {
public:
    virtual ~ITickSink() = default;
    virtual bool enqueue(const TickUpdate& update) = 0;
};

// 2. Implement adapter for real queue
class QueueTickSink : public ITickSink {
    moodycamel::ConcurrentQueue<TickUpdate>& m_queue;
public:
    explicit QueueTickSink(moodycamel::ConcurrentQueue<TickUpdate>& queue)
        : m_queue(queue) {}
    
    bool enqueue(const TickUpdate& update) override {
        return m_queue.try_enqueue(update);
    }
};

// 3. TwsClient depends on interface
class TwsClient {
    ITickSink& m_tickSink;  // Interface!
public:
    TwsClient(ITickSink& sink) : m_tickSink(sink) {}
    
    void tickByTickBidAsk(...) {
        m_tickSink.enqueue(update);  // Calls interface method
    }
};

// 4. Test with mock
class MockTickSink : public ITickSink {
public:
    std::vector<TickUpdate> captured;
    
    bool enqueue(const TickUpdate& update) override {
        captured.push_back(update);
        return true;
    }
};

// NOW TESTABLE!
TEST_CASE("TwsClient tickByTickBidAsk creates correct update") {
    MockTickSink mockSink;
    TwsClient client(mockSink);
    
    client.tickByTickBidAsk(1001, 1234567890, 100.5, 100.6, 10, 20, ...);
    
    REQUIRE(mockSink.captured.size() == 1);
    REQUIRE(mockSink.captured[0].bidPrice == 100.5);
    REQUIRE(mockSink.captured[0].askPrice == 100.6);
}
```

---

### 2.2. No Seams for Testing

**Definition of "Seam":** A place where you can alter behavior without modifying code.

**Your Code Has Zero Seams:**

```cpp
void redisWorkerLoop(...) {
    // HARDCODED: No seam to change symbol resolution
    if (update.tickerId == 1001) symbol = "AAPL";
    
    // HARDCODED: No seam to change state aggregation
    auto& state = stateMap[symbol];
    state.bidPrice = update.bidPrice;
    
    // HARDCODED: No seam to change publishing
    redis.publish(channel, json);
}
```

**How to Add Seams:**

```cpp
// 1. Strategy Pattern: Seam for symbol resolution
class ISymbolResolver {
public:
    virtual ~ISymbolResolver() = default;
    virtual std::string resolve(int tickerId) const = 0;
};

// 2. Strategy Pattern: Seam for state aggregation
class ITickHandler {
public:
    virtual ~ITickHandler() = default;
    virtual void handle(const TickUpdate& update, InstrumentState& state) = 0;
};

// 3. Interface: Seam for publishing
class IMessagePublisher {
public:
    virtual ~IMessagePublisher() = default;
    virtual void publish(const std::string& channel, const std::string& msg) = 0;
};

// Worker loop with seams
class RedisWorker {
    ISymbolResolver& m_resolver;     // Seam 1
    ITickHandler& m_handler;         // Seam 2
    IMessagePublisher& m_publisher;  // Seam 3
    
public:
    void processUpdate(const TickUpdate& update) {
        std::string symbol = m_resolver.resolve(update.tickerId);  // Seam!
        InstrumentState& state = m_states[symbol];
        m_handler.handle(update, state);  // Seam!
        
        std::string json = serializeState(state);
        m_publisher.publish("TWS:TICKS:" + symbol, json);  // Seam!
    }
};

// NOW TESTABLE: Can inject mocks at all 3 seams
```

---

### 2.3. Tight Coupling to Implementation Details

**Example: TwsClient knows about ConcurrentQueue**

```cpp
// PROBLEM: TwsClient is coupled to moodycamel queue implementation
class TwsClient {
    moodycamel::ConcurrentQueue<TickUpdate>& m_queue;  // Specific library!
};
```

**Impact:**
- Cannot test without moodycamel library
- Cannot switch to different queue (boost, folly, std::queue)
- Cannot inject mock queue
- Unit tests require linking full moodycamel library

**Solution: Depend on abstraction, not concretion**

```cpp
// GOOD: TwsClient doesn't care about queue implementation
class TwsClient {
    ITickSink& m_sink;  // Interface - could be queue, file, mock, anything!
};
```

---

### 2.4. Side Effects Everywhere

**Your code is all side effects, no return values:**

```cpp
// IMPOSSIBLE TO TEST: No return value, only side effects
void tickByTickBidAsk(...) {
    TickUpdate update;
    // ... populate update
    m_queue.try_enqueue(update);  // Side effect: modifies queue
    // No return value - how do we verify correctness?
}

void redisWorkerLoop(...) {
    // Side effect: modifies stateMap
    // Side effect: calls redis.publish()
    // Side effect: prints to console
    // No return value - cannot verify behavior
}
```

**Why This Matters:**
- Cannot verify output without observing side effects
- Side effects require real dependencies (queue, Redis, etc.)
- Cannot isolate logic from I/O

**Functional Approach (More Testable):**

```cpp
// TESTABLE: Pure function, returns result
TickUpdate createTickUpdate(int reqId, time_t time, double bid, double ask, ...) {
    TickUpdate update;
    update.tickerId = reqId;
    update.timestamp = time * 1000;
    update.bidPrice = bid;
    update.askPrice = ask;
    return update;  // Return value - easy to verify!
}

// Callback delegates to pure function
void tickByTickBidAsk(int reqId, time_t time, double bid, double ask, ...) {
    TickUpdate update = createTickUpdate(reqId, time, bid, ask, ...);
    m_sink.enqueue(update);  // Separated: pure logic + side effect
}

// NOW TESTABLE:
TEST_CASE("createTickUpdate sets correct fields") {
    TickUpdate update = createTickUpdate(1001, 1234567890, 100.5, 100.6, 10, 20);
    
    REQUIRE(update.tickerId == 1001);
    REQUIRE(update.bidPrice == 100.5);
    // No mocks needed!
}
```

---

## 3. Testing Hierarchy & Strategy

### 3.1. The Testing Pyramid

```
           /\
          /  \
         / E2E \         ← 5% of tests (slow, expensive, brittle)
        /------\
       /        \
      /   Integ  \       ← 15% of tests (medium speed, Docker)
     /------------\
    /              \
   /   Unit Tests   \    ← 80% of tests (fast, isolated, many)
  /------------------\
```

**Your Current Reality:**

```
           /\
          /E2E\          ← 100% of "testing" (manual, slow)
         /-----\
        /   0   \        ← 0% integration tests
       /---------\
      /     0     \      ← 0% unit tests (except 2 serialization tests)
     /-------------\
```

**Goal After Refactoring:**

```
           /\
          / 5% \         ← End-to-end: TWS → Queue → Redis
         /------\
        /  15%  \        ← Integration: Component pairs (TwsClient+Queue)
       /--------\
      /   80%    \       ← Unit: Pure logic, mocked dependencies
     /------------\
```

---

### 3.2. Unit Testing Strategy

**What Should Be Unit Tested:**

| Component | Test Count | What to Test |
|-----------|------------|--------------|
| Callback data transformation | 20+ tests | Each callback type, edge cases |
| State aggregation logic | 15+ tests | BidAsk + AllLast merge, incomplete state |
| Symbol resolution | 10+ tests | Known/unknown tickerId, multi-mapping |
| Serialization | 30+ tests | Valid/invalid states, edge cases, performance |
| Configuration parsing | 15+ tests | Valid/invalid configs, defaults, overrides |
| Error handling | 20+ tests | Each error code, fatal vs transient |
| Channel naming | 5+ tests | Ticks vs bars, symbol validation |

**Total Target: 100+ unit tests**

---

### 3.3. Integration Testing Strategy

**What Should Be Integration Tested:**

| Integration Point | Test Count | What to Test |
|-------------------|------------|--------------|
| TwsClient → Queue | 10+ tests | Callback → enqueue flow |
| Queue → Worker | 10+ tests | Dequeue → state update |
| Worker → Redis | 10+ tests | Serialize → publish flow |
| Configuration → Components | 5+ tests | Config loads correctly |
| Error scenarios | 15+ tests | Disconnect, reconnect, failures |

**Total Target: 50+ integration tests**

**Setup Required:**
- Docker Compose with Redis
- Mock TWS server (simulate callbacks)
- Fixture data (sample tick files)

---

### 3.4. End-to-End Testing Strategy

**What Should Be E2E Tested:**

| Scenario | Test Count | What to Test |
|----------|------------|--------------|
| Happy path | 3 tests | Live market data flows correctly |
| Multi-instrument | 2 tests | 3+ concurrent subscriptions |
| Reconnection | 3 tests | TWS restart, network failure |
| Performance | 2 tests | Latency < 50ms, throughput > 10K msg/sec |

**Total Target: 10 E2E tests**

**Setup Required:**
- Paper trading TWS Gateway
- Real Redis instance
- Test instruments (SPY, QQQ, IWM)

---

## 4. Required Refactoring for Testability

### 4.1. Phase 1: Extract Interfaces (Priority: Critical)

**Before:**
```cpp
class TwsClient {
    moodycamel::ConcurrentQueue<TickUpdate>& m_queue;
};
```

**After:**
```cpp
// 1. Define interface
class ITickSink {
public:
    virtual ~ITickSink() = default;
    virtual bool enqueue(const TickUpdate& update) = 0;
};

// 2. Adapter for real queue
class QueueTickSink : public ITickSink {
    moodycamel::ConcurrentQueue<TickUpdate>& m_queue;
public:
    bool enqueue(const TickUpdate& update) override {
        return m_queue.try_enqueue(update);
    }
};

// 3. Mock for testing
class MockTickSink : public ITickSink {
public:
    std::vector<TickUpdate> captured;
    int enqueueCallCount = 0;
    
    bool enqueue(const TickUpdate& update) override {
        captured.push_back(update);
        ++enqueueCallCount;
        return true;
    }
    
    void clear() { captured.clear(); enqueueCallCount = 0; }
};

// 4. TwsClient uses interface
class TwsClient {
    ITickSink& m_sink;
public:
    explicit TwsClient(ITickSink& sink) : m_sink(sink) {}
};
```

**Estimated Time:** 4 hours

---

### 4.2. Phase 2: Extract Pure Functions (Priority: High)

**Before:**
```cpp
void tickByTickBidAsk(int reqId, time_t time, ...) {
    TickUpdate update;
    update.tickerId = reqId;
    update.timestamp = time * 1000;
    update.bidPrice = bidPrice;
    // ... 10 more lines
    m_queue.try_enqueue(update);  // Side effect mixed with logic
}
```

**After:**
```cpp
// Pure function (no side effects)
TickUpdate createBidAskUpdate(int reqId, time_t time, double bid, double ask,
                               int bidSize, int askSize) {
    TickUpdate update;
    update.tickerId = reqId;
    update.type = TickUpdateType::BidAsk;
    update.timestamp = time * 1000;
    update.bidPrice = bid;
    update.askPrice = ask;
    update.bidSize = bidSize;
    update.askSize = askSize;
    return update;
}

// Callback delegates to pure function
void tickByTickBidAsk(int reqId, time_t time, ...) {
    TickUpdate update = createBidAskUpdate(reqId, time, bidPrice, askPrice,
                                            bidSize, askSize);
    m_sink.enqueue(update);  // Separated side effect
}

// NOW TESTABLE:
TEST_CASE("createBidAskUpdate sets all fields correctly") {
    TickUpdate update = createBidAskUpdate(1001, 1234567890, 100.5, 100.6, 10, 20);
    
    REQUIRE(update.tickerId == 1001);
    REQUIRE(update.type == TickUpdateType::BidAsk);
    REQUIRE(update.timestamp == 1234567890000);  // Converted to ms
    REQUIRE(update.bidPrice == 100.5);
    REQUIRE(update.askPrice == 100.6);
    REQUIRE(update.bidSize == 10);
    REQUIRE(update.askSize == 20);
}
```

**Estimated Time:** 6 hours (all callbacks)

---

### 4.3. Phase 3: Extract State Aggregation (Priority: High)

**Before:**
```cpp
void redisWorkerLoop(...) {
    std::unordered_map<std::string, InstrumentState> stateMap;  // Local!
    
    while (running.load()) {
        if (queue.try_dequeue(update)) {
            auto& state = stateMap[symbol];
            if (update.type == TickUpdateType::BidAsk) {
                state.bidPrice = update.bidPrice;
                // ... embedded logic
            }
        }
    }
}
```

**After:**
```cpp
// 1. Extract to class
class StateAggregator {
    std::unordered_map<std::string, InstrumentState> m_states;
    
public:
    void applyUpdate(const std::string& symbol, const TickUpdate& update) {
        InstrumentState& state = m_states[symbol];
        
        if (update.type == TickUpdateType::BidAsk) {
            state.bidPrice = update.bidPrice;
            state.askPrice = update.askPrice;
            state.bidSize = update.bidSize;
            state.askSize = update.askSize;
            state.quoteTimestamp = update.timestamp;
            state.hasQuote = true;
        }
        // ... other types
    }
    
    const InstrumentState& getState(const std::string& symbol) const {
        return m_states.at(symbol);
    }
    
    bool isComplete(const std::string& symbol) const {
        const auto& state = m_states.at(symbol);
        return state.hasQuote && state.hasTrade;
    }
};

// NOW TESTABLE:
TEST_CASE("StateAggregator merges BidAsk and AllLast") {
    StateAggregator aggregator;
    
    TickUpdate bidAsk;
    bidAsk.type = TickUpdateType::BidAsk;
    bidAsk.bidPrice = 100.5;
    bidAsk.askPrice = 100.6;
    
    aggregator.applyUpdate("AAPL", bidAsk);
    REQUIRE(aggregator.getState("AAPL").bidPrice == 100.5);
    REQUIRE_FALSE(aggregator.isComplete("AAPL"));  // Still missing AllLast
    
    TickUpdate allLast;
    allLast.type = TickUpdateType::AllLast;
    allLast.lastPrice = 100.55;
    
    aggregator.applyUpdate("AAPL", allLast);
    REQUIRE(aggregator.getState("AAPL").lastPrice == 100.55);
    REQUIRE(aggregator.isComplete("AAPL"));  // Now complete!
}
```

**Estimated Time:** 8 hours

---

### 4.4. Phase 4: Extract Symbol Resolution (Priority: Medium)

**Before:**
```cpp
// Hardcoded in worker loop
std::string symbol = "UNKNOWN";
if (update.tickerId == 1001) symbol = "AAPL";
else if (update.tickerId == 1002) symbol = "SPY";
// ... 7 more hardcoded mappings
```

**After:**
```cpp
// 1. Interface
class ISymbolResolver {
public:
    virtual ~ISymbolResolver() = default;
    virtual std::string resolve(int tickerId) const = 0;
    virtual void registerSymbol(int tickerId, const std::string& symbol) = 0;
};

// 2. Implementation
class ConfigurableSymbolResolver : public ISymbolResolver {
    std::unordered_map<int, std::string> m_mapping;
    mutable std::shared_mutex m_mutex;  // Thread-safe!
    
public:
    void registerSymbol(int tickerId, const std::string& symbol) override {
        std::unique_lock lock(m_mutex);
        m_mapping[tickerId] = symbol;
    }
    
    std::string resolve(int tickerId) const override {
        std::shared_lock lock(m_mutex);
        auto it = m_mapping.find(tickerId);
        return (it != m_mapping.end()) ? it->second : "UNKNOWN";
    }
};

// 3. Mock for testing
class MockSymbolResolver : public ISymbolResolver {
    std::unordered_map<int, std::string> m_mapping;
    mutable int resolveCallCount = 0;
    
public:
    void registerSymbol(int tickerId, const std::string& symbol) override {
        m_mapping[tickerId] = symbol;
    }
    
    std::string resolve(int tickerId) const override {
        ++resolveCallCount;
        auto it = m_mapping.find(tickerId);
        return (it != m_mapping.end()) ? it->second : "UNKNOWN";
    }
    
    int getResolveCallCount() const { return resolveCallCount; }
};

// NOW TESTABLE:
TEST_CASE("SymbolResolver returns correct symbol") {
    ConfigurableSymbolResolver resolver;
    resolver.registerSymbol(1001, "AAPL");
    resolver.registerSymbol(1002, "SPY");
    
    REQUIRE(resolver.resolve(1001) == "AAPL");
    REQUIRE(resolver.resolve(1002) == "SPY");
    REQUIRE(resolver.resolve(9999) == "UNKNOWN");
}

TEST_CASE("SymbolResolver is thread-safe") {
    ConfigurableSymbolResolver resolver;
    std::atomic<int> errors{0};
    
    // Thread 1: Write
    std::thread writer([&] {
        for (int i = 0; i < 1000; ++i) {
            resolver.registerSymbol(i, "SYM" + std::to_string(i));
        }
    });
    
    // Thread 2: Read
    std::thread reader([&] {
        for (int i = 0; i < 1000; ++i) {
            std::string result = resolver.resolve(i);
            if (result != "SYM" + std::to_string(i) && result != "UNKNOWN") {
                ++errors;
            }
        }
    });
    
    writer.join();
    reader.join();
    
    REQUIRE(errors == 0);  // No corrupted reads
}
```

**Estimated Time:** 4 hours

---

## 5. Comprehensive Test Plan

### 5.1. Unit Test Suite

#### 5.1.1. TwsClient Callback Tests (20 tests)

```cpp
// tests/unit/test_tws_callbacks.cpp

TEST_CASE("createBidAskUpdate converts timestamp to milliseconds") {
    TickUpdate update = createBidAskUpdate(1001, 1234567890, 100.5, 100.6, 10, 20);
    REQUIRE(update.timestamp == 1234567890000);  // seconds → ms
}

TEST_CASE("createBidAskUpdate sets correct type") {
    TickUpdate update = createBidAskUpdate(1001, 1234567890, 100.5, 100.6, 10, 20);
    REQUIRE(update.type == TickUpdateType::BidAsk);
}

TEST_CASE("createAllLastUpdate preserves pastLimit flag") {
    TickUpdate update = createAllLastUpdate(1001, 1234567890, 100.55, 50, true);
    REQUIRE(update.pastLimit == true);
}

TEST_CASE("createBarUpdate copies all OHLCV fields") {
    TickUpdate update = createBarUpdate(1001, 1234567890, 100.0, 101.0, 99.0, 100.5, 10000, 100.3, 150);
    REQUIRE(update.open == 100.0);
    REQUIRE(update.high == 101.0);
    REQUIRE(update.low == 99.0);
    REQUIRE(update.close == 100.5);
    REQUIRE(update.volume == 10000);
    REQUIRE(update.wap == 100.3);
    REQUIRE(update.barCount == 150);
}

TEST_CASE("TwsClient enqueues BidAsk update via mock sink") {
    MockTickSink mockSink;
    TwsClient client(mockSink);
    
    client.tickByTickBidAsk(1001, 1234567890, 100.5, 100.6, 10, 20, TickAttribBidAsk());
    
    REQUIRE(mockSink.captured.size() == 1);
    REQUIRE(mockSink.captured[0].tickerId == 1001);
    REQUIRE(mockSink.captured[0].bidPrice == 100.5);
}

TEST_CASE("TwsClient enqueues AllLast update via mock sink") {
    MockTickSink mockSink;
    TwsClient client(mockSink);
    
    client.tickByTickAllLast(1001, 1, 1234567890, 100.55, 50, TickAttribLast(), "NASDAQ", "");
    
    REQUIRE(mockSink.captured.size() == 1);
    REQUIRE(mockSink.captured[0].lastPrice == 100.55);
    REQUIRE(mockSink.captured[0].lastSize == 50);
}

TEST_CASE("TwsClient handles unknown tickerId gracefully") {
    MockTickSink mockSink;
    TwsClient client(mockSink);
    
    // No symbol registered for tickerId 9999
    client.tickByTickBidAsk(9999, 1234567890, 100.5, 100.6, 10, 20, TickAttribBidAsk());
    
    // Should still enqueue (symbol resolution happens in worker)
    REQUIRE(mockSink.captured.size() == 1);
}

// ... 13 more tests for edge cases, error conditions, etc.
```

---

#### 5.1.2. State Aggregation Tests (15 tests)

```cpp
// tests/unit/test_state_aggregation.cpp

TEST_CASE("StateAggregator starts with no symbols") {
    StateAggregator aggregator;
    REQUIRE_THROWS(aggregator.getState("AAPL"));  // Not registered yet
}

TEST_CASE("StateAggregator creates state on first update") {
    StateAggregator aggregator;
    
    TickUpdate bidAsk;
    bidAsk.type = TickUpdateType::BidAsk;
    bidAsk.bidPrice = 100.5;
    
    aggregator.applyUpdate("AAPL", bidAsk);
    
    REQUIRE(aggregator.getState("AAPL").bidPrice == 100.5);
}

TEST_CASE("StateAggregator merges BidAsk and AllLast correctly") {
    StateAggregator aggregator;
    
    TickUpdate bidAsk;
    bidAsk.type = TickUpdateType::BidAsk;
    bidAsk.bidPrice = 100.5;
    bidAsk.askPrice = 100.6;
    bidAsk.timestamp = 1000;
    
    aggregator.applyUpdate("AAPL", bidAsk);
    
    TickUpdate allLast;
    allLast.type = TickUpdateType::AllLast;
    allLast.lastPrice = 100.55;
    allLast.lastSize = 50;
    allLast.timestamp = 1500;
    
    aggregator.applyUpdate("AAPL", allLast);
    
    const auto& state = aggregator.getState("AAPL");
    REQUIRE(state.bidPrice == 100.5);
    REQUIRE(state.askPrice == 100.6);
    REQUIRE(state.lastPrice == 100.55);
    REQUIRE(state.lastSize == 50);
    REQUIRE(state.quoteTimestamp == 1000);
    REQUIRE(state.tradeTimestamp == 1500);
}

TEST_CASE("StateAggregator maintains separate states per symbol") {
    StateAggregator aggregator;
    
    TickUpdate aaplUpdate;
    aaplUpdate.type = TickUpdateType::BidAsk;
    aaplUpdate.bidPrice = 170.0;
    
    TickUpdate spyUpdate;
    spyUpdate.type = TickUpdateType::BidAsk;
    spyUpdate.bidPrice = 450.0;
    
    aggregator.applyUpdate("AAPL", aaplUpdate);
    aggregator.applyUpdate("SPY", spyUpdate);
    
    REQUIRE(aggregator.getState("AAPL").bidPrice == 170.0);
    REQUIRE(aggregator.getState("SPY").bidPrice == 450.0);
}

TEST_CASE("StateAggregator isComplete returns false until both quote and trade") {
    StateAggregator aggregator;
    
    TickUpdate bidAsk;
    bidAsk.type = TickUpdateType::BidAsk;
    bidAsk.bidPrice = 100.5;
    
    aggregator.applyUpdate("AAPL", bidAsk);
    REQUIRE_FALSE(aggregator.isComplete("AAPL"));  // Only quote
    
    TickUpdate allLast;
    allLast.type = TickUpdateType::AllLast;
    allLast.lastPrice = 100.55;
    
    aggregator.applyUpdate("AAPL", allLast);
    REQUIRE(aggregator.isComplete("AAPL"));  // Both quote and trade
}

TEST_CASE("StateAggregator updates existing state on subsequent updates") {
    StateAggregator aggregator;
    
    TickUpdate update1;
    update1.type = TickUpdateType::BidAsk;
    update1.bidPrice = 100.0;
    
    aggregator.applyUpdate("AAPL", update1);
    REQUIRE(aggregator.getState("AAPL").bidPrice == 100.0);
    
    TickUpdate update2;
    update2.type = TickUpdateType::BidAsk;
    update2.bidPrice = 101.0;
    
    aggregator.applyUpdate("AAPL", update2);
    REQUIRE(aggregator.getState("AAPL").bidPrice == 101.0);  // Updated
}

// ... 9 more tests for edge cases
```

---

#### 5.1.3. Symbol Resolution Tests (10 tests)

```cpp
// tests/unit/test_symbol_resolver.cpp

TEST_CASE("ConfigurableSymbolResolver returns UNKNOWN for unregistered tickerId") {
    ConfigurableSymbolResolver resolver;
    REQUIRE(resolver.resolve(9999) == "UNKNOWN");
}

TEST_CASE("ConfigurableSymbolResolver returns correct symbol after registration") {
    ConfigurableSymbolResolver resolver;
    resolver.registerSymbol(1001, "AAPL");
    REQUIRE(resolver.resolve(1001) == "AAPL");
}

TEST_CASE("ConfigurableSymbolResolver handles multiple registrations") {
    ConfigurableSymbolResolver resolver;
    resolver.registerSymbol(1001, "AAPL");
    resolver.registerSymbol(1002, "SPY");
    resolver.registerSymbol(1003, "TSLA");
    
    REQUIRE(resolver.resolve(1001) == "AAPL");
    REQUIRE(resolver.resolve(1002) == "SPY");
    REQUIRE(resolver.resolve(1003) == "TSLA");
}

TEST_CASE("ConfigurableSymbolResolver overwrites existing mapping") {
    ConfigurableSymbolResolver resolver;
    resolver.registerSymbol(1001, "AAPL");
    resolver.registerSymbol(1001, "MSFT");  // Overwrite
    
    REQUIRE(resolver.resolve(1001) == "MSFT");
}

TEST_CASE("ConfigurableSymbolResolver is thread-safe for concurrent reads") {
    ConfigurableSymbolResolver resolver;
    resolver.registerSymbol(1001, "AAPL");
    
    std::atomic<int> mismatches{0};
    
    auto readThread = [&]() {
        for (int i = 0; i < 10000; ++i) {
            if (resolver.resolve(1001) != "AAPL") {
                ++mismatches;
            }
        }
    };
    
    std::thread t1(readThread);
    std::thread t2(readThread);
    std::thread t3(readThread);
    
    t1.join();
    t2.join();
    t3.join();
    
    REQUIRE(mismatches == 0);
}

// ... 5 more tests
```

---

### 5.2. Integration Test Suite

#### 5.2.1. TwsClient → Queue Integration (10 tests)

```cpp
// tests/integration/test_tws_to_queue.cpp

TEST_CASE("End-to-end: TwsClient callback → Queue → dequeue") {
    moodycamel::ConcurrentQueue<TickUpdate> queue(100);
    QueueTickSink sink(queue);
    TwsClient client(sink);
    
    // Simulate TWS callback
    client.tickByTickBidAsk(1001, 1234567890, 100.5, 100.6, 10, 20, TickAttribBidAsk());
    
    // Verify enqueued
    TickUpdate update;
    REQUIRE(queue.try_dequeue(update));
    REQUIRE(update.bidPrice == 100.5);
}

TEST_CASE("Multiple callbacks enqueue multiple updates") {
    moodycamel::ConcurrentQueue<TickUpdate> queue(100);
    QueueTickSink sink(queue);
    TwsClient client(sink);
    
    client.tickByTickBidAsk(1001, 1234567890, 100.5, 100.6, 10, 20, TickAttribBidAsk());
    client.tickByTickAllLast(1001, 1, 1234567890, 100.55, 50, TickAttribLast(), "NASDAQ", "");
    
    TickUpdate update1, update2;
    REQUIRE(queue.try_dequeue(update1));
    REQUIRE(queue.try_dequeue(update2));
    REQUIRE(update1.type == TickUpdateType::BidAsk);
    REQUIRE(update2.type == TickUpdateType::AllLast);
}

// ... 8 more integration tests
```

---

#### 5.2.2. Worker → Redis Integration (10 tests)

```cpp
// tests/integration/test_worker_to_redis.cpp
// Requires Docker Compose with Redis running

TEST_CASE("RedisWorker publishes to correct channel") {
    // Start Redis container
    RedisPublisher redis("tcp://127.0.0.1:6379");
    
    // Subscribe to channel (in separate thread or process)
    // Verify message received
    
    // ... test implementation
}

// ... 9 more integration tests
```

---

### 5.3. End-to-End Test Suite

```cpp
// tests/e2e/test_full_pipeline.cpp

TEST_CASE("E2E: Historical bar data flows from TWS to Redis") {
    // Requires TWS Gateway running on port 7497
    // Requires Redis running on port 6379
    
    // 1. Start application
    // 2. Subscribe to historical bars
    // 3. Verify Redis receives messages
    // 4. Verify JSON schema
    // 5. Verify latency < 50ms
}

// ... 9 more E2E tests
```

---

## 6. Testing Tools & Frameworks

### 6.1. Recommended Stack

| Tool | Purpose | Why |
|------|---------|-----|
| **Catch2** | Unit testing framework | Already integrated, lightweight, modern |
| **Google Mock** | Mocking framework | Industry standard, powerful matchers |
| **Docker Compose** | Integration test environment | Redis, mock TWS server |
| **Valgrind** | Memory leak detection | Catches RAII violations |
| **ThreadSanitizer** | Data race detection | Catches race conditions |
| **AddressSanitizer** | Memory error detection | Catches buffer overflows, use-after-free |
| **Benchmark** | Micro-benchmarking | Google Benchmark library |

### 6.2. CMake Test Configuration

```cmake
# tests/CMakeLists.txt

find_package(Catch2 3 REQUIRED)
find_package(GTest REQUIRED)  # For Google Mock

# Unit tests
add_executable(test_tws_callbacks unit/test_tws_callbacks.cpp)
target_link_libraries(test_tws_callbacks 
    PRIVATE 
    tws_bridge_lib  # Need to extract library from executable
    Catch2::Catch2WithMain
)

# Integration tests
add_executable(test_tws_to_queue integration/test_tws_to_queue.cpp)
target_link_libraries(test_tws_to_queue 
    PRIVATE 
    tws_bridge_lib
    Catch2::Catch2WithMain
)

# E2E tests
add_executable(test_full_pipeline e2e/test_full_pipeline.cpp)
target_link_libraries(test_full_pipeline 
    PRIVATE 
    tws_bridge_lib
    Catch2::Catch2WithMain
)

# Register tests
include(CTest)
include(Catch)
catch_discover_tests(test_tws_callbacks)
catch_discover_tests(test_tws_to_queue)
catch_discover_tests(test_full_pipeline)

# Add sanitizers for test builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=address,undefined,thread)
    add_link_options(-fsanitize=address,undefined,thread)
endif()
```

---

## 7. Implementation Roadmap

### 7.1. Week 1: Refactor for Testability

**Day 1-2: Extract Interfaces**
- [ ] Create `ITickSink` interface + `QueueTickSink` adapter + `MockTickSink`
- [ ] Create `IMessagePublisher` interface + `RedisPublisher` impl + `MockPublisher`
- [ ] Create `ISymbolResolver` interface + implementations
- [ ] Update TwsClient to use `ITickSink`
- [ ] Update worker to use `IMessagePublisher`

**Day 3-4: Extract Pure Functions**
- [ ] Extract `createBidAskUpdate()`, `createAllLastUpdate()`, `createBarUpdate()`
- [ ] Extract `StateAggregator` class from worker loop
- [ ] Extract `RedisWorker` class with injected dependencies

**Day 5: Setup Testing Infrastructure**
- [ ] Add Google Mock to dependencies
- [ ] Create test directory structure
- [ ] Setup Docker Compose for integration tests
- [ ] Configure CMake for tests with sanitizers

---

### 7.2. Week 2: Write Unit Tests

**Day 1: Callback Tests**
- [ ] Write 20 tests for callback data transformation
- [ ] Test all TickUpdate field assignments
- [ ] Test timestamp conversion
- [ ] Test type assignment

**Day 2: State Aggregation Tests**
- [ ] Write 15 tests for state merging
- [ ] Test BidAsk + AllLast merge
- [ ] Test incomplete states
- [ ] Test multi-symbol isolation

**Day 3: Symbol Resolution Tests**
- [ ] Write 10 tests for symbol mapping
- [ ] Test registration/lookup
- [ ] Test thread-safety
- [ ] Test unknown tickerId handling

**Day 4: Serialization Tests**
- [ ] Expand existing 2 tests to 30 tests
- [ ] Test edge cases (empty state, NaN, infinity)
- [ ] Test JSON schema compliance
- [ ] Benchmark serialization performance

**Day 5: Configuration & Error Handling Tests**
- [ ] Write 15 tests for config parsing
- [ ] Write 20 tests for error code handling
- [ ] Test fatal vs transient classification

---

### 7.3. Week 3: Integration & E2E Tests

**Day 1-2: Integration Tests**
- [ ] TwsClient → Queue (10 tests)
- [ ] Queue → Worker (10 tests)
- [ ] Worker → Redis (10 tests)

**Day 3-4: E2E Tests**
- [ ] Happy path (3 tests)
- [ ] Multi-instrument (2 tests)
- [ ] Reconnection (3 tests)
- [ ] Performance (2 tests)

**Day 5: CI/CD Integration**
- [ ] GitHub Actions workflow for tests
- [ ] Docker Compose in CI
- [ ] Test coverage reporting
- [ ] Performance benchmarking in CI

---

## Summary: Path to 100% Testability

**Current State:** 2% testable (only serialization)

**Target State:** 100% testable (all components)

**Key Changes Required:**

1. ✅ Extract interfaces for all dependencies
2. ✅ Extract pure functions from callbacks
3. ✅ Extract business logic from side effects
4. ✅ Inject all dependencies via constructors
5. ✅ Remove global state
6. ✅ Create mock implementations for all interfaces

**Estimated Effort:**
- Refactoring: 2 weeks
- Unit tests: 1 week
- Integration tests: 1 week
- Total: **4 weeks** to achieve comprehensive test coverage

**Benefits:**
- ✅ Can test components in isolation
- ✅ Can verify correctness without external dependencies
- ✅ Can simulate error scenarios
- ✅ Can measure performance characteristics
- ✅ Can refactor with confidence
- ✅ Can add new features without breaking existing functionality

**Your code is currently untestable. Refactor first, then test.**

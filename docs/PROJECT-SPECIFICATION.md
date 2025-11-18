# TWS-Redis Bridge: Project Specification

**Version:** 2.0  
**Date:** November 15, 2025  
**Status:** Draft

<!-- METADATA: scope=full-system, priority=critical, phase=architecture -->

---

## Document Purpose

This specification defines the requirements, architecture, and technical design for the TWS-Redis Bridge—a low-latency C++ microservice that consumes market data from Interactive Brokers' TWS API and publishes to Redis Pub/Sub.

**Document Scope:**
- ✅ Requirements and acceptance criteria
- ✅ System architecture and design decisions
- ✅ Technical design patterns and constraints
- ❌ Implementation code examples (see IMPLEMENTATION-GUIDE.md)
- ❌ Historical decision rationale (see DESIGN-DECISIONS.md)

---

## Quick Navigation: Cross-Reference Index

<!-- METADATA: scope=navigation, priority=critical -->

**📍 Fast Information Discovery:** Use this index to quickly locate topics and understand their relationships.

| Topic | Primary Section | Related Sections | Tags |
|-------|----------------|------------------|------|
| **TWS API Integration** | [§5.1](#51-tws-integration) | [§2.1.2](#212-api-contracts), [§4.2](#42-threading-model), [§5.6](#56-anti-patterns) | `[CRITICAL]`, `tws-api`, `callbacks` |
| **Threading Model** | [§4.2](#42-threading-model) | [§3.1](#31-architecture-pattern), [§5.2](#52-data-processing), [§C.1](#c1-threading-model-cheat-sheet) | `[ARCHITECTURE]`, `performance`, `concurrency` |
| **Lock-Free Queue** | [§3.5 ADR-004](#adr-004-lock-free-queue-for-inter-thread-communication) | [§4.2](#42-threading-model), [§5.2](#52-data-processing) | `[PERFORMANCE]`, `moodycamel`, `spsc` |
| **State Management** | [§4.5](#45-state-management) | [§3.5 ADR-005](#adr-005-state-aggregation-in-consumer-thread), [§5.2](#52-data-processing) | `[ARCHITECTURE]`, `aggregation`, `snapshots` |
| **JSON Serialization** | [§5.4](#54-serialization) | [§3.5 ADR-006](#adr-006-rapidjson-for-high-performance-serialization), [§2.1.3](#213-data-requirements) | `[PERFORMANCE]`, `rapidjson`, `schema` |
| **Redis Pub/Sub** | [§5.3](#53-redis-publishing) | [§3.1 ADR-002](#adr-002-redis-pubsub-messaging-pattern), [§3.4](#34-interface-definitions) | `[ARCHITECTURE]`, `fan-out`, `messaging` |
| **Error Handling** | [§4.4](#44-error-handling-strategy) | [§5.5](#55-key-patterns), [§C.2](#c2-tws-error-code-quick-reference) | `[REQUIRED]`, `reconnection`, `fault-tolerance` |
| **Performance Targets** | [§2.2.1](#221-performance-targets) | [§1.5](#15-success-metrics), [§7.3](#73-performance-testing) | `[PERFORMANCE]`, `latency`, `throughput` |
| **Build System** | [§6](#6-build--devops) | [§4.1](#41-technology-stack), [§C.3](#c3-build--test-commands) | `conan`, `cmake`, `makefile` |
| **Testing Strategy** | [§7](#7-testing-strategy) | [§7.4](#74-acceptance-criteria), [§C.4](#c4-testing-workflow) | `[REQUIRED]`, `catch2`, `tdd` |
| **Architectural Decisions** | [§3.5](#35-architecture-decisions) | [§3.1](#31-architecture-pattern), [§4](#4-technical-design) | `[ARCHITECTURE]`, `adr`, `rationale` |
| **Data Flow** | [§3.3](#33-data-flow) | [§3.2](#32-component-diagram), [§4.2](#42-threading-model) | `[ARCHITECTURE]`, `pipeline`, `latency-budget` |
| **Reconnection Logic** | [§5.5](#55-key-patterns) | [§2.2.2](#222-reliability-expectations), [§4.4](#44-error-handling-strategy) | `[REQUIRED]`, `exponential-backoff`, `fault-tolerance` |
| **Rate Limiting** | [§5.5](#55-key-patterns) | [§2.2.2](#222-reliability-expectations), [§C.2](#c2-tws-error-code-quick-reference) | `tws-constraints`, `api-limits` |
| **Anti-Patterns** | [§5.6](#56-anti-patterns) | [§4.2](#42-threading-model), [§5.1](#51-tws-integration) | `[ANTI-PATTERN]`, `[PITFALL]`, `best-practices` |
| **Tick-by-Tick API** | [§3.5 ADR-008](#adr-008-tws-tick-by-tick-api-selection) | [§2.1.2](#212-api-contracts), [§5.1](#51-tws-integration) | `[CRITICAL]`, `tws-api`, `real-time` |
| **Contract Definition** | [§2.1.2](#212-api-contracts) | [§5.1](#51-tws-integration), [§7.2](#72-integration-testing) | `tws-api`, `instrument-spec` |
| **Channel Naming** | [§3.4.1](#341-redis-channels) | [§5.3](#53-redis-publishing), [§C.5](#c5-json-schema-quick-reference) | `redis`, `pub-sub`, `conventions` |
| **JSON Schema** | [§3.4.2](#342-json-schemas) | [§2.1.3](#213-data-requirements), [§5.4](#54-serialization), [§C.5](#c5-json-schema-quick-reference) | `schema`, `api-contract` |
| **Acceptance Criteria** | [§7.4](#74-acceptance-criteria) | [§1.5](#15-success-metrics), [§2](#2-requirements) | `[ACCEPTANCE]`, `mvp`, `validation` |
| **CI/CD Pipeline** | [§6.3](#63-cicd-pipeline) | [§6.2](#62-build-process), [§7](#7-testing-strategy) | `github-actions`, `automation` |
| **Memory Management** | [§4.3](#43-memory-management) | [§4.2](#42-threading-model), [§5.4](#54-serialization) | `[REQUIRED]`, `raii`, `smart-pointers` |
| **Security** | [§2.2.4](#224-security-constraints) | [§6.4](#64-development-workflow) | `[SECURITY]`, `read-only-api` |
| **Replay Mode** | [§7.5](#75-test-utilities) | [§1.1](#11-what), [§7.2](#72-integration-testing), [§C.4](#c4-testing-workflow) | `testing`, `offline`, `cli` |

**Usage Guide:**
- **Primary Section:** Main technical discussion
- **Related Sections:** Dependencies, context, examples
- **Tags:** Filter by `[CRITICAL]`, `[PERFORMANCE]`, `[REQUIRED]`, `[ARCHITECTURE]`, etc.
- **Quick References:** Jump to [§8.3 Quick Reference Cards](#83-quick-reference-cards) for cheat sheets

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Requirements](#2-requirements)
3. [System Architecture](#3-system-architecture)
4. [Technical Design](#4-technical-design)
5. [Implementation Details](#5-implementation-details)
6. [Build & DevOps](#6-build--devops)
7. [Testing Strategy](#7-testing-strategy)
8. [Appendices](#8-appendices)

---

## 1. Executive Summary

<!-- METADATA: scope=overview, priority=critical -->

### 1.1. What

**TWS-Redis Bridge** is a low-latency C++ microservice that consumes real-time market data from Interactive Brokers' Trader Workstation (TWS) API and publishes normalized tick data to Redis Pub/Sub channels. It serves as a high-performance data ingestion and distribution backbone for the [trader-pro](https://github.com/faroukBakari/trader-pro) Vue.js + FastAPI trading platform.

**Core Functionality:**
- **Dynamic subscription management** via Redis command channel (FastAPI → Bridge)
- **Real-time tick-by-tick data ingestion** via TWS C++ API (`reqTickByTickData`)
- **Producer-consumer architecture** with lock-free queue for non-blocking data flow
- **State aggregation** of partial TWS updates into complete market snapshots
- **JSON serialization** using RapidJSON for downstream compatibility
- **Redis Pub/Sub publishing** for fan-out to multiple consumers
- **CLI replay utility** for offline testing from historical tick files

### 1.2. Why

**Problem:** The existing trader-pro application serves randomly generated data. Integrating TWS directly into Python/FastAPI would create tight coupling between market data ingestion and web application state, risking data loss on crashes and limiting scalability.

**Solution Benefits:**
- **Independent Failure Domains:** C++ bridge runs independently; web app crashes don't disrupt data flow
- **Fan-Out Architecture:** Redis Pub/Sub enables 1→N consumption (FastAPI, analytics, logging) without producer changes
- **Performance:** C++ low-latency processing (< 50ms TWS → Redis) with zero-cost abstractions
- **Scalability:** Platform enabler for future services (real-time analytics, alerts, persistent logging)
- **Decoupled Stack:** Reduces computational load on Python/JavaScript components

### 1.3. How

**Architecture Pattern:** Event-driven producer-consumer with decoupled microservice

**Key Components:**
1. **Command Listener:** Subscribes to `TWS:COMMANDS` channel for dynamic subscription requests from FastAPI
2. **TWS Integration Layer:** EWrapper/EClient/EReader pattern for tick-by-tick subscriptions
3. **Lock-Free Queue:** `moodycamel::ConcurrentQueue` for non-blocking inter-thread handoff (< 100ns)
4. **State Manager:** Aggregates partial updates (BidAsk/AllLast) into complete instrument snapshots
5. **JSON Serializer:** RapidJSON Writer API for high-performance serialization (10-50μs)
6. **Redis Publisher:** `redis-plus-plus` with connection pooling for reliable publishing

**Threading Model:**
- **Thread 1 (TWS EReader):** Reads TCP socket, enqueues messages (managed by TWS API)
- **Thread 2 (Main/Producer):** Processes TWS callbacks, enqueues `TickUpdate` structs (< 1μs per callback)
- **Thread 3 (Redis Worker/Consumer):** Listens for commands, dequeues tick data, updates state, serializes, publishes (> 10K msg/sec)
- **Thread 4 (Command Listener):** Subscribes to `TWS:COMMANDS`, processes subscribe/unsubscribe requests from FastAPI

**Technology Stack:** C++17/20, Conan 2.x, CMake, Catch2, TWS API, redis-plus-plus, RapidJSON, moodycamel::ConcurrentQueue

### 1.4. When

**Timeline:** 7-day MVP (minimum achievable timeline)

**Phase Breakdown:**
- **Days 1-2:** TWS connection, EReader threading, tick-by-tick subscriptions
- **Days 3-4:** Producer-consumer queue, state management, multi-instrument support
- **Days 5-6:** RapidJSON serialization, Redis integration, error handling
- **Day 7:** Reconnection logic, rate limiting, CLI replay mode, benchmarking

**Critical Note:** Initial 3-day timeline was too ambitious. TWS API threading model, error handling, and reconnection logic have steep learning curve. Full week allows proper implementation without cutting corners.

### 1.5. Success Metrics

**MVP Acceptance Criteria:**

**Functional Requirements:**
- ✅ Process tick-by-tick data for 3+ instruments simultaneously
- ✅ Receive and aggregate BidAsk and AllLast updates
- ✅ Publish complete JSON snapshots to Redis channels
- ✅ Handle TWS disconnection/reconnection with subscription recovery
- ✅ CLI replay mode functional with historical tick files

**Performance Targets:**
- **Latency:** < 50ms median (TWS → Redis on LAN)
- **Throughput:** > 10,000 messages/second (Redis Worker)
- **EReader Overhead:** < 1μs per callback (just enqueue)
- **Queue Handoff:** 50-200ns (lock-free atomic)
- **JSON Serialization:** 10-50μs per message (RapidJSON Writer)

**Reliability Requirements:**
- Zero message loss during normal operation
- Graceful degradation on TWS disconnection
- Successful state rebuild after reconnection
- Exponential backoff reconnection (1s → 60s cap)
- Rate limit compliance (100 requests/10min, 60 requests/sec)

**CI/CD Requirements:**
- Automated build/test pipeline (GitHub Actions)
- All unit tests pass (Catch2 + CTest)
- Zero compiler warnings (`-Werror`)
- Docker-based Redis integration tests
- Reproducible builds (Conan lockfiles)


---

## 2. Requirements

<!-- METADATA: scope=requirements, priority=critical, dependencies=[3-architecture] -->

### 2.1. Functional Requirements

#### 2.1.1. Use Cases

**[REQUIRED]** Primary use cases for MVP:

**UC-1: Dynamic Subscription Request (FastAPI → Bridge)**
- **Actor:** FastAPI backend (on behalf of user)
- **Precondition:** Bridge connected to TWS and listening on `TWS:COMMANDS`
- **Flow:**
  1. User requests market data for AAPL via FastAPI endpoint
  2. FastAPI publishes command to `TWS:COMMANDS`: `{"action":"subscribe","symbol":"AAPL",...}`
  3. Bridge Command Listener receives command
  4. Bridge validates contract and calls `reqTickByTickData()`
  5. Bridge publishes confirmation to `TWS:STATUS`
  6. TWS starts sending tick data
  7. Bridge publishes ticks to `TWS:TICKS:AAPL`
- **Postcondition:** Real-time market data flowing to `TWS:TICKS:AAPL`
- **Exception:** Invalid symbol → error published to `TWS:STATUS`

**UC-2: Real-Time Market Data Consumption**
- **Actor:** TWS-Redis Bridge (automated)
- **Precondition:** TWS Gateway running, subscription active via UC-1
- **Flow:**
  1. Receives `tickByTickBidAsk` callbacks (quote updates)
  2. Receives `tickByTickAllLast` callbacks (trade updates)
  3. Aggregates partial updates into complete instrument state
  4. Serializes to JSON and publishes to Redis channel
- **Postcondition:** Real-time market data available on `TWS:TICKS:{SYMBOL}` channel
- **Exception:** TWS disconnection triggers reconnection logic ([§5.5](#55-key-patterns))

**UC-3: Multi-Instrument Subscription**
- **Actor:** FastAPI backend + TWS-Redis Bridge
- **Requirement:** Support 3+ concurrent instrument subscriptions
- **Flow:**
  1. FastAPI sends multiple subscribe commands to `TWS:COMMANDS`
  2. Bridge maintains map of `tickerId → InstrumentState`
  3. Route callbacks to correct state object based on `tickerId`
  4. Publish each instrument to dedicated Redis channel
- **Benefit:** Independent failure domains per instrument

**UC-4: Historical Data Replay (Testing)**
- **Actor:** Developer/CI Pipeline
- **Precondition:** CSV tick file available (`timestamp,price,size`)
- **Flow:**
  1. Launch bridge with `--replay ticks.csv --symbol AAPL` flag
  2. Parse timestamps, simulate real-time spacing with `sleep_for()`
  3. Enqueue to same lock-free queue as live system
  4. Verify Redis output matches expected JSON schema
- **Benefit:** Offline testing without TWS dependency

**UC-5: Consumer Fan-Out**
- **Actor:** FastAPI backend, analytics service, data logger (any subscriber)
- **Precondition:** Bridge publishing to Redis after receiving subscription command
- **Flow:**
  1. Consumer subscribes to `TWS:TICKS:{SYMBOL}` or `TWS:TICKS:*` (wildcard)
  2. Receives JSON messages via Redis Pub/Sub
  3. Parses JSON and processes data
- **Benefit:** Add new consumers without bridge changes

**UC-6: Graceful Shutdown**
- **Actor:** System administrator or process manager
- **Flow:**
  1. Send SIGINT/SIGTERM signal
  2. Bridge stops listening for commands, unsubscribes from TWS
  3. Drains remaining messages from queue
  4. Joins worker threads, exits cleanly
- **Postcondition:** No resource leaks, no data corruption

#### 2.1.2. API Contracts

**[REQUIRED]** External API interfaces:

**[PIVOT]** **Market Hours Considerations:**

The bridge supports multiple data subscription strategies to enable development and testing both during and outside market hours:

**Primary Strategy (Markets Open - 9:30 AM - 4:00 PM ET):**
- Use `reqTickByTickData()` for true tick-level market data
- Callbacks: `tickByTickBidAsk` (quotes) and `tickByTickAllLast` (trades)
- **Granularity:** Every market event triggers immediate callback
- **Use Case:** Production trading, live market analysis

**Fallback Strategy 1 (Markets Closed - Development/Testing):**
- Use `reqHistoricalData()` for historical bar data (5-minute OHLCV bars, last 1-24 hours)
- Use `reqRealTimeBars()` for 5-second real-time bars (available 24/7 for major indices)
- **Granularity:** 5-second to 5-minute intervals
- **Use Case:** Architecture validation, offline development, testing without live market dependency
- **Impact:** Validates producer-consumer pipeline, state management, and Redis publishing without requiring market hours
- **Reference:** See `docs/FAIL-FAST-PLAN.md` § 2.1 (Day 1 Afternoon) for bar-based testing approach

**Fallback Strategy 2 (Historical Tick Data - Testing):**
- Use `reqHistoricalTicks()` for BidAsk and Last trade data
- **Limitation:** 1000 ticks per request (requires pagination for larger datasets)
- **Use Case:** Testing tick aggregation logic offline
- **Reference:** See `docs/FAIL-FAST-PLAN.md` § 2.2 (Day 2 Morning) for historical tick testing

**Architecture Note:** The producer-consumer threading model remains identical across all data sources—only the subscription method and callback signatures change. State aggregation, JSON serialization, and Redis publishing logic are data-source agnostic.

---

**TWS C++ API (Input):**
- **Connection:** `EClient::eConnect(host, port, clientId)`
- **Subscription:** `EClient::reqTickByTickData(reqId, contract, "BidAsk"|"AllLast", 0, false)`
- **Callbacks:**
  - `EWrapper::tickByTickBidAsk(reqId, time, bidPrice, askPrice, bidSize, askSize, attribs)`
  - `EWrapper::tickByTickAllLast(reqId, tickType, time, price, size, attribs, exchange, specialConditions)`
  - `EWrapper::error(id, errorCode, errorString, ...)`
  - `EWrapper::nextValidId(orderId)` - connection handshake complete
- **Contract Definition:**
  ```cpp
  Contract c;
  c.symbol = "AAPL";
  c.secType = "STK";
  c.currency = "USD";
  c.exchange = "SMART";
  c.primaryExchange = "NASDAQ"; // Required for disambiguation
  ```

**Redis Pub/Sub API (Bidirectional):**
- **Input Channel:** `TWS:COMMANDS` (FastAPI → Bridge subscription requests)
- **Output Channels:** `TWS:TICKS:{SYMBOL}` (Bridge → consumers tick data)
- **Status Channel:** `TWS:STATUS` (Bridge → consumers system events)
- **Commands:** `SUBSCRIBE` (listen for commands), `PUBLISH` (send data/status)
- **Library:** `redis-plus-plus` (`sw::redis::Redis`, `sw::redis::Subscriber`)

**CLI Interface:**
```bash
# Live mode with dynamic subscriptions (default)
./tws_bridge --host 127.0.0.1 --port 4002 --client-id 0

# Live mode with initial subscriptions (optional)
./tws_bridge --host 127.0.0.1 --port 4002 --subscribe AAPL,SPY,TSLA

# Replay mode (offline testing, no command listener)
./tws_bridge --replay ticks.csv --symbol AAPL --ticker-id 1

# Configuration file
./tws_bridge --config config.yaml
```

#### 2.1.3. Data Requirements

**[REQUIRED]** Data flow and transformation:

**Input Data (TWS Callbacks):**
| Callback | Field | Type | Description |
|----------|-------|------|-------------|
| `tickByTickBidAsk` | `time` | `long` | Unix timestamp (ms) |
| | `bidPrice` | `double` | Best bid price |
| | `askPrice` | `double` | Best ask price |
| | `bidSize` | `int` | Bid quantity |
| | `askSize` | `int` | Ask quantity |
| `tickByTickAllLast` | `time` | `long` | Unix timestamp (ms) |
| | `price` | `double` | Last trade price |
| | `size` | `int` | Last trade quantity |
| | `pastLimit` | `bool` | Trade outside regular hours |
| | `exchange` | `string` | Execution venue |

**Internal State (InstrumentState):**
```cpp
struct InstrumentState {
    std::string symbol;
    int conId, tickerId;
    double bidPrice, askPrice, lastPrice;
    int bidSize, askSize, lastSize;
    long quoteTimestamp, tradeTimestamp;
    std::string exchange;
    bool hasQuote, hasTrade, pastLimit;
};
```

**Output Data (JSON Schema):**
```json
{
  "instrument": "AAPL",
  "conId": 265598,
  "timestamp": 1700000000500,
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
  "timestamps": {
    "quote": 1700000000000,
    "trade": 1700000000500
  },
  "exchange": "NASDAQ",
  "tickAttrib": {
    "pastLimit": false
  }
}
```

**[CRITICAL]** **Data Integrity Rules:**
- **[ANTI-PATTERN]** Never use `std::time(nullptr)` for timestamps - use TWS-provided `time` parameter
- **[REQUIRED]** Publish complete state snapshots (not partial updates)
- **[REQUIRED]** Maintain separate quote/trade timestamps for accuracy
- **[PERFORMANCE]** Use `std::max(quoteTimestamp, tradeTimestamp)` for top-level timestamp

### 2.2. Non-Functional Requirements

#### 2.2.1. Performance Targets

**[PERFORMANCE]** **MVP Performance Requirements:**

| Metric | Target | Measurement Method |
|--------|--------|-------------------|
| **End-to-End Latency** | < 50ms (median) | TWS callback timestamp → Redis `PUBLISH` completion |
| **EReader Callback Overhead** | < 1μs | Time spent in `tickByTickBidAsk`/`tickByTickAllLast` |
| **Queue Enqueue** | 50-200ns | Lock-free atomic operation |
| **State Update** | 1-10μs | Mutex lock + map lookup + field update |
| **JSON Serialization** | 10-50μs | RapidJSON Writer API |
| **Redis Publish** | 0.1-1ms | Network I/O (LAN) |
| **Worker Throughput** | > 10,000 msg/sec | Messages processed by Redis Worker thread |
| **Queue Size** | < 1000 (99th percentile) | Monitor `ConcurrentQueue::size_approx()` |

**[PERFORMANCE]** **Latency Budget Breakdown:**
```
Exchange → TWS Gateway → TCP → EReader Thread → 
  processMsgs() → EWrapper callback → TickUpdate → 
  Lock-Free Queue (< 1ms) → Redis Worker → State Update → 
  JSON Serialize → Redis Publish (< 2ms) → Subscribers
```

**[FUTURE]** **Post-MVP Optimizations:**
- CPU affinity/thread pinning for predictable latency
- Async I/O with Boost.Asio instead of blocking sockets
- Binary serialization (Protobuf: 5-10x faster than JSON)
- Redis pipelining for batch publishing
- NUMA-aware memory allocation

#### 2.2.2. Reliability Expectations

**[REQUIRED]** **Fault Tolerance:**

**TWS Disconnection Handling:**
- **Detection:** Monitor `EClient::isConnected()`, handle `connectionClosed()` callback
- **Response:** Enter reconnection loop with exponential backoff (1s → 2s → 4s → ... → 60s cap)
- **Recovery:** Clear stale state, resubscribe to all instruments, resume publishing
- **[REQUIRED]** Zero manual intervention required for transient failures

**Redis Disconnection Handling:**
- **Detection:** `redis-plus-plus` connection pool auto-detection
- **Response:** Log error, continue processing (queue buffering)
- **Recovery:** Auto-reconnect on next `publish()` attempt
- **[PITFALL]** Queue overflow protection: drop messages if queue > 10,000 (backpressure)

**Error Code Mapping:**
| Error Code | Meaning | Action |
|------------|---------|--------|
| 502 | TWS not running | **Fatal:** Log and exit |
| 504 | Connection lost | **Transient:** Trigger reconnection |
| 10089 | No market data permissions | **Fatal:** Log and exit |
| 2104, 2106, 2158 | Informational messages | **Info:** Log only |
| 321 | Rate limit exceeded | **Warning:** Activate rate limiter backoff |

**[REQUIRED]** **State Consistency:**
- Clear `m_instrumentStates` map on TWS disconnect
- Rebuild state from fresh callbacks after reconnect
- Never publish stale/pre-disconnect data

**[REQUIRED]** **Graceful Degradation:**
- Continue processing available instruments if one subscription fails
- Log per-instrument errors without crashing bridge
- Publish to `TWS:STATUS` channel on errors for monitoring

#### 2.2.3. Scalability Needs

**MVP Scope:**
- **Instruments:** Support 3-10 concurrent subscriptions
- **Message Rate:** Handle 1,000-10,000 ticks/second aggregate
- **Memory:** < 100 MB steady-state footprint
- **CPU:** Single-core sufficient for MVP (3 threads)

**[ARCHITECTURE]** **Scalability Design:**
- **Lock-Free Queue:** SPSC pattern eliminates contention bottleneck
- **State Map:** `std::unordered_map` with O(1) lookup, no global lock
- **Redis Connection Pool:** `redis-plus-plus` pool size = 3 connections
- **Thread Model:** Fixed 3-thread design (no thread pool overhead)

**[FUTURE]** **Beyond MVP:**
- Horizontal scaling: Multiple bridge instances with instrument sharding
- Vertical scaling: Multi-producer queue for parallel TWS connections
- Redis Cluster: Distribute load across Redis nodes
- Async I/O: Event loop to handle 100+ instruments on single thread

#### 2.2.4. Security Constraints

**[SECURITY]** **MVP Security Requirements:**

**TWS Connection Security:**
- **[REQUIRED]** Configure TWS with "Read-Only API" mode (disable order placement)
- **[REQUIRED]** Use paper trading account (port 4002) for development
- **[REQUIRED]** Restrict TWS API to `localhost` connections only
- **Network:** No external network exposure of TWS port

**Redis Security:**
- **[REQUIRED]** Use `redis://localhost:6379` (no authentication in MVP)
- **[FUTURE]** Production: Enable Redis AUTH (`requirepass`)
- **[FUTURE]** Production: Use TLS for Redis connections (`rediss://`)
- **Network:** Bind Redis to `127.0.0.1` only (no external access)

**Application Security:**
- **Input Validation:** Sanitize all TWS callback data (check for NaN, infinity)
- **Memory Safety:** Use smart pointers (`std::unique_ptr`, `std::shared_ptr`)
- **Resource Limits:** Enforce queue size cap to prevent memory exhaustion
- **Logging:** Never log sensitive account details (only sanitized errors)

**[FUTURE]** **Production Hardening:**
- Principle of least privilege: Run as non-root user
- Seccomp/AppArmor sandboxing
- Encrypted configuration storage
- Audit logging for all TWS API calls
- Rate limiting per instrument (prevent abuse)


---

## 3. System Architecture

<!-- METADATA: scope=architecture, priority=critical, dependencies=[2-requirements] -->

### 3.1. Architecture Pattern

**[ARCHITECTURE]** **Decoupled Microservice with Event-Driven Fan-Out Messaging**

**Pattern Rationale:**
- **Independent Failure Domains:** C++ bridge crashes don't affect consumers; consumer crashes don't affect data ingestion
- **Loose Coupling:** Bridge and consumers evolve independently
- **Scalability:** Fan-out enables 1→N consumption without producer modification
- **Platform Enabler:** Foundation for future services (analytics, logging, alerting)

---

#### ADR-001: Standalone C++ Microservice Architecture

**[DECISION]** Standalone C++ Microservice (not Python library) **[Date: 2025-11-15]**

**Context:** Need to integrate TWS market data with FastAPI/Vue.js trading platform

**Rationale:**
- Independent failure domains: data ingestion survives web app crashes
- Process isolation prevents resource contention between C++ bridge and Python FastAPI
- C++ performance critical for low-latency I/O-bound market data processing
- Enables horizontal scaling: multiple bridge instances for different instrument sets

**Alternatives Rejected:**
- **Python extension module:** Shared process space = coupled failure domain, memory leaks affect both
- **Shared library (.so):** Still coupled, FFI overhead, complex lifecycle management
- **Direct WebSocket:** No message broker = tight coupling, no fan-out, single point of failure
- **Embedded in FastAPI:** Python GIL contention, crash risk, resource competition

**Consequences:**
- ✅ Independent deployment/restart cycles
- ✅ Process-level isolation for debugging
- ✅ Clear service boundaries
- ❌ Additional deployment complexity (two processes vs one)
- ❌ IPC overhead (negligible for Redis latency)

---

#### ADR-002: Redis Pub/Sub Messaging Pattern

**[DECISION]** Redis Pub/Sub (not direct WebSocket, not RabbitMQ) **[Date: 2025-11-15]**

**Context:** Need 1→N fan-out for market data distribution to multiple consumers

**Rationale:**
- **[PERFORMANCE]** Fan-out capability: 1 producer → N subscribers without producer changes
- High throughput: 1M+ messages/sec on commodity hardware
- Sub-millisecond latency on LAN (0.1-1ms publish time)
- Simple operational model: single Redis instance sufficient for MVP
- Native support in Python (redis-py), Node.js, C++

**Alternatives Rejected:**
- **Direct WebSocket:** No fan-out (1:1 connection), producer must track all consumers, backpressure complexity
- **RabbitMQ:** Over-engineered for simple Pub/Sub, higher latency (5-10ms), operational overhead (clustering, queues)
- **gRPC streaming:** Complex setup, no built-in fan-out, requires reverse proxy for multiple clients
- **Kafka:** Overkill for MVP, operational complexity (ZooKeeper/KRaft), partition management, disk I/O overhead

**Consequences:**
- ✅ Add consumers without bridge code changes
- ✅ Sub-millisecond latency (acceptable for < 50ms budget)
- ✅ Simple deployment (single Redis container)
- ❌ At-most-once delivery (acceptable for real-time data; stale data has no value)
- ❌ No message persistence (use separate logger consumer if needed)

---

#### ADR-003: Four-Thread Producer-Consumer Pattern

**[DECISION]** Fixed Four-Thread Architecture with Command Listener **[Date: 2025-11-15]**

**Context:** TWS API enforces EReader threading model; need to isolate blocking Redis I/O; need to handle dynamic subscriptions from FastAPI

**Rationale:**
- **TWS API constraint:** EReader thread managed by TWS library (cannot refactor)
- **I/O isolation:** Blocking Redis `publish()` must not block EWrapper callbacks
- **Command handling:** Separate thread for Redis SUBSCRIBE prevents blocking main event loop
- **[PERFORMANCE]** Lock-free queue enables < 1μs callback overhead (critical path)
- Fixed workload: single TWS connection, single Redis publisher, single command listener
- Simplicity: predictable threading model, easier debugging

**Alternatives Rejected:**
- **Single-threaded event loop (async I/O):** Blocks on Redis I/O, adds complexity (state machines), requires async Redis client
- **Thread pool:** Unnecessary overhead for fixed workload (3-4 threads always active), scheduling latency, context switching
- **Boost.Asio async:** Over-engineered for MVP, steep learning curve, callback spaghetti, premature optimization
- **Three threads (merge command + worker):** Command listening (blocking SUBSCRIBE) would block tick publishing
- **Polling command channel:** Adds latency, wastes CPU cycles, less responsive than blocking SUBSCRIBE

**Consequences:**
- ✅ Non-blocking command processing (FastAPI requests don't block data flow)
- ✅ Minimal context switching (4 threads pinned to cores)
- ✅ Lock-free critical path (50-200ns queue enqueue)
- ✅ Predictable latency profile
- ✅ Dynamic subscription management without restart
- ❌ Fixed concurrency (cannot scale to multiple TWS connections without redesign)
- ❌ No async I/O (acceptable: Redis latency 0.1-1ms is tolerable off critical path)
- ❌ Additional thread overhead (~8MB stack per thread)

---

**See also:**
- [§3.2 Component Diagram](#32-component-diagram) - Visual architecture overview
- [§4.2 Threading Model](#42-threading-model) - Detailed thread responsibilities
- [§3.5 Architecture Decisions](#35-architecture-decisions) - Additional ADRs

### 3.2. Component Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                        External Systems                          │
│              (Interactive Brokers Servers/Services)              │
└─────────────────────────────────┬───────────────────────────────┘
                                  │
                                  │ Market Data Feed
                                  │ (Internet/Leased Line)
                                  │
                          ┌───────▼────────┐
                          │  TWS Gateway   │
                          │   (Port 4002)  │
                          │  API Exposer   │
                          └───────┬────────┘
                                  │
                                  │ TCP Socket (TWS API Protocol)
                                  │
┌─────────────────────────────────▼───────────────────────────────┐
│                   TWS-Redis Bridge (C++)                         │
│                                                                   │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              Thread 4: EReader (TWS API)                 │   │
│  │  • Reads TCP socket (blocking)                           │   │
│  │  • Parses TWS protocol messages                          │   │
│  │  • Enqueues EMessage objects                             │   │
│  └───────────────────────┬─────────────────────────────────┘   │
│                          │ EReaderOSSignal                       │
│                          │                                       │
│  ┌───────────────────────▼─────────────────────────────────┐   │
│  │         Thread 1: Message Processing (Main)              │   │
│  │  • Waits on EReaderOSSignal                              │   │
│  │  • Calls processMsgs() → EWrapper callbacks              │   │
│  │  • Normalizes data to TickUpdate struct                  │   │
│  │  • Enqueues to lock-free queue (<1μs)                    │   │
│  └───────────────────────┬─────────────────────────────────┘   │
│                          │ moodycamel::ConcurrentQueue           │
│                          │ (Lock-Free SPSC)                      │
│                          │                                       │
│  ┌───────────────────────▼─────────────────────────────────┐   │
│  │          Thread 3: Redis Worker (Consumer)               │   │
│  │  • Dequeues TickUpdate (blocking wait)                   │   │
│  │  • Updates InstrumentState map (mutex)                   │   │
│  │  • Serializes to JSON (RapidJSON Writer)                 │   │
│  │  • Publishes to TWS:TICKS:{SYMBOL}                       │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                   │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │       Thread 2: Command Listener (Subscription Mgr)      │   │
│  │  • Subscribes to TWS:COMMANDS channel                    │   │
│  │  • Receives subscribe/unsubscribe from FastAPI           │   │
│  │  • Validates contracts and calls TwsClient methods       │   │
│  │  • Publishes confirmations to TWS:STATUS                 │   │
│  └─────────────────────────────────────────────────────────┘   │
└────────────────────────────┬──────────────────┬────────────────┘
                             │                  │
                             │ PUBLISH          │ SUBSCRIBE
                             │ (Data/Status)    │ (Commands)
                             │                  │
                     ┌───────▼──────────────────▼────┐
                     │       Redis Server            │
                     │       (Port 6379)             │
                     │     Pub/Sub Broker            │
                     └───────┬──────────────┬────────┘
                             │              │
                   SUBSCRIBE │              │ PUBLISH
                   (Data)    │              │ (Commands)
              ┌──────────────┼──────┐       │
              │              │      │       │
      ┌───────▼────────┐ ┌──▼──────▼───┐ ┌▼──────────────┐
      │ Data Logger    │ │ FastAPI      │ │  Analytics    │
      │ (Python)       │ │ (WebSocket)  │ │  Service      │
      └────────────────┘ └──────┬───────┘ └───────────────┘
                                │
                                │ WebSocket
                                │
                         ┌──────▼────────┐
                         │  Web Clients  │
                         │  (Browser)    │
                         └───────────────┘
```

**Component Relationships:**

1. **External Systems (IB Servers)** → **TWS Gateway**
   - TWS Gateway is the local proxy that connects to Interactive Brokers' servers
   - Exposes TWS API for local clients to consume

2. **TWS Gateway** → **TWS-Redis Bridge**
   - Bridge is the sole TWS API consumer
   - Direct TCP connection via TWS C++ API

3. **FastAPI** → **Redis** → **TWS-Redis Bridge** (Command Flow)
   - FastAPI publishes subscribe/unsubscribe commands to `TWS:COMMANDS`
   - Bridge Thread 2 (Command Listener) receives and processes commands
   - Dynamic subscription management based on user requests

4. **TWS-Redis Bridge** → **Redis Server** (Data Flow)
   - Thread 3 (Redis Worker) publishes tick data to `TWS:TICKS:{SYMBOL}`
   - Thread 2 (Command Listener) publishes status to `TWS:STATUS`
   - Bidirectional Redis communication (subscribe to commands, publish data/status)

5. **Redis Server** → **Downstream Ecosystem**
   - Redis acts as data feed provider (fan-out broker)
   - FastAPI, Data Logger, Analytics all consume via Redis Pub/Sub
   - FastAPI also sends commands via Redis (bidirectional)

---

**See also:**
- [§3.3 Data Flow](#33-data-flow) - Detailed message flow sequence
- [§3.4 Interface Definitions](#34-interface-definitions) - API contracts
- [§4.2 Threading Model](#42-threading-model) - Thread implementation details

### 3.3. Data Flow

**[ARCHITECTURE]** **Command Flow (FastAPI → Bridge):**

```
User Request → FastAPI → Redis PUBLISH (TWS:COMMANDS) → 
Command Listener (Thread 2) → Parse JSON → Validate Contract → 
TwsClient::subscribe() → TWS Gateway → Subscription Active
```

**[ARCHITECTURE]** **Data Flow (TWS → Consumers):**

```
Exchange → TWS Gateway → TCP → EReader (Thread 4) → 
processMsgs() → EWrapper callback (Thread 1) → TickUpdate → 
Lock-Free Queue (< 1ms) → Redis Worker (Thread 3) → 
State Update → JSON Serialize → Redis Publish → Subscribers
```

**Stage-by-Stage Breakdown:**

**Command Path (User-Initiated Subscription):**

| Stage | Component | Action | Latency | Notes |
|-------|-----------|--------|---------|-------|
| 1. User Request | FastAPI | User requests AAPL data | - | HTTP/WebSocket |
| 2. Command Publish | FastAPI | `PUBLISH TWS:COMMANDS {...}` | 0.1-1ms | Redis network I/O |
| 3. Command Receive | Bridge T2 | Blocking `SUBSCRIBE` receives msg | ~0.1-1ms | Redis Pub/Sub |
| 4. Parse & Validate | Bridge T2 | RapidJSON parse, validate contract | 10-50μs | CPU-bound |
| 5. TWS Subscribe | Bridge T1 | `reqTickByTickData()` to TWS | 1-10ms | TWS API call |
| 6. Confirmation | Bridge T2 | Publish to `TWS:STATUS` | 0.1-1ms | Redis network I/O |
| **Total Command Path** | - | **User → Active Subscription** | **< 100ms** | **Acceptable** |

**Data Path (Market Event → Consumer):**

| Stage | Thread | Action | Latency | Notes |
|-------|--------|--------|---------|-------|
| 1. Market Event | - | Exchange executes trade/quote | ~5-10ms | Network + TWS aggregation |
| 2. TCP Read | 4 | Block on socket, decode protocol | ~0.1-1ms | Blocking I/O |
| 3. EMessage Queue | 4 | Enqueue + signal | ~100ns | TWS internal atomic |
| 4. Signal Wait | 1 | Futex wait for new messages | ~1-10μs | OS synchronization |
| 5. processMsgs() | 1 | Decode + dispatch callback | ~10-100μs | TWS API processing |
| 6. EWrapper Callback | 1 | **Copy to TickUpdate struct** | **< 1μs** | **CRITICAL: no I/O** |
| 7. Lock-Free Enqueue | 1 | Atomic CAS operation | **50-200ns** | **CRITICAL: lock-free** |
| **Producer Total** | **1+4** | **TWS → Queue** | **< 5ms** | **Target met** |
| 8. Dequeue Wait | 3 | Futex wait on queue | ~1-100μs | Blocking OK (off critical path) |
| 9. State Update | 3 | Mutex + map lookup + update | 1-10μs | Exclusive access (no contention) |
| 10. JSON Serialize | 3 | RapidJSON Writer | 10-50μs | SAX API, reusable buffer |
| 11. Redis Publish | 3 | Network I/O to Redis | 0.1-1ms | LAN latency |
| **Consumer Total** | **3** | **Queue → Redis** | **< 2ms** | **Target met** |
| 12. Redis Fan-Out | - | Pub/Sub dispatch | ~0.1-1ms | Redis internal |
| **End-to-End** | **All** | **Exchange → Subscribers** | **< 20ms** | **< 50ms budget** |
| 3. EMessage Queue | 1 | Enqueue + signal | ~100ns | TWS internal atomic |
| 4. Signal Wait | 2 | Futex wait for new messages | ~1-10μs | OS synchronization |
| 5. processMsgs() | 2 | Decode + dispatch callback | ~10-100μs | TWS API processing |
| 6. EWrapper Callback | 2 | **Copy to TickUpdate struct** | **< 1μs** | **CRITICAL: no I/O** |
| 7. Lock-Free Enqueue | 2 | Atomic CAS operation | **50-200ns** | **CRITICAL: lock-free** |
| **Producer Total** | **1+2** | **TWS → Queue** | **< 5ms** | **Target met** |
| 8. Dequeue Wait | 3 | Futex wait on queue | ~1-100μs | Blocking OK (off critical path) |
| 9. State Update | 3 | Mutex + map lookup + update | 1-10μs | Exclusive access (no contention) |
| 10. JSON Serialize | 3 | RapidJSON Writer | 10-50μs | SAX API, reusable buffer |
| 11. Redis Publish | 3 | Network I/O to Redis | 0.1-1ms | LAN latency |
| **Consumer Total** | **3** | **Queue → Redis** | **< 2ms** | **Target met** |
| 12. Redis Fan-Out | - | Pub/Sub dispatch | ~0.1-1ms | Redis internal |
| **End-to-End** | **All** | **Exchange → Subscribers** | **< 20ms** | **< 50ms budget** |

**[PERFORMANCE]** **Latency Budget:**
- **Command Path:** < 100ms (user-initiated, infrequent, not on critical path)
- **Critical Path (Threads 1+4):** < 5ms (no blocking I/O, lock-free queue)
- **Consumer Path (Thread 3):** < 2ms (state update + serialize + publish)
- **Total Data Budget:** < 50ms end-to-end (includes network ~5-10ms)

### 3.4. Interface Definitions

#### 3.4.1. Redis Channels

**[REQUIRED]** **Channel Naming Convention:**

```
TWS:COMMANDS           # Command channel (FastAPI → Bridge)
TWS:TICKS:{SYMBOL}     # Real-time tick updates (per instrument)
TWS:BARS:{SYMBOL}      # 5-second OHLCV bars (future)
TWS:STATUS             # System events (connect/disconnect/error)
```

**Examples:**
- `TWS:COMMANDS` - Subscription management commands from FastAPI
- `TWS:TICKS:AAPL` - Apple Inc. tick data
- `TWS:TICKS:SPY` - SPDR S&P 500 ETF tick data
- `TWS:TICKS:TSLA` - Tesla Inc. tick data
- `TWS:STATUS` - Bridge health and error events

**Subscription Patterns:**
```bash
# Bridge subscribes to commands (internal)
redis-cli SUBSCRIBE TWS:COMMANDS

# FastAPI publishes subscription request
redis-cli PUBLISH TWS:COMMANDS '{"action":"subscribe","symbol":"AAPL","secType":"STK"}'

# Consumers subscribe to tick data
redis-cli SUBSCRIBE TWS:TICKS:AAPL

# All tick channels (wildcard)
redis-cli PSUBSCRIBE "TWS:TICKS:*"

# Status monitoring
redis-cli SUBSCRIBE TWS:STATUS
```

**Status Message Types:**
```json
// Connection established
{
  "type": "connected",
  "timestamp": 1700000000000,
  "host": "127.0.0.1",
  "port": 4002,
  "clientId": 0
}

// Connection lost
{
  "type": "disconnected",
  "timestamp": 1700000001000,
  "reason": "TWS Gateway restart"
}

// Error event
{
  "type": "error",
  "timestamp": 1700000002000,
  "errorCode": 504,
  "errorMsg": "Not connected"
}

// Subscription confirmed
{
  "type": "subscribed",
  "timestamp": 1700000003000,
  "symbol": "AAPL",
  "tickerId": 1,
  "requestId": "req-12345",
  "contract": {
    "symbol": "AAPL",
    "secType": "STK",
    "exchange": "SMART",
    "primaryExchange": "NASDAQ"
  }
}

// Unsubscription confirmed
{
  "type": "unsubscribed",
  "timestamp": 1700000004000,
  "symbol": "AAPL",
  "tickerId": 1,
  "requestId": "req-67890"
}
```

#### 3.4.2. JSON Schemas

**[REQUIRED]** **Command Message Schema (FastAPI → Bridge):**

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "required": ["action", "symbol"],
  "properties": {
    "action": {
      "type": "string",
      "enum": ["subscribe", "unsubscribe"],
      "description": "Command type"
    },
    "symbol": {
      "type": "string",
      "description": "Ticker symbol",
      "example": "AAPL"
    },
    "secType": {
      "type": "string",
      "enum": ["STK", "OPT", "FUT", "IND", "FOP", "CASH", "BAG", "NEWS"],
      "default": "STK",
      "description": "Security type"
    },
    "exchange": {
      "type": "string",
      "default": "SMART",
      "description": "Exchange routing",
      "example": "SMART"
    },
    "currency": {
      "type": "string",
      "default": "USD",
      "description": "Currency",
      "example": "USD"
    },
    "primaryExchange": {
      "type": "string",
      "description": "Primary listing exchange (required for disambiguation)",
      "example": "NASDAQ"
    },
    "requestId": {
      "type": "string",
      "description": "Client-generated unique ID for tracking",
      "example": "req-12345"
    }
  }
}
```

**Example Commands:**
```json
// Subscribe to AAPL stock
{
  "action": "subscribe",
  "symbol": "AAPL",
  "secType": "STK",
  "exchange": "SMART",
  "currency": "USD",
  "primaryExchange": "NASDAQ",
  "requestId": "req-12345"
}

// Unsubscribe from TSLA
{
  "action": "unsubscribe",
  "symbol": "TSLA",
  "requestId": "req-67890"
}

// Subscribe with minimal fields (defaults apply)
{
  "action": "subscribe",
  "symbol": "SPY"
}
```

**[REQUIRED]** **Tick Data Message Schema (Bridge → Consumers):**

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "required": ["instrument", "conId", "timestamp", "price", "size"],
  "properties": {
    "instrument": {
      "type": "string",
      "description": "Ticker symbol",
      "example": "AAPL"
    },
    "conId": {
      "type": "integer",
      "description": "IB contract ID (unique instrument identifier)",
      "example": 265598
    },
    "timestamp": {
      "type": "integer",
      "description": "Market event timestamp (Unix milliseconds, from TWS)",
      "example": 1700000000500
    },
    "price": {
      "type": "object",
      "required": ["bid", "ask", "last"],
      "properties": {
        "bid": {"type": "number", "example": 171.55},
        "ask": {"type": "number", "example": 171.57},
        "last": {"type": "number", "example": 171.56}
      }
    },
    "size": {
      "type": "object",
      "required": ["bid", "ask", "last"],
      "properties": {
        "bid": {"type": "integer", "example": 100},
        "ask": {"type": "integer", "example": 200},
        "last": {"type": "integer", "example": 50}
      }
    },
    "timestamps": {
      "type": "object",
      "description": "Separate timestamps for quote vs trade updates",
      "required": ["quote", "trade"],
      "properties": {
        "quote": {"type": "integer", "example": 1700000000000},
        "trade": {"type": "integer", "example": 1700000000500}
      }
    },
    "exchange": {
      "type": "string",
      "description": "Primary listing exchange",
      "example": "NASDAQ"
    },
    "tickAttrib": {
      "type": "object",
      "description": "Tick attributes from TWS",
      "properties": {
        "pastLimit": {
          "type": "boolean",
          "description": "Trade occurred outside regular hours",
          "example": false
        }
      }
    }
  }
}
```

**[PERFORMANCE]** **Compact Field Names (Alternative for High Throughput):**

For systems processing >100K msg/sec, use abbreviated field names to reduce bandwidth:

```json
{
  "sym": "AAPL",
  "cid": 265598,
  "ts": 1700000000500,
  "p": {"b": 171.55, "a": 171.57, "l": 171.56},
  "s": {"b": 100, "a": 200, "l": 50},
  "tss": {"q": 1700000000000, "t": 1700000000500},
  "ex": "NASDAQ",
  "attr": {"pl": false}
}
```

### 3.5. Architecture Decisions

**[ARCHITECTURE]** **Key Design Decisions and Rationale:**

#### ADR-004: Lock-Free Queue for Inter-Thread Communication

**[DECISION]** Use `moodycamel::ConcurrentQueue` for Thread 2 → Thread 3 handoff **[Date: 2025-11-15]**

**Context:** Thread 2 (EWrapper callbacks) is on critical path; any blocking degrades end-to-end latency

**Rationale:**
- **[PERFORMANCE]** 50-200ns enqueue vs 1-10μs for mutex-based queue (50x faster)
- No kernel-space synchronization: eliminates priority inversion risk
- Predictable latency: no syscalls, no scheduler involvement
- Battle-tested: used in high-frequency trading systems (proven at scale)
- Header-only library: minimal integration overhead

**Alternatives Rejected:**
- **`std::queue` + `std::mutex`:** 10-50x slower, kernel futex overhead, priority inversion risk
- **`boost::lockfree::queue`:** Benchmarks show 2-3x slower than moodycamel, MPMC overhead unnecessary
- **Unbounded queue:** Memory exhaustion risk during TWS flood (moodycamel supports bounded mode with backpressure)
- **`folly::MPMCQueue`:** MPMC overhead unnecessary (SPSC pattern sufficient), Facebook dependency

**Consequences:**
- ✅ < 1μs total callback overhead (enqueue + minimal data copy)
- ✅ Zero mutex contention on critical path
- ✅ Cache-friendly (lock-free atomic operations)
- ❌ Additional dependency (mitigated: header-only, Conan package available)
- ❌ SPSC limitation (acceptable: single producer/consumer by design, no plans for multi-producer)

#### ADR-005: State Aggregation in Consumer Thread

**[DECISION]** Maintain `std::unordered_map<int, InstrumentState>` in Thread 3, publish complete snapshots **[Date: 2025-11-15]**

**Context:** TWS API sends partial updates (BidAsk callbacks separate from AllLast callbacks); downstream consumers expect complete market data snapshots

**Rationale:**
- **Simplicity:** Consumers receive self-contained messages, no client-side merge logic
- **Correctness:** Thread 3 has exclusive ownership (single consumer) = simple mutex, no race conditions
- **Debuggability:** Every Redis message is complete audit record (no need to correlate partial updates)
- **Stateless consumers:** New subscribers immediately receive complete state (no bootstrap protocol)
- Off critical path: State aggregation in Thread 3 doesn't impact EWrapper callback latency

**Alternatives Rejected:**
- **Delta publishing:** Consumers must maintain state maps, complex error recovery on disconnect, out-of-order message handling
- **State in Thread 2:** Adds mutex contention to critical path (EWrapper callbacks), degrades latency
- **Stateless (forward raw TWS callbacks):** Consumers must implement merge logic (violates DRY), 3+ consumers = 3x duplicated code
- **Redis server-side state:** Requires Lua scripts or RedisJSON, adds operational complexity

**Consequences:**
- ✅ Simplified consumer implementation (FastAPI, logger, analytics all stateless)
- ✅ Complete audit trail (every message is snapshot)
- ✅ New subscribers immediately synchronized
- ❌ Increased bandwidth: 200-500 bytes per update vs 50-100 for delta (acceptable: LAN bandwidth abundant)
- ❌ Redundant data: bid/ask republished on trade-only updates (mitigated: 3 instruments × 10 updates/sec = 15KB/sec, negligible)

#### ADR-006: RapidJSON for High-Performance Serialization

**[DECISION]** Use RapidJSON Writer (SAX) API for JSON serialization **[Date: 2025-11-15]**

**Context:** Need JSON for FastAPI/Python ecosystem compatibility; performance critical for 10K+ msg/sec throughput target

**Rationale:**
- **[PERFORMANCE]** 10-50μs per message vs 50-500μs for nlohmann/json (5-10x faster)
- Low-latency design: minimal heap allocations, cache-friendly sequential writes
- Production-proven: battle-tested in HFT systems, game engines, high-throughput APIs
- StringBuffer reuse: allocate once, serialize many (reduces GC pressure)
- SAX API: direct write (no intermediate DOM tree = lower memory footprint)

**Alternatives Rejected:**
- **`nlohmann/json`:** Convenient API but 5-10x slower (DOM-based), unacceptable for 10K+ msg/sec
- **Protobuf:** Binary format incompatible with Python `redis.pubsub()` subscribers (future optimization for C++ consumers)
- **Manual string concatenation:** Error-prone (escaping, quote handling), no schema validation, unmaintainable
- **JSON for Modern C++ (`simdjson`):** Optimized for parsing (not serialization), no clear Writer API
- **`cJSON`:** C library, manual memory management, slower than RapidJSON

**Consequences:**
- ✅ Sub-100μs serialization fits < 50ms latency budget
- ✅ Memory efficient (reusable StringBuffer per thread)
- ✅ Cache-friendly sequential writes
- ❌ Verbose API: manual `writer.Key()`, `writer.Double()` calls (mitigated: helper functions)
- ❌ No automatic struct→JSON mapping (acceptable: explicit control over schema)

#### ADR-007: Redis Connection Pooling Strategy

**[DECISION]** Use `redis-plus-plus` with connection pool (size=3) **[Date: 2025-11-15]**

**Context:** Single Redis Worker thread needs thread-safe, fault-tolerant Redis client

**Rationale:**
- **Thread safety:** Pool provides synchronized access (future-proof for multi-publisher)
- **Fault tolerance:** Built-in auto-reconnect on TCP failure (exponential backoff)
- **Performance:** Connection reuse eliminates TCP handshake overhead per publish
- **Simplicity:** High-level C++ API (no manual memory management)
- **Active maintenance:** Regular updates, C++17 compatible

**Alternatives Rejected:**
- **Raw `hiredis`:** Manual memory management (`redisReply*` cleanup), no auto-reconnect, error-prone
- **`cpp_redis`:** Abandoned project (last commit 2019), no C++17 support, security vulnerabilities
- **Single connection (no pool):** No auto-reconnect, fragile on network hiccup, manual reconnection logic
- **`bredis`:** Boost.Asio dependency (overkill for blocking I/O), complex async model
- **Direct Redis protocol:** Reinventing wheel, RESP3 complexity, no pipelining support

**Consequences:**
- ✅ Thread-safe `Redis` object (single consumer now, multi-publisher ready)
- ✅ Automatic reconnection with exponential backoff
- ✅ Connection reuse (no TCP handshake per publish)
- ❌ Pool overhead: 3 connections × ~16KB = 48KB memory (negligible)
- ❌ Slight latency: connection checkout from pool (~1-10μs, acceptable off critical path)

#### ADR-008: TWS Tick-by-Tick API Selection

**[DECISION]** Use `reqTickByTickData()` with `tickByTickBidAsk`/`tickByTickAllLast` callbacks **[Date: 2025-11-15]**

**Context:** Need true tick-level market data granularity for real-time trading decisions

**[CRITICAL]** **Rationale:**
- **Accuracy:** Every market event triggers immediate callback (no sampling, no throttling)
- **Latency:** Real-time delivery (not batched by TWS Gateway)
- **Completeness:** Separate timestamps for quote updates vs trade updates (critical for order book analysis)
- **Transparency:** Raw exchange data (no TWS aggregation or interpolation)
- **Professional-grade:** Same API used by institutional traders

**[ANTI-PATTERN]** **Alternatives Rejected:**
- **`reqMktData()` (Market Data API):** Throttled/sampled data (250ms intervals), unsuitable for tick-level analysis
  - **Why rejected:** Misses fast-moving markets, interpolated prices, batched updates
- **`reqRealTimeBars()` (5-second bars):** Too coarse for real-time trading (5s latency)
- **Historical data polling:** Introduces lag, no real-time updates, API rate limits

**Consequences:**
- ✅ True tick-level granularity (every bid/ask/trade)
- ✅ Millisecond timestamp precision (exchange timestamps)
- ✅ Separate quote/trade streams (independent analysis)
- ❌ Higher message rate: 100-1000x vs `reqMktData` (AAPL: 10-100 ticks/sec vs 4 samples/sec)
- ❌ TWS API complexity: two callback types vs one unified callback (acceptable: cleaner data model)

---

**See also:**
- [§3.1 Architecture Pattern](#31-architecture-pattern) - High-level architecture overview
- [§4 Technical Design](#4-technical-design) - Implementation guidelines
- [§8.4 Document History](#84-document-history) - Version tracking and changes

---

**End of Section 3: System Architecture**


---

## 4. Technical Design

<!-- METADATA: scope=design, priority=high, dependencies=[3-architecture] -->

### 4.1. Technology Stack

#### 4.1.1. Language Standards

**[REQUIRED]** **C++17/20** (Modern C++ with strict standards enforcement)

**Mandatory Rules:**
- ✅ `nullptr`, modern casts, smart pointers, STL containers, `const` correctness
- ✅ `std::optional`, `std::string_view`, `constexpr`, structured bindings
- ❌ `using namespace std;` in headers, C-style arrays/casts, manual `new`/`delete`, macros

#### 4.1.2. Core Libraries

| Library | Version | Purpose | Performance Impact |
|---------|---------|---------|-------------------|
| **TWS API** | 10.19+ | TWS communication | Critical (socket I/O) |
| **redis-plus-plus** | 1.3.11+ | Redis client | Medium (network I/O) |
| **RapidJSON** | cci.20230929+ | JSON serialization | High (10-50μs/msg) |
| **moodycamel::ConcurrentQueue** | 1.0.4+ | Lock-free queue | Critical (50-200ns) |
| **Catch2** | 3.5.0+ | Unit testing | N/A (test only) |

#### 4.1.3. Build Toolchain

**[REQUIRED]** **Stack:** Conan 2.x → CMake 3.20+ → Make/Ninja → GCC 11+/Clang 14+ → CTest

**Key Configurations:**
- **Conan:** `requires = ["redis-plus-plus/1.3.11", "rapidjson/cci.20230929", "concurrentqueue/1.0.4"]`
- **CMake:** `-DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake`, strict flags: `-Wall -Wextra -Wpedantic -Werror`
- **Makefile:** `make deps` → `make build` → `make test`

#### 4.1.4. External Services

**TWS Gateway:** Port 4002 (paper) / 7496 (live), enable "ActiveX and Socket Clients", read-only API, localhost only

**Redis (docker-compose.yml):**
```yaml
services:
  redis:
    image: redis:7.0
    ports: ["6379:6379"]
    command: redis-server --maxmemory 256mb --maxmemory-policy allkeys-lru
```

### 4.2. Threading Model

**[ARCHITECTURE]** **Fixed Four-Thread Design:**

| Thread | Created | Location | Responsibilities | Performance |
|--------|---------|----------|------------------|-------------|
| **1: Main** | Implicit (entry point) | `main()` | Wait on signal, dispatch callbacks, enqueue `TickUpdate` | **< 1μs per callback** |
| **2: Command Listener** | `std::thread` | `main.cpp` | Subscribe to `TWS:COMMANDS`, process subscribe/unsubscribe | Non-blocking |
| **3: Redis Worker** | `std::thread` | `main.cpp` | Dequeue, aggregate state, serialize JSON, publish | > 10K msg/sec |
| **4: EReader** | TWS API internal | `TwsClient.cpp:39` | Block on socket, parse TWS protocol, enqueue messages | I/O bound |

**[IMPORTANT]** Thread numbering reflects architectural order. Implementation details in source files.

#### Thread Creation Points

**Thread 1 (Main Thread):**
- **Created**: Implicit (program entry point)
- **Entry Function**: `int main()`
- **Purpose**: Runs message processing loop that dispatches EWrapper callbacks

**Thread 2 (Command Listener):**
- **Created**: `std::thread commandThread(...)`
- **Entry Function**: `commandListenerLoop()`
- **Purpose**: Subscribes to `TWS:COMMANDS`, processes subscribe/unsubscribe requests

**Thread 3 (Redis Worker):**
- **Created**: `std::thread workerThread(...)`
- **Entry Function**: `redisWorkerLoop()`
- **Purpose**: Consumes tick updates, aggregates state, publishes to Redis

**Thread 4 (EReader - TWS Internal):**
- **Created**: `m_reader->start()` inside `TwsClient::connect()`
- **Entry Function**: Hidden inside TWS library (not visible in application code)
- **Purpose**: Socket I/O, TWS protocol parsing, signaling Thread 1

#### Thread 1: Main Thread (Message Processing)

**Lifecycle:**
- Application main thread (implicit creation at program start)
- Runs message processing loop until disconnect

**Responsibilities:**
- Wait for signal: `m_signal->waitForSignal()` (futex, ~1-10μs)
- Call `client.processMessages()` which invokes `m_pReader->processMsgs()`
- **processMsgs() dispatches EWrapper callbacks ON THIS THREAD** (Thread 1)
- Callbacks (e.g., `tickByTickBidAsk()`) copy data to `TickUpdate` struct
- Enqueue: `m_queue.try_enqueue(update)` (lock-free, 50-200ns)

**[CRITICAL]** **Performance Constraints:**
- **Target:** < 1μs per callback
- **[ANTI-PATTERN]** No I/O operations (Redis, file, logging)
- **[ANTI-PATTERN]** No mutex locks (critical path)
- **[REQUIRED]** Just copy data and enqueue

**Code Pattern:**
```cpp
void TwsClient::tickByTickBidAsk(int reqId, time_t time, 
                                  double bidPrice, double askPrice,
                                  Decimal bidSize, Decimal askSize, ...) {
    // CRITICAL PATH: Construct update on stack, enqueue without heap allocation
    TickUpdate update;
    update.tickerId = reqId;
    update.type = TickUpdateType::BidAsk;
    update.timestamp = time * 1000; // Convert to milliseconds
    update.bidPrice = bidPrice;
    update.askPrice = askPrice;
    update.bidSize = static_cast<int>(bidSize);
    update.askSize = static_cast<int>(askSize);
    
    // CRITICAL: Non-blocking enqueue, target < 1μs
    if (!m_queue.try_enqueue(update)) {
        std::cerr << "[TWS] Queue full! Dropping update\n";
    }
}
```

#### Thread 2: Command Listener (Subscription Manager)

**Lifecycle:**
- Created in `main()` before TWS connection
- Entry function: `commandListenerLoop()`
- Runs until `g_running.store(false)` signaled
- Joined on graceful shutdown

**Responsibilities:**
- Subscribe to `TWS:COMMANDS` Redis channel (blocking subscribe)
- Receive command messages from FastAPI
- Parse JSON commands (RapidJSON Document API)
- Validate contract fields (symbol, secType, exchange)
- Call `TwsClient::subscribe()` or `TwsClient::unsubscribe()`
- Publish confirmation/error to `TWS:STATUS`

**Performance Characteristics:**
- Command rate: 1-10 commands/minute (low frequency, user-driven)
- Latency: 10-100ms acceptable (not on critical path)
- Blocking Redis SUBSCRIBE OK (separate thread)

**Code Pattern:**
```cpp
void commandListenerLoop(sw::redis::Redis& redis,
                         TwsClient& client,
                         std::atomic<bool>& running) {
    auto subscriber = redis.subscriber();
    subscriber.subscribe("TWS:COMMANDS");
    
    subscriber.on_message([&](std::string channel, std::string msg) {
        // Parse JSON command
        rapidjson::Document doc;
        doc.Parse(msg.c_str());
        
        std::string action = doc["action"].GetString();
        std::string symbol = doc["symbol"].GetString();
        
        if (action == "subscribe") {
            // Build TWS contract
            Contract contract;
            contract.symbol = symbol;
            contract.secType = doc.HasMember("secType") ? 
                doc["secType"].GetString() : "STK";
            contract.exchange = doc.HasMember("exchange") ? 
                doc["exchange"].GetString() : "SMART";
            contract.currency = doc.HasMember("currency") ? 
                doc["currency"].GetString() : "USD";
            
            // Request subscription from TWS
            int tickerId = client.subscribe(contract);
            
            // Publish confirmation to TWS:STATUS
            publishStatus(redis, "subscribed", symbol, tickerId, 
                         doc.HasMember("requestId") ? doc["requestId"].GetString() : "");
        } else if (action == "unsubscribe") {
            client.unsubscribe(symbol);
            publishStatus(redis, "unsubscribed", symbol, 0,
                         doc.HasMember("requestId") ? doc["requestId"].GetString() : "");
        }
    });
    
    while (running.load()) {
        subscriber.consume(); // Blocking wait for messages
    }
}
```

#### Thread 3: Redis Worker (Consumer Thread)

**Lifecycle:**
- Created in `main()` after command listener
- Entry function: `redisWorkerLoop()`
- Runs until `g_running.store(false)` signaled
- Joined on graceful shutdown

**Responsibilities:**
- Non-blocking dequeue: `queue.try_dequeue(update)` with sleep fallback
- State aggregation: Merge BidAsk + AllLast updates into `InstrumentState`
- Serialize: `serializeState(state)` → JSON string (RapidJSON)
- Publish: `redis.publish(channel, json)` (blocking network I/O - acceptable here)

**Performance Characteristics:**
- Target: > 10,000 messages/second
- Latency: 1-2ms per message (acceptable, off critical path)
- Blocking I/O isolated from Thread 1 callbacks

**Code Pattern:**
```cpp
void redisWorkerLoop(ConcurrentQueue<TickUpdate>& queue, 
                     RedisPublisher& redis,
                     std::atomic<bool>& running) {
    std::unordered_map<std::string, InstrumentState> stateMap;
    TickUpdate update;
    
    while (running.load()) {
        if (queue.try_dequeue(update)) {
            // Route by tickerId to symbol (dynamic mapping from subscriptions)
            std::string symbol = getSymbolFromTickerId(update.tickerId);
            
            // Aggregate BidAsk + AllLast into complete state
            auto& state = stateMap[symbol];
            if (update.type == TickUpdateType::BidAsk) {
                state.bidPrice = update.bidPrice;
                state.askPrice = update.askPrice;
                // ... update fields
                state.hasQuote = true;
            }
            
            // Publish only when we have complete snapshot (BidAsk AND AllLast)
            if (state.hasQuote && state.hasTrade) {
                std::string json = serializeState(state);
                std::string channel = "TWS:TICKS:" + symbol;
                redis.publish(channel, json); // Blocking I/O OK here
            }
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
}
```

#### Thread 4: EReader (TWS API Internal)

**Lifecycle:**
- Created by `m_reader->start()` inside `TwsClient::connect()`
- Managed entirely by TWS API library
- Runs until `eDisconnect()` called

**Responsibilities:**
- Block on TCP socket: `read()` system call
- Parse TWS binary protocol
- Create `EMessage` objects
- Enqueue to TWS API's internal thread-safe queue
- Signal Thread 1: `m_osSignal->issueSignal()`

**Performance Characteristics:**
- I/O bound (blocks on socket)
- Minimal CPU usage (protocol parsing only)
- **No application code runs on this thread** - completely managed by TWS library

**Thread Synchronization:**

| Resource | Mechanism | Threads | Performance | Location |
|----------|-----------|---------|-------------|----------|
| `m_queue` (lock-free) | Atomic operations | 1 → 3 | 50-200ns | Passed to TwsClient |
| `m_signal` | EReaderOSSignal | 4 → 1 | ~1-10μs | TWS API internal |
| `g_running` | `std::atomic<bool>` | main, 2, 3 | ~1ns | main.cpp |
| `m_subscriptions` | `std::mutex` | 1, 2 | ~10μs | Shared subscription map |

### 4.3. Memory Management

**[REQUIRED]** **Principles:**
- **RAII:** Resources acquired in constructor, released in destructor
- **Smart Pointers:** `std::unique_ptr` (exclusive), `std::shared_ptr` (shared), raw pointers (non-owning only)
- **Allocation:** Prefer stack, reuse buffers (StringBuffer), reserve capacity, avoid heap churn

**Memory Budget:** < 50 MB typical, < 100 MB max

### 4.4. Error Handling Strategy

**[REQUIRED]** **Error Classification:**

| Type | Examples | Response |
|------|----------|----------|
| **Fatal** | 502 (TWS not running), 10089 (no subscription) | Log and `exit(1)` |
| **Transient** | 504 (connection lost), 321 (rate limit) | Exponential backoff (1s → 60s cap) |
| **Informational** | 2104, 2106, 2158 | Log only |

**Exception Safety:**
- Constructors throw on failure
- Worker loops catch and log (don't crash)
- `main()` top-level catch for unexpected errors

### 4.5. State Management

**[REQUIRED]** **InstrumentState Structure:**
```cpp
struct InstrumentState {
    std::string symbol; int conId, tickerId;
    double bidPrice, askPrice, lastPrice;
    int bidSize, askSize, lastSize;
    long quoteTimestamp, tradeTimestamp;
    std::string exchange;
    bool hasQuote, hasTrade, pastLimit;
};
```

#### State Map Management

```cpp
class RedisWorker {
private:
    std::unordered_map<int, InstrumentState> m_instrumentStates; // tickerId → state
    std::mutex m_stateMutex; // Protects m_instrumentStates
    
    void processUpdate(const TickUpdate& update) {
        std::string json;
        {
            std::lock_guard<std::mutex> lock(m_stateMutex);
            InstrumentState& state = m_instrumentStates[update.tickerId];
            
            // Update partial state based on callback type
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
                state.pastLimit = update.pastLimit;
                state.hasTrade = true;
            }
            
            // Serialize complete state (not just the update)
            json = serializeState(state);
        } // Mutex released here
        
        // Publish outside mutex (blocking I/O)
        std::string channel = "TWS:TICKS:" + state.symbol;
        m_redis->publish(channel, json);
    }
};
```

#### State Lifecycle

**Initialization:**
```cpp
// When subscription confirmed (nextValidId callback)
void registerInstrument(int tickerId, const std::string& symbol, int conId) {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    InstrumentState& state = m_instrumentStates[tickerId];
    state.symbol = symbol;
    state.conId = conId;
    state.tickerId = tickerId;
}
```

**Cleanup:**
```cpp
// On disconnect, clear stale state
void clearState() {
    std::lock_guard<std::mutex> lock(m_stateMutex);
    m_instrumentStates.clear();
}
```

**Rationale:**
- **Mutex Scope:** Minimize locked region (only state update, not I/O)
- **Complete Snapshots:** Consumers always receive full state, no merge logic
- **Separate Timestamps:** Preserve quote vs trade timestamp for accuracy

---

**See also:**
- [§3.5 ADR-005](#adr-005-state-aggregation-in-consumer-thread) - State aggregation decision rationale
- [§5.2 Data Processing](#52-data-processing) - TickUpdate structure and processing
- [§5.4 Serialization](#54-serialization) - JSON serialization implementation

---

**End of Section 4: Technical Design**


---

## 5. Implementation Details

<!-- METADATA: scope=implementation, priority=high, dependencies=[4-design] -->

### 5.1. TWS Integration

<!-- METADATA: scope=tws-api, priority=critical, dependencies=[4.2-threading, 5.2-data-processing] -->

#### Understanding TWS API Naming Confusion

**[IMPORTANT]** The TWS API has **misleading naming** that confuses developers:

**EWrapper is NOT a Wrapper!**
- **What it actually is**: A **callback interface** defining methods TWS invokes when messages arrive
- **Misleading name**: "Wrapper" implies it wraps something (it doesn't)
- **Correct mental model**: Think "ECallbackInterface" or "EMessageHandler"
- **Implementation**: `TwsClient` inherits from `EWrapper` to receive callbacks

**TwsClient IS the Actual Bidirectional Adapter:**
- **Inbound Communication**: Implements `EWrapper` callbacks (TWS → Application)
  - TWS calls our methods: `tickByTickBidAsk()`, `tickByTickAllLast()`, `error()`, etc.
  - See `include/TwsClient.h` lines 1-8 for architecture clarification
- **Outbound Communication**: Wraps `EClientSocket` (Application → TWS)
  - We call TWS methods: `eConnect()`, `reqTickByTickData()`, etc.
  - Abstraction layer hiding TWS API complexity

**Architectural Layers (Highest to Lowest):**
```
┌─────────────────────────────────────────────────────────────┐
│                   APPLICATION CODE                          │
│                      TwsClient                              │
│                  (Bidirectional Adapter)                    │
│                         |                                   │
│              implements (inbound)                           │
│                         ↓                                   │
│                    EWrapper (callback interface)            │
│                  - tickByTickBidAsk()                       │
│                  - tickByTickAllLast()                      │
│                  - error()                                  │
│                  - nextValidId()                            │
│                  - [90+ callback methods]                   │
│                                                             │
│              wraps (outbound)                               │
│                         ↓                                   │
│                  EClientSocket (command interface)          │
│                  - reqTickByTickData()                      │
│                  - eConnect()                               │
│                  - eDisconnect()                            │
└─────────────────────────────────────────────────────────────┘
                         ↑ callbacks
                         | (Thread 1 - Main)
┌────────────────────────┴─────────────────────────────────────┐
│                   TWS API LIBRARY                            │
│                                                              │
│   EReader (Thread 3)            EClientSocket                │
│   - reads socket         ←───→  - sends requests            │
│   - decodes messages            - manages connection        │
│   - dispatches callbacks        [outbound commands]         │
│   [inbound events]                                          │
└──────────────────────────────────────────────────────────────┘
                         ↕
                    TCP Socket
                         ↕
┌──────────────────────────────────────────────────────────────┐
│                    TWS Gateway / IB Servers                  │
└──────────────────────────────────────────────────────────────┘
```

**Key Points:**
- **EWrapper**: Misleadingly named - it's a **callback interface**, not a wrapper
- **TwsClient**: The actual adapter - implements EWrapper (inbound) + wraps EClientSocket (outbound)
- **Thread 1 (Main)**: Where EWrapper callbacks execute after EReader dispatches them
- **Thread 3 (EReader)**: Internal to TWS API, handles socket I/O and message parsing

**[CRITICAL]** **Use `reqTickByTickData()` (not `reqMktData()`)** - true tick-level data, no sampling

**Connection Pattern:**
```cpp
// 1. Create components
TwsClient wrapper; // [MISLEADING NAME] Actually a bidirectional adapter
EClientSocket client(&wrapper, &signal); // Pass "this" for callbacks
EReader reader(&client, &signal);

// 2. Connect & start reader
client.eConnect(host, port, clientId);
reader.start(); // Spawns Thread 3 (EReader internal)

// 3. Wait for nextValidId() callback (connection handshake complete)
// 4. Subscribe: client.reqTickByTickData(tickerId, contract, "BidAsk"|"AllLast", 0, false)
```

**Contract Definition:**
```cpp
Contract c;
c.symbol = "AAPL";
c.secType = "STK";
c.currency = "USD";
c.exchange = "SMART";
c.primaryExchange = "NASDAQ"; // Required for disambiguation
```

**Callbacks to Implement (Inbound API):**
- `nextValidId()` - Connection ready, start subscriptions
- `tickByTickBidAsk()` - Quote updates (bid/ask price/size) - **executes on Thread 1**
- `tickByTickAllLast()` - Trade updates (last price/size) - **executes on Thread 1**
- `error()` - Error handling with code mapping - **executes on Thread 1**
- `connectionClosed()` - Clear state, trigger reconnect - **executes on Thread 1**

**[PITFALL]** Common Mistakes:
- Subscribing before `nextValidId()` callback (requests ignored)
- Using `std::time(nullptr)` instead of TWS `time` parameter
- Blocking in callbacks (kills performance)
- Missing `primaryExchange` (ambiguous symbols)
- Thinking "EWrapper" wraps something (it doesn't - it's a callback interface)

### 5.2. Data Processing

<!-- METADATA: scope=message-queue, priority=critical, dependencies=[4.2-threading, 5.1-tws-integration] -->

**TickUpdate Structure:**
```cpp
enum class TickUpdateType { BidAsk, AllLast };

struct TickUpdate {
    int tickerId;
    TickUpdateType type;
    long timestamp; // From TWS callback
    double bidPrice, askPrice, lastPrice;
    int bidSize, askSize, lastSize;
    bool pastLimit;
};
```

**Processing Flow:**
1. EWrapper callback fires (Thread 2)
2. Copy data to `TickUpdate` struct (< 1μs)
3. Enqueue: `g_tickQueue.enqueue(update)` (lock-free)
4. Thread 3 dequeues, updates state, serializes, publishes

**[REQUIRED]** Callback Pattern:
```cpp
void tickByTickBidAsk(int reqId, long time, double bid, double ask, ...) {
    TickUpdate update{reqId, TickUpdateType::BidAsk, time, bid, ask, ...};
    g_tickQueue.enqueue(update); // Non-blocking
    // NO I/O HERE
}
```

### 5.3. Redis Publishing

<!-- METADATA: scope=redis-client, priority=high, dependencies=[5.2-data-processing, 5.4-serialization] -->

**RedisPublisher Class:**
```cpp
class RedisPublisher {
    std::unique_ptr<sw::redis::Redis> m_redis;
    std::atomic<bool> m_running;
    
public:
    RedisPublisher(const std::string& host, int port);
    void run();  // Worker loop for Thread 3
    void stop(); // Signal shutdown
};
```

**Worker Loop:**
```cpp
void RedisPublisher::run() {
    while (m_running.load()) {
        TickUpdate update;
        if (g_tickQueue.wait_dequeue_timed(update, 100ms)) {
            // Update state (mutex protected)
            // Serialize to JSON
            // Publish: m_redis->publish(channel, json)
        }
    }
}
```

**Channel Pattern:** `TWS:TICKS:{SYMBOL}`, `TWS:STATUS`

**[PITFALL]** Connection pooling: Use `ConnectionPoolOptions` (size=3) for thread safety

### 5.4. Serialization

<!-- METADATA: scope=json-serialization, priority=high, dependencies=[2.1.3-data-requirements, 4.5-state-management] -->

**RapidJSON Writer Pattern:**
```cpp
std::string serializeState(const InstrumentState& state) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    
    writer.StartObject();
    writer.Key("instrument"); writer.String(state.symbol.c_str());
    writer.Key("timestamp"); writer.Int64(std::max(state.quoteTimestamp, state.tradeTimestamp));
    writer.Key("price");
    writer.StartObject();
    writer.Key("bid"); writer.Double(state.bidPrice);
    writer.Key("ask"); writer.Double(state.askPrice);
    writer.Key("last"); writer.Double(state.lastPrice);
    writer.EndObject();
    // ... size, timestamps, exchange
    writer.EndObject();
    
    return buffer.GetString();
}
```

**[PERFORMANCE]** Buffer reuse: Create once, call `buffer.Clear()` for each serialization

### 5.5. Key Patterns

<!-- METADATA: scope=implementation-patterns, priority=medium, dependencies=[5.1-tws-integration, 4.4-error-handling] -->

**Reconnection with Exponential Backoff:**
```cpp
void reconnectLoop() {
    int delay = 1;
    const int maxDelay = 60;
    while (!shutdown && delay <= maxDelay) {
        std::this_thread::sleep_for(std::chrono::seconds(delay));
        if (client.connect(host, port, clientId)) {
            clearState(); resubscribeAll(); return;
        }
        delay = std::min(delay * 2, maxDelay);
    }
}
```

**Rate Limiting:**
```cpp
class RateLimiter {
    std::deque<time_point> m_timestamps;
    std::mutex m_mutex;
public:
    bool allowRequest(int maxRequests, duration window) {
        // Remove expired, check size < maxRequests
    }
};
```

### 5.6. Anti-Patterns

<!-- METADATA: scope=pitfalls, priority=critical, dependencies=[4.2-threading, 5.1-tws-integration] -->

**[ANTI-PATTERN]** Things to NEVER do:

| Anti-Pattern | Why | Correct Approach |
|--------------|-----|------------------|
| I/O in EWrapper callbacks | Blocks critical path | Enqueue and return |
| `using namespace std;` in headers | Namespace pollution | Use in `.cpp` only |
| `reqMktData()` | Sampled data | Use `reqTickByTickData()` |
| `std::time(nullptr)` for timestamps | Server time, not market time | Use TWS `time` parameter |
| Publishing partial updates | Consumers must merge | Publish complete state |
| Global mutex on state map | Contention | Thread 3 exclusive access |
| Manual `new`/`delete` | Memory leaks | Smart pointers |

---

**See also:**
- [§4.2 Threading Model](#42-threading-model) - Understanding critical path constraints
- [§5.1 TWS Integration](#51-tws-integration) - Proper callback implementation
- [§C.1 Threading Model Cheat Sheet](#c1-threading-model-cheat-sheet) - Quick reference

---

**End of Section 5: Implementation Details**

---

## 6. Build & DevOps

<!-- METADATA: scope=devops, priority=high, dependencies=[4-design] -->

### 6.1. Dependency Management

**[REQUIRED]** **Conan 2.x Workflow:**

```bash
# Install dependencies
conan install . --build=missing -s build_type=Release

# Output: conan_toolchain.cmake, conanbuildinfo.txt
```

**conanfile.py:**
```python
requires = [
    "redis-plus-plus/1.3.11",
    "rapidjson/cci.20230929",
    "concurrentqueue/1.0.4"
]
test_requires = ["catch2/3.5.0"]
generators = "CMakeToolchain", "CMakeDeps"
```

**TWS API:** Not in Conan - vendor in `vendor/tws-api/` or manual install

### 6.2. Build Process

**[REQUIRED]** **Build Commands:**

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake

# Build
cmake --build build -j$(nproc)

# Output: build/tws_bridge
```

**Makefile Targets:**
```makefile
make deps        # Conan install
make build       # CMake configure + build
make test        # Run CTest
make clean       # Remove build artifacts
make docker-up   # Start Redis container
make run         # Build + run bridge
```

**Compiler Flags:** `-Wall -Wextra -Wpedantic -Werror -O3` (Release), `-g -O0 -fsanitize=address` (Debug)

### 6.3. CI/CD Pipeline

**[REQUIRED]** **GitHub Actions Workflow:**

**Build Job (`.github/workflows/ci.yml`):**
1. Install Conan 2.x, detect profile
2. Cache `~/.conan2` (key: hash of `conanfile.py`)
3. `conan install . --build=missing`
4. `cmake -B build && cmake --build build`
5. Start Redis container (`docker run -d redis:7.0`)
6. `ctest --test-dir build --output-on-failure`
7. Upload build artifacts

**Matrix:** Ubuntu 22.04, GCC 11/12, Clang 14/15

**Triggers:** Push to `main`, pull requests, tags `v*`

**Release Job (on tag `v*`):**
- Build release binaries
- Create GitHub Release with artifacts
- Optional: `conan create` for package publishing

### 6.4. Development Workflow

**First-Time Setup:**
```bash
# Clone and setup
git clone <repo-url> && cd tws-redis-bridge
make deps
make docker-up

# Configure TWS Gateway (port 4002, enable API)
```

**Development Loop:**
```bash
# Edit code
vim src/TwsClient.cpp

# Build and test
make build test

# Run locally
./build/tws_bridge
```

**Testing Modes:**
```bash
# Live mode (requires TWS Gateway)
./build/tws_bridge --host 127.0.0.1 --port 4002

# Replay mode (offline testing)
./build/tws_bridge --replay data/ticks.csv --symbol AAPL
```

**Docker Redis:**
```bash
docker-compose up -d                    # Start
docker exec -it tws-redis redis-cli MONITOR  # Watch activity
docker-compose down                     # Stop
```

---

**See also:**
- [§6.2 Build Process](#62-build-process) - CMake configuration details
- [§6.3 CI/CD Pipeline](#63-cicd-pipeline) - Automation workflow
- [§C.3 Build & Test Commands](#c3-build--test-commands) - Quick reference cheat sheet

---

**End of Section 6: Build & DevOps**


---

## 7. Testing Strategy

<!-- METADATA: scope=testing, priority=critical, dependencies=[5-implementation] -->

### 7.1. Unit Testing

<!-- METADATA: scope=unit-tests, priority=high, dependencies=[5.2-data-processing, 5.4-serialization] -->

**[REQUIRED]** **Framework:** Catch2 v3.5.0+

**Test Coverage:**
- JSON serialization (RapidJSON Writer output)
- State aggregation (partial updates → complete snapshot)
- Timestamp handling (TWS time vs system time)
- Error code mapping (fatal vs transient vs informational)
- Rate limiter logic

**Test Pattern:**
```cpp
TEST_CASE("State aggregates BidAsk and AllLast updates", "[state]") {
    InstrumentState state;
    TickUpdate bidAsk{1, TickUpdateType::BidAsk, 1000, 171.55, 171.57, ...};
    TickUpdate allLast{1, TickUpdateType::AllLast, 1500, 171.56, ...};
    
    updateState(state, bidAsk);
    updateState(state, allLast);
    
    REQUIRE(state.bidPrice == 171.55);
    REQUIRE(state.lastPrice == 171.56);
    REQUIRE(state.hasQuote);
    REQUIRE(state.hasTrade);
}
```

**File Structure:** `tests/test_<component>.cpp` for each `src/<component>.cpp`

**Run:** `make test` or `ctest --test-dir build --output-on-failure`

### 7.2. Integration Testing

<!-- METADATA: scope=integration-tests, priority=high, dependencies=[5.1-tws-integration, 5.3-redis-publishing, 6.2-build-process] -->

**Live TWS Testing:**
```bash
# 1. Start TWS Gateway (paper trading, port 4002)
# 2. Start Redis: docker-compose up -d
# 3. Run bridge: ./build/tws_bridge
# 4. Monitor: redis-cli SUBSCRIBE "TWS:TICKS:*"
# 5. Verify: JSON messages published on tick updates
```

**Replay Mode Testing:**
```bash
# 1. Create test data: data/aapl_ticks.csv
# 2. Run: ./build/tws_bridge --replay data/aapl_ticks.csv --symbol AAPL
# 3. Verify: Redis receives expected messages
```

**Subscription Lifecycle:**
- Connect → `nextValidId()` → Subscribe → Receive data
- Disconnect → Reconnect → Resubscribe → Resume data

**End-to-End Validation:**
- 3+ concurrent instrument subscriptions
- Latency < 50ms (measure: TWS timestamp → Redis PUBLISH)
- Zero message loss during normal operation

### 7.3. Performance Testing

<!-- METADATA: scope=benchmarks, priority=medium, dependencies=[2.2.1-performance-targets, 4.2-threading] -->

**Latency Benchmarks:**
```bash
# Measure each stage
- EWrapper callback: < 1μs (use std::chrono in tests)
- Queue enqueue: 50-200ns (measure with lock-free queue)
- JSON serialization: 10-50μs (RapidJSON Writer)
- Redis publish: 0.1-1ms (network I/O)
```

**Throughput Test:**
- Worker target: > 10,000 messages/second
- Queue backlog: Monitor `g_tickQueue.size_approx()` (should be < 1000)

**Tools:** `std::chrono::high_resolution_clock`, `perf stat`, `valgrind --tool=callgrind`

### 7.4. Acceptance Criteria

<!-- METADATA: scope=acceptance, priority=critical, dependencies=[1.5-success-metrics, 2-requirements] -->

**[ACCEPTANCE]** **MVP Completion Requirements:**

| Category | Criteria | Verification |
|----------|----------|--------------|
| **Functional** | Process 3+ instruments | Subscribe to AAPL, SPY, TSLA simultaneously |
| | Receive BidAsk + AllLast | Verify both callback types fire |
| | Publish complete snapshots | JSON contains bid, ask, last fields |
| | Handle disconnect/reconnect | Kill TWS, restart, verify resubscribe |
| **Performance** | Latency < 50ms (median) | Measure timestamp → Redis |
| | Throughput > 10K msg/sec | Load test with replay mode |
| | Callback < 1μs | Unit test with timer |
| **Reliability** | Zero loss (normal operation) | Count TWS ticks vs Redis publishes |
| | Graceful degradation | Disconnect scenarios |
| | State rebuild on reconnect | Clear and resubscribe |
| **CI/CD** | Automated pipeline | All tests pass on commit |
| | Reproducible builds | Conan lockfiles |

#### Validation Gates

**[ACCEPTANCE]** **Progressive Validation Strategy:**

The project uses a fail-fast approach with 12 validation gates that must pass sequentially. Each gate validates a critical assumption before proceeding to the next phase.

| Gate | Criteria | Evidence | Status |
|------|----------|----------|--------|
| **Gate 1a** | TWS API compiles and links | Build log shows success, binary created | ✅ **PASSED** |
| **Gate 1b** | Dev environment ready | Redis container runs, `make dev-up` works | ⏳ **IN PROGRESS** |
| **Gate 2a** | First bar callback received | Console shows `historicalData()` log | 🔴 **BLOCKED** |
| **Gate 2b** | Data flows to queue | TWS → Queue → Console prints bars | 🔴 **BLOCKED** |
| **Gate 2c** | End-to-end to Redis | `redis-cli` receives bar messages | 🔴 **BLOCKED** |
| **Gate 3a** | Real-time bars work | Real-time bar updates flow to Redis | 🔴 **PENDING** |
| **Gate 3b** | Queue performance | Benchmark: enqueue < 1μs | 🔴 **PENDING** |
| **Gate 4** | Historical ticks work | `historicalTicksBidAsk()` callback received | 🔴 **PENDING** |
| **Gate 5** | State aggregation | Console prints merged bid/ask/last snapshots | 🔴 **PENDING** |
| **Gate 6** | JSON schema valid | Unit tests pass, benchmark < 50μs | 🔴 **PENDING** |
| **Gate 7** | Latency target met | End-to-end < 50ms (p50) | 🔴 **PENDING** |
| **Gate 8** | Multi-instrument works | 3+ instruments, no state corruption | 🔴 **PENDING** |
| **Gate 9** | Error handling robust | Gateway restart: graceful shutdown | 🔴 **PENDING** |
| **Gate 10** | Reconnection works | Auto-reconnect after disconnect | 🔴 **PENDING** |
| **Gate 11** | Replay mode functional | Offline test without TWS works | 🔴 **PENDING** |
| **Gate 12** | Performance validated | Throughput > 10K msg/sec | 🔴 **PENDING** |

**Legend:**
- ✅ **PASSED** - Gate completed successfully, evidence documented
- ⏳ **IN PROGRESS** - Currently working on this gate, debugging/implementing
- 🔴 **BLOCKED** - Waiting for previous gate to pass or external dependency
- 🔴 **PENDING** - Not yet started, scheduled for future session

**Decision Protocol at Each Gate:**
1. **✅ PASS:** Document result, commit code, proceed to next phase
2. **⚠️ CONDITIONAL PASS:** Works but needs optimization → add tech debt ticket, proceed
3. **❌ FAIL:** Stop, diagnose root cause, decide: Fix (extend timeline), Pivot (simplify design), or Abort (acknowledge blocker)

**Reference:** See `docs/FAIL-FAST-PLAN.md` § 3 for detailed gate criteria, validation procedures, and contingency plans.


### 7.5. Test Utilities

<!-- METADATA: scope=test-tools, priority=medium, dependencies=[5.2-data-processing, 7.2-integration-testing] -->

**CLI Replay Utility:**

**Purpose:** Test serialization/publishing without live TWS

**Input Format (CSV):**
```csv
timestamp,price,size
2025-11-15T10:00:01.123Z,150.25,100
2025-11-15T10:00:01.456Z,150.26,50
```

**Implementation:**
- Parse timestamps with `std::chrono::parse()` (C++20) or `date.h`
- Sleep between enqueues to simulate real-time spacing
- Enqueue to same `g_tickQueue` as live system

**Usage:**
```bash
./build/tws_bridge --replay data/ticks.csv --symbol AAPL
```

**Verification:**
```bash
# Terminal 1
./build/tws_bridge --replay data/aapl_ticks.csv --symbol AAPL

# Terminal 2
redis-cli SUBSCRIBE "TWS:TICKS:AAPL"
```

---

**See also:**
- [§7.2 Integration Testing](#72-integration-testing) - Live testing procedures
- [§7.4 Acceptance Criteria](#74-acceptance-criteria) - MVP validation checklist
- [§C.4 Testing Workflow](#c4-testing-workflow) - Quick reference for testing

---

**End of Section 7: Testing Strategy**

---

## 8. Appendices

<!-- METADATA: scope=reference-materials, priority=low, dependencies=[] -->

### 8.1. Glossary

<!-- METADATA: scope=terminology, priority=low -->

**ADR (Architecture Decision Record)**
- Structured documentation capturing architectural decisions with context, rationale, alternatives rejected, and consequences

**AllLast Callback**
- TWS API callback (`tickByTickAllLast`) delivering trade execution data: last price, size, exchange, timestamp

**BidAsk Callback**
- TWS API callback (`tickByTickBidAsk`) delivering quote updates: bid/ask prices and sizes

**CAS (Compare-And-Swap)**
- Atomic CPU instruction enabling lock-free algorithms; basis for `moodycamel::ConcurrentQueue`

**Conan**
- C++ package manager for dependency management; v2.x uses toolchain files for CMake integration

**conId (Contract ID)**
- Interactive Brokers' unique integer identifier for financial instruments (e.g., AAPL = 265598)

**CRTP (Curiously Recurring Template Pattern)**
- Static polymorphism technique using templates; mentioned for future performance optimizations

**EClient**
- TWS API class for sending requests to TWS Gateway (e.g., `reqTickByTickData`, `eConnect`)

**EReader**
- TWS API class managing background thread that reads/parses TCP socket data from TWS Gateway

**EReaderOSSignal**
- TWS API synchronization primitive (futex-based) signaling main thread when messages are ready

**EWrapper**
- TWS API callback interface implemented by application; receives market data, errors, connection events

**Fan-Out**
- Messaging pattern where 1 producer publishes to N consumers; Redis Pub/Sub provides native fan-out

**Futex (Fast Userspace Mutex)**
- Linux kernel mechanism for efficient thread synchronization; used by `EReaderOSSignal` and `std::mutex`

**HFT (High-Frequency Trading)**
- Trading strategy requiring sub-millisecond latency; design inspiration for lock-free queues

**Lock-Free**
- Concurrency algorithm using atomic operations instead of mutexes; guarantees system-wide progress

**MPMC (Multi-Producer Multi-Consumer)**
- Queue design supporting multiple concurrent readers/writers; unnecessary overhead for this project

**MVP (Minimum Viable Product)**
- 7-day development sprint delivering core functionality: 3 instruments, < 50ms latency, reconnection

**NUMA (Non-Uniform Memory Access)**
- Multi-socket CPU architecture; future optimization for thread/memory affinity

**pastLimit**
- TWS tick attribute indicating trade occurred outside regular market hours (pre-market/after-hours)

**Pub/Sub (Publish/Subscribe)**
- Messaging pattern decoupling producers from consumers; Redis provides in-memory Pub/Sub broker

**RAII (Resource Acquisition Is Initialization)**
- C++ idiom where resource lifetime is tied to object lifetime (constructor acquires, destructor releases)

**RapidJSON**
- High-performance JSON library; Writer (SAX) API used for 10-50μs serialization

**Redis**
- In-memory data structure store used as Pub/Sub message broker (port 6379)

**SAX (Simple API for XML)**
- Event-driven parsing/serialization pattern; RapidJSON Writer uses SAX-style API (vs DOM)

**SPSC (Single-Producer Single-Consumer)**
- Lock-free queue design optimized for one writer, one reader; `moodycamel::ConcurrentQueue` pattern

**Tick**
- Individual market data event: quote update (bid/ask change) or trade execution

**tickerId**
- Application-assigned integer mapping TWS callbacks to instruments; key for `m_instrumentStates` map

**Tick-by-Tick**
- TWS API mode delivering every market event immediately (vs sampled/throttled `reqMktData`)

**TWS (Trader Workstation)**
- Interactive Brokers desktop application exposing TCP API for market data and order placement

**TWS Gateway**
- Headless version of TWS for server deployments; exposes same API (ports 4002 paper, 7496 live)


### 8.2. References

<!-- METADATA: scope=external-docs, priority=low -->

#### Official Documentation

**Interactive Brokers TWS API**
- **Official Documentation:** https://interactivebrokers.github.io/tws-api/
- **C++ Client Portal:** https://interactivebrokers.github.io/tws-api/cpp_client.html
- **Tick-by-Tick Data:** https://interactivebrokers.github.io/tws-api/tick_by_tick.html
- **Error Codes:** https://interactivebrokers.github.io/tws-api/message_codes.html
- **GitHub Repository:** https://github.com/InteractiveBrokers/tws-api-public
- **Download Page:** https://www.interactivebrokers.com/en/trading/tws-api.php

**Redis**
- **Official Site:** https://redis.io/
- **Pub/Sub Documentation:** https://redis.io/docs/interact/pubsub/
- **Commands Reference:** https://redis.io/commands/
- **Docker Image:** https://hub.docker.com/_/redis

#### C++ Libraries

**redis-plus-plus**
- **GitHub:** https://github.com/sewenew/redis-plus-plus
- **Documentation:** https://github.com/sewenew/redis-plus-plus/blob/master/README.md
- **Conan Package:** https://conan.io/center/recipes/redis-plus-plus
- **Version Used:** 1.3.11+

**RapidJSON**
- **GitHub:** https://github.com/Tencent/rapidjson
- **Documentation:** https://rapidjson.org/
- **Tutorial:** https://rapidjson.org/md_doc_tutorial.html
- **Conan Package:** https://conan.io/center/recipes/rapidjson
- **Version Used:** cci.20230929+

**moodycamel::ConcurrentQueue**
- **GitHub:** https://github.com/cameron314/concurrentqueue
- **Benchmarks:** https://github.com/cameron314/concurrentqueue/wiki/Benchmarks
- **Design Notes:** https://moodycamel.com/blog/2014/a-fast-general-purpose-lock-free-queue-for-c++
- **Conan Package:** https://conan.io/center/recipes/concurrentqueue
- **Version Used:** 1.0.4+

**Catch2**
- **GitHub:** https://github.com/catchorg/Catch2
- **Documentation:** https://github.com/catchorg/Catch2/tree/devel/docs
- **Tutorial:** https://github.com/catchorg/Catch2/blob/devel/docs/tutorial.md
- **Conan Package:** https://conan.io/center/recipes/catch2
- **Version Used:** 3.5.0+

#### Build Tools

**Conan**
- **Official Site:** https://conan.io/
- **Documentation:** https://docs.conan.io/2/
- **Getting Started:** https://docs.conan.io/2/tutorial.html
- **CMake Integration:** https://docs.conan.io/2/reference/tools/cmake.html
- **Version Required:** 2.0+

**CMake**
- **Official Site:** https://cmake.org/
- **Documentation:** https://cmake.org/documentation/
- **Tutorial:** https://cmake.org/cmake/help/latest/guide/tutorial/index.html
- **Version Required:** 3.20+

**CTest**
- **Documentation:** https://cmake.org/cmake/help/latest/manual/ctest.1.html
- **Dashboard:** https://cmake.org/cmake/help/latest/manual/ctest.1.html#ctest-dashboard-client

#### Design Patterns & Best Practices

**Lock-Free Programming**
- **Introduction:** https://preshing.com/20120612/an-introduction-to-lock-free-programming/
- **Memory Ordering:** https://en.cppreference.com/w/cpp/atomic/memory_order
- **C++ Atomics:** https://en.cppreference.com/w/cpp/atomic/atomic

**Modern C++ Guidelines**
- **C++ Core Guidelines:** https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
- **C++17 Features:** https://en.cppreference.com/w/cpp/17
- **C++20 Features:** https://en.cppreference.com/w/cpp/20

**Architecture Decision Records (ADR)**
- **ADR GitHub:** https://adr.github.io/
- **Template:** https://github.com/joelparkerhenderson/architecture-decision-record
- **Best Practices:** https://cognitect.com/blog/2011/11/15/documenting-architecture-decisions

#### Related Projects

**trader-pro Platform**
- **GitHub:** https://github.com/faroukBakari/trader-pro
- **Description:** Vue.js + FastAPI trading platform consuming TWS-Redis Bridge data


### 8.3. Quick Reference Cards

<!-- METADATA: scope=cheat-sheets, priority=medium -->

#### C.1. Threading Model Cheat Sheet

| Thread | Managed By | Creation Point | Purpose | Critical Path | Performance Target | Context |
|--------|-----------|----------------|---------|---------------|-------------------|---------|
| **Thread 1: Main** | Application | Implicit (entry point) | Process callbacks, enqueue `TickUpdate` | **YES** | **< 1μs per callback** | EWrapper callbacks execute here |
| **Thread 2: Command Listener** | Application | `main.cpp` | Subscribe to `TWS:COMMANDS`, process FastAPI requests | No | 1-10 cmd/min | Blocking SUBSCRIBE OK |
| **Thread 3: Redis Worker** | Application | `main.cpp` | Dequeue, update state, serialize, publish | No | > 10K msg/sec | Blocking I/O OK |
| **Thread 4: EReader** | TWS API | `TwsClient.cpp:39` | Read TCP socket, parse TWS protocol | No | I/O bound | Don't modify (TWS managed) |

**Creation Locations:**
- **Thread 1**: Implicit at `int main()` entry point
- **Thread 2**: `std::thread commandThread(...)` for command listening
- **Thread 3**: `std::thread workerThread(...)` for Redis publishing
- **Thread 4**: `m_reader->start()` inside `TwsClient::connect()`

**Inter-Thread Communication:**
- **Thread 4 → Thread 1:** `EReaderOSSignal` (TWS internal futex, ~1-10μs)
- **Thread 1 → Thread 3:** `moodycamel::ConcurrentQueue` (lock-free, 50-200ns)
- **Thread 2 ↔ Main:** Shared `TwsClient` with mutex for subscribe/unsubscribe

**Synchronization:**
- `m_queue` - Lock-free atomic (Thread 1 → Thread 3)
- `m_subscriptions` - `std::mutex` (Threads 1, 2)
- `g_running` - `std::atomic<bool>` (main, Threads 2, 3)

#### C.2. TWS API Communication Model

**Bidirectional Pattern:**
```
Application
    ↕
TwsClient (Bidirectional Adapter)
    ↓ implements (Inbound)         ↑ wraps (Outbound)
EWrapper (callbacks)            EClientSocket (commands)
    ↖ dispatched by            ↗ sends requests to
         EReader (Thread 3)
                 ↕
            TCP Socket
```

**Inbound Communication (TWS → Application):**
- **Interface**: `EWrapper` (misleadingly named - it's a callback interface)
- **Implementation**: `TwsClient` inherits `EWrapper`
- **Execution Context**: Callbacks execute on **Thread 1** (Main)
- **Key Methods**: `tickByTickBidAsk()`, `tickByTickAllLast()`, `error()`, `nextValidId()`
- **Performance Constraint**: < 1μs per callback (NO I/O, NO mutex)

**Outbound Communication (Application → TWS):**
- **Interface**: `EClientSocket`
- **Wrapper**: `TwsClient` has `std::unique_ptr<EClientSocket> m_client`
- **Execution Context**: Called from **Thread 1** (Main) or **Thread 2** (Command Listener)
- **Key Methods**: `eConnect()`, `reqTickByTickData()`, `cancelTickByTickData()`, `eDisconnect()`
- **Pattern**: `client.subscribe()` → `m_client->reqTickByTickData(...)` → network I/O

**[IMPORTANT]** "EWrapper" is a confusing name - think "ECallbackInterface" instead.

#### C.3. TWS Error Code Quick Reference

| Code | Meaning | Classification | Action | Retry Strategy |
|------|---------|----------------|--------|----------------|
| **502** | TWS not running | **FATAL** | Log and `exit(1)` | Manual restart required |
| **504** | Connection lost | **TRANSIENT** | Trigger reconnection | Exponential backoff (1s→60s) |
| **10089** | No market data permissions | **FATAL** | Log and `exit(1)` | Fix subscription, restart |
| **321** | Rate limit exceeded | **WARNING** | Activate backoff | Pause 10s, reduce rate |
| **2104** | Market data farm connected | **INFO** | Log only | None |
| **2106** | HMDS data farm connected | **INFO** | Log only | None |
| **2158** | Sec-def data farm connected | **INFO** | Log only | None |

**Reconnection Pattern:**
```cpp
int delay = 1; // seconds
while (!shutdown && delay <= 60) {
    sleep(delay);
    if (connect()) { clearState(); resubscribe(); break; }
    delay = min(delay * 2, 60);
}
```

#### C.4. Build & Test Commands

**First-Time Setup:**
```bash
# Install dependencies
make deps              # = conan install . --build=missing

# Start services
make docker-up         # = docker-compose up -d redis
```

**Development Workflow:**
```bash
# Build
make build             # = cmake -B build + cmake --build build

# Test
make test              # = ctest --test-dir build --output-on-failure

# Run live
make run               # = build + ./build/tws_bridge

# Clean
make clean             # Remove build artifacts
```

**Manual Commands:**
```bash
# Configure with Conan toolchain
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake

# Build with parallel jobs
cmake --build build -j$(nproc)

# Run with flags
./build/tws_bridge --host 127.0.0.1 --port 4002 --client-id 0

# Replay mode (offline testing)
./build/tws_bridge --replay data/ticks.csv --symbol AAPL --ticker-id 1
```

**Docker Redis:**
```bash
docker-compose up -d                          # Start Redis
docker exec -it tws-redis redis-cli MONITOR   # Watch activity
redis-cli SUBSCRIBE "TWS:TICKS:*"             # Monitor all ticks
docker-compose down                           # Stop services
```

#### C.4. Testing Workflow

**Unit Tests (Catch2):**
```bash
# Run all tests
ctest --test-dir build --output-on-failure

# Run specific test
./build/tests/test_serialization

# Verbose output
ctest --test-dir build -V
```

**Integration Test Checklist:**
- [ ] TWS Gateway running (paper: 4002, live: 7496)
- [ ] Redis container started (`docker-compose up -d`)
- [ ] API enabled in TWS (Settings → API → Enable ActiveX)
- [ ] Read-only mode enabled (Settings → API → Read-Only API)

**Live Testing Steps:**
```bash
# Terminal 1: Start bridge
./build/tws_bridge --host 127.0.0.1 --port 4002

# Terminal 2: Monitor Redis
redis-cli SUBSCRIBE "TWS:TICKS:AAPL"

# Terminal 3: Monitor status
redis-cli SUBSCRIBE "TWS:STATUS"
```

**Replay Testing:**
```bash
# Create test data: data/aapl_ticks.csv
# Format: timestamp,price,size
# 2025-11-15T10:00:01.123Z,150.25,100

# Run replay
./build/tws_bridge --replay data/aapl_ticks.csv --symbol AAPL

# Verify output in Terminal 2 (redis-cli)
```

**Performance Benchmarks:**
```bash
# Measure latency
# Add timing in callbacks: auto t1 = steady_clock::now();

# Profile with perf
perf stat -e cycles,instructions,cache-misses ./build/tws_bridge

# Memory profiling
valgrind --tool=massif ./build/tws_bridge
```

#### C.5. JSON Schema Quick Reference

**Complete Tick Message:**
```json
{
  "instrument": "AAPL",
  "conId": 265598,
  "timestamp": 1700000000500,
  "price": {"bid": 171.55, "ask": 171.57, "last": 171.56},
  "size": {"bid": 100, "ask": 200, "last": 50},
  "timestamps": {"quote": 1700000000000, "trade": 1700000000500},
  "exchange": "NASDAQ",
  "tickAttrib": {"pastLimit": false}
}
```

**Channel Naming:**
- `TWS:TICKS:{SYMBOL}` - Per-instrument tick data
- `TWS:STATUS` - System events (connect/disconnect/errors)

**Status Message Types:**
- `"type": "connected"` - TWS connection established
- `"type": "disconnected"` - Connection lost
- `"type": "error"` - Error event (includes errorCode, errorMsg)
- `"type": "subscribed"` - Instrument subscription confirmed


### 8.4. Document History

<!-- METADATA: scope=meta, priority=low -->

**Version 2.0** (November 15, 2025)
- Complete restructure with ADR-style architectural decisions
- Added comprehensive cross-reference index (moved to document top)
- Enhanced metadata tags for AI agent readability
- Added 5 quick reference cards in Appendix C
- Populated glossary (35+ terms) and external references
- Simplified data flow section with latency budget table
- Verified all cross-references and section numbering

**Version 1.0** (Initial Draft)
- Basic project specification structure
- Requirements and architecture overview


---

**End of Specification**

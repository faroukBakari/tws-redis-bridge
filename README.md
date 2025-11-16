# TWS-Redis Bridge

A low-latency C++ microservice that consumes real-time market data from Interactive Brokers' TWS API and publishes to Redis Pub/Sub channels.

## 🚀 Quick Start

### Prerequisites

- **C++17** compiler (GCC 11+, Clang 14+)
- **CMake** 3.20+
- **Conan** 2.x package manager
- **Docker** (for Redis)
- **IB Gateway 10.30+** or TWS Desktop (Interactive Brokers)
- **TWS API v1037.02** (installation instructions below)

### 1. Install TWS API

The TWS C++ API v1037.02 (compatible with IB Gateway 10.30+) must be manually installed:

```bash
cd vendor
./install_tws_api.sh
# Or follow instructions in vendor/README.md
```

### 2. Install Dependencies

```bash
make deps
```

### 3. Build

```bash
make build
```

### 4. Start Redis

```bash
make docker-up
```

### 5. Configure IB Gateway

1. Open **IB Gateway 10.30+** (or TWS Desktop)
2. Go to: **Configure** → **Settings** → **API** → **Settings**
3. Enable: ✅ **"Enable ActiveX and Socket Clients"**
4. Enable: ✅ **"Read-Only API"** (recommended for safety)
5. Note the socket port:
   - **Paper Trading**: 4002
   - **Live Trading**: 7497 (default for this project)

**⚠️ Market Hours Testing Note:**

If testing **outside market hours** (9:30 AM - 4:00 PM ET):
- Bridge automatically uses historical bar data (`reqHistoricalData`) for testing
- Real-time bars available 24/7 for major indices (SPY, QQQ, IWM)
- Tick-by-tick data requires live market hours
- Architecture validation works identically with bars or ticks
- See `docs/FAIL-FAST-PLAN.md` § 2.1 for market hours pivot strategy

### 6. Run the Bridge

```bash
./build/tws_bridge
```

### 7. Monitor Redis Output

In another terminal:

```bash
# Subscribe to all tick channels
make redis-cli
SUBSCRIBE "TWS:TICKS:*"

# Or monitor all Redis commands
make redis-monitor
```

## 📖 Architecture

See comprehensive documentation in `docs/`:
- **[PROJECT-SPECIFICATION.md](docs/PROJECT-SPECIFICATION.md)** - Complete technical specification
- **[FAIL-FAST-PLAN.md](docs/FAIL-FAST-PLAN.md)** - 5-day MVP implementation plan
- **[DESIGN-DECISIONS.md](docs/DESIGN-DECISIONS.md)** - Architectural decision records
- **[TWS-REDIS-BRIDGE-IMPL.md](docs/TWS-REDIS-BRIDGE-IMPL.md)** - Implementation guide

### Key Design Points

- **3-Thread Model**: Thread 1 (Main/Callbacks) → Thread 2 (Redis Worker) ← Thread 3 (EReader - TWS)
  - Thread 1: Message processing, EWrapper callbacks execute here (< 1μs constraint)
  - Thread 2: State aggregation, JSON serialization, Redis publishing
  - Thread 3: Socket I/O, TWS protocol parsing (managed by TWS API)
- **Bidirectional Adapter**: `TwsClient` implements `EWrapper` (inbound callbacks) and wraps `EClientSocket` (outbound commands)
- **Lock-Free Queue**: `moodycamel::ConcurrentQueue` for < 1μs enqueue (Thread 1 → Thread 2)
- **State Aggregation**: Publishes complete snapshots (no partial updates)
- **RapidJSON**: 10-50μs serialization (SAX Writer API)
- **Performance Target**: < 50ms end-to-end latency (TWS → Redis)

**[NOTE]** Thread numbering matches implementation. See `include/TwsClient.h` lines 1-8 for architecture clarification.

## 🛠️ Development

### Build Targets

```bash
make help           # Show all available targets
make deps           # Install dependencies
make build          # Build project
make test           # Run tests
make clean          # Remove build artifacts
make run            # Build and run
```

### Docker Targets

```bash
make docker-up      # Start Redis
make docker-down    # Stop Redis
make docker-logs    # View logs
make redis-cli      # Connect to Redis CLI
make redis-monitor  # Monitor Redis commands
make validate-env   # Validate dev environment (Gate 1b)
```

**Validation Gate 1b:** Use `make validate-env` to verify:
- ✅ Redis container running (`docker ps | grep redis`)
- ✅ Redis connectivity (`redis-cli PING` returns `PONG`)
- ✅ Port 7497/4002 available for TWS Gateway
- ✅ Bridge binary compiled (`./build/tws_bridge` exists)

### Testing

```bash
# Unit tests
make test

# Live integration test (requires TWS Gateway running)
./build/tws_bridge --host 127.0.0.1 --port 7497
```

## 📊 JSON Schema

Published to Redis channel: `TWS:TICKS:{SYMBOL}`

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

## 🔧 Configuration

### TWS Connection

Edit `src/main.cpp`:

```cpp
const char* TWS_HOST = "127.0.0.1";
const int TWS_PORT = 7497;  // Paper: 4002, Live: 7497
const int CLIENT_ID = 0;
```

**Environment Variables (optional):**

Set these to override default configuration:
```bash
export TWS_HOST=127.0.0.1      # TWS Gateway hostname
export TWS_PORT=7497           # TWS Gateway port (4002 paper, 7497 live)
export TWS_CLIENT_ID=0         # Client ID for connection
export REDIS_URL=redis://127.0.0.1:6379  # Redis connection string
```

**Automatic Behavior Adaptation:**

The bridge automatically detects market conditions:
- **Markets Open (9:30 AM - 4:00 PM ET):** Uses tick-by-tick data (`reqTickByTickData`)
- **Markets Closed:** Falls back to historical bars (`reqHistoricalData`) or real-time bars (`reqRealTimeBars`)
- **Offline Testing:** Use `--replay` mode with CSV tick files

See `docs/PROJECT-SPECIFICATION.md` § 2.1.2 for complete API contract details and fallback strategies.

### Instrument Subscription

Edit `src/TwsClient.cpp` in `nextValidId()` callback:

```cpp
Contract contract;
contract.symbol = "AAPL";
contract.secType = "STK";
contract.currency = "USD";
contract.exchange = "SMART";
contract.primaryExchange = "NASDAQ";

requestTickByTickData(1, contract);
```

## 🐛 Troubleshooting

### "Failed to connect to TWS"

- Verify TWS Gateway is running
- Check API is enabled: Settings → API → Enable ActiveX
- Verify correct port (4002 for paper, 7497 for live)
- Check firewall allows localhost connections

### "No market data permissions"

- Ensure account has market data subscription
- Use paper trading account for testing
- Check contract definition (primaryExchange required)

### Build errors with TWS API

- Verify TWS API v1037.02 is installed in `vendor/tws-api/`
- Check directory structure matches `vendor/README.md`
- Run `./vendor/install_tws_api.sh` to auto-install
- Ensure you have IB Gateway 10.30+ for compatibility

## 📝 License

[Add your license here]

## 🤝 Contributing

[Add contribution guidelines]

## 📧 Contact

[Add contact information]

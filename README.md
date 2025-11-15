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

- **3-Thread Model**: EReader (TWS) → Main (callbacks) → Redis Worker
- **Lock-Free Queue**: `moodycamel::ConcurrentQueue` for < 1μs enqueue
- **State Aggregation**: Publishes complete snapshots (no partial updates)
- **RapidJSON**: 10-50μs serialization (SAX Writer API)
- **Performance Target**: < 50ms end-to-end latency (TWS → Redis)

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
```

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

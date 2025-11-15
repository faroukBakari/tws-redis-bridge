# TWS API Integration

## Version Information

**TWS API Version:** 1037.02 (Stable)  
**Compatible with:** IB Gateway 10.30+  
**Download URL:** https://interactivebrokers.github.io/downloads/twsapi_macunix.1037.02.zip

## Installation Required

The Interactive Brokers TWS C++ API is not available via Conan and must be manually installed.

### Download

1. Visit: https://www.interactivebrokers.com/en/trading/tws-api.php
2. Download the TWS API v1037.02 (Stable)
3. Extract the archive

### Installation

Copy the TWS API source files to this vendor directory:

```bash
# Expected structure after installation:
vendor/tws-api/
└── source/
    └── cppclient/
        └── client/
            ├── EClient.h
            ├── EClient.cpp
            ├── EWrapper.h
            ├── EReader.h
            ├── EReader.cpp
            ├── EReaderOSSignal.h
            ├── EReaderOSSignal.cpp
            ├── EClientSocket.h
            ├── EClientSocket.cpp
            ├── Contract.h
            └── ... (all other API files)
```

### Quick Install Script

```bash
# From project root
cd vendor

# Download and extract TWS API v1037.02
curl -L -o twsapi_macunix.1037.02.zip https://interactivebrokers.github.io/downloads/twsapi_macunix.1037.02.zip
unzip twsapi_macunix.1037.02.zip
mv IBJts tws-api
rm twsapi_macunix.1037.02.zip

# Verify structure
ls tws-api/source/cppclient/client/
```

### Verification

The CMakeLists.txt expects the API files at:
- `${CMAKE_SOURCE_DIR}/vendor/tws-api/source/cppclient/client/*.cpp`
- `${CMAKE_SOURCE_DIR}/vendor/tws-api/source/cppclient/client/*.h`

## Alternative: System-Wide Installation

If you prefer a system-wide installation, modify `CMakeLists.txt` to point to your installation path:

```cmake
set(TWS_API_DIR "/usr/local/include/tws-api")
```

## Documentation

Official TWS C++ API documentation:
- https://interactivebrokers.github.io/tws-api/
- https://interactivebrokers.github.io/tws-api/cpp_client.html

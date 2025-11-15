#!/bin/bash
# Download and install TWS API v1037.02 (compatible with IB Gateway 10.30)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENDOR_DIR="${SCRIPT_DIR}"
TWS_API_VERSION="1037.02"
TWS_API_URL="https://interactivebrokers.github.io/downloads/twsapi_macunix.${TWS_API_VERSION}.zip"

echo "=== TWS API Installation Script ==="
echo "Version: ${TWS_API_VERSION}"
echo "Compatible with: IB Gateway 10.30+"
echo ""

# Check if already installed
if [ -d "${VENDOR_DIR}/tws-api/source/cppclient/client" ]; then
    echo "✅ TWS API already installed at: ${VENDOR_DIR}/tws-api"
    echo ""
    ls -la "${VENDOR_DIR}/tws-api/source/cppclient/client" | head -20
    exit 0
fi

echo "Downloading TWS API v${TWS_API_VERSION}..."
echo "URL: ${TWS_API_URL}"
echo ""

# Download
if command -v curl &> /dev/null; then
    curl -L -o "${VENDOR_DIR}/twsapi.zip" "${TWS_API_URL}"
elif command -v wget &> /dev/null; then
    wget -O "${VENDOR_DIR}/twsapi.zip" "${TWS_API_URL}"
else
    echo "Error: Neither curl nor wget is available"
    echo "Please install curl or wget and try again"
    exit 1
fi

# Extract
echo "Extracting..."
unzip -q "${VENDOR_DIR}/twsapi.zip" -d "${VENDOR_DIR}"
mv "${VENDOR_DIR}/IBJts" "${VENDOR_DIR}/tws-api"
rm "${VENDOR_DIR}/twsapi.zip"

echo ""
echo "✅ TWS API v${TWS_API_VERSION} installed successfully!"
echo ""
echo "Expected structure:"
echo "  vendor/tws-api/"
echo "  └── source/"
echo "      └── cppclient/"
echo "          └── client/"
echo "              ├── EClient.h"
echo "              ├── EClient.cpp"
echo "              ├── EWrapper.h"
echo "              └── ... (all other API files)"
echo ""
echo "5. Run 'make build' to verify installation"
echo ""

echo ""
echo "Installation path:"
echo "  ${VENDOR_DIR}/tws-api/source/cppclient/client"
echo ""
echo "Files installed:"
echo "  Headers: $(ls ${VENDOR_DIR}/tws-api/source/cppclient/client/*.h | wc -l)"
echo "  Sources: $(ls ${VENDOR_DIR}/tws-api/source/cppclient/client/*.cpp | wc -l)"
echo ""
echo "Next steps:"
echo "  1. Run 'make build' to compile the project"
echo "  2. Ensure IB Gateway 10.30+ is running on port 7497"
echo ""
echo "For more information, see: vendor/README.md"

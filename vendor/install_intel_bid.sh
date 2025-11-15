#!/bin/bash
# Install Intel Decimal Floating-Point Math Library (libbid)
# Required by TWS API for Decimal type support
# 
# Based on: vendor/tws-api/source/cppclient/Intel_lib_build.txt
#
# The library is available from netlib.org (unsecured HTTP mirror)
# Official Intel page: https://www.intel.com/content/www/us/en/developer/articles/tool/intel-decimal-floating-point-math-library.html

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENDOR_DIR="$SCRIPT_DIR"
INTEL_DFPM_VERSION="IntelRDFPMathLib20U2"
# IMPORTANT: This is HTTP (not HTTPS) - netlib.org mirror
INTEL_DFPM_URL="http://www.netlib.org/misc/intel/IntelRDFPMathLib20U2.tar.gz"

echo "[INTEL-BID] Installing Intel Decimal Floating-Point Math Library..."

# Download if not already present
if [ ! -f "${VENDOR_DIR}/${INTEL_DFPM_VERSION}.tar.gz" ]; then
    echo "[INTEL-BID] Downloading from netlib.org (HTTP mirror)..."
    # Use --no-check-certificate for HTTP (insecure but necessary)
    wget --no-check-certificate -O "${VENDOR_DIR}/${INTEL_DFPM_VERSION}.tar.gz" "$INTEL_DFPM_URL"
else
    echo "[INTEL-BID] Archive already downloaded"
fi

# Verify the file has content (not 0 bytes from failed download)
FILE_SIZE=$(stat -c%s "${VENDOR_DIR}/${INTEL_DFPM_VERSION}.tar.gz" 2>/dev/null || echo "0")
if [ "$FILE_SIZE" -lt 1000 ]; then
    echo "[INTEL-BID] ERROR: ${INTEL_DFPM_VERSION}.tar.gz appears corrupt (too small)"
    echo "Download may have failed. Removing corrupt file..."
    rm -f "${VENDOR_DIR}/${INTEL_DFPM_VERSION}.tar.gz"
    exit 1
fi

echo "[INTEL-BID] Found archive (${FILE_SIZE} bytes)"

# Extract if not already extracted
if [ ! -d "${VENDOR_DIR}/${INTEL_DFPM_VERSION}" ]; then
    echo "[INTEL-BID] Extracting ${INTEL_DFPM_VERSION}..."
    tar -xzf "${VENDOR_DIR}/${INTEL_DFPM_VERSION}.tar.gz" -C "$VENDOR_DIR"
else
    echo "[INTEL-BID] Already extracted"
fi

# Build libbid.a (static library)
# Following official instructions from vendor/tws-api/source/cppclient/Intel_lib_build.txt
echo "[INTEL-BID] Building libbid.a (static library)..."
cd "${VENDOR_DIR}/${INTEL_DFPM_VERSION}/LIBRARY"

# Clean previous build
make clean 2>/dev/null || true

# Build with flags from official TWS API documentation
# See: vendor/tws-api/source/cppclient/Intel_lib_build.txt
make CC=gcc CALL_BY_REF=0 GLOBAL_RND=0 GLOBAL_FLAGS=0 UNCHANGED_BINARY_FLAGS=0

# Clean intermediate object files
rm -f *.o

# Verify library was built
if [ ! -f "libbid.a" ]; then
    echo "[INTEL-BID] ERROR: libbid.a was not built!"
    exit 1
fi

echo "[INTEL-BID] ✓ libbid.a built successfully"
echo "[INTEL-BID] Location: ${VENDOR_DIR}/${INTEL_DFPM_VERSION}/LIBRARY/libbid.a"
echo "[INTEL-BID] Installation complete"

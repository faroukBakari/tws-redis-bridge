# Intel Decimal Floating-Point Math Library Setup

## Background

The TWS C++ API uses the Intel Decimal Floating-Point Math Library for its `Decimal` type implementation. This library (`libbid.a`) is required to link the application.

## Official Documentation

See: `vendor/tws-api/source/cppclient/Intel_lib_build.txt` for the official TWS API instructions.

## Quick Setup (Automated)

The installation is now fully automated:

```bash
cd /home/farouk/workspace/tws-redis-bridge/vendor
./install_intel_bid.sh
```

The script will:
1. Download `IntelRDFPMathLib20U2.tar.gz` from netlib.org HTTP mirror
2. Extract the archive
3. Build `libbid.a` with official TWS API flags
4. Report success and library location

## Download Source

**Primary:** http://www.netlib.org/misc/intel/IntelRDFPMathLib20U2.tar.gz
- ⚠️ Note: HTTP (not HTTPS) - requires `wget --no-check-certificate`
- This is a mirror of Intel's official library hosted by netlib.org

**Alternative (Manual):**
1. Visit Intel's official page:
   ```
   https://www.intel.com/content/www/us/en/developer/articles/tool/intel-decimal-floating-point-math-library.html
   ```
2. Download: `IntelRDFPMathLib20U2.tar.gz`
## Build Details

The script builds with the exact flags specified in TWS API documentation:

```bash
make CC=gcc CALL_BY_REF=0 GLOBAL_RND=0 GLOBAL_FLAGS=0 UNCHANGED_BINARY_FLAGS=0
```

## Linking to Your Application

The CMakeLists.txt is already configured to find and link `libbid.a`:

```cmake
find_library(INTEL_BID_LIB
    NAMES libbid.a
    PATHS ${CMAKE_SOURCE_DIR}/vendor/IntelRDFPMathLib20U2/LIBRARY
    REQUIRED
)

target_link_libraries(tws_api INTERFACE ${INTEL_BID_LIB})
```

After running the installation script, rebuild:

```bash
make clean
make build
```

## Verification

Check that the library was built:

```bash
ls -lh vendor/IntelRDFPMathLib20U2/LIBRARY/libbid.a
```

You should see a file approximately 1-2 MB in size.

## Troubleshooting

### "Archive not found" Error

The script will automatically download from netlib.org. If this fails:
1. Check your internet connection
2. Verify HTTP (not HTTPS) is not blocked by firewall
3. Manually download and place in `vendor/` directory

### "File too small" Error

If the download failed (0 bytes or corrupt), the script will:
- Automatically remove the corrupt file
- Exit with error message
- Re-run the script to retry download

### Build Errors

If the `make` command fails:
1. Ensure you have `gcc` installed: `sudo apt install build-essential`
2. Check the `IntelRDFPMathLib20U2/README` file for additional dependencies
3. Review the build output for specific error messages

### Linking Errors

If CMake cannot find `libbid.a`:
1. Verify the library exists: `find vendor/ -name libbid.a`
2. Check CMake's search path in `CMakeLists.txt`
3. Run `cmake --trace` to debug the find_library() call

## Alternative: Quick Fix for Testing

If you cannot download the Intel library and just want to test the build, you can temporarily disable the Decimal type usage:

1. Comment out Decimal-related code in `vendor/tws-api/source/cppclient/client/Decimal.cpp`
2. Replace with `std::numeric_limits` workarounds
3. **⚠️ WARNING:** This breaks TWS API's Decimal functionality and is only for testing

## See Also

- `vendor/install_intel_bid.sh` - Automated build script
- `vendor/tws-api/source/cppclient/Intel_lib_build.txt` - Official TWS instructions
- `docs/TROUBLESHOOTING.md` - General build troubleshooting

## Key Findings (Session Log)

**2025-11-15:** Intel BID Library Resolution
- ✅ Found official build instructions in TWS API source tree
- ✅ Discovered netlib.org HTTP mirror: `http://www.netlib.org/misc/intel/IntelRDFPMathLib20U2.tar.gz`
- ✅ Required `wget --no-check-certificate` for HTTP (not HTTPS) download
- ✅ Script now fully automated - no manual download needed
- ✅ Build flags confirmed from official TWS documentation


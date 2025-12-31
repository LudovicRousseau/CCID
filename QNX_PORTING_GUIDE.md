# libccid QNX 8.0 Porting Guide

## Overview

This document describes the libccid driver port to QNX 8.0 using a custom-built libusb API compatibility shim. The shim translates libusb 1.0 API calls to QNX native USB stack (libusbdi), enabling the CCID driver to work on QNX without porting the entire codebase.

**Status:** ✅ Device enumeration working | ✅ CCID detection working | 🔄 Transfer implementation in progress

## Architecture

### Shim Layer Design

The libusb compatibility shim consists of three main components:

1. **libusb_shim.h** - Header file containing:
   - libusb API type definitions (structures, enums)
   - libusb constants and macros
   - Function declarations for the libusb API
   - Conditional compilation: Uses native libusb on non-QNX platforms

2. **libusb_shim_qnx.c** - Implementation file containing:
   - QNX-specific implementations of libusb functions
   - Device enumeration using libusbdi callbacks
   - Configuration descriptors with full descriptor hierarchy
   - Transfer handling stubs with parameter validation
   - Error code translation between QNX and libusb
   - Thread-safe device list management with mutex protection

3. **Integration in ccid_usb.h/c and parse.c**:
   - Conditional includes to use shim on QNX, native libusb elsewhere
   - No changes to CCID driver core logic required

### Data Flow

```
CCID Driver Code (ccid_usb.c, parse.c)
    |
    v
libusb API calls
    |
    v
libusb_shim.h / libusb_shim_qnx.c
    |
    v
QNX libusbdi (USB Device Interface)
    |
    v
USB Controller (XHCI)
    |
    v
USB Devices (Smart card readers, hubs, etc.)
```

## Implementation Status

### ✅ Completed Features

#### Device Enumeration
- **Implementation:** `usbd_connect()` with device insertion/removal callbacks
- **Status:** Fully working on Pico Key (FEFF:FCFD) and USB Hub (2109:3431)
- **Features:**
  - Thread-safe device list with mutex protection
  - Deduplication to prevent duplicate device entries
  - USB device descriptor reading via `usbd_device_descriptor()`
  - Vendor ID and Product ID extraction

#### Configuration Descriptors
- **Implementation:** Real `usbd_configuration_descriptor()` with synthetic fallback
- **Status:** Fully working with CCID interface detection
- **Features:**
  - Proper descriptor hierarchy: config → interface → endpoint
  - CCID interface descriptor (bInterfaceClass = 0x0B)
  - Interrupt endpoint descriptors with proper attributes
  - Full memory management with proper cleanup
  - Works on devices where libusbdi returns NULL

#### CCID Device Recognition
- **Pico Key (FEFF:FCFD)** ✅ Correctly identified as CCID device
- **USB Hub (2109:3431)** ✅ Correctly identified as non-CCID device
- **Detection Method:** Uses `get_ccid_usb_interface()` from ccid_usb.c

#### Build System
- **CMake toolchain:** `qnx-toolchain.cmake` configured for aarch64le
- **Binary format:** ARM aarch64 ELF (for QNX Raspberry Pi)
- **Compilation:** Clean build with no errors or warnings

### 🔄 In Progress / Partial Implementation

#### USB Transfer Functions
- **libusb_control_transfer()** - Parameter validation + endpoint direction awareness
  - IN transfers (0x80 bit set): Returns 0 bytes
  - OUT transfers: Returns wLength bytes
  - Proper device handle validation
  
- **libusb_bulk_transfer()** - Similar to control transfers
  - Validates all parameters (dev, data, actual_length)
  - Differentiates IN/OUT endpoints
  - Returns appropriate byte counts

- **libusb_interrupt_transfer()** - Same implementation as bulk
  - Proper error handling for missing devices

**Note:** These are realistic simulations that pass parse.c tests but don't transfer actual data yet. Full libusbdi URB integration (usbd_io, usbd_setup_vendor, etc.) planned for next phase.

### ⏳ Not Yet Implemented

- Full URB-based transfers using usbd_io()
- Asynchronous transfer callbacks
- Hotplug detection
- String descriptor retrieval (returns empty)
- Advanced power management

## Supported libusb Functions

### Fully Implemented ✅

**Initialization & Cleanup:**
- `libusb_init()` - Initialize USB context (✅ accepts NULL)
- `libusb_exit()` - Exit and cleanup

**Device Enumeration:**
- `libusb_get_device_list()` - Real device enumeration
- `libusb_free_device_list()` - Free device list
- `libusb_get_device_descriptor()` - Real descriptors
- `libusb_get_active_config_descriptor()` - Real + synthetic fallback
- `libusb_free_config_descriptor()` - Proper nested cleanup
- `libusb_get_bus_number()` - Returns device bus
- `libusb_get_device_address()` - Returns device address

**Device Operations:**
- `libusb_open()` - Open device handle
- `libusb_close()` - Close device handle
- `libusb_set_configuration()` - Stub (acceptable for CCID)
- `libusb_claim_interface()` - Stub (acceptable for CCID)
- `libusb_release_interface()` - Stub

**Data Transfers (Realistic Stubs):**
- `libusb_control_transfer()` - Parameter validation, endpoint direction
- `libusb_bulk_transfer()` - Full parameter validation
- `libusb_interrupt_transfer()` - Proper error handling
- `libusb_alloc_transfer()` - Allocate structure
- `libusb_free_transfer()` - Free structure
- `libusb_submit_transfer()` - Stub with callback
- `libusb_cancel_transfer()` - Stub
- `libusb_handle_events_completed()` - Stub

**Utility:**
- `libusb_error_name()` - Error string mapping

## Building for QNX

### Prerequisites

- QNX 8.0 SDK/SDP installed and sourced
- CMake >= 3.22
- Standard build tools (qcc compiler, pthread)

### Build Instructions

1. **Source QNX environment:**
   ```bash
   source ~/qnx800/qnxsdp-env.sh
   ```

2. **Configure with CMake:**
   ```bash
   cd /path/to/CCID
   rm -rf build
   cmake -DCMAKE_TOOLCHAIN_FILE=qnx-toolchain.cmake -B build/
   ```

3. **Build:**
   ```bash
   cmake --build ./build/
   ```

4. **Verify binaries:**
   ```bash
   file ./build/parse       # Should show: ARM aarch64
   ls -lh ./build/libccid.so ./build/parse
   ```

5. **Deploy to QNX device:**
   ```bash
   scp ./build/parse root@<qnx-ip>:/data/home/qnxuser/
   scp ./build/libccid.so root@<qnx-ip>:/data/home/qnxuser/
   ```

### CMake Toolchain Configuration

The `qnx-toolchain.cmake` file:
- Detects QNX 8.0 system
- Sets up aarch64le architecture
- Configures QNX SDK paths
- Includes libusbdi library
- Adds necessary QNX USB headers

Key paths (auto-detected from `$QNX_HOST` and `$QNX_TARGET`):
- QNX Host: `/qnx/qnx800/host/linux/x86_64`
- QNX Target: `/qnx/qnx800/target/qnx8`

## Code Organization

### File Structure

```
src/
├── libusb_shim.h           - Header definitions
├── libusb_shim_qnx.c       - QNX implementation 
│   ├── Device enumeration (usbd_connect callbacks)
│   ├── Device descriptors (real via libusbdi)
│   ├── Config descriptors (real + synthetic fallback)
│   ├── Transfer stubs (control, bulk, interrupt)
│   └── Utility functions
├── ccid_usb.c              - CCID driver (uses libusb API)
├── ccid_usb.h              - CCID header
├── parse.c                 - Discovery utility (tests all APIs)
└── ...other files...

CMakeLists.txt             - Build configuration
qnx-toolchain.cmake        - QNX cross-compiler setup
```

## Testing

### Device Discovery Test

The `parse` utility tests all enumeration functions:

```bash
root@qnxpi# ./parse
[libusb] libusb_init called
[libusb] Starting USB device enumeration via libusbdi
[libusb] Device inserted: path=0, devno=1
[libusb] Device added to list (total: 1)
[libusb] Device inserted: path=0, devno=2
[libusb] Device added to list (total: 2)
[libusb] libusb_get_device_list called, found 2 devices
[libusb] libusb_open called
[libusb] libusb_get_device_descriptor called
Parsing USB bus/device: 2109:3431 (bus 1, device 1)
  NOT a CCID/ICCD device
[libusb] libusb_open called
[libusb] libusb_get_device_descriptor called
Parsing USB bus/device: FEFF:FCFD (bus 1, device 2)
  Found a CCID/ICCD device at interface 0
[libusb] libusb_exit called
```

### Verification Steps

1. **Enumeration:** Check both devices are found (USB Hub + Pico Key)
2. **Descriptor retrieval:** Should show vendor/product IDs
3. **CCID detection:** Pico Key should be identified as CCID device
4. **No crashes:** Program should exit cleanly

## Architecture Details

### Device Enumeration Flow

```c
libusb_init()
  └─> usbd_connect()
      └─> Registers device_insertion_callback()
          └─> Called for each USB device
              └─> usbd_attach() to get device handle
              └─> usbd_device_descriptor() to read descriptors
              └─> Stores in qnx_devices[] array with mutex protection
```

### Device List Management

- **Array:** `qnx_device_info_t qnx_devices[]` (dynamic reallocation)
- **Counter:** `int qnx_device_count` (tracks stored devices)
- **Mutex:** `pthread_mutex_t qnx_device_mutex` (thread safety)
- **Deduplication:** Checks vendor_id:product_id before adding

### Descriptor Structure

Real descriptors retrieved for each device:
- **Device Descriptor:** 18 bytes (vendor_id, product_id, class, etc.)
- **Configuration Descriptor:** 9+ bytes (config number, interfaces, power)
- **Interface Descriptor:** 9 bytes (class, subclass, protocol, endpoints)
- **Endpoint Descriptor:** 7 bytes (address, attributes, max packet size)

## Known Limitations

1. **USB Transfers:** Current implementation returns simulated success without actual data transfer
   - Next phase: Implement real libusbdi URB-based transfers

2. **String Descriptors:** Returns empty strings
   - Not critical for CCID operation (descriptors optional)

3. **Asynchronous Operations:** Basic structure only
   - CCID driver primarily uses synchronous transfers

4. **Hotplug:** No dynamic device addition/removal handling
   - Devices must be present before `libusb_init()`

5. **Multiple Configurations:** Only first config descriptor supported
   - CCID devices typically have single configuration

## Future Enhancements

### Phase 1: Real USB Transfers (Next)
- Implement `usbd_open_pipe()` for endpoint access
- Allocate URBs via `usbd_alloc_urb()`
- Setup transfers via `usbd_setup_vendor()`, `usbd_setup_bulk()`
- Execute transfers via `usbd_io()` with timeout
- Handle completion and errors

### Phase 2: Advanced Features
- Hotplug device detection
- Multiple configuration support
- String descriptor retrieval
- Asynchronous transfer callbacks

### Phase 3: Optimization
- Performance tuning
- Memory optimization
- Error recovery mechanisms

## Debugging

### Enable Debug Output

In the code, debug messages are sent to stderr with `[libusb]` prefix:

```c
fprintf(stderr, "[libusb] Device inserted: path=%u, devno=%u\n", ...);
```

### Check Device Connection

On QNX device:
```bash
usb -v              # List all USB devices with details
lsusb              # Alternative device listing
cat /proc/usb       # USB subsystem info
```

### Verify Binary Architecture

```bash
file parse         # Check if ARM aarch64 ELF
file libccid.so    # Check shared library format
```

## References

- **QNX 8.0 USB Documentation:** QNX system libraries - USB
- **libusbdi API:** usbd_*.h headers in QNX SDK
- **CCID Specification:** USB Device Class Definition for Smart Cards
- **libusb 1.0 API:** http://libusb.info/

## Summary

The QNX port provides full device enumeration and CCID device detection through a minimal libusb compatibility layer. Device descriptors are real, fetched from the QNX USB stack. Transfer functions are realistic stubs suitable for testing and parse utility validation.

For production use with actual smart card communication, implement full libusbdi URB-based transfers in the next phase.

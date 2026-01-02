# libccid QNX 8.0 Porting Guide

## Overview

This document describes the libccid driver port to QNX 8.0 using a custom-built libusb API compatibility shim. The shim translates libusb 1.0 API calls to QNX native USB stack (libusbdi), enabling the CCID driver to work on QNX without porting the entire codebase.

**Status:** ✅ Device enumeration working | ✅ CCID detection working | ✅ Synchronous transfers working | 🔄 Control transfers simulated

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
   - **Full synchronous transfer implementation** using `usbd_io` and condition variables
   - Dynamic pipe management and persistent descriptor storage
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
- **Status:** Fully working on RP2350 a.k.a Pico-2 (simulated as a CCID device)
- **Features:**
  - Thread-safe device list with mutex protection
  - Deduplication to prevent duplicate device entries
  - USB device descriptor reading via `usbd_device_descriptor()`
  - Vendor ID and Product ID extraction

#### Configuration Descriptors
- **Implementation:** `usbd_configuration_descriptor()` with iteration support
- **Status:** Fully working with CCID interface detection
- **Features:**
  - Proper descriptor hierarchy: config → interface → endpoint
  - CCID interface descriptor (bInterfaceClass = 0x0B)
  - Interrupt endpoint descriptors with proper attributes
  - Full memory management with proper cleanup
  - Works on devices where libusbdi returns NULL

#### USB Transfer Functions (Synchronous)
- **libusb_bulk_transfer()** - Fully implemented
  - Uses `usbd_setup_bulk` and `usbd_io`
  - Synchronous execution via `pthread_cond_wait`
  - Handles DMA buffer allocation (`usbd_alloc`)
  - Supports both IN and OUT directions
  - Verified with `test_transfer` (10 bytes sent/received)

- **libusb_interrupt_transfer()** - Fully implemented
  - Uses `usbd_setup_interrupt` and `usbd_io`
  - Shares synchronous helper logic with bulk transfers
  - Correctly maps QNX status codes to libusb errors (e.g., Timeout)

#### CCID Device Recognition
- **Pico Key** ✅ Correctly identified as simulated CCID device
- **USB Hub** ✅ Correctly identified as non-CCID device
- **Detection Method:** Uses `get_ccid_usb_interface()` from ccid_usb.c

#### Build System
- **CMake toolchain:** `qnx-toolchain.cmake` configured for aarch64le
- **Binary format:** ARM aarch64 ELF (for QNX Raspberry Pi)
- **Compilation:** Clean build with no errors or warnings

### 🔄 In Progress / Partial Implementation

#### Control Transfers
- **libusb_control_transfer()** - Simulated
  - IN transfers (0x80 bit set): Returns 0 bytes
  - OUT transfers: Returns wLength bytes (Success)
  - Sufficient for basic CCID operation but vendor commands will fail
  - **Next Step:** Implement `usbd_setup_vendor` and `usbd_io` for full support

#### Async API
- **libusb_submit_transfer()** - Synchronous wrapper
  - Executes callback immediately
  - Sufficient for synchronous-only CCID driver usage

### ⏳ Not Yet Implemented

- `libusb_clear_halt()` (Currently no-op)
- `libusb_get_string_descriptor_ascii()` (Returns empty string)
- Hotplug detection (Callbacks exist but don't update live context)
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
- `libusb_get_active_config_descriptor()` - Full descriptor retrieval
- `libusb_free_config_descriptor()` - Proper nested cleanup
- `libusb_get_bus_number()` - Returns device bus
- `libusb_get_device_address()` - Returns device address

**Device Operations:**
- `libusb_open()` - Open device handle
- `libusb_close()` - Close device handle
- `libusb_set_configuration()` - Stub (acceptable for CCID)
- `libusb_claim_interface()` - Stub (acceptable for CCID)
- `libusb_release_interface()` - Stub

**Data Transfers (Synchronous):**
- `libusb_bulk_transfer()` - **Fully Implemented** (DMA-safe, synchronous)
- `libusb_interrupt_transfer()` - **Fully Implemented** (Synchronous)
- `libusb_control_transfer()` - Simulated (Returns success/0 bytes)
- `libusb_alloc_transfer()` - Allocate structure
- `libusb_free_transfer()` - Free structure
- `libusb_submit_transfer()` - Stub (calls callback immediately)
- `libusb_cancel_transfer()` - Stub
- `libusb_handle_events_completed()` - Stub

**Utility:**
- `libusb_error_name()` - Error string mapping
- `libusb_clear_halt()` - Stub (No-op)
- `libusb_get_string_descriptor_ascii()` - Stub (Empty string)

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
│   ├── Device descriptors (via libusbdi)
│   ├── Config descriptors (full hierarchy)
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
root@qnxpi:/data/home/qnxuser# ./parse
[libusb] libusb_init called
[libusb] Starting USB device enumeration via libusbdi
[libusb] Calling usbd_connect (path=NULL)...
[libusb] USB stack connected successfully
[libusb] Device inserted: path=0, devno=1
[libusb] usbd_attach succeeded, device: 1f578ba520
[libusb] Device descriptor: 2109:3431
[libusb] Device added to list (total: 1)
[libusb] Device inserted: path=0, devno=3
[libusb] usbd_attach succeeded, device: 1f578ba5d0
[libusb] Device descriptor: 1050:0405
[libusb] Device added to list (total: 2)
[libusb] Device inserted: path=0, devno=3
[libusb] usbd_attach succeeded, device: 1f578ba6b0
[libusb] Device descriptor: 1050:0405
[libusb] Device already in list (duplicate), skipping
[libusb] libusb_get_device_list called, found 2 devices
[libusb] libusb_open called
[libusb] libusb_get_device_descriptor called
Parsing USB bus/device: 2109:3431 (bus 1, device 1)
 idVendor:  0x2109  iManufacturer:
 idProduct: 0x3431  iProduct:
[libusb] Getting config descriptor for device 2109:3431
[libusb] DEBUG: qnx_dev->dev = 1f578ba520
[libusb] DEBUG: usbd_configuration_descriptor returned 0, node = 358045b0ac
[libusb] Config descriptor index 0 returned NULL, attempting iteration
[libusb] Found config descriptor at index 1
[libusb] Found config descriptor from QNX
[libusb] Found interface descriptor 0 from QNX
[libusb] Interface 0: num=0 alt=0 class: 0x09, endpoints: 1 (node=358045b0dc)
[libusb] Trying to get 1 endpoint descriptors
[libusb] Found endpoint 0 (addr 0x81): attr=0x03, maxpkt=1
[libusb] Got 1 of 1 expected endpoints
[libusb] libusb_close called
  NOT a CCID/ICCD device
[libusb] libusb_open called
[libusb] libusb_get_device_descriptor called
Parsing USB bus/device: 1050:0405 (bus 1, device 2)
 idVendor:  0x1050  iManufacturer:
 idProduct: 0x0405  iProduct:
[libusb] Getting config descriptor for device 1050:0405
[libusb] DEBUG: qnx_dev->dev = 1f578ba5d0
[libusb] DEBUG: usbd_configuration_descriptor returned 0, node = 3580460088
[libusb] Config descriptor index 0 returned NULL, attempting iteration
[libusb] Found config descriptor at index 1
[libusb] Found config descriptor from QNX
[libusb] Found interface descriptor 0 from QNX
[libusb] Interface 0: num=0 alt=0 class: 0x0b, endpoints: 3 (node=35804600b8)
[libusb] Trying to get 3 endpoint descriptors
[libusb] Found endpoint 0 (addr 0x01): attr=0x02, maxpkt=64
[libusb] Found endpoint 1 (addr 0x81): attr=0x02, maxpkt=64
[libusb] Found endpoint 2 (addr 0x82): attr=0x03, maxpkt=64
[libusb] Got 3 of 3 expected endpoints
[libusb] Found interface descriptor 1 from QNX
[libusb] Interface 1: num=1 alt=0 class: 0xff, endpoints: 2 (node=35804601e4)
[libusb] Trying to get 2 endpoint descriptors
[libusb] Found endpoint 0 (addr 0x03): attr=0x02, maxpkt=64
[libusb] Found endpoint 1 (addr 0x83): attr=0x02, maxpkt=64
[libusb] Got 2 of 2 expected endpoints
  Found a CCID/ICCD device at interface 0
[libusb] libusb_claim_interface: interface=0
[libusb] Selected configuration 1
[libusb] Found interface 0 (idx 0), endpoints: 3
[libusb] Skipping SetInterface for Alt Setting 0 (default)
[libusb] Scanning 3 endpoints for Interface 0
[libusb] Skipping Control Endpoint 0x00 at index 0
[libusb] Found Endpoint 0x01 (Attr 0x02) MaxPacket=64 at index 1
[libusb] Pipe opened for ep 0x01 (idx 1) pipe=1f578bac00
[libusb] Found Endpoint 0x81 (Attr 0x02) MaxPacket=64 at index 2
[libusb] Pipe opened for ep 0x81 (idx 17) pipe=1f578bac50
[libusb] Found Endpoint 0x82 (Attr 0x03) MaxPacket=64 at index 3
[libusb] Pipe opened for ep 0x82 (idx 18) pipe=1f578baca0
[libusb] Endpoint scan complete. Found 3 endpoints.
[libusb] libusb_release_interface: interface=0
[libusb] Getting config descriptor for device 1050:0405
[libusb] DEBUG: qnx_dev->dev = 1f578ba5d0
[libusb] DEBUG: usbd_configuration_descriptor returned 0, node = 3580460088
[libusb] Config descriptor index 0 returned NULL, attempting iteration
[libusb] Found config descriptor at index 1
[libusb] Found config descriptor from QNX
[libusb] Found interface descriptor 0 from QNX
[libusb] Interface 0: num=0 alt=0 class: 0x0b, endpoints: 3 (node=35804600b8)
[libusb] Trying to get 3 endpoint descriptors
[libusb] Found endpoint 0 (addr 0x01): attr=0x02, maxpkt=64
[libusb] Found endpoint 1 (addr 0x81): attr=0x02, maxpkt=64
[libusb] Found endpoint 2 (addr 0x82): attr=0x03, maxpkt=64
[libusb] Got 3 of 3 expected endpoints
[libusb] Found interface descriptor 1 from QNX
[libusb] Interface 1: num=1 alt=0 class: 0xff, endpoints: 2 (node=35804601e4)
[libusb] Trying to get 2 endpoint descriptors
[libusb] Found endpoint 0 (addr 0x03): attr=0x02, maxpkt=64
[libusb] Found endpoint 1 (addr 0x83): attr=0x02, maxpkt=64
[libusb] Got 2 of 2 expected endpoints
[libusb] libusb_close called
[libusb] libusb_exit called

 idVendor: 0x1050
  iManufacturer:
 idProduct: 0x0405
  iProduct:
 bcdDevice: 8.00 (firmware release?)
 bLength: 9
 bDescriptorType: 4
 bInterfaceNumber: 0
 bAlternateSetting: 0
 bNumEndpoints: 3
  bulk-IN, bulk-OUT and Interrupt-IN
 bInterfaceClass: 0x0B [Chip Card Interface Device Class (CCID)]
 bInterfaceSubClass: 0
 bInterfaceProtocol: 0
  bulk transfer, optional interrupt-IN (CCID)
 iInterface:
```

### Transfer Test
```bash
root@qnxpi:/data/home/qnxuser# ./test_transfer
Initializing libusb...
[libusb] libusb_init called
[libusb] Starting USB device enumeration via libusbdi
[libusb] Calling usbd_connect (path=NULL)...
[libusb] USB stack connected successfully
[libusb] Device inserted: path=0, devno=1
[libusb] usbd_attach succeeded, device: 264446e950
[libusb] Device descriptor: 2109:3431
[libusb] Device added to list (total: 1)
[libusb] Device inserted: path=0, devno=3
[libusb] usbd_attach succeeded, device: 264446ea00
[libusb] Device descriptor: 1050:0405
[libusb] Device added to list (total: 2)
[libusb] Device inserted: path=0, devno=3
[libusb] usbd_attach succeeded, device: 264446eae0
[libusb] Device descriptor: 1050:0405
[libusb] Device already in list (duplicate), skipping
Getting device list...
[libusb] libusb_get_device_list called, found 2 devices
Found 2 devices
[libusb] libusb_get_device_descriptor called
Device 0: ID 2109:3431 Class 09
Skipping Hub device
[libusb] libusb_get_device_descriptor called
Device 1: ID 1050:0405 Class 00
Opening device...
[libusb] libusb_open called
Device opened
[libusb] libusb_free_device_list called
Claiming interface 0...
[libusb] libusb_claim_interface: interface=0
[libusb] Config descriptor index 0 returned NULL, attempting iteration
[libusb] Found config descriptor at index 1
[libusb] Selected configuration 1
[libusb] Found interface 0 (idx 0), endpoints: 3
[libusb] Skipping SetInterface for Alt Setting 0 (default)
[libusb] Scanning 3 endpoints for Interface 0
[libusb] Skipping Control Endpoint 0x00 at index 0
[libusb] Found Endpoint 0x01 (Attr 0x02) MaxPacket=64 at index 1
[libusb] Pipe opened for ep 0x01 (idx 1) pipe=264446ed90
[libusb] Found Endpoint 0x81 (Attr 0x02) MaxPacket=64 at index 2
[libusb] Pipe opened for ep 0x81 (idx 17) pipe=264446ede0
[libusb] Found Endpoint 0x82 (Attr 0x03) MaxPacket=64 at index 3
[libusb] Pipe opened for ep 0x82 (idx 18) pipe=264446ee30
[libusb] Endpoint scan complete. Found 3 endpoints.
Interface claimed
Sending GetSlotStatus command to EP 0x01...
Sent 10 bytes
Reading response from EP 0x81...
Received 10 bytes:
81 00 00 00 00 00 00 01 00 00
Reading interrupt from EP 0x82...
Interrupt Read result: -7 (Timeout is normal if no event)
[libusb] libusb_release_interface: interface=0
[libusb] libusb_close called
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

1. **Control Transfers:** Current implementation returns simulated success without actual data transfer
   - Next phase: Implement real libusbdi URB-based control transfers

2. **String Descriptors:** Returns empty strings
   - Not critical for CCID operation (descriptors optional)

3. **Asynchronous Operations:** Basic structure only
   - CCID driver primarily uses synchronous transfers

4. **Hotplug:** No dynamic device addition/removal handling
   - Devices must be present before `libusb_init()`

5. **Multiple Configurations:** Only first config descriptor supported
   - CCID devices typically have single configuration

## Future Enhancements

### Phase 1: Control Transfers (Next)
- Implement `usbd_setup_vendor()` for control transfers
- Support vendor-specific commands (e.g., firmware version)
- Handle IN/OUT control data phases

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

The QNX port provides full device enumeration and CCID device detection through a minimal libusb compatibility layer. Device descriptors are real, fetched from the QNX USB stack. Bulk and Interrupt transfers are fully implemented and verified, enabling standard CCID communication. Control transfers are currently simulated and will be implemented in the next phase.

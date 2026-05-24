# Used to verify CCID device on Linux

import usb.core
import usb.util
import sys

# CCID Class
CCID_CLASS = 0x0B

def find_ccid_device():
    # Find all devices
    devs = usb.core.find(find_all=True)
    
    for dev in devs:
        # Check if device class is CCID (0x0B) or if any interface is CCID
        if dev.bDeviceClass == CCID_CLASS:
            return dev
        
        # Check interfaces
        for cfg in dev:
            for intf in cfg:
                if intf.bInterfaceClass == CCID_CLASS:
                    return dev
    return None

def main():
    print("Looking for CCID device...")
    dev = find_ccid_device()
    
    if dev is None:
        print("No CCID device found. Please connect the reader.")
        sys.exit(1)
        
    print(f"Found device: {dev.idVendor:04x}:{dev.idProduct:04x}")
    
    try:
        dev.set_configuration()
        print("Configuration set.")
    except usb.core.USBError as e:
        print(f"Warning: Could not set configuration: {e}")

    # Find the CCID interface and endpoints
    cfg = dev.get_active_configuration()
    intf = None
    ep_out = None
    ep_in = None
    ep_intr = None

    print("Scanning interfaces:")
    for i in cfg:
        print(f" - Interface {i.bInterfaceNumber}: Class 0x{i.bInterfaceClass:02x} SubClass 0x{i.bInterfaceSubClass:02x} Protocol 0x{i.bInterfaceProtocol:02x}")
        if i.bInterfaceClass == CCID_CLASS:
            if intf is None:
                intf = i

    if intf is None:
        # Fallback: just take the first interface if class 0
        print("No explicit CCID interface found, using first interface.")
        intf = cfg[(0,0)]

    print(f"Claiming interface {intf.bInterfaceNumber}...")
    
    # Detach kernel driver if active
    if dev.is_kernel_driver_active(intf.bInterfaceNumber):
        print(f"Detaching kernel driver from interface {intf.bInterfaceNumber}...")
        try:
            dev.detach_kernel_driver(intf.bInterfaceNumber)
            print("Kernel driver detached.")
        except usb.core.USBError as e:
            print(f"Could not detach kernel driver: {e}")
            sys.exit(1)
    
    # Find endpoints
    for ep in intf:
        if usb.util.endpoint_direction(ep.bEndpointAddress) == usb.util.ENDPOINT_OUT:
            ep_out = ep
        elif usb.util.endpoint_type(ep.bmAttributes) == usb.util.ENDPOINT_TYPE_BULK:
            ep_in = ep
        elif usb.util.endpoint_type(ep.bmAttributes) == usb.util.ENDPOINT_TYPE_INTR:
            ep_intr = ep

    if not ep_out or not ep_in:
        print("Could not find Bulk IN/OUT endpoints.")
        sys.exit(1)

    print(f"EP OUT: 0x{ep_out.bEndpointAddress:02x}")
    print(f"EP IN:  0x{ep_in.bEndpointAddress:02x}")
    if ep_intr:
        print(f"EP INTR: 0x{ep_intr.bEndpointAddress:02x}")

    # PC_to_RDR_GetSlotStatus
    # bMessageType = 0x65
    # dwLength = 0
    # bSlot = 0
    # bSeq = 0
    # bRFU = 0, 0, 0
    cmd = bytes([0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00])
    
    print(f"\nSending GetSlotStatus command ({len(cmd)} bytes): {cmd.hex()}")
    
    try:
        # Write
        dev.write(ep_out, cmd)
        print("Write success.")
        
        # Read
        resp = dev.read(ep_in, 256, timeout=5000)
        print(f"Read success ({len(resp)} bytes):")
        print(bytes(resp).hex())
        
        # Parse response
        if len(resp) > 0:
            msg_type = resp[0]
            if msg_type == 0x81: # RDR_to_PC_SlotStatus
                print("Response: RDR_to_PC_SlotStatus (Success)")
                status = resp[7]
                print(f"Slot Status: 0x{status:02x}")
                if status & 0x01: print(" - An ICC is present")
                else: print(" - No ICC present")
            else:
                print(f"Response Type: 0x{msg_type:02x}")

    except usb.core.USBError as e:
        print(f"Transfer failed: {e}")

if __name__ == "__main__":
    main()

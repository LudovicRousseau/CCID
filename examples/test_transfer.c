/*
    test_transfer.c: Simple test for libusb shim on QNX
    Sends PC_to_RDR_IccPowerOn and reads response
    Copyright (C) 2025-2026   Harinath Nampally

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc., 51
	Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libusb_shim.h"

#define CCID_CLASS 0x0B

int main(void) {
    int r;
    libusb_context *ctx = NULL;
    libusb_device **devs;
    libusb_device_handle *dev_handle = NULL;
    ssize_t cnt;
    int i;

    printf("Initializing libusb...\n");
    r = libusb_init(&ctx);
    if (r < 0) {
        fprintf(stderr, "Init Error %d\n", r);
        return 1;
    }

    printf("Getting device list...\n");
    cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        fprintf(stderr, "Get Device List Error %d\n", (int)cnt);
        return 1;
    }

    printf("Found %d devices\n", (int)cnt);

    for (i = 0; i < cnt; i++) {
        struct libusb_device_descriptor desc;
        r = libusb_get_device_descriptor(devs[i], &desc);
        if (r < 0) {
            fprintf(stderr, "Failed to get device descriptor\n");
            continue;
        }

        printf("Device %d: ID %04x:%04x Class %02x\n", i, desc.idVendor, desc.idProduct, desc.bDeviceClass);

        /* Skip Hubs (Class 0x09) */
        if (desc.bDeviceClass == 0x09) {
            printf("Skipping Hub device\n");
            continue;
        }

        /* If class is 0, check interface class (not implemented here for simplicity, assuming device class is CCID or we just try opening) */
        /* Actually, many CCID devices have class 0 at device level and 0x0B at interface level. 
           But for this test, let's just try to open the first device that looks like the one we saw in logs (Access IS?) 
           or just try to open any device and claim interface 0. */
        
        printf("Opening device...\n");
        r = libusb_open(devs[i], &dev_handle);
        if (r == 0) {
            printf("Device opened\n");
            break;
        } else {
            printf("Failed to open: %d\n", r);
        }
    }

    libusb_free_device_list(devs, 1);

    if (!dev_handle) {
        fprintf(stderr, "No device opened\n");
        libusb_exit(ctx);
        return 1;
    }

    printf("Claiming interface 0...\n");
    r = libusb_claim_interface(dev_handle, 0);
    if (r < 0) {
        fprintf(stderr, "Claim Interface Error %d\n", r);
        libusb_close(dev_handle);
        libusb_exit(ctx);
        return 1;
    }
    printf("Interface claimed\n");

    /* Send PC_to_RDR_GetSlotStatus */
    /* 65 00 00 00 00 00 00 00 00 00 */
    unsigned char cmd[10] = {0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    int actual_length;
    
    /* Endpoint 0x01 is Bulk OUT (from logs) */
    printf("Sending GetSlotStatus command to EP 0x01...\n");
    r = libusb_bulk_transfer(dev_handle, 0x01, cmd, sizeof(cmd), &actual_length, 5000);
    if (r < 0) {
        fprintf(stderr, "Bulk Write Error %d\n", r);
    } else {
        printf("Sent %d bytes\n", actual_length);
    }

    /* Read response from EP 0x81 (Bulk IN) */
    unsigned char resp[256];
    printf("Reading response from EP 0x81...\n");
    r = libusb_bulk_transfer(dev_handle, 0x81, resp, sizeof(resp), &actual_length, 5000);
    if (r < 0) {
        fprintf(stderr, "Bulk Read Error %d\n", r);
    } else {
        printf("Received %d bytes:\n", actual_length);
        for (int j = 0; j < actual_length; j++) {
            printf("%02X ", resp[j]);
        }
        printf("\n");
    }
    
    /* Also try Interrupt EP 0x82 */
    printf("Reading interrupt from EP 0x82...\n");
    r = libusb_interrupt_transfer(dev_handle, 0x82, resp, sizeof(resp), &actual_length, 1000);
    if (r < 0) {
        /* Timeout is expected if no interrupt pending */
        printf("Interrupt Read result: %d (Timeout is normal if no event)\n", r);
    } else {
        printf("Interrupt received %d bytes\n", actual_length);
    }

    libusb_release_interface(dev_handle, 0);
    libusb_close(dev_handle);
    libusb_exit(ctx);
    return 0;
}

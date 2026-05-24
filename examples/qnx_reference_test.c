/*
 * qnx_reference_test.c: Generic QNX USB example adapted from documentation
   (https://www.qnx.com/developers/docs/qnxeverywhere/com.qnx.doc.qnxeverywhere/topic/lpg/driver_USB_example.html)
   
 * Copyright (C) 2025-2026   Harinath Nampally
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/usbdi.h>

/* Structures */
typedef struct _buffer_data_bundle buffer_data;
struct _buffer_data_bundle{
    int length;
    void* ptr;
};

/* Global Variables */
struct usbd_connection* conn = NULL;

/* Function Declarations */
void on_usbd_insert(struct usbd_connection* conn, usbd_device_instance_t *inst);
void on_usbd_remove(struct usbd_connection* conn, usbd_device_instance_t *inst);
void urb_callback(struct usbd_urb* urb, struct usbd_pipe* pipe, void* user_data);
void signal_handler(int signo);

/**
* USBD Insert: Called for each device in the tree on connection, and on device insertion
* -> Attempt to attach to the matching device
* -> Set up an I/O Callback at its input endpoints
*/
void on_usbd_insert(struct usbd_connection* connection, usbd_device_instance_t *inst){
    printf("Device Inserted: path=%d devno=%d\n", inst->path, inst->devno);

    /* Attempt to attach to the device */
    struct usbd_device* device = NULL;
    int ret = usbd_attach(connection, inst, 0, &device);
    if(ret != EOK) {
        printf("Failed to attach: %d\n", ret);
        return;
    }

    /* Print Device Info */
    struct usbd_desc_node *node;
    usbd_device_descriptor_t *ddesc = usbd_device_descriptor(device, &node);
    if (ddesc) {
        printf("Device: Vendor=0x%04x, Product=0x%04x\n", ddesc->idVendor, ddesc->idProduct);
    }

    /* Select Configuration 1 */
    if (usbd_select_config(device, 1) != EOK) {
        printf("Failed to select configuration 1\n");
        /* Some devices might already be configured or have only 1 config? Continue anyway. */
    }
    
    /* Iterate through interfaces to find a valid one */
    int interface_found = 0;
    for (int iface = 0; iface < 5; iface++) {
        int sel_ret = usbd_select_interface(device, iface, 0);
        if (sel_ret == EOK) {
            printf("Successfully selected Interface %d\n", iface);
        } else {
            printf("Failed to select Interface %d (Error: %d), but scanning endpoints anyway...\n", iface, sel_ret);
        }
        
        /* Always scan endpoints, even if selection failed (it might be already selected or busy) */
        {
            interface_found = 1; /* Mark that we tried */
            
            /* Now try to find endpoints for this interface */
            usbd_endpoint_descriptor_t *desc;
            struct usbd_desc_node *iface_node;
            struct usbd_desc_node *ep_node;
            int endpoint_index = 0;
            
            usbd_endpoint_descriptor_t *ep_in = NULL;
            usbd_endpoint_descriptor_t *ep_out = NULL;

            printf("Scanning endpoints for Interface %d...\n", iface);

            /* Get the Interface Descriptor Node first to scope our search */
            /* Try Config 1 (Value) first, then Config 0 (Index) */
            if (usbd_interface_descriptor(device, 1, iface, 0, &iface_node) == NULL) {
                 if (usbd_interface_descriptor(device, 0, iface, 0, &iface_node) == NULL) {
                     printf("  Failed to get interface descriptor node for Iface %d (checked Config 1 and 0)\n", iface);
                     continue;
                 }
            }

            /* Parse endpoints starting from the interface node */
            while ((desc = (usbd_endpoint_descriptor_t*) usbd_parse_descriptors(device, iface_node, USB_DESC_ENDPOINT,
                                                    endpoint_index, &ep_node)) != NULL) {
                
                printf("  [%d] Endpoint 0x%02x Attr=0x%02x MaxPacket=%d\n", 
                       endpoint_index, desc->bEndpointAddress, desc->bmAttributes, desc->wMaxPacketSize);
                
                endpoint_index++;
                
                /* We are looking for Bulk endpoints */
                if ((desc->bmAttributes & 0x03) == 0x02) {
                    if ((desc->bEndpointAddress & 0x80) == 0x80) {
                        if (!ep_in) {
                             ep_in = desc;
                             printf("    -> Selected as Bulk IN\n");
                        }
                    } else {
                        if (!ep_out) {
                             ep_out = desc;
                             printf("    -> Selected as Bulk OUT\n");
                        }
                    }
                }
            }
            printf("Endpoint scan complete. Found IN=%p OUT=%p\n", ep_in, ep_out);

            if (ep_in && ep_out) {
                printf("Found Bulk Pair: IN=0x%02x OUT=0x%02x\n", ep_in->bEndpointAddress, ep_out->bEndpointAddress);
                
                struct usbd_pipe *pipe_in, *pipe_out;
                
                /* Allocate proper descriptors for open_pipe */
                usbd_descriptors_t *desc_in_struct = malloc(sizeof(usbd_descriptors_t));
                usbd_descriptors_t *desc_out_struct = malloc(sizeof(usbd_descriptors_t));
                
                memset(desc_in_struct, 0, sizeof(usbd_descriptors_t));
                memset(desc_out_struct, 0, sizeof(usbd_descriptors_t));
                
                memcpy(&desc_in_struct->endpoint, ep_in, ep_in->bLength);
                memcpy(&desc_out_struct->endpoint, ep_out, ep_out->bLength);

                printf("Opening IN pipe...\n");
                if (usbd_open_pipe(device, desc_in_struct, &pipe_in) != EOK) {
                    printf("Failed to open IN pipe\n");
                    free(desc_in_struct); free(desc_out_struct);
                    continue;
                }
                printf("Opening OUT pipe...\n");
                if (usbd_open_pipe(device, desc_out_struct, &pipe_out) != EOK) {
                    printf("Failed to open OUT pipe\n");
                    free(desc_in_struct); free(desc_out_struct);
                    continue;
                }

                /* 1. Setup IN URB (to receive response) */
                struct usbd_urb *urb_in = usbd_alloc_urb(NULL);
                void* buf_in = usbd_alloc(64); /* Standard max packet */
                buffer_data* user_data_in = calloc(1, sizeof(buffer_data));
                user_data_in->length = 64;
                user_data_in->ptr = buf_in;
                
                usbd_setup_bulk(urb_in, URB_DIR_IN, buf_in, 64);
                printf("Queueing IN transfer...\n");
                usbd_io(urb_in, pipe_in, urb_callback, user_data_in, USBD_TIME_INFINITY);
                

                /* 2. Setup OUT URB (to send command) */
                /* CCID PC_to_RDR_GetSlotStatus: 65 00 00 00 00 00 00 00 00 00 */
                uint8_t ccid_cmd[] = {0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
                
                struct usbd_urb *urb_out = usbd_alloc_urb(NULL);
                void* buf_out = usbd_alloc(sizeof(ccid_cmd));
                memcpy(buf_out, ccid_cmd, sizeof(ccid_cmd));
                
                printf("Sending CCID Command (Blocking)...\n");
                usbd_setup_bulk(urb_out, URB_DIR_OUT, buf_out, sizeof(ccid_cmd));
                /* Use a timeout instead of INFINITY to prevent permanent hang */
                int ret = usbd_io(urb_out, pipe_out, NULL, NULL, 1000); 
                
                if (ret == EOK) {
                    printf("Sent CCID GetSlotStatus Command (10 bytes)\n");
                } else {
                    printf("Failed to send command: %d\n", ret);
                }
                
                /* We found our pair, no need to keep searching interfaces for this device */
                return; 
            } else {
                printf("Did not find both Bulk IN and Bulk OUT endpoints on this interface.\n");
            }
        }
    }
    
    if (!interface_found) {
        printf("Could not select any interface (0-4)\n");
    }
}

/**
* USBD Remove: Called on device removal
* -> Checks if we have an attached device
* -> Removes attached device as needed
*/
void on_usbd_remove(struct usbd_connection* connection, usbd_device_instance_t *inst){
    printf("Device Removed\n");
    struct usbd_device * device;
    device = usbd_device_lookup(connection, inst);

    if(device != NULL){
        usbd_detach(device);
    }
}

/**
* URB Callback: Callback for handling URB receipt.
*/
void urb_callback(struct usbd_urb* urb, struct usbd_pipe* pipe, void* user_data){
    /* Get a data pointer and length */
    buffer_data * buf_data = (buffer_data *)(user_data);
    uint8_t * buffer = (uint8_t *)(buf_data->ptr);

    uint32_t status, transferred;
    usbd_urb_status(urb, &status, &transferred);

    printf("URB Callback: Status=0x%x Transferred=%d\n", status, transferred);

    /* Print byte breakdown of received data */
    #define WORD_LEN 4

    printf("Receive:");
    for(int byteno = 0; byteno < transferred; byteno++){
        printf("%02x", buffer[byteno]);
        if(byteno % WORD_LEN == (WORD_LEN - 1))
            printf(" ");
    }
    printf("\n");

    /* Prepare for re-queueing (continuous reporting) */
    usbd_setup_bulk(urb, URB_DIR_IN, ((buffer_data*)user_data)->ptr, buf_data->length);

    /* re-submit for I/O */
    usbd_io(urb, pipe, urb_callback, user_data, USBD_TIME_INFINITY);
}

/**
* Signal Handler: Quits program safely when terminated (frees memory)
*/
void signal_handler(int signo){
    printf("Signal caught, exiting...\n");
    if(conn != NULL) usbd_disconnect(conn); 
    _exit(0);
}

/**
* Main Function
*/
int main(int argc, char* argv[]){

    printf("Starting QNX Reference Test...\n");

    /* Initialize USBD connection */
    usbd_device_ident_t filter = {
        USBD_CONNECT_WILDCARD,
        USBD_CONNECT_WILDCARD,
        USBD_CONNECT_WILDCARD,
        USBD_CONNECT_WILDCARD,
        USBD_CONNECT_WILDCARD
    };

    usbd_funcs_t functions = {
      _USBDI_NFUNCS,
      on_usbd_insert,
      on_usbd_remove,
      NULL 
    };

    usbd_connect_parm_t connect_parms = {
        NULL,       /* Indicate that the default io-usb path is correct */
        USB_VERSION,
        USBD_VERSION,
        0,          /* No flags to be passed */
        argc,
        argv,
        0,          /* Default event buffer size */
        &filter,
        &functions
    };

    /* Attempt to start a connection via usbd_connect*/
    int error;
    error = usbd_connect(&connect_parms, &conn);

    if(error!=EOK) {
        printf("usbd_connect failed: %d\n", error);
        _exit(1);
    }

    printf("Connected. Waiting for devices (Ctrl+C to exit)...\n");

    /* Signals */
    signal (SIGHUP, SIG_IGN);
    signal (SIGTERM, signal_handler);
    signal (SIGINT, signal_handler);

    /* Loop until SIGTERM/SIGKILL */
    for( ; ; ) sleep(60);
}

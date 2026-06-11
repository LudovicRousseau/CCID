/*
    defs.h:
    Copyright (C) 2003-2023   Ludovic Rousseau

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License
	along with this library; if not, write to the Free Software Foundation,
	Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifndef __DEFS_H__
#define __DEFS_H__

#include <stdbool.h>
#include <pthread.h>

#define max( a, b )   ( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )

#include <pcsclite.h>

#define CCID_INTERRUPT_SIZE 8

// Uncomment if you want to synchronize the card movements on the 2
// interfaces of the Microchip SEC 1210 reader
// #define SEC1210_SYNC

#ifdef SEC1210_SYNC
struct _sec1210_cond {
	pthread_cond_t sec1210_cond;
	pthread_mutex_t sec1210_mutex;
};
#endif

typedef struct _ccid_descriptor
{
	/*
	 * CCID Sequence number
	 */
	unsigned char *pbSeq;
	unsigned char real_bSeq;

	/*
	 * VendorID << 16 + ProductID
	 */
	int readerID;

	/*
	 * Maximum message length
	 */
	unsigned int dwMaxCCIDMessageLength;

	/*
	 * Maximum IFSD
	 */
	int dwMaxIFSD;

	/*
	 * Features supported by the reader (directly from Class Descriptor)
	 */
	int dwFeatures;

	/*
	 * PIN support of the reader (directly from Class Descriptor)
	 */
	char bPINSupport;

	/*
	 * Display dimensions of the reader (directly from Class Descriptor)
	 */
	unsigned int wLcdLayout;

	/*
	 * Default Clock
	 */
	int dwDefaultClock;

	/*
	 * Max Data Rate
	 */
	unsigned int dwMaxDataRate;

	/*
	 * Number of available slots
	 */
	char bMaxSlotIndex;

	/*
	 * Maximum number of slots which can be simultaneously busy
	 */
	char bMaxCCIDBusySlots;

	/*
	 * Slot in use
	 */
	char bCurrentSlotIndex;

	/*
	 * The array of data rates supported by the reader
	 */
	unsigned int *arrayOfSupportedDataRates;

	/*
	 * Read communication port timeout
	 * value is milliseconds
	 * this value can evolve dynamically if card request it (time processing).
	 */
	unsigned int readTimeout;

	/*
	 * Card protocol
	 */
	int cardProtocol;

	/*
	 * Reader protocols
	 */
	int dwProtocols;

	/*
	 * bInterfaceProtocol (CCID, ICCD-A, ICCD-B)
	 */
	int bInterfaceProtocol;

	/*
	 * bNumEndpoints
	 */
	int bNumEndpoints;

	/*
	 * GemCore SIM PRO slot status management
	 * The reader always reports a card present even if no card is inserted.
	 * If the Power Up fails the driver will report IFD_ICC_NOT_PRESENT instead
	 * of IFD_ICC_PRESENT
	 */
	int dwSlotStatus;

	/*
	 * bVoltageSupport (bit field)
	 * 1 = 5.0V
	 * 2 = 3.0V
	 * 4 = 1.8V
	 */
	int bVoltageSupport;

	/*
	 * USB serial number of the device (if any)
	 */
	char *sIFD_serial_number;

	/*
	 * USB iManufacturer string
	 */
	char *sIFD_iManufacturer;

	/*
	 * USB bcdDevice
	 */
	int IFD_bcdDevice;

#ifdef USE_COMPOSITE_AS_MULTISLOT
	int num_interfaces;
#endif

	/*
	 * Gemalto extra features, if any
	 */
	struct GEMALTO_FIRMWARE_FEATURES *gemalto_firmware_features;

#ifdef ENABLE_ZLP
	/*
	 * Zero Length Packet fixup (boolean)
	 */
	bool zlp;
#endif

#ifdef SEC1210_SYNC
	struct _sec1210_cond * sec1210_shared;
	struct _ccid_descriptor *sec1210_other_interface;
	int sec1210_interface;
#endif
} _ccid_descriptor;

#ifdef TWIN_SERIAL

/* 271 = max size for short APDU
 * 2 bytes for header
 * 1 byte checksum
 * doubled for echo
 */
#define GEMPCTWIN_MAXBUF (271 +2 +1) * 2

typedef struct
{
	/*
	 * File handle on the serial port
	 */
	int fd;

	/*
	 * device used ("/dev/ttyS?" under Linux)
	 */
	/*@null@*/ char *device;

	/*
	 * Number of slots using the same device
	 */
	int real_nb_opened_slots;
	int *nb_opened_slots;
	struct CCID_DESC *previous_slot;

	/*
	 * does the reader echoes the serial communication bytes?
	 */
	bool echo;

	/*
	 * serial communication buffer
	 */
	unsigned char buffer[GEMPCTWIN_MAXBUF];

	/*
	 * next available byte
	 */
	int buffer_offset;

	/*
	 * number of available bytes
	 */
	int buffer_offset_last;

	/*
	 * CCID infos common to USB and serial
	 */
	_ccid_descriptor ccid;

} _serialDevice;

typedef _serialDevice _Device;

#else

#include <libusb.h>

struct multiSlot_ConcurrentAccess
{
	unsigned char buffer[10 + MAX_BUFFER_SIZE_EXTENDED];
	int length;

	pthread_mutex_t slot_mutex;
	pthread_cond_t slot_condition;
};

struct usbDevice_MultiSlot_Extension
{
	struct CCID_DESC * ccid_reader;

	/* The multi-threaded polling part */
	_Atomic bool terminated;
	int status;
	unsigned char buffer[CCID_INTERRUPT_SIZE];
	pthread_t thread_proc;
	pthread_mutex_t mutex;
	pthread_cond_t condition;

	pthread_t thread_concurrent;
	struct multiSlot_ConcurrentAccess *concurrent;
	libusb_device_handle *dev_handle;
};

typedef struct
{
	libusb_device_handle *dev_handle;
	uint8_t bus_number;
	uint8_t device_address;
	int interface;

	/*
	 * Endpoints
	 */
	int bulk_in;
	int bulk_out;
	int interrupt;

	/* Number of slots using the same device */
	int real_nb_opened_slots;
	int *nb_opened_slots;
	struct CCID_DESC *previous_slot;

	/*
	 * CCID infos common to USB and serial
	 */
	_ccid_descriptor ccid;

	/* libusb transfer for the polling (or NULL) */
	pthread_mutex_t polling_transfer_mutex;
	struct libusb_transfer *polling_transfer;
	/* whether the polling should be terminated */
	bool terminate_requested;

	/* pointer to the multislot extension (if any) */
	struct usbDevice_MultiSlot_Extension *multislot_extension;

	_Atomic bool disconnected;
} _usbDevice;

typedef _usbDevice _Device;

#endif

#include "openct/proto-t1.h"

typedef struct CCID_DESC
{
	/* index in CcidSlots array */
	int reader_index;

	/*
	 * ATR
	 */
	int nATRLength;
	unsigned char pcATRBuffer[MAX_ATR_SIZE];

	/*
	 * Card state
	 */
	unsigned char bPowerFlags;

	/*
	 * T=1 Protocol context
	 */
	t1_state_t t1;

	/* reader name passed to IFDHCreateChannelByName() */
	char *readerName;

	/* reader lun for logs */
	int lun;

	/* USB or serial device */
	_Device device;
} CcidDesc;

typedef enum {
	STATUS_NO_SUCH_DEVICE        = 0xF9,
	STATUS_SUCCESS               = 0xFA,
	STATUS_UNSUCCESSFUL          = 0xFB,
	STATUS_COMM_ERROR            = 0xFC,
	STATUS_DEVICE_PROTOCOL_ERROR = 0xFD,
	STATUS_COMM_NAK              = 0xFE,
	STATUS_SECONDARY_SLOT        = 0xFF
} status_t;

/* Powerflag (used to detect quick insertion removals unnoticed by the
 * resource manager) */
/* Initial value */
#define POWERFLAGS_RAZ 0x00
/* Flag set when a power up has been requested */
#define MASK_POWERFLAGS_PUP 0x01
/* Flag set when a power down is requested */
#define MASK_POWERFLAGS_PDWN 0x02

/* Communication buffer size (max=adpu+Lc+data+Le)
 * we use a 64kB for extended APDU on APDU mode readers */
#define CMD_BUF_SIZE (4 +3 +64*1024 +3)

/* Protocols */
#define T_0 0
#define T_1 1

/* Default communication read timeout in milliseconds */
#define DEFAULT_COM_READ_TIMEOUT (3*1000)

/* DWORD type formatting */
#ifdef __APPLE__
/* Apple defines DWORD as uint32_t */
#define DWORD_X "%X"
#define DWORD_D "%d"
#else
/* pcsc-lite defines DWORD as unsigned long */
#define DWORD_X "%lX"
#define DWORD_D "%ld"
#endif

/*
 * communication ports abstraction
 */
#ifdef TWIN_SERIAL

#define OpenPortByName OpenSerialByName
#define OpenPort OpenSerial
#define ClosePort CloseSerial
#define ReadPort ReadSerial
#define WritePort WriteSerial
#define DisconnectPort DisconnectSerial
#include "ccid_serial.h"

#else

#define OpenPortByName OpenUSBByName
#define OpenPort OpenUSB
#define ClosePort CloseUSB
#define ReadPort ReadUSB
#define WritePort WriteUSB
#define DisconnectPort DisconnectUSB
#include "ccid_usb.h"

#endif
#endif


/*
    ccid.h: CCID structures
    Copyright (C) 2003-2010   Ludovic Rousseau

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

typedef struct
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

	/*
	 * Gemalto extra features, if any
	 */
	struct GEMALTO_FIRMWARE_FEATURES *gemalto_firmware_features;

#ifdef ENABLE_ZLP
	/*
	 * Zero Length Packet fixup (boolean)
	 */
	char zlp;
#endif
} _ccid_descriptor;

/* Features from dwFeatures */
#define CCID_CLASS_AUTO_CONF_ATR	0x00000002
#define CCID_CLASS_AUTO_ACTIVATION	0x00000004
#define CCID_CLASS_AUTO_VOLTAGE		0x00000008
#define CCID_CLASS_AUTO_BAUD		0x00000020
#define CCID_CLASS_AUTO_PPS_PROP	0x00000040
#define CCID_CLASS_AUTO_PPS_CUR		0x00000080
#define CCID_CLASS_AUTO_IFSD		0x00000400
#define CCID_CLASS_CHARACTER		0x00000000
#define CCID_CLASS_TPDU				0x00010000
#define CCID_CLASS_SHORT_APDU		0x00020000
#define CCID_CLASS_EXTENDED_APDU	0x00040000
#define CCID_CLASS_EXCHANGE_MASK	0x00070000

/* Features from bPINSupport */
#define CCID_CLASS_PIN_VERIFY		0x01
#define CCID_CLASS_PIN_MODIFY		0x02

/* See CCID specs ch. 4.2.1 */
#define CCID_ICC_PRESENT_ACTIVE		0x00	/* 00 0000 00 */
#define CCID_ICC_PRESENT_INACTIVE	0x01	/* 00 0000 01 */
#define CCID_ICC_ABSENT				0x02	/* 00 0000 10 */
#define CCID_ICC_STATUS_MASK		0x03	/* 00 0000 11 */

#define CCID_COMMAND_FAILED			0x40	/* 01 0000 00 */
#define CCID_TIME_EXTENSION			0x80	/* 10 0000 00 */

/* bInterfaceProtocol for ICCD */
#define PROTOCOL_CCID	0	/* plain CCID */
#define PROTOCOL_ICCD_A	1	/* ICCD Version A */
#define PROTOCOL_ICCD_B	2	/* ICCD Version B */

/* Product identification for special treatments */
#define GEMPC433	0x08E64433
#define GEMPCKEY	0x08E63438
#define GEMPCTWIN	0x08E63437
#define GEMPCPINPAD 0x08E63478
#define GEMCORESIMPRO 0x08E63480
#define GEMCORESIMPRO2 0x08E60000 /* Does NOT match a real VID/PID as new firmware release exposes same VID/PID */
#define GEMCOREPOSPRO 0x08E63479
#define GEMALTOPROXDU 0x08E65503
#define GEMALTOPROXSU 0x08E65504
#define GEMALTO_EZIO_CBP 0x08E634C3
#define CARDMAN3121	0x076B3021
#define LTC31		0x07830003
#define C3PO_LTC31_v2 0x07830006
#define SCR331DI	0x04E65111
#define SCR331DINTTCOM	0x04E65120
#define SDI010		0x04E65121
#define SEC1210	0x04241202
#define CHERRYXX33	0x046A0005
#define CHERRYST2000	0x046A003E
#define OZ776		0x0B977762
#define OZ776_7772	0x0B977772
#define SPR532		0x04E6E003
#define MYSMARTPAD	0x09BE0002
#define CHERRYXX44	0x046a0010
#define CL1356D		0x0B810200
#define REINER_SCT	0x0C4B0300
#define SEG			0x08E68000
#define BLUDRIVEII_CCID	0x1B0E1078
#define DELLSCRK    0x413C2101
#define DELLSK      0x413C2100
#define KOBIL_TRIBANK	0x0D463010
#define KOBIL_MIDENTITY_VISUAL	0x0D464289
#define VEGAALPHA   0x09820008
#define HPSMARTCARDKEYBOARD 0x03F01024
#define HP_CCIDSMARTCARDKEYBOARD 0x03F00036
#define KOBIL_IDTOKEN 0x0D46301D
#define FUJITSUSMARTKEYB 0x0BF81017
#define FEITIANR502DUAL 0x096E060D
#define MICROCHIP_SEC1100 0x04241104
#define CHERRY_KC1000SC 0x046A00A1
#define ElatecTWN4_CCID_CDC	0x09D80427
#define ElatecTWN4_CCID	0x09D80428
#define SCM_SCL011 0x04E65293
#define HID_AVIATOR	0x076B3A21
#define HID_OMNIKEY_5422 0x076B5422
#define HID_OMNIKEY_3X21 0x076B3031 /* OMNIKEY 3121 or 3021 or 1021 */
#define HID_OMNIKEY_6121 0x076B6632 /* OMNIKEY 6121 */
#define CHERRY_XX44	0x046A00A7 /* Cherry Smart Terminal xx44 */
#define FUJITSU_D323 0x0BF81024 /* Fujitsu Smartcard Reader D323 */

#define VENDOR_GEMALTO 0x08E6
#define GET_VENDOR(readerID) ((readerID >> 16) & 0xFFFF)

/*
 * The O2Micro OZ776S reader has a wrong USB descriptor
 * The extra[] field is associated with the last endpoint instead of the
 * main USB descriptor
 */
#define O2MICRO_OZ776_PATCH

/* Escape sequence codes */
#define ESC_GEMPC_SET_ISO_MODE		1
#define ESC_GEMPC_SET_APDU_MODE		2

/*
 * Possible values :
 * 3 -> 1.8V, 3V, 5V
 * 2 -> 3V, 5V, 1.8V
 * 1 -> 5V, 1.8V, 3V
 * 0 -> automatic (selection made by the reader)
 */
/*
 * The default is to start at 5V
 * otherwise we would have to parse the ATR and get the value of TAi (i>2) when
 * in T=15
 */
#define VOLTAGE_AUTO 0
#define VOLTAGE_5V 1
#define VOLTAGE_3V 2
#define VOLTAGE_1_8V 3

int ccid_open_hack_pre(unsigned int reader_index);
int ccid_open_hack_post(unsigned int reader_index);
void ccid_error(int log_level, int error, const char *file, int line,
	const char *function);
_ccid_descriptor *get_ccid_descriptor(unsigned int reader_index);

/* convert a 4 byte integer in USB format into an int */
#define dw2i(a, x) (unsigned int)((((((a[x+3] << 8) + a[x+2]) << 8) + a[x+1]) << 8) + a[x])

/* all the data rates specified by ISO 7816-3 Fi/Di tables */
#define ISO_DATA_RATES 10753, 14337, 15625, 17204, \
		20833, 21505, 23438, 25806, 28674, \
		31250, 32258, 34409, 39063, 41667, \
		43011, 46875, 52083, 53763, 57348, \
		62500, 64516, 68817, 71685, 78125, \
		83333, 86022, 93750, 104167, 107527, \
		114695, 125000, 129032, 143369, 156250, \
		166667, 172043, 215054, 229391, 250000, \
		344086

/* data rates supported by the secondary slots on the GemCore Pos Pro & SIM Pro */
#define GEMPLUS_CUSTOM_DATA_RATES 10753, 21505, 43011, 125000

/* data rates for GemCore SIM Pro 2 */
#define SIMPRO2_ISO_DATA_RATES 8709, 10322, 12403, 12500, \
		12903, 17204, 18750, 20645, 24806, \
		25000, 25806, 28125, 30967, 34408, \
		37500, 41290, 46875, 49612, 50000, \
		51612, 56250, 62500, 64516, 68817, \
		74418, 75000, 82580, 86021, 93750, \
		99224, 100000, 103225, 112500, 124031, \
		125000, 137634, 150000, 154838, 165161, \
		172043, 187500, 198449, 200000, 206451, \
		258064, 275268, 300000, 396899, 400000, \
		412903, 550537, 600000, 825806

/* Structure returned by Gemalto readers for the CCID Escape command 0x6A */
struct GEMALTO_FIRMWARE_FEATURES
{
	unsigned char	bLogicalLCDLineNumber;	/* Logical number of LCD lines */
	unsigned char	bLogicalLCDRowNumber;	/* Logical number of characters per LCD line */
	unsigned char	bLcdInfo;				/* b0 indicates if scrolling is available */
	unsigned char	bEntryValidationCondition;	/* See PIN_PROPERTIES */

	/* Here come the PC/SC bit features to report */
	unsigned char	VerifyPinStart:1;
	unsigned char	VerifyPinFinish:1;
	unsigned char	ModifyPinStart:1;
	unsigned char	ModifyPinFinish:1;
	unsigned char	GetKeyPressed:1;
	unsigned char	VerifyPinDirect:1;
	unsigned char	ModifyPinDirect:1;
	unsigned char	Abort:1;

	unsigned char	GetKey:1;
	unsigned char	WriteDisplay:1;
	unsigned char	SetSpeMessage:1;
	unsigned char	RFUb1:5;

	unsigned char	RFUb2[2];

	/* Additional flags */
	unsigned char	bTimeOut2:1;
	unsigned char	bListSupportedLanguages:1;	/* Reader is able to indicate
	   the list of supported languages through CCID-ESC 0x6B */
	unsigned char	bNumberMessageFix:1;	/* Reader handles correctly shifts
		made by bNumberMessage in PIN modification data structure */
	unsigned char	bPPDUSupportOverXferBlock:1;	/* Reader supports PPDU over
		PC_to_RDR_XferBlock command */
	unsigned char	bPPDUSupportOverEscape:1;	/* Reader supports PPDU over
		PC_to_RDR_Escape command with abData[0]=0xFF */
	unsigned char	RFUb3:3;

	unsigned char	RFUb4[3];

	unsigned char	VersionNumber;	/* ?? */
	unsigned char	MinimumPINSize;	/* for Verify and Modify */
	unsigned char	MaximumPINSize;

	/* Miscellaneous reader features */
	unsigned char	Firewall:1;
	unsigned char	RFUb5:7;

	/* The following fields, FirewalledCommand_SW1 and
	 * FirewalledCommand_SW2 are only valid if Firewall=1
	 * These fields give the SW1 SW2 value used by the reader to
	 * indicate a command has been firewalled */
	unsigned char	FirewalledCommand_SW1;
	unsigned char	FirewalledCommand_SW2;
	unsigned char	RFUb6[3];
};


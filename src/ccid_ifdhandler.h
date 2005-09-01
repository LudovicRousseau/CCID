/*
    ccid_ifdhandler.h: non-generic ifdhandler functions
    Copyright (C) 2004   Ludovic Rousseau

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/

/*
 * $Id$
 */

#ifndef _ccid_ifd_handler_h_
#define _ccid_ifd_handler_h_

#define IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE	SCARD_CTL_CODE(1)

#define CLASS2_IOCTL_MAGIC 0x330000
#define IOCTL_FEATURE_VERIFY_PIN_DIRECT \
	SCARD_CTL_CODE(FEATURE_VERIFY_PIN_DIRECT + CLASS2_IOCTL_MAGIC)
#define IOCTL_FEATURE_MODIFY_PIN_DIRECT \
	SCARD_CTL_CODE(FEATURE_MODIFY_PIN_DIRECT + CLASS2_IOCTL_MAGIC)

#define DRIVER_OPTION_CCID_EXCHANGE_AUTHORIZED 1
#define DRIVER_OPTION_GEMPC_TWIN_KEY_APDU 2
#define DRIVER_OPTION_USE_BOGUS_FIRMWARE 4
#define DRIVER_OPTION_RESET_ON_CLOSE 8

extern int LogLevel;
extern int DriverOptions;

/*
 * Maximum number of CCID readers supported simultaneously
 *
 * The maximum number of readers is also limited in pcsc-lite (16 by default)
 * see the definition of PCSCLITE_MAX_READERS_CONTEXTS in src/PCSC/pcsclite.h
 */
#define CCID_DRIVER_MAX_READERS 16

/*
 * CCID driver specific functions
 */
CcidDesc *get_ccid_slot(unsigned int reader_index);

#endif


/*
    ccid_ifdhandler.h: non-generic ifdhandler functions
    Copyright (C) 2004   Ludovic Rousseau

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 * $Id$
 */

#ifndef _ccid_ifd_handler_h_
#define _ccid_ifd_handler_h_

#define SCARD_CTL_CODE(code) (0x42000000 + (code))

#define IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE	SCARD_CTL_CODE(1)
#define IOCTL_SMARTCARD_VENDOR_VERIFY_PIN	SCARD_CTL_CODE(2)
#define IOCTL_SMARTCARD_VENDOR_MODIFY_PIN	SCARD_CTL_CODE(3)
#define IOCTL_SMARTCARD_VENDOR_TRANSFER_PIN	SCARD_CTL_CODE(4)

#define DRIVER_OPTION_CCID_EXCHANGE_AUTHORIZED 1
#define DRIVER_OPTION_GEMPC_TWIN_KEY_APDU 2

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


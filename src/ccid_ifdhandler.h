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

#define IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE 2048

#define DRIVER_OPTION_CCID_EXCHANGE_AUTHORIZED 1
#define DRIVER_OPTION_GEMPC_TWIN_KEY_APDU 2

extern int LogLevel;
extern int DriverOptions;

/*
 * Maximum number of CCID readers supported simultaneously
 */
#define PCSCLITE_MAX_READERS 16

/*
 * CCID driver specific functions
 */
CcidDesc *get_ccid_slot(int lun);

#endif


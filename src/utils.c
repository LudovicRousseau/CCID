/*
    utils.c:
    Copyright (C) 2003   Ludovic Rousseau

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

#include <PCSC/pcsclite.h>

#include "ccid.h"
#include "defs.h"
#include "ccid_ifdhandler.h"
#include "utils.h"

/* Check if the Lun is not to large for the pgSlots table
 * returns TRUE in case of error */
int CheckLun(DWORD Lun)
{
	if ((LunToReaderIndex(Lun) >= PCSCLITE_MAX_READERS) ||
		LunToReaderIndex(Lun) < 0)
		return TRUE;

	return FALSE;
} /* CheckLun */


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
#include "debug.h"

int ReaderIndex[CCID_DRIVER_MAX_READERS];

void InitReaderIndex(void)
{
	int i;

	for (i=0; i<CCID_DRIVER_MAX_READERS; i++)
		ReaderIndex[i] = -1;
} /* InitReaderIndex */

int GetNewReaderIndex(const DWORD Lun)
{
	int i;

	for (i=0; i<CCID_DRIVER_MAX_READERS; i++)
		if (-1 == ReaderIndex[i])
		{
			ReaderIndex[i] = Lun;
			return i;
		}

	DEBUG_CRITICAL("ReaderIndex[] is full");
	return -1;
} /* GetReaderIndex */

int LunToReaderIndex(const DWORD Lun)
{
	int i;

	for (i=0; i<CCID_DRIVER_MAX_READERS; i++)
		if (Lun == ReaderIndex[i])
			return i;

	DEBUG_CRITICAL2("Lun: %X not found", Lun);
	return -1;
} /* LunToReaderIndex */

int ReleaseReaderIndex(const int index)
{
	ReaderIndex[index] = -1;
} /* ReleaseReaderIndex */


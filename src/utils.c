/*
    utils.c:
    Copyright (C) 2003-2008   Ludovic Rousseau

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

#include <string.h>
#include <pcsclite.h>

#include <config.h>
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

int GetNewReaderIndex(const int Lun)
{
	int i;

	/* check that Lun is NOT already used */
	for (i=0; i<CCID_DRIVER_MAX_READERS; i++)
		if (Lun == ReaderIndex[i])
			break;

	if (i < CCID_DRIVER_MAX_READERS)
	{
		DEBUG_CRITICAL2("Lun: %d is already used", Lun);
		return -1;
	}

	for (i=0; i<CCID_DRIVER_MAX_READERS; i++)
		if (-1 == ReaderIndex[i])
		{
			ReaderIndex[i] = Lun;
			return i;
		}

	DEBUG_CRITICAL("ReaderIndex[] is full");
	return -1;
} /* GetReaderIndex */

int LunToReaderIndex(const int Lun)
{
	int i;

	for (i=0; i<CCID_DRIVER_MAX_READERS; i++)
		if (Lun == ReaderIndex[i])
			return i;

	DEBUG_CRITICAL2("Lun: %X not found", Lun);
	return -1;
} /* LunToReaderIndex */

void ReleaseReaderIndex(const int index)
{
	ReaderIndex[index] = -1;
} /* ReleaseReaderIndex */

/* Read a non aligned 16-bit integer */
uint16_t get_U16(void *buf)
{
	uint16_t value;

	memcpy(&value, buf, sizeof value);

	return value;
}

/* Read a non aligned 32-bit integer */
uint32_t get_U32(void *buf)
{
	uint32_t value;

	memcpy(&value, buf, sizeof value);

	return value;
}

/* Write a non aligned 16-bit integer */
void set_U16(void *buf, uint16_t value)
{
	memcpy(buf, &value, sizeof value);
}

/* Write a non aligned 32-bit integer */
void set_U32(void *buf, uint32_t value)
{
	memcpy(buf, &value, sizeof value);
}

/* swap a 16-bits integer in memory */
/* "AB" -> "BA" */
void p_bswap_16(void *ptr)
{
	uint8_t *array, tmp;

	array = ptr;
	tmp = array[0];
	array[0] = array[1];
	array[1] = tmp;
}

/* swap a 32-bits integer in memory */
/* "ABCD" -> "DCBA" */
void p_bswap_32(void *ptr)
{
	uint8_t *array, tmp;

	array = ptr;
	tmp = array[0];
	array[0] = array[3];
	array[3] = tmp;

	tmp = array[1];
	array[1] = array[2];
	array[2] = tmp;
}

/*
	utils.c:
	Copyright (C) 2003-2024   Ludovic Rousseau

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
#include <stdlib.h>
#include <pcsclite.h>

#include <config.h>
#include "ccid.h"
#include "defs.h"
#include "ccid_ifdhandler.h"
#include "utils.h"
#include "debug.h"

#define FREE_ENTRY -42

#define INITIAL_CCID_DRIVER_MAX_READERS 2

CcidDesc **CcidSlots;
int ccid_driver_max_readers = -1;

void InitReaderIndex(void)
{
	ccid_driver_max_readers = INITIAL_CCID_DRIVER_MAX_READERS;
	CcidSlots = calloc(ccid_driver_max_readers, sizeof(CcidSlots[0]));

	for (int i=0; i<ccid_driver_max_readers; i++)
	{
		CcidSlots[i] = calloc(1, sizeof(*CcidSlots[0]));
		CcidSlots[i]->lun = FREE_ENTRY;
	}
} /* InitReaderIndex */

int GetNewReaderIndex(const int Lun)
{
	int i;
	int ret;

	/* check that Lun is NOT already used */
	for (i=0; i<ccid_driver_max_readers; i++)
		if (Lun == CcidSlots[i]->lun)
		{
			DEBUG_CRITICAL2("Lun: %X is already used", Lun);
			return -1;
		}

	for (i=0; i<ccid_driver_max_readers; i++)
		if (FREE_ENTRY == CcidSlots[i]->lun)
		{
			CcidSlots[i]->lun = Lun;
			return i;
		}

	/* all the slots are used */
	int new_size = ccid_driver_max_readers + INITIAL_CCID_DRIVER_MAX_READERS;
	Log3(PCSC_LOG_DEBUG, "from %d to %d", ccid_driver_max_readers, new_size);

	CcidSlots = realloc(CcidSlots, new_size * sizeof(CcidSlots[0]));
	for (i=ccid_driver_max_readers; i<new_size; i++)
	{
		CcidSlots[i] = calloc(1, sizeof(*CcidSlots[0]));
		CcidSlots[i]->lun = FREE_ENTRY;
	}

	ret = ccid_driver_max_readers;
	CcidSlots[ret]->lun = Lun;

	ccid_driver_max_readers = new_size;

	return ret;
} /* GetReaderIndex */

int LunToReaderIndex(const int Lun)
{
	int i;

	for (i=0; i<ccid_driver_max_readers; i++)
		if (Lun == CcidSlots[i]->lun)
			return i;

	DEBUG_CRITICAL2("Lun: %X not found", Lun);
	return -1;
} /* LunToReaderIndex */

CcidDesc * LunToCcidDesc(const int Lun)
{
	int i;

	for (i=0; i<ccid_driver_max_readers; i++)
		if (Lun == CcidSlots[i]->lun)
			return CcidSlots[i];

	DEBUG_CRITICAL2("Lun: %X not found", Lun);
	return NULL;
} /* LunToReaderIndex */

void ReleaseReaderIndex(const int index)
{
	memset(CcidSlots[index], 0, sizeof(*CcidSlots[0]));
	CcidSlots[index]->lun = FREE_ENTRY;
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

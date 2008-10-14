/*
    atr.c
    ISO 7816 ICC's answer to reset abstract data type implementation

    This file is part of the Unix driver for Towitoko smartcard readers
    Copyright (C) 2000 Carlos Prados <cprados@yahoo.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public License
	along with this library; if not, write to the Free Software Foundation,
	Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "atr.h"
#include <stdlib.h>
#include <string.h>
#include "debug.h"

/*
 * Not exported variables definition
 */

static unsigned
atr_num_ib_table[16] =
{
  0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4
};

/*
 * Exported variables definition
 */

static unsigned
atr_f_table[16] =
{
  372, 372, 558, 744, 1116, 1488, 1860, 0, 0, 512, 768, 1024, 1536, 2048, 0, 0
};

static unsigned
atr_d_table[16] =
{
  0, 1, 2, 4, 8, 16, 32, 64, 12, 20, 0, 0, 0, 0, 0, 0
};

static unsigned
atr_i_table[4] =
{
  25, 50, 100, 0
};

/*
 * Exported functions definition
 */

int
ATR_InitFromArray (ATR_t * atr, const BYTE atr_buffer[ATR_MAX_SIZE], unsigned length)
{
  BYTE TDi;
  unsigned pointer = 0, pn = 0;

  /* Check size of buffer */
  if (length < 2)
    return (ATR_MALFORMED);

  /* Store T0 and TS */
  atr->TS = atr_buffer[0];

  atr->T0 = TDi = atr_buffer[1];
  pointer = 1;

  /* Store number of historical bytes */
  atr->hbn = TDi & 0x0F;

  /* TCK is not present by default */
  (atr->TCK).present = FALSE;

  /* Extract interface bytes */
  while (pointer < length)
    {
      /* Check buffer is long enought */
      if (pointer + atr_num_ib_table[(0xF0 & TDi) >> 4] >= length)
	{
	  return (ATR_MALFORMED);
	}
      /* Check TAi is present */
      if ((TDi | 0xEF) == 0xFF)
	{
	  pointer++;
	  atr->ib[pn][ATR_INTERFACE_BYTE_TA].value = atr_buffer[pointer];
	  atr->ib[pn][ATR_INTERFACE_BYTE_TA].present = TRUE;
	}
      else
	atr->ib[pn][ATR_INTERFACE_BYTE_TA].present = FALSE;
      /* Check TBi is present */
      if ((TDi | 0xDF) == 0xFF)
	{
	  pointer++;
	  atr->ib[pn][ATR_INTERFACE_BYTE_TB].value = atr_buffer[pointer];
	  atr->ib[pn][ATR_INTERFACE_BYTE_TB].present = TRUE;
	}
      else
	atr->ib[pn][ATR_INTERFACE_BYTE_TB].present = FALSE;

      /* Check TCi is present */
      if ((TDi | 0xBF) == 0xFF)
	{
	  pointer++;
	  atr->ib[pn][ATR_INTERFACE_BYTE_TC].value = atr_buffer[pointer];
	  atr->ib[pn][ATR_INTERFACE_BYTE_TC].present = TRUE;
	}
      else
	atr->ib[pn][ATR_INTERFACE_BYTE_TC].present = FALSE;

      /* Read TDi if present */
      if ((TDi | 0x7F) == 0xFF)
	{
	  pointer++;
	  TDi = atr->ib[pn][ATR_INTERFACE_BYTE_TD].value = atr_buffer[pointer];
	  atr->ib[pn][ATR_INTERFACE_BYTE_TD].present = TRUE;
	  (atr->TCK).present = ((TDi & 0x0F) != ATR_PROTOCOL_TYPE_T0);
	  if (pn >= ATR_MAX_PROTOCOLS)
	    return (ATR_MALFORMED);
	  pn++;
	}
      else
	{
	  atr->ib[pn][ATR_INTERFACE_BYTE_TD].present = FALSE;
	  break;
	}
    }

  /* Store number of protocols */
  atr->pn = pn + 1;

  /* Store historical bytes */
  if (pointer + atr->hbn >= length)
    return (ATR_MALFORMED);

  memcpy (atr->hb, atr_buffer + pointer + 1, atr->hbn);
  pointer += (atr->hbn);

  /* Store TCK  */
  if ((atr->TCK).present)
    {

      if (pointer + 1 >= length)
	return (ATR_MALFORMED);

      pointer++;

      (atr->TCK).value = atr_buffer[pointer];
    }

  atr->length = pointer + 1;
  return (ATR_OK);
}

int
ATR_GetConvention (ATR_t * atr, int *convention)
{
  if (atr->TS == 0x3B)
    (*convention) = ATR_CONVENTION_DIRECT;
  else if (atr->TS == 0x3F)
    (*convention) = ATR_CONVENTION_INVERSE;
  else
    return (ATR_MALFORMED);
  return (ATR_OK);
}

int
ATR_GetIntegerValue (ATR_t * atr, int name, BYTE * value)
{
  int ret;

  if (name == ATR_INTEGER_VALUE_FI)
    {
      if (atr->ib[0][ATR_INTERFACE_BYTE_TA].present)
	{
	  (*value) = (atr->ib[0][ATR_INTERFACE_BYTE_TA].value & 0xF0) >> 4;
	  ret = ATR_OK;
	}
      else
	ret = ATR_NOT_FOUND;
    }

  else if (name == ATR_INTEGER_VALUE_DI)
    {
      if (atr->ib[0][ATR_INTERFACE_BYTE_TA].present)
	{
	  (*value) = (atr->ib[0][ATR_INTERFACE_BYTE_TA].value & 0x0F);
	  ret = ATR_OK;
	}
      else
	ret = ATR_NOT_FOUND;
    }

  else if (name == ATR_INTEGER_VALUE_II)
    {
      if (atr->ib[0][ATR_INTERFACE_BYTE_TB].present)
	{
	  (*value) = (atr->ib[0][ATR_INTERFACE_BYTE_TB].value & 0x60) >> 5;
	  ret = ATR_OK;
	}
      else
	ret = ATR_NOT_FOUND;
    }

  else if (name == ATR_INTEGER_VALUE_PI1)
    {
      if (atr->ib[0][ATR_INTERFACE_BYTE_TB].present)
	{
	  (*value) = (atr->ib[0][ATR_INTERFACE_BYTE_TB].value & 0x1F);
	  ret = ATR_OK;
	}
      else
	ret = ATR_NOT_FOUND;
    }

  else if (name == ATR_INTEGER_VALUE_PI2)
    {
      if (atr->ib[1][ATR_INTERFACE_BYTE_TB].present)
	{
	  (*value) = atr->ib[1][ATR_INTERFACE_BYTE_TB].value;
	  ret = ATR_OK;
	}
      else
	ret = ATR_NOT_FOUND;
    }

  else if (name == ATR_INTEGER_VALUE_N)
    {
      if (atr->ib[0][ATR_INTERFACE_BYTE_TC].present)
	{
	  (*value) = atr->ib[0][ATR_INTERFACE_BYTE_TC].value;
	  ret = ATR_OK;
	}
      else
	ret = ATR_NOT_FOUND;
    }
  else
    ret = ATR_NOT_FOUND;

  return ret;
}

int
ATR_GetParameter (ATR_t * atr, int name, double *parameter)
{
  BYTE FI, DI, II, PI1, PI2, N;

  if (name == ATR_PARAMETER_F)
    {
      if (ATR_GetIntegerValue (atr, ATR_INTEGER_VALUE_FI, &FI) == ATR_OK)
	(*parameter) = (double) (atr_f_table[FI]);
      else
	(*parameter) = (double) ATR_DEFAULT_F;
      return (ATR_OK);
    }

  else if (name == ATR_PARAMETER_D)
    {
      if (ATR_GetIntegerValue (atr, ATR_INTEGER_VALUE_DI, &DI) == ATR_OK)
	(*parameter) = (double) (atr_d_table[DI]);
      else
	(*parameter) = (double) ATR_DEFAULT_D;
      return (ATR_OK);
    }

  else if (name == ATR_PARAMETER_I)
    {
      if (ATR_GetIntegerValue (atr, ATR_INTEGER_VALUE_II, &II) == ATR_OK)
	(*parameter) = (double) (atr_i_table[II]);
      else
	(*parameter) = ATR_DEFAULT_I;
      return (ATR_OK);
    }

  else if (name == ATR_PARAMETER_P)
    {
      if (ATR_GetIntegerValue (atr, ATR_INTEGER_VALUE_PI2, &PI2) == ATR_OK)
	(*parameter) = (double) PI2;
      else if (ATR_GetIntegerValue (atr, ATR_INTEGER_VALUE_PI1, &PI1) == ATR_OK)
	(*parameter) = (double) PI1;
      else
	(*parameter) = (double) ATR_DEFAULT_P;
      return (ATR_OK);
    }

  else if (name == ATR_PARAMETER_N)
    {
      if (ATR_GetIntegerValue (atr, ATR_INTEGER_VALUE_N, &N) == ATR_OK)
	(*parameter) = (double) N;
      else
	(*parameter) = (double) ATR_DEFAULT_N;
      return (ATR_OK);
    }

  return (ATR_NOT_FOUND);
}

/*
 * This function was greatly inspired by ATRDecodeAtr() and
 * PHGetDefaultProtocol() from pcsc-lite
 *
 * It was rewritten by Ludovic Rousseau, 2004
 */
#define PROTOCOL_UNSET -1
int ATR_GetDefaultProtocol(ATR_t * atr, int *protocol)
{
	int i;

	/* default value */
	*protocol = PROTOCOL_UNSET;

	for (i=0; i<ATR_MAX_PROTOCOLS; i++)
		if (atr->ib[i][ATR_INTERFACE_BYTE_TD].present && (PROTOCOL_UNSET == *protocol))
		{
			/* set to the first protocol byte found */
			*protocol = atr->ib[i][ATR_INTERFACE_BYTE_TD].value & 0x0F;
			DEBUG_COMM2("default protocol: T=%d", *protocol);
		}

	/* specific mode if TA2 present */
	if (atr->ib[1][ATR_INTERFACE_BYTE_TA].present)
	{
		*protocol = atr->ib[1][ATR_INTERFACE_BYTE_TA].value & 0x0F;
		DEBUG_COMM2("specific mode found: T=%d", *protocol);
	}

	if (PROTOCOL_UNSET == *protocol)
	{
		DEBUG_INFO("no default protocol found in ATR. Using T=0");
		*protocol = ATR_PROTOCOL_TYPE_T0;
	}

	return ATR_OK;
}


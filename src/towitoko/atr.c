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

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

unsigned
atr_f_table[16] = 
{ 
  372, 372, 558, 744, 1116, 1488, 1860, 0, 0, 512, 768, 1024, 1536, 2048, 0, 0 
};

unsigned 
atr_d_table[16] = 
{ 
  0, 1, 2, 4, 8, 16, 32, 0, 12, 20, 0, 0, 0, 0, 0, 0 
};

unsigned 
atr_i_table[4] = 
{ 
  25, 50, 100, 0 
};

/* 
 * Exported funcions definition
 */

ATR *
ATR_New (void)
{
  ATR *atr;

  /* Allocate memory */
  atr = (ATR *) malloc (sizeof (ATR));

  return atr;
}

int
ATR_InitFromArray (ATR * atr, BYTE atr_buffer[ATR_MAX_SIZE], unsigned length)
{
  BYTE TDi;
  BYTE buffer[ATR_MAX_SIZE];
  unsigned pointer = 0, pn = 0;

  /* Check size of buffer */
  if (length < 2)
    return (ATR_MALFORMED);

  /* Check if ATR is from a inverse convention card */
  if (atr_buffer[0] == 0x03)
    {
      for (pointer = 0; pointer < length; pointer++)
	buffer[pointer] = ~(INVERT_BYTE (atr_buffer[pointer]));
    }
  else
    memcpy (buffer, atr_buffer, length);

  /* Store T0 and TS */
  atr->TS = buffer[0];

  atr->T0 = TDi = buffer[1];
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
	  atr->ib[pn][ATR_INTERFACE_BYTE_TA].value = buffer[pointer];
	  atr->ib[pn][ATR_INTERFACE_BYTE_TA].present = TRUE;
	}
      else
	atr->ib[pn][ATR_INTERFACE_BYTE_TA].present = FALSE;
      /* Check TBi is present */
      if ((TDi | 0xDF) == 0xFF)
	{
	  pointer++;
	  atr->ib[pn][ATR_INTERFACE_BYTE_TB].value = buffer[pointer];
	  atr->ib[pn][ATR_INTERFACE_BYTE_TB].present = TRUE;
	}
      else
	atr->ib[pn][ATR_INTERFACE_BYTE_TB].present = FALSE;

      /* Check TCi is present */
      if ((TDi | 0xBF) == 0xFF)
	{
	  pointer++;
	  atr->ib[pn][ATR_INTERFACE_BYTE_TC].value = buffer[pointer];
	  atr->ib[pn][ATR_INTERFACE_BYTE_TC].present = TRUE;
	}
      else
	atr->ib[pn][ATR_INTERFACE_BYTE_TC].present = FALSE;

      /* Read TDi if present */
      if ((TDi | 0x7F) == 0xFF)
	{
	  pointer++;
	  TDi = atr->ib[pn][ATR_INTERFACE_BYTE_TD].value = buffer[pointer];
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

  memcpy (atr->hb, buffer + pointer + 1, atr->hbn);
  pointer += (atr->hbn);

  /* Store TCK  */
  if ((atr->TCK).present)
    {

      if (pointer + 1 >= length)
	return (ATR_MALFORMED);

      pointer++;

      (atr->TCK).value = buffer[pointer];
    }

  atr->length = pointer + 1;
  return (ATR_OK);
}


void
ATR_Delete (ATR * atr)
{
  free (atr);
}

int
ATR_GetConvention (ATR * atr, int *convention)
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
ATR_GetSize (ATR * atr, unsigned *size)
{
  (*size) = atr->length;
  return (ATR_OK);
}

int
ATR_GetNumberOfProtocols (ATR * atr, unsigned *number_protocols)
{
  (*number_protocols) = atr->pn;
  return (ATR_OK);
}

int
ATR_GetProtocolType (ATR * atr, unsigned number_protocol, BYTE *protocol_type)
{
  if ((number_protocol > atr->pn) || number_protocol < 2)
    return ATR_NOT_FOUND;

  if (atr->ib[number_protocol - 2][ATR_INTERFACE_BYTE_TD].present)
    (*protocol_type) =
      (atr->ib[number_protocol - 2][ATR_INTERFACE_BYTE_TD].value & 0x0F);
  else
    (*protocol_type) = ATR_PROTOCOL_TYPE_T0;

  return (ATR_OK);
}

int
ATR_GetInterfaceByte (ATR * atr, unsigned number, int character, BYTE * value)
{
  if (number > atr->pn || number < 1)
    return (ATR_NOT_FOUND);

  if (atr->ib[number - 1][character].present &&
      (character == ATR_INTERFACE_BYTE_TA ||
       character == ATR_INTERFACE_BYTE_TB ||
       character == ATR_INTERFACE_BYTE_TC ||
       character == ATR_INTERFACE_BYTE_TD))
    (*value) = atr->ib[number - 1][character].value;
  else
    return (ATR_NOT_FOUND);

  return (ATR_OK);
}

int
ATR_GetIntegerValue (ATR * atr, int name, BYTE * value)
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
ATR_GetParameter (ATR * atr, int name, double *parameter)
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

int
ATR_GetHistoricalBytes (ATR * atr, BYTE hist[ATR_MAX_HISTORICAL], unsigned *length)
{
  if (atr->hbn == 0)
    return (ATR_NOT_FOUND);

  (*length) = atr->hbn;
  memcpy (hist, atr->hb, atr->hbn);
  return (ATR_OK);
}

int
ATR_GetRaw (ATR * atr, BYTE buffer[ATR_MAX_SIZE], unsigned *length)
{
  unsigned i, j;

  buffer[0] = atr->TS;
  buffer[1] = atr->T0;

  j = 2;

  for (i = 0; i < atr->pn; i++)
    {
      if (atr->ib[i][ATR_INTERFACE_BYTE_TA].present)
	buffer[j++] = atr->ib[i][ATR_INTERFACE_BYTE_TA].value;

      if (atr->ib[i][ATR_INTERFACE_BYTE_TB].present)
	buffer[j++] = atr->ib[i][ATR_INTERFACE_BYTE_TB].value;

      if (atr->ib[i][ATR_INTERFACE_BYTE_TC].present)
	buffer[j++] = atr->ib[i][ATR_INTERFACE_BYTE_TC].value;

      if (atr->ib[i][ATR_INTERFACE_BYTE_TD].present)
	buffer[j++] = atr->ib[i][ATR_INTERFACE_BYTE_TD].value;
    }

  if (atr->hbn > 0)
    {
      memcpy (&(buffer[j]), atr->hb, atr->hbn);
      j += atr->hbn;
    }

  if ((atr->TCK).present)
    buffer[j++] = (atr->TCK).value;

  (*length) = j;

  return ATR_OK;
}

int
ATR_GetCheckByte (ATR * atr, BYTE * check_byte)
{
  if (!((atr->TCK).present))
    return (ATR_NOT_FOUND);

  (*check_byte) = (atr->TCK).value;
  return (ATR_OK);
}

int
ATR_GetFsMax (ATR * atr, unsigned long *fsmax)
{
  BYTE FI;

  if (ATR_GetIntegerValue (atr, ATR_INTEGER_VALUE_FI, &FI) == ATR_OK)
    (*fsmax) = atr_fs_table[FI];
  else
    (*fsmax) = atr_fs_table[1];

  return (ATR_OK);
}


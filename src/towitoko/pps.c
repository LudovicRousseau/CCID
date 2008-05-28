/*
    pps.c
    Protocol Parameters Selection

    This file is part of the Unix driver for Towitoko smartcard readers
    Copyright (C) 2000 2001 Carlos Prados <cprados@yahoo.com>

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

#include "pps.h"
#include "atr.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ifdhandler.h>

#include "commands.h"
#include "defs.h"
#include "debug.h"

/*
 * Not exported funtions declaration
 */

static bool PPS_Match (BYTE * request, unsigned len_request, BYTE * reply, unsigned len_reply);

static unsigned PPS_GetLength (BYTE * block);

static BYTE PPS_GetPCK (BYTE * block, unsigned length);

int
PPS_Exchange (int lun, BYTE * params, unsigned *length, unsigned char *pps1)
{
  BYTE confirm[PPS_MAX_LENGTH];
  unsigned len_request, len_confirm;
  int ret;

  len_request = PPS_GetLength (params);
  params[len_request - 1] = PPS_GetPCK(params, len_request - 1);

  DEBUG_XXD ("PPS: Sending request: ", params, len_request);

  /* Send PPS request */
  if (CCID_Transmit (lun, len_request, params, isCharLevel(lun) ? 4 : 0, 0)
	!= IFD_SUCCESS)
    return PPS_ICC_ERROR;

  /* Get PPS confirm */
  len_confirm = sizeof(confirm);
  if (CCID_Receive (lun, &len_confirm, confirm, NULL) != IFD_SUCCESS)
    return PPS_ICC_ERROR;

  DEBUG_XXD ("PPS: Receiving confirm: ", confirm, len_confirm);

  if (!PPS_Match (params, len_request, confirm, len_confirm))
    ret = PPS_HANDSAKE_ERROR;
  else
    ret = PPS_OK;

  *pps1 = 0x11;	/* default TA1 */

  /* if PPS1 is echoed */
  if (PPS_HAS_PPS1 (params) && PPS_HAS_PPS1 (confirm))
      *pps1 = confirm[2];

  /* Copy PPS handsake */
  memcpy (params, confirm, len_confirm);
  (*length) = len_confirm;

  return ret;
}

static bool
PPS_Match (BYTE * request, unsigned len_request, BYTE * confirm, unsigned len_confirm)
{
  /* See if the reply differs from request */
  if ((len_request == len_confirm) &&	/* same length */
      memcmp (request, confirm, len_request))	/* different contents */
	return FALSE;

  if (len_request < len_confirm)	/* confirm longer than request */
    return FALSE;

  /* See if the card specifies other than default FI and D */
  if ((PPS_HAS_PPS1 (confirm)) && (confirm[2] != request[2]))
    return FALSE;

  return TRUE;
}

static unsigned
PPS_GetLength (BYTE * block)
{
  unsigned length = 3;

  if (PPS_HAS_PPS1 (block))
    length++;

  if (PPS_HAS_PPS2 (block))
    length++;

  if (PPS_HAS_PPS3 (block))
    length++;

  return length;
}

static BYTE
PPS_GetPCK (BYTE * block, unsigned length)
{
  BYTE pck;
  unsigned i;

  pck = block[0];
  for (i = 1; i < length; i++)
    pck ^= block[i];

  return pck;
}


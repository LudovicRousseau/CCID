/*
    pps.h
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

#ifndef _PPS_
#define _PPS_

#include "defines.h"

/*
 * Exported constants definition
 */

#define PPS_OK			0	/* Negotiation OK */
#define PPS_ICC_ERROR		1	/* Comunication error */
#define PPS_HANDSAKE_ERROR	2	/* Agreement not reached */
#define PPS_PROTOCOL_ERROR	3	/* Error starting protocol */
#define PPS_MAX_LENGTH		6

#define PPS_HAS_PPS1(block)	((block[1] & 0x10) == 0x10)
#define PPS_HAS_PPS2(block)	((block[1] & 0x20) == 0x20)
#define PPS_HAS_PPS3(block)	((block[1] & 0x40) == 0x40)

/*
 * Exported data types definition
 */

typedef struct
{
  double f;
  double d;
  double n;
  BYTE t;
}
PPS_ProtocolParameters;

typedef struct
{
  int icc;
  void *protocol;
  PPS_ProtocolParameters parameters;
}
PPS;

/*
 * Exported functions declaration
 */

int PPS_Exchange (int lun, BYTE * params, /*@out@*/ unsigned *length,
	unsigned char *pps1);

#endif /* _PPS_ */


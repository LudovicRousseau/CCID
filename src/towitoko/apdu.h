/*
    apdu.h
    Definitions for ISO 7816-4 Application Layer PDU's handling

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

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef _APDU_
#define _APDU_

#include "defines.h"

/*
 * Exported data types definition
 */

/* Command APDU */
typedef struct
{
  BYTE *command;
  unsigned long length;
}
APDU_Cmd;

/* Response APDU */
typedef struct
{
  BYTE *response;
  unsigned long length;
}
APDU_Rsp;

#endif


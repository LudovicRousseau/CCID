/*
    t1_block.h
    T=1 block abstract data type definitions

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

#ifndef _T1_BLOCK_
#define _T1_BLOCK_

#include "defines.h"

/*
 * Exported constants definition
 */

/* Buffer sizes */
#define T1_BLOCK_MAX_SIZE                259
#define T1_BLOCK_INF_MAX_SIZE            254

/* Types of block */
#define T1_BLOCK_I                0x00
#define T1_BLOCK_R_OK             0x80
#define T1_BLOCK_R_EDC_ERR        0x81
#define T1_BLOCK_R_OTHER_ERR      0x82
#define T1_BLOCK_S_RESYNCH_REQ    0xC0
#define T1_BLOCK_S_RESYNCH_RES    0xE0
#define T1_BLOCK_S_IFS_REQ        0xC1
#define T1_BLOCK_S_IFS_RES        0xE1
#define T1_BLOCK_S_ABORT_REQ      0xC2
#define T1_BLOCK_S_ABORT_RES      0xE2
#define T1_BLOCK_S_WTX_REQ        0xC3
#define T1_BLOCK_S_WTX_RES        0xE3
#define T1_BLOCK_S_VPP_ERR        0xE4

/*
 * Exported data types definition
 */

typedef struct
{
  BYTE * data;
  unsigned length;
}
T1_Block;
 
/*
 * Exported functions declaration
 */

extern T1_Block *
T1_Block_New (BYTE * buffer, unsigned length);

extern T1_Block *
T1_Block_NewIBlock (BYTE len, BYTE * inf, BYTE ns, bool more);

extern T1_Block *
T1_Block_NewRBlock (BYTE type, BYTE nr);

extern T1_Block *
T1_Block_NewSBlock (BYTE type, BYTE len, BYTE * inf);

extern BYTE
T1_Block_GetType (T1_Block * block);

extern BYTE
T1_Block_GetNS (T1_Block * block);

extern bool
T1_Block_GetMore (T1_Block * block);

extern BYTE
T1_Block_GetNR (T1_Block * block);

extern BYTE
T1_Block_GetLen (T1_Block * block);

extern BYTE *
T1_Block_GetInf (T1_Block * block);

extern BYTE *
T1_Block_Raw (T1_Block * block);

extern unsigned
T1_Block_RawLen (T1_Block * block);

extern void
T1_Block_Delete (T1_Block * block);

extern void
T1_Block_Dump(T1_Block * block);

#endif /* _T1_BLOCK_ */


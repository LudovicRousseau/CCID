/*
    t1_block.h
    T=1 block abstract data type implementation

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

#include "t1_block.h"
#include <stdlib.h>
#include <string.h>

/*
 * Not exported constants definition
 */
 
#define T1_BLOCK_NAD        0x00

/*
 * Not exported functions declaration
 */
static BYTE
T1_Block_LRC (BYTE * data, unsigned length);
 
/*
 * Exported functions definition
 */

T1_Block *
T1_Block_New (BYTE * buffer, unsigned length)
{
  T1_Block * block;
  
  if (length < 4)
    return NULL;
  
  block = (T1_Block *) malloc (sizeof (T1_Block));
  
  if (block != NULL)
    {
      block->length = MIN(length, T1_BLOCK_MAX_SIZE);
      block->data = (BYTE *) calloc (block->length, sizeof (BYTE));
      
      if (block->data != NULL)
        {
          memcpy (block->data, buffer, block->length);
        }
      else
        {
          free (block);
          block = NULL;
        }
    }
  
  return block;
}

T1_Block *
T1_Block_NewIBlock (BYTE len, BYTE * inf, BYTE ns, bool more)
{
  T1_Block * block;
  
  block = (T1_Block *) malloc (sizeof (T1_Block));
  
  if (block != NULL)
    {
      block->length = len + 4;
      block->data = (BYTE *) calloc (block->length, sizeof (BYTE));
      
      if (block->data != NULL)
        {
          block->data[0] = T1_BLOCK_NAD;
          block->data[1] = T1_BLOCK_I | ((ns << 6) & 0x40);
          
          if (more)
            block->data[1] |= 0x20;
            
          block->data[2] = len;
          
          if (len != 0x00)
            memcpy (block->data + 3, inf, len);
          
          block->data[len+3] = T1_Block_LRC (block->data, len+3);
        }
      else
        {
          free (block);
          block = NULL;
        }
    }
  
  return block;
}

T1_Block *
T1_Block_NewRBlock (BYTE type, BYTE nr)
{
  T1_Block * block;
  
  block = (T1_Block *) malloc (sizeof (T1_Block));
  
  if (block != NULL)
    {
      block->length = 4;
      block->data = (BYTE *) calloc (block->length, sizeof (BYTE));
      
      if (block->data != NULL)
        {
          block->data[0] = T1_BLOCK_NAD;
          block->data[1] = type | ((nr << 4) & 0x10);
          block->data[2] = 0x00;
          block->data[3] = T1_Block_LRC (block->data, 3);
        }
      else
        {
          free (block);
          block = NULL;
        }
    }
  
  return block;
}

T1_Block *
T1_Block_NewSBlock (BYTE type, BYTE len, BYTE * inf)
{
  T1_Block * block;
  
  block = (T1_Block *) malloc (sizeof (T1_Block));
  
  if (block != NULL)
    {
      block->length = 4 + len;
      block->data = (BYTE *) calloc (block->length, sizeof (BYTE));
      
      if (block->data != NULL)
        {
          block->data[0] = T1_BLOCK_NAD;
          block->data[1] = type;
          block->data[2] = len;

          if (len != 0x00)
            memcpy (block->data + 3, inf, len);
          
          block->data[len+3] = T1_Block_LRC (block->data, len+3);
        }
      else
        {
          free (block);
          block = NULL;
        }
    }
  
  return block;
}

BYTE
T1_Block_GetType (T1_Block * block)
{
  if ((block->data[1] & 0x80) == T1_BLOCK_I)
    return T1_BLOCK_I;
    
  return (block->data[1] & 0xEF);
}

BYTE
T1_Block_GetNS (T1_Block * block)
{
  return ((block->data[1] >> 6)& 0x01);
}

bool
T1_Block_GetMore (T1_Block * block)
{
  return ((block->data[1] >> 5) & 0x01);
}

BYTE
T1_Block_GetNR (T1_Block * block)
{
  return ((block->data[1] >> 4) & 0x01);
}

BYTE
T1_Block_GetLen (T1_Block * block)
{
  return block->data[2];
}

BYTE *
T1_Block_GetInf (T1_Block * block)
{
  if (block->length < 5)
    return NULL;
    
  return block->data + 3;
}

BYTE *
T1_Block_Raw (T1_Block * block)
{
  return block->data;
}

unsigned
T1_Block_RawLen (T1_Block * block)
{
  return block->length;
}

void
T1_Block_Delete (T1_Block * block)
{
  free (block->data);
  free (block);
}

/*
 * Not exported functions definition
 */

static BYTE
T1_Block_LRC (BYTE * data, unsigned length)
{
  BYTE lrc;
  unsigned i;

  lrc = 0x00;
  for (i = 0; i < length; i++)
    {
      lrc ^= data[i];
    }
    
  return lrc;       
}


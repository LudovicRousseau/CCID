/*
    protocol_t1.c
    Handling of ISO 7816 T=1 protocol
    
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

#include "defines.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "protocol_t1.h"
#include "t1_block.h"
#include "atr.h"
#include "pps.h"

#include "defs.h"
#include "ccid.h"
#include "commands.h"
#include "ifdhandler.h"

/*
 * Not exported constants definition
 */
#define PROTOCOL_T1_DEFAULT_IFSC        32
#define PROTOCOL_T1_DEFAULT_IFSD        32
#define PROTOCOL_T1_MAX_IFSC            251  /* Cannot send > 255 buffer */
#define PROTOCOL_T1_DEFAULT_CWI         13
#define PROTOCOL_T1_DEFAULT_BWI         4
#define PROTOCOL_T1_EDC_LRC             0
#define PROTOCOL_T1_EDC_CRC             1

/* #define DEBUG_PROTOCOL */

/*
 * Not exported functions declaration
 */

static void 
Protocol_T1_Clear (Protocol_T1 * t1);

static int
Protocol_T1_SendBlock (Protocol_T1 * t1, T1_Block * block);

static int
Protocol_T1_ReceiveBlock (Protocol_T1 * t1, T1_Block ** block);

/*
 * Exproted funtions definition
 */

int
Protocol_T1_Init (Protocol_T1 * t1, int lun)
{
  BYTE ta, tc;
  ATR atr;

  /* Set ICC */
  t1->lun = lun;

  /* Get ATR of the card */
  ATR_InitFromArray(&atr, get_ccid_slot(lun) -> pcATRBuffer,
    get_ccid_slot(lun) -> nATRLength);

  /* Set IFSC */
  if (ATR_GetInterfaceByte (&atr, 3, ATR_INTERFACE_BYTE_TA, &ta) == ATR_NOT_FOUND)
    t1->ifsc = PROTOCOL_T1_DEFAULT_IFSC;
  else if ((ta != 0x00) && (ta != 0xFF))
    t1->ifsc = ta;
  else
    t1->ifsc = PROTOCOL_T1_DEFAULT_IFSC;

  /* Towitoko does not allow IFSC > 251 */
  t1->ifsc = MIN (t1->ifsc, PROTOCOL_T1_MAX_IFSC);

  /* Set IFSD */
  t1->ifsd = PROTOCOL_T1_DEFAULT_IFSD;

  /* Set the error detection code type */
  if (ATR_GetInterfaceByte (&atr, 3, ATR_INTERFACE_BYTE_TC, &tc) == ATR_NOT_FOUND)
    t1->edc = PROTOCOL_T1_EDC_LRC;
  else
    t1->edc = tc & 0x01;

  /* Set initial send sequence (NS) */
  t1->ns = 1;
  
#ifdef DEBUG_PROTOCOL
  printf ("Protocol: T=1: IFSC=%d, IFSD=%d, EDC=%s\n",
     t1->ifsc, t1->ifsd, (t1->edc == PROTOCOL_T1_EDC_LRC) ? "LRC" : "CRC");
#endif

  return PROTOCOL_T1_OK;
}

int
Protocol_T1_Command (Protocol_T1 * t1, APDU_Cmd * cmd, APDU_Rsp * rsp)
{
  T1_Block *block;
  BYTE *buffer, rsp_type, bytes, nr, wtx;
  unsigned short counter;
  int ret;
  bool more;

  /* Calculate the number of bytes to send */
  counter = 0;
  bytes = MIN (cmd->length, t1->ifsc);

  /* See if chaining is needed */
  more = (cmd->length > t1->ifsc);

  /* Increment ns */
  t1->ns = (t1->ns + 1) %2;

  /* Create an I-Block */
  block = T1_Block_NewIBlock (bytes, cmd->command, t1->ns, more);

#ifdef DEBUG_PROTOCOL
  printf ("Sending block I(%d,%d)\n", t1->ns, more);
#endif

  /* Send a block */
  ret = Protocol_T1_SendBlock (t1, block);

  /* Delete I-block */
  T1_Block_Delete (block);

  while ((ret == PROTOCOL_T1_OK) && more)
    {
      /* Receive a block */
      ret = Protocol_T1_ReceiveBlock (t1, &block);

      if (ret == PROTOCOL_T1_OK)
        {
          rsp_type = T1_Block_GetType (block);

          /* Positive ACK R-Block received */
          if (rsp_type == T1_BLOCK_R_OK)
            {
#ifdef DEBUG_PROTOCOL
              printf ("Protocol: Received block R(%d)\n", T1_Block_GetNR (block));
#endif                   
              /* Delete block */
              T1_Block_Delete (block);
 
              /* Increment ns  */
              t1->ns = (t1->ns + 1) % 2;

              /* Calculate the number of bytes to send */
              counter += bytes;
              bytes = MIN (cmd->length - counter, t1->ifsc);

              /* See if chaining is needed */
              more = (cmd->length - counter > t1->ifsc);

              /* Create an I-Block */
              block =
                T1_Block_NewIBlock (bytes, cmd->command + counter,
                                    t1->ns, more);
#ifdef DEBUG_PROTOCOL
              printf ("Protocol: Sending block I(%d,%d)\n", t1->ns, more);
#endif
              /* Send a block */
              ret = Protocol_T1_SendBlock (t1, block);

              /* Delete I-block */
              T1_Block_Delete (block);
            }
                                   
          else
            {
              /* Delete block */
              T1_Block_Delete (block);

              ret = PROTOCOL_T1_NOT_IMPLEMENTED;
            }
        }

      else
        {
          ret = PROTOCOL_T1_NOT_IMPLEMENTED;
        }
    }

  /* Reset counter */
  buffer = NULL;
  counter = 0;      
  more = TRUE;
  wtx = 0;
      
  while ((ret == PROTOCOL_T1_OK) && more)
    {
      /* Receive a block */
      ret = Protocol_T1_ReceiveBlock (t1, &block);

      if (ret == PROTOCOL_T1_OK)
        {
          rsp_type = T1_Block_GetType (block);

          if (rsp_type == T1_BLOCK_I)
            {
#ifdef DEBUG_PROTOCOL
              printf ("Protocol: Received block I(%d,%d)\n", 
              T1_Block_GetNS(block), T1_Block_GetMore (block));
#endif
              /* Calculate nr */
              nr = (T1_Block_GetNS (block) + 1) % 2;
                               
              /* Save inf field */
              bytes = T1_Block_GetLen (block);
	      buffer = (BYTE *) realloc(buffer, counter + bytes);
              memcpy (buffer + counter, T1_Block_GetInf (block), bytes);
              counter += bytes;

              /* See if chaining is requested */
              more = T1_Block_GetMore (block);

              /* Delete block */
              T1_Block_Delete (block);

              if (more)
                {
                  /* Create an R-Block */
                  block = T1_Block_NewRBlock (T1_BLOCK_R_OK, nr);
#ifdef DEBUG_PROTOCOL
                  printf ("Protocol: Sending block R(%d)\n", nr);
#endif                    
                  /* Send R-Block */
                  ret = Protocol_T1_SendBlock (t1, block);

                  /* Delete I-block */
                  T1_Block_Delete (block);
                }
            }

          /* WTX Request S-Block received */ 
          else if (rsp_type == T1_BLOCK_S_WTX_REQ)
            {
              /* Get wtx multiplier */
              wtx = (*T1_Block_GetInf (block));
#ifdef DEBUG_PROTOCOL
              printf ("Protocol: Received block S(WTX request, %d)\n", wtx);
#endif                                  
              /* Delete block */
              T1_Block_Delete (block);
             
              /* Create an WTX response S-Block */
              block = T1_Block_NewSBlock (T1_BLOCK_S_WTX_RES, 1, &wtx);
#ifdef DEBUG_PROTOCOL
              printf ("Protocol: Sending block S(WTX response, %d)\n", wtx);
#endif                    
              /* Send WTX response */
              ret = Protocol_T1_SendBlock (t1, block);
                  
              /* Delete block */
              T1_Block_Delete (block);
            }

          else
            {
              ret = PROTOCOL_T1_NOT_IMPLEMENTED;
            }
        }
    }

  if (ret == PROTOCOL_T1_OK)
  {
    rsp -> response = buffer;
    rsp -> length = counter;
  }
  else
  {
    rsp -> response = NULL;
    rsp -> length = 0;
  }

  return ret;
}

int
Protocol_T1_Close (Protocol_T1 * t1)
{
  Protocol_T1_Clear (t1);

  return PROTOCOL_T1_OK;
}

/*
 * Not exported functions definition
 */

static int
Protocol_T1_SendBlock (Protocol_T1 * t1, T1_Block * block)
{
  /* Send T=1 block */
  if (CCID_Transmit(t1->lun, T1_Block_RawLen (block), T1_Block_Raw (block))
      != IFD_SUCCESS)
	  return PROTOCOL_T1_ICC_ERROR;

  return PROTOCOL_T1_OK;
}

static int
Protocol_T1_ReceiveBlock (Protocol_T1 * t1, T1_Block ** block)
{
  unsigned char cmd[T1_BLOCK_MAX_SIZE]; /* CCID + T1 block */
  int ret;
  int len = sizeof(cmd);

  /* Receive T=1 block */
  if (CCID_Receive(t1->lun, &len, cmd) != IFD_SUCCESS)
    {
      if (len >= ERROR_OFFSET)
        ccid_error(cmd[ERROR_OFFSET], __FILE__, __LINE__);	/* bError */
      ret = PROTOCOL_T1_ICC_ERROR;
      (*block) = NULL;
    }

  else
    {
      ret = PROTOCOL_T1_OK;
      (*block) = T1_Block_New (cmd, len);
    }

  return ret;
}

static void
Protocol_T1_Clear (Protocol_T1 * t1)
{
  t1->lun = 0;
  t1->ifsc = 0;
  t1->ifsd = 0;
  t1->edc = 0;
  t1->ns = 0;
}


/*****************************************************************
/
/ File   :   pcscdefines.h
/ Author :   David Corcoran <corcoran@linuxnet.com>
/ Date   :   June 15, 2000
/ Purpose:   This provides PC/SC shared defines.
/            See http://www.linuxnet.com for more information.
/ License:   See file LICENSE.BSD
/
/ $Id$
/
******************************************************************/

#ifndef _pcscdefines_h_
#define _pcscdefines_h_

#ifdef __cplusplus
extern "C" {
#endif 

/* Defines a list of pseudo types. */

/* these types are also defined in wintypes.h */
#ifndef __wintypes_h__
  typedef unsigned long      DWORD;
  typedef unsigned long*     PDWORD;
  typedef unsigned char      UCHAR;
  typedef unsigned char*     PUCHAR;
  typedef char*              LPSTR;
  typedef long               RESPONSECODE;
  typedef void               VOID;
#endif

  // do not use RESPONSECODE (long, 64 bits) when 32 bits are enough
  typedef int ifd_t;

  typedef enum {
    STATUS_SUCCESS               = 0xFA,
    STATUS_UNSUCCESSFUL          = 0xFB,
    STATUS_COMM_ERROR            = 0xFC,
    STATUS_DEVICE_PROTOCOL_ERROR = 0xFD
  } status_t;

  #define MAX_RESPONSE_SIZE  264
  #define MAX_ATR_SIZE       33
  #define PCSCLITE_MAX_CHANNELS           16      /* Maximum channels     */
#ifdef __cplusplus
}
#endif

#endif /* _pcscdefines_h_ */

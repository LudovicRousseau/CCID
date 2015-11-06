/*
    proto-t1.h: header file for proto-t1.c
    Copyright (C) 2004   Ludovic Rousseau

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

#ifndef __PROTO_T1_H__
#define __PROTO_T1_H__

#include <config.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "buffer.h"

/* T=1 protocol constants */
#define T1_I_BLOCK		0x00
#define T1_R_BLOCK		0x80
#define T1_S_BLOCK		0xC0
#define T1_MORE_BLOCKS		0x20

enum {
	IFD_PROTOCOL_RECV_TIMEOUT = 0x0000,
	IFD_PROTOCOL_T1_BLOCKSIZE,
	IFD_PROTOCOL_T1_CHECKSUM_CRC,
	IFD_PROTOCOL_T1_CHECKSUM_LRC,
	IFD_PROTOCOL_T1_IFSC,
	IFD_PROTOCOL_T1_IFSD,
	IFD_PROTOCOL_T1_STATE,
	IFD_PROTOCOL_T1_MORE
};

#define T1_BUFFER_SIZE		(3 + 254 + 2)

/* see /usr/include/PCSC/ifdhandler.h for other values
 * this one is for internal use only */
#define IFD_PARITY_ERROR 699

typedef struct {
	int		lun;
	int		state;

	unsigned char	ns;	/* reader side */
	unsigned char	nr;	/* card side */
	unsigned int	ifsc;
	unsigned int	ifsd;

	unsigned char	wtx;
	unsigned int	retries;
	unsigned int	rc_bytes;

	unsigned int	(*checksum)(const uint8_t *, size_t, unsigned char *);

	char			more;	/* more data bit */
	unsigned char	previous_block[4];	/* to store the last R-block */
} t1_state_t;

int t1_transceive(t1_state_t *t1, unsigned int dad,
		const void *snd_buf, size_t snd_len,
		void *rcv_buf, size_t rcv_len);
int t1_init(t1_state_t *t1, int lun);
void t1_release(t1_state_t *t1);
int t1_set_param(t1_state_t *t1, int type, long value);
int t1_negotiate_ifsd(t1_state_t *t1, unsigned int dad, int ifsd);
unsigned int t1_build(t1_state_t *, unsigned char *,
	unsigned char, unsigned char, ct_buf_t *, size_t *);

#endif


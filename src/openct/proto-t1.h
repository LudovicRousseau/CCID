/*
    proto-t1.h: header file for proto-t1.c
    Copyright (C) 2004   Ludovic Rousseau

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* $Id$ */

#ifndef __PROTO_T1_H__
#define __PROTO_T1_H__

#include <unistd.h>

enum {
	IFD_PROTOCOL_RECV_TIMEOUT = 0x0000,
	IFD_PROTOCOL_T1_BLOCKSIZE,
	IFD_PROTOCOL_T1_CHECKSUM_CRC,
	IFD_PROTOCOL_T1_CHECKSUM_LRC,
	IFD_PROTOCOL_T1_IFSC,
	IFD_PROTOCOL_T1_IFSD,
	IFD_PROTOCOL_STATE,
	IFD_PROTOCOL_MORE
};

#define T1_BUFFER_SIZE		(3 + 254 + 2)

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

	unsigned int	(*checksum)(const unsigned char *,
					size_t, unsigned char *);

	char			more;	/* more data bit */
	unsigned char	previous_block[4];	/* to store the last R-block */
} t1_state_t;

int t1_transceive(t1_state_t *t1, int dad,
		const void *snd_buf, size_t snd_len,
		void *rcv_buf, size_t rcv_len);
int t1_init(t1_state_t *t1);
void t1_release(t1_state_t *t1);
int t1_set_param(t1_state_t *t1, int type, long value);
int t1_negociate_ifsd(t1_state_t *t1, int dad, int ifsd);

#endif


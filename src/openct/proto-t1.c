/*
 * Implementation of T=1
 *
 * Copyright (C) 2003, Olaf Kirch <okir@suse.de>
 *
 * improvements by:
 * Copyright (C) 2004 Ludovic Rousseau <ludovic.rousseau@free.fr>
 */

#include <pcsclite.h>
#include <ifdhandler.h>
#include "commands.h"
#include "buffer.h"
#include "debug.h"
#include "proto-t1.h"
#include "checksum.h"

#include "ccid.h"

#include <sys/poll.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/* I block */
#define T1_I_SEQ_SHIFT		6

/* R block */
#define T1_IS_ERROR(pcb)	((pcb) & 0x0F)
#define T1_EDC_ERROR		0x01
#define T1_OTHER_ERROR		0x02
#define T1_R_SEQ_SHIFT		4

/* S block stuff */
#define T1_S_IS_RESPONSE(pcb)	((pcb) & T1_S_RESPONSE)
#define T1_S_TYPE(pcb)		((pcb) & 0x0F)
#define T1_S_RESPONSE		0x20
#define T1_S_RESYNC		0x00
#define T1_S_IFS		0x01
#define T1_S_ABORT		0x02
#define T1_S_WTX		0x03

#define swap_nibbles(x) ( (x >> 4) | ((x & 0xF) << 4) )

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define NAD 0
#define PCB 1
#define LEN 2
#define DATA 3

/* internal state, do not mess with it. */
/* should be != DEAD after reset/init */
enum {
	SENDING, RECEIVING, RESYNCH, DEAD
};

static void t1_set_checksum(t1_state_t *, int);
static unsigned int t1_block_type(unsigned char);
static unsigned int t1_seq(unsigned char);
static unsigned int t1_rebuild(t1_state_t *t1, unsigned char *block);
static unsigned int t1_compute_checksum(t1_state_t *, unsigned char *, size_t);
static int t1_verify_checksum(t1_state_t *, unsigned char *, size_t);
static int t1_xcv(t1_state_t *, unsigned char *, size_t, size_t);

/*
 * Set default T=1 protocol parameters
 */
static void t1_set_defaults(t1_state_t * t1)
{
	t1->retries = 3;
	/* This timeout is rather insane, but we need this right now
	 * to support cryptoflex keygen */
	t1->ifsc = 32;
	t1->ifsd = 32;
	t1->nr = 0;
	t1->ns = 0;
	t1->wtx = 0;
}

static void t1_set_checksum(t1_state_t * t1, int csum)
{
	switch (csum) {
	case IFD_PROTOCOL_T1_CHECKSUM_LRC:
		t1->rc_bytes = 1;
		t1->checksum = csum_lrc_compute;
		break;
	case IFD_PROTOCOL_T1_CHECKSUM_CRC:
		t1->rc_bytes = 2;
		t1->checksum = csum_crc_compute;
		break;
	}
}

/*
 * Attach t1 protocol
 */
int t1_init(t1_state_t * t1, int lun)
{
	t1_set_defaults(t1);
	t1_set_param(t1, IFD_PROTOCOL_T1_CHECKSUM_LRC, 0);
	t1_set_param(t1, IFD_PROTOCOL_T1_STATE, SENDING);
	t1_set_param(t1, IFD_PROTOCOL_T1_MORE, FALSE);

	t1->lun = lun;

	return 0;
}

/*
 * Detach t1 protocol
 */
void t1_release(/*@unused@*/ t1_state_t * t1)
{
	/* NOP */
}

/*
 * Get/set parmaters for T1 protocol
 */
int t1_set_param(t1_state_t * t1, int type, long value)
{
	switch (type) {
	case IFD_PROTOCOL_T1_CHECKSUM_LRC:
	case IFD_PROTOCOL_T1_CHECKSUM_CRC:
		t1_set_checksum(t1, type);
		break;
	case IFD_PROTOCOL_T1_IFSC:
		t1->ifsc = value;
		break;
	case IFD_PROTOCOL_T1_IFSD:
		t1->ifsd = value;
		break;
	case IFD_PROTOCOL_T1_STATE:
		t1->state = value;
		break;
	case IFD_PROTOCOL_T1_MORE:
		t1->more = value;
		break;
	default:
		DEBUG_INFO2("Unsupported parameter %d", type);
		return -1;
	}

	return 0;
}

/*
 * Send an APDU through T=1
 */
int t1_transceive(t1_state_t * t1, unsigned int dad,
		const void *snd_buf, size_t snd_len,
		void *rcv_buf, size_t rcv_len)
{
	ct_buf_t sbuf, rbuf, tbuf;
	unsigned char sdata[T1_BUFFER_SIZE], sblk[5];
	unsigned int slen, retries, resyncs, sent_length = 0;
	size_t last_send = 0;

	if (snd_len == 0)
		return -1;

	/* we can't talk to a dead card / reader. Reset it! */
	if (t1->state == DEAD)
	{
		DEBUG_CRITICAL("T=1 state machine is DEAD. Reset the card first.");
		return -1;
	}

	t1->state = SENDING;
	retries = t1->retries;
	resyncs = 3;

	/* Initialize send/recv buffer */
	ct_buf_set(&sbuf, (void *)snd_buf, snd_len);
	ct_buf_init(&rbuf, rcv_buf, rcv_len);

	/* Send the first block */
	slen = t1_build(t1, sdata, dad, T1_I_BLOCK, &sbuf, &last_send);

	while (1) {
		unsigned char pcb;
		int n;

		retries--;

		n = t1_xcv(t1, sdata, slen, sizeof(sdata));
		if (-2 == n)
		{
			DEBUG_COMM("Parity error");
			/* ISO 7816-3 Rule 7.4.2 */
			if (retries == 0)
				goto resync;

			/* ISO 7816-3 Rule 7.2 */
			if (T1_R_BLOCK == t1_block_type(t1->previous_block[PCB]))
			{
				DEBUG_COMM("Rule 7.2");
				slen = t1_rebuild(t1, sdata);
				continue;
			}

			slen = t1_build(t1, sdata,
					dad, T1_R_BLOCK | T1_EDC_ERROR,
					NULL, NULL);
			continue;
		}

		if (n < 0) {
			DEBUG_CRITICAL("fatal: transmit/receive failed");
			t1->state = DEAD;
			goto error;
		}

		if ((sdata[NAD] != swap_nibbles(dad)) /* wrong NAD */
			|| (sdata[LEN] == 0xFF))	/* length == 0xFF (illegal) */
		{
			DEBUG_COMM("R-BLOCK required");
			/* ISO 7816-3 Rule 7.4.2 */
			if (retries == 0)
				goto resync;

			/* ISO 7816-3 Rule 7.2 */
			if (T1_R_BLOCK == t1_block_type(t1->previous_block[PCB]))
			{
				DEBUG_COMM("Rule 7.2");
				slen = t1_rebuild(t1, sdata);
				continue;
			}

			slen = t1_build(t1, sdata,
				dad, T1_R_BLOCK | T1_OTHER_ERROR,
				NULL, NULL);
			continue;
		}

		if (!t1_verify_checksum(t1, sdata, n)) {
			DEBUG_COMM("checksum failed");
			/* ISO 7816-3 Rule 7.4.2 */
			if (retries == 0)
				goto resync;

			/* ISO 7816-3 Rule 7.2 */
			if (T1_R_BLOCK == t1_block_type(t1->previous_block[PCB]))
			{
				DEBUG_COMM("Rule 7.2");
				slen = t1_rebuild(t1, sdata);
				continue;
			}

			slen = t1_build(t1, sdata,
				dad, T1_R_BLOCK | T1_EDC_ERROR,
				NULL, NULL);
			continue;
		}

		pcb = sdata[PCB];
		switch (t1_block_type(pcb)) {
		case T1_R_BLOCK:
			if ((sdata[LEN] != 0x00)	/* length != 0x00 (illegal) */
				|| (pcb & 0x20)			/* b6 of pcb is set */
			   )
			{
				DEBUG_COMM("R-Block required");
				/* ISO 7816-3 Rule 7.4.2 */
				if (retries == 0)
					goto resync;

				/* ISO 7816-3 Rule 7.2 */
				if (T1_R_BLOCK == t1_block_type(t1->previous_block[1]))
				{
					DEBUG_COMM("Rule 7.2");
					slen = t1_rebuild(t1, sdata);
					continue;
				}

				slen = t1_build(t1, sdata,
						dad, T1_R_BLOCK | T1_OTHER_ERROR,
						NULL, NULL);
				continue;
			}

			if (((t1_seq(pcb) != t1->ns)	/* wrong sequence number & no bit more */
					&& ! t1->more)
			   )
			{
				DEBUG_COMM4("received: %d, expected: %d, more: %d",
					t1_seq(pcb), t1->ns, t1->more);

				/* ISO 7816-3 Rule 7.2 */
				if (T1_R_BLOCK == t1_block_type(t1->previous_block[PCB]))
				{
					DEBUG_COMM("Rule 7.2");
					slen = t1_rebuild(t1, sdata);
					continue;
				}

				DEBUG_COMM("R-Block required");
				/* ISO 7816-3 Rule 7.4.2 */
				if (retries == 0)
					goto resync;
				slen = t1_build(t1, sdata,
						dad, T1_R_BLOCK | T1_OTHER_ERROR,
						NULL, NULL);
				continue;
			}

			if (t1->state == RECEIVING) {
				/* ISO 7816-3 Rule 7.2 */
				if (T1_R_BLOCK == t1_block_type(t1->previous_block[1]))
				{
					DEBUG_COMM("Rule 7.2");
					slen = t1_rebuild(t1, sdata);
					continue;
				}

				DEBUG_COMM("");
				slen = t1_build(t1, sdata,
						dad, T1_R_BLOCK,
						NULL, NULL);
				break;
			}

			/* If the card terminal requests the next
			 * sequence number, it received the previous
			 * block successfully */
			if (t1_seq(pcb) != t1->ns) {
				ct_buf_get(&sbuf, NULL, last_send);
				sent_length += last_send;
				last_send = 0;
				t1->ns ^= 1;
			}

			/* If there's no data available, the ICC
			 * shouldn't be asking for more */
			if (ct_buf_avail(&sbuf) == 0)
				goto resync;

			slen = t1_build(t1, sdata, dad, T1_I_BLOCK,
					&sbuf, &last_send);
			break;

		case T1_I_BLOCK:
			/* The first I-block sent by the ICC indicates
			 * the last block we sent was received successfully. */
			if (t1->state == SENDING) {
				DEBUG_COMM("");
				ct_buf_get(&sbuf, NULL, last_send);
				last_send = 0;
				t1->ns ^= 1;
			}

			t1->state = RECEIVING;

			/* If the block sent by the card doesn't match
			 * what we expected it to send, reply with
			 * an R block */
			if (t1_seq(pcb) != t1->nr) {
				DEBUG_COMM("wrong nr");
				slen = t1_build(t1, sdata, dad,
						T1_R_BLOCK | T1_OTHER_ERROR,
						NULL, NULL);
				continue;
			}

			t1->nr ^= 1;

			if (ct_buf_put(&rbuf, sdata + 3, sdata[LEN]) < 0)
			{
				DEBUG_CRITICAL2("buffer overrun by %d bytes", sdata[LEN] - (rbuf.size - rbuf.tail));
				goto error;
			}

			if ((pcb & T1_MORE_BLOCKS) == 0)
				goto done;

			slen = t1_build(t1, sdata, dad, T1_R_BLOCK, NULL, NULL);
			break;

		case T1_S_BLOCK:
			if (T1_S_IS_RESPONSE(pcb) && t1->state == RESYNCH) {
				/* ISO 7816-3 Rule 6.2 */
				DEBUG_COMM("S-Block answer received");
				/* ISO 7816-3 Rule 6.3 */
				t1->state = SENDING;
				sent_length = 0;
				last_send = 0;
				resyncs = 3;
				retries = t1->retries;
				ct_buf_init(&rbuf, rcv_buf, rcv_len);
				slen = t1_build(t1, sdata, dad, T1_I_BLOCK,
						&sbuf, &last_send);
				continue;
			}

			if (T1_S_IS_RESPONSE(pcb))
			{
				/* ISO 7816-3 Rule 7.4.2 */
				if (retries == 0)
					goto resync;

				/* ISO 7816-3 Rule 7.2 */
				if (T1_R_BLOCK == t1_block_type(t1->previous_block[PCB]))
				{
					DEBUG_COMM("Rule 7.2");
					slen = t1_rebuild(t1, sdata);
					continue;
				}

				DEBUG_CRITICAL("wrong response S-BLOCK received");
				slen = t1_build(t1, sdata,
						dad, T1_R_BLOCK | T1_OTHER_ERROR,
						NULL, NULL);
				continue;
			}

			ct_buf_init(&tbuf, sblk, sizeof(sblk));

			DEBUG_COMM("S-Block request received");
			switch (T1_S_TYPE(pcb)) {
			case T1_S_RESYNC:
				if (sdata[LEN] != 0)
				{
					DEBUG_COMM2("Wrong length: %d", sdata[LEN]);
					slen = t1_build(t1, sdata, dad,
						T1_R_BLOCK | T1_OTHER_ERROR,
						NULL, NULL);
					continue;
				}

				DEBUG_COMM("Resync requested");
				/* the card is not allowed to send a resync. */
				goto resync;

			case T1_S_ABORT:
				if (sdata[LEN] != 0)
				{
					DEBUG_COMM2("Wrong length: %d", sdata[LEN]);
					slen = t1_build(t1, sdata, dad,
						T1_R_BLOCK | T1_OTHER_ERROR,
						NULL, NULL);
					continue;
				}

				/* ISO 7816-3 Rule 9 */
				DEBUG_CRITICAL("abort requested");
				break;

			case T1_S_IFS:
				if (sdata[LEN] != 1)
				{
					DEBUG_COMM2("Wrong length: %d", sdata[LEN]);
					slen = t1_build(t1, sdata, dad,
						T1_R_BLOCK | T1_OTHER_ERROR,
						NULL, NULL);
					continue;
				}

				DEBUG_CRITICAL2("CT sent S-block with ifs=%u", sdata[DATA]);
				if (sdata[DATA] == 0)
					goto resync;
				t1->ifsc = sdata[DATA];
				ct_buf_putc(&tbuf, sdata[DATA]);
				break;

			case T1_S_WTX:
				if (sdata[LEN] != 1)
				{
					DEBUG_COMM2("Wrong length: %d", sdata[LEN]);
					slen = t1_build(t1, sdata, dad,
						T1_R_BLOCK | T1_OTHER_ERROR,
						NULL, NULL);
					continue;
				}

				DEBUG_COMM2("CT sent S-block with wtx=%u", sdata[DATA]);
				t1->wtx = sdata[DATA];
				ct_buf_putc(&tbuf, sdata[DATA]);
				break;

			default:
				DEBUG_CRITICAL2("T=1: Unknown S block type 0x%02x", T1_S_TYPE(pcb));
				goto resync;
			}

			slen = t1_build(t1, sdata, dad,
				T1_S_BLOCK | T1_S_RESPONSE | T1_S_TYPE(pcb),
				&tbuf, NULL);
		}

		/* Everything went just splendid */
		retries = t1->retries;
		continue;

resync:
		/* the number or resyncs is limited, too */
		/* ISO 7816-3 Rule 6.4 */
		if (resyncs == 0)
			goto error;

		/* ISO 7816-3 Rule 6 */
		resyncs--;
		t1->ns = 0;
		t1->nr = 0;
		slen = t1_build(t1, sdata, dad, T1_S_BLOCK | T1_S_RESYNC, NULL,
				NULL);
		t1->state = RESYNCH;
		t1->more = FALSE;
		retries = 1;
		continue;
	}

done:
	return ct_buf_avail(&rbuf);

error:
	t1->state = DEAD;
	return -1;
}

static unsigned t1_block_type(unsigned char pcb)
{
	switch (pcb & 0xC0) {
	case T1_R_BLOCK:
		return T1_R_BLOCK;
	case T1_S_BLOCK:
		return T1_S_BLOCK;
	default:
		return T1_I_BLOCK;
	}
}

static unsigned int t1_seq(unsigned char pcb)
{
	switch (pcb & 0xC0) {
	case T1_R_BLOCK:
		return (pcb >> T1_R_SEQ_SHIFT) & 1;
	case T1_S_BLOCK:
		return 0;
	default:
		return (pcb >> T1_I_SEQ_SHIFT) & 1;
	}
}

unsigned int t1_build(t1_state_t * t1, unsigned char *block,
	unsigned char dad, unsigned char pcb,
	ct_buf_t *bp, size_t *lenp)
{
	unsigned int len;
	char more = FALSE;

	len = bp ? ct_buf_avail(bp) : 0;
	if (len > t1->ifsc) {
		pcb |= T1_MORE_BLOCKS;
		len = t1->ifsc;
		more = TRUE;
	}

	/* Add the sequence number */
	switch (t1_block_type(pcb)) {
	case T1_R_BLOCK:
		pcb |= t1->nr << T1_R_SEQ_SHIFT;
		break;
	case T1_I_BLOCK:
		pcb |= t1->ns << T1_I_SEQ_SHIFT;
		t1->more = more;
		DEBUG_COMM2("more bit: %d", more);
		break;
	}

	block[0] = dad;
	block[1] = pcb;
	block[2] = len;

	if (len)
		memcpy(block + 3, ct_buf_head(bp), len);
	if (lenp)
		*lenp = len;

	len = t1_compute_checksum(t1, block, len + 3);

	/* memorize the last sent block */
	/* only 4 bytes since we are only interesed in R-blocks */
	memcpy(t1->previous_block, block, 4);

	return len;
}

static unsigned int
t1_rebuild(t1_state_t *t1, unsigned char *block)
{
	unsigned char pcb = t1 -> previous_block[1];

	/* copy the last sent block */
	if (T1_R_BLOCK == t1_block_type(pcb))
		memcpy(block, t1 -> previous_block, 4);
	else
	{
		DEBUG_CRITICAL2("previous block was not R-Block: %02X", pcb);
		return 0;
	}

	return 4;
}

/*
 * Build/verify checksum
 */
static unsigned int t1_compute_checksum(t1_state_t * t1,
	unsigned char *data, size_t len)
{
	return len + t1->checksum(data, len, data + len);
}

static int t1_verify_checksum(t1_state_t * t1, unsigned char *rbuf,
	size_t len)
{
	unsigned char csum[2];
	int m, n;

	m = len - t1->rc_bytes;
	n = t1->rc_bytes;

	if (m < 0)
		return 0;

	t1->checksum(rbuf, m, csum);
	if (!memcmp(rbuf + m, csum, n))
		return 1;

	return 0;
}

/*
 * Send/receive block
 */
static int t1_xcv(t1_state_t * t1, unsigned char *block, size_t slen,
	size_t rmax)
{
	int n, m;
	_ccid_descriptor *ccid_desc ;
	int oldReadTimeout;
	unsigned int rmax_int;

	DEBUG_XXD("sending: ", block, slen);

	ccid_desc = get_ccid_descriptor(t1->lun);
	oldReadTimeout = ccid_desc->readTimeout;

	if (t1->wtx > 1)
	{
		/* set the new temporary timeout at WTX card request */
		ccid_desc->readTimeout *=  t1->wtx;
		DEBUG_INFO2("New timeout at WTX request: %d sec",
			ccid_desc->readTimeout);
	}

	if (isCharLevel(t1->lun))
	{
		rmax = 3;

		n = CCID_Transmit(t1 -> lun, slen, block, rmax, t1->wtx);
		if (n != IFD_SUCCESS)
			return n;

		/* the second argument of CCID_Receive() is (unsigned int *)
		 * so we can't use &rmax since &rmax is a (size_t *) and may not
		 * be the same on 64-bits architectures for example (iMac G5) */
		rmax_int = rmax;
		n = CCID_Receive(t1 -> lun, &rmax_int, block, NULL);
		rmax = rmax_int;

		if (n == IFD_PARITY_ERROR)
			return -2;
		if (n != IFD_SUCCESS)
			return -1;

		rmax = block[2] + 1;

		n = CCID_Transmit(t1 -> lun, 0, block, rmax, t1->wtx);
		if (n != IFD_SUCCESS)
			return n;

		rmax_int = rmax;
		n = CCID_Receive(t1 -> lun, &rmax_int, &block[3], NULL);
		rmax = rmax_int;
		if (n == IFD_PARITY_ERROR)
			return -2;
		if (n != IFD_SUCCESS)
			return -1;

		n = rmax + 3;
	}
	else
	{
		n = CCID_Transmit(t1 -> lun, slen, block, 0, t1->wtx);
		t1->wtx = 0;	/* reset to default value */
		if (n != IFD_SUCCESS)
			return n;

		/* Get the response en bloc */
		rmax_int = rmax;
		n = CCID_Receive(t1 -> lun, &rmax_int, block, NULL);
		rmax = rmax_int;
		if (n == IFD_PARITY_ERROR)
			return -2;
		if (n != IFD_SUCCESS)
			return -1;

		n = rmax;
	}

	if (n >= 0)
	{
		m = block[2] + 3 + t1->rc_bytes;
		if (m < n)
			n = m;
	}

	if (n >= 0)
		DEBUG_XXD("received: ", block, n);

	/* Restore initial timeout */
	ccid_desc->readTimeout = oldReadTimeout;

	return n;
}

int t1_negotiate_ifsd(t1_state_t * t1, unsigned int dad, int ifsd)
{
	ct_buf_t sbuf;
	unsigned char sdata[T1_BUFFER_SIZE];
	unsigned int slen;
	unsigned int retries;
	size_t snd_len;
	int n;
	unsigned char snd_buf[1];

	retries = t1->retries;

	/* S-block IFSD request */
	snd_buf[0] = ifsd;
	snd_len = 1;

	/* Initialize send/recv buffer */
	ct_buf_set(&sbuf, (void *)snd_buf, snd_len);

	while (TRUE)
	{
		/* Build the block */
		slen = t1_build(t1, sdata, 0, T1_S_BLOCK | T1_S_IFS, &sbuf, NULL);

		/* Send the block */
		n = t1_xcv(t1, sdata, slen, sizeof(sdata));

		retries--;
		/* ISO 7816-3 Rule 7.4.2 */
		if (retries == 0)
			goto error;

		if (-1 == n)
		{
			DEBUG_CRITICAL("fatal: transmit/receive failed");
			goto error;
		}

		if ((-2 == n)								/* Parity error */
			|| (sdata[DATA] != ifsd)				/* Wrong ifsd received */
			|| (sdata[NAD] != swap_nibbles(dad))	/* wrong NAD */
			|| (!t1_verify_checksum(t1, sdata, n))	/* checksum failed */
			|| (n != 4 + t1->rc_bytes)				/* wrong frame length */
			|| (sdata[LEN] != 1)					/* wrong data length */
			|| (sdata[PCB] != (T1_S_BLOCK | T1_S_RESPONSE | T1_S_IFS))) /* wrong PCB */
			continue;

		/* no more error */
		goto done;
	}

done:
	return n;

error:
	t1->state = DEAD;
	return -1;
}

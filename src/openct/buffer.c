/*
 * Buffer handling functions
 *
 * Copyright (C) 2003, Olaf Kirch <okir@suse.de>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <openct/buffer.h>

void
ct_buf_init(ct_buf_t *bp, void *mem, size_t len)
{
	memset(bp, 0, sizeof(*bp));
	bp->base = (unsigned char *) mem;
	bp->size = len;
}

void
ct_buf_set(ct_buf_t *bp, void *mem, size_t len)
{
	ct_buf_init(bp, mem, len);
	bp->tail = len;
}

int
ct_buf_get(ct_buf_t *bp, void *mem, size_t len)
{
	if (len > bp->tail - bp->head)
		return -1;
	if (mem)
		memcpy(mem, bp->base + bp->head, len);
	bp->head += len;
	return len;
}

int
ct_buf_put(ct_buf_t *bp, const void *mem, size_t len)
{
	if (len > bp->size - bp->tail) {
		bp->overrun = 1;
		return -1;
	}
	if (mem)
		memcpy(bp->base + bp->tail, mem, len);
	bp->tail += len;
	return len;
}

int
ct_buf_putc(ct_buf_t *bp, int byte)
{
	unsigned char	c = byte;

	return ct_buf_put(bp, &c, 1);
}

unsigned int
ct_buf_avail(ct_buf_t *bp)
{
	return bp->tail - bp->head;
}

void *
ct_buf_head(ct_buf_t *bp)
{
	return bp->base + bp->head;
}


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

void
ct_buf_clear(ct_buf_t *bp)
{
	bp->head = bp->tail = 0;
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
ct_buf_gets(ct_buf_t *bp, char *buffer, size_t size)
{
	unsigned int	n, avail;
	unsigned char	*s;

	size -= 1; /* room for NUL byte */

	/* Limit string to what we have */
	avail = bp->tail - bp->head;
	if (size > avail)
		size = avail;

	/* Look for newline */
	s = bp->base + bp->head;
	for (n = 0; n < size && s[n] != '\n'; n++)
		;

	/* Copy string (excluding newline) */
	memcpy(buffer, s, n);
	buffer[n] = '\0';

	/* And eat any characters that weren't copied
	 * (including the newline)
	 */
	while (n < avail && s[n++] != '\n')
		;
	
	bp->head += n;
	return 0;
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

int
ct_buf_puts(ct_buf_t *bp, const char *string)
{
	return ct_buf_put(bp, string, strlen(string));
}

unsigned int
ct_buf_avail(ct_buf_t *bp)
{
	return bp->tail - bp->head;
}

unsigned int
ct_buf_tailroom(ct_buf_t *bp)
{
	return bp->size - bp->tail;
}

unsigned int
ct_buf_size(ct_buf_t *bp)
{
	return bp->size;
}

void *
ct_buf_head(ct_buf_t *bp)
{
	return bp->base + bp->head;
}

void *
ct_buf_tail(ct_buf_t *bp)
{
	return bp->base + bp->tail;
}

int
ct_buf_read(ct_buf_t *bp, int fd)
{
	unsigned int	count;
	int		n;

	ct_buf_compact(bp);

	count = bp->size - bp->tail;
	if ((n = read(fd, bp->base + bp->tail, count)) < 0)
		return -1;
	bp->tail += n;
	return 0;
}

void
ct_buf_compact(ct_buf_t *bp)
{
	unsigned int	count;

	if (bp->head == 0)
		return;

	count = bp->tail - bp->head;
	memmove(bp->base, bp->base + bp->head, count);
	bp->tail -= bp->head;
	bp->head  = 0;
}

int
ct_buf_overrun(ct_buf_t *bp)
{
	return bp->overrun;
}

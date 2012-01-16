/*
    debug.c: log (or not) messages
    Copyright (C) 2003-2011   Ludovic Rousseau

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

/*
 * $Id$
 */


#include "config.h"
#include "debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "strlcpycat.h"

#undef LOG_TO_STDERR

#ifdef LOG_TO_STDERR
#define LOG_STREAM stderr
#else
#define LOG_STREAM stdout
#endif

void log_msg(const int priority, const char *fmt, ...)
{
	char debug_buffer[160]; /* up to 2 lines of 80 characters */
	va_list argptr;

	(void)priority;

	va_start(argptr, fmt);
	(void)vsnprintf(debug_buffer, sizeof debug_buffer, fmt, argptr);
	va_end(argptr);

	(void)fprintf(LOG_STREAM, "%s\n", debug_buffer);
	fflush(LOG_STREAM);
} /* log_msg */

void log_xxd(const int priority, const char *msg, const unsigned char *buffer,
	const int len)
{
	int i;
	char *c, debug_buffer[len*3 + strlen(msg) +1];
	size_t l;

	(void)priority;

	l = strlcpy(debug_buffer, msg, sizeof debug_buffer);
	c = debug_buffer + l;

	for (i = 0; i < len; ++i)
	{
		/* 2 hex characters, 1 space, 1 NUL : total 4 characters */
		(void)snprintf(c, 4, "%02X ", buffer[i]);
		c += 3;
	}

	(void)fprintf(LOG_STREAM, "%s\n", debug_buffer);
	fflush(LOG_STREAM);
} /* log_xxd */

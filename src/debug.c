/*
    debug.c: log (or not) messages
    Copyright (C) 2003-2004   Ludovic Rousseau

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

/*
 * $Id$
 */


#include "config.h"
#include "debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define DEBUG_BUF_SIZE ((256+20)*3+10)

static char DebugBuffer[DEBUG_BUF_SIZE];

void debug_msg(char *fmt, ...)
{
	va_list argptr;

	va_start(argptr, fmt);
	vsnprintf(DebugBuffer, DEBUG_BUF_SIZE, fmt, argptr);
	va_end(argptr);

	fprintf(stderr, "%s\n", DebugBuffer);
} /* debug_msg */

void debug_xxd(const char *msg, const unsigned char *buffer, const int len)
{
	int i;
	unsigned char *c, *debug_buf_end;

	debug_buf_end = DebugBuffer + DEBUG_BUF_SIZE - 5;

	strncpy(DebugBuffer, msg, sizeof(DebugBuffer)-1);
	c = DebugBuffer + strlen(DebugBuffer);

	for (i = 0; (i < len) && (c < debug_buf_end); ++i)
	{
		sprintf(c, "%02X ", buffer[i]);
		c += strlen(c);
	}

	if (c >= debug_buf_end)
		fprintf(stderr, "Debug buffer too short\n");

	fprintf(stderr, "%s\n", DebugBuffer);
} /* debug_xxd */


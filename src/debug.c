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

#include <config.h>
#include "misc.h"
#include "debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <stdlib.h>

#ifdef USE_SYSLOG
#include <syslog.h>
#endif

#include "strlcpycat.h"

#undef LOG_TO_STDERR

#ifdef LOG_TO_STDERR
#define LOG_STREAM stderr
#else
#define LOG_STREAM stdout
#endif

void log_msg(const int priority, const char *fmt, ...)
{
	char debug_buffer[3 * 80]; /* up to 3 lines of 80 characters */
	va_list argptr;
	static struct timeval last_time = { 0, 0 };
	struct timeval new_time = { 0, 0 };
	struct timeval tmp;
	int delta;
#ifdef USE_SYSLOG
	int syslog_level;

	switch(priority)
	{
		case PCSC_LOG_CRITICAL:
			syslog_level = LOG_CRIT;
			break;
		case PCSC_LOG_ERROR:
			syslog_level = LOG_ERR;
			break;
		case PCSC_LOG_INFO:
			syslog_level = LOG_INFO;
			break;
		default:
			syslog_level = LOG_DEBUG;
	}
#else
	const char *color_pfx = "", *color_sfx = "";
	const char *time_pfx = "", *time_sfx = "";
	static int initialized = 0;
	static int LogDoColor = 0;

	if (!initialized)
	{
		char *term;

		initialized = 1;
		term = getenv("TERM");
		if (term)
		{
			const char *terms[] = { "linux", "xterm", "xterm-color", "Eterm", "rxvt", "rxvt-unicode", "xterm-256color" };
			unsigned int i;

			/* for each known color terminal */
			for (i = 0; i < COUNT_OF(terms); i++)
			{
				/* we found a supported term? */
				if (0 == strcmp(terms[i], term))
				{
					LogDoColor = 1;
					break;
				}
			}
		}
	}

	if (LogDoColor)
	{
		color_sfx = "\33[0m";
		time_sfx = color_sfx;
		time_pfx = "\33[36m"; /* Cyan */

		switch (priority)
		{
			case PCSC_LOG_CRITICAL:
				color_pfx = "\33[01;31m"; /* bright + Red */
				break;

			case PCSC_LOG_ERROR:
				color_pfx = "\33[35m"; /* Magenta */
				break;

			case PCSC_LOG_INFO:
				color_pfx = "\33[34m"; /* Blue */
				break;

			case PCSC_LOG_DEBUG:
				color_pfx = ""; /* normal (black) */
				color_sfx = "";
				break;
		}
	}
#endif

	gettimeofday(&new_time, NULL);
	if (0 == last_time.tv_sec)
		last_time = new_time;

	tmp.tv_sec = new_time.tv_sec - last_time.tv_sec;
	tmp.tv_usec = new_time.tv_usec - last_time.tv_usec;
	if (tmp.tv_usec < 0)
	{
		tmp.tv_sec--;
		tmp.tv_usec += 1000000;
	}
	if (tmp.tv_sec < 100)
		delta = tmp.tv_sec * 1000000 + tmp.tv_usec;
	else
		delta = 99999999;

	last_time = new_time;

	va_start(argptr, fmt);
	(void)vsnprintf(debug_buffer, sizeof debug_buffer, fmt, argptr);
	va_end(argptr);

#ifdef USE_SYSLOG
	syslog(syslog_level, "%.8d %s", delta, debug_buffer);
#else
	(void)fprintf(LOG_STREAM, "%s%.8d%s %s%s%s\n", time_pfx, delta, time_sfx,
		color_pfx, debug_buffer, color_sfx);
	fflush(LOG_STREAM);
#endif
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

#ifdef USE_SYSLOG
	syslog(LOG_DEBUG, "%s", debug_buffer);
#else
	(void)fprintf(LOG_STREAM, "%s\n", debug_buffer);
	fflush(LOG_STREAM);
#endif
} /* log_xxd */

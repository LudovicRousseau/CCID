/*
    debug.h: log (or not) messages using syslog
    Copyright (C) 2003-2008   Ludovic Rousseau

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
 * DEBUG_CRITICAL("text");
 *	log "text" if (LogLevel & DEBUG_LEVEL_CRITICAL) is TRUE
 *
 * DEBUG_CRITICAL2("text: %d", 1234);
 *  log "text: 1234" if (DEBUG_LEVEL_CRITICAL & DEBUG_LEVEL_CRITICAL) is TRUE
 * the format string can be anything printf() can understand
 *
 * same thing for DEBUG_INFO, DEBUG_COMM and DEBUG_PERIODIC
 *
 * DEBUG_XXD(msg, buffer, size);
 *  log a dump of buffer if (LogLevel & DEBUG_LEVEL_COMM) is TRUE
 *
 */

#ifndef _GCDEBUG_H_
#define  _GCDEBUG_H_

/* You can't do #ifndef __FUNCTION__ */
#if !defined(__GNUC__) && !defined(__IBMC__)
#define __FUNCTION__ ""
#endif

extern int LogLevel;

#define DEBUG_LEVEL_CRITICAL 1
#define DEBUG_LEVEL_INFO     2
#define DEBUG_LEVEL_COMM     4
#define DEBUG_LEVEL_PERIODIC 8

#include <debuglog.h>	/* from pcsc-lite */

/* DEBUG_CRITICAL */
#define DEBUG_CRITICAL(fmt) if (LogLevel & DEBUG_LEVEL_CRITICAL) Log1(PCSC_LOG_CRITICAL, fmt)

#define DEBUG_CRITICAL2(fmt, data) if (LogLevel & DEBUG_LEVEL_CRITICAL) Log2(PCSC_LOG_CRITICAL, fmt, data)

#define DEBUG_CRITICAL3(fmt, data1, data2) if (LogLevel & DEBUG_LEVEL_CRITICAL) Log3(PCSC_LOG_CRITICAL, fmt, data1, data2)

#define DEBUG_CRITICAL4(fmt, data1, data2, data3) if (LogLevel & DEBUG_LEVEL_CRITICAL) Log4(PCSC_LOG_CRITICAL, fmt, data1, data2, data3)

#define DEBUG_CRITICAL5(fmt, data1, data2, data3, data4) if (LogLevel & DEBUG_LEVEL_CRITICAL) Log5(PCSC_LOG_CRITICAL, fmt, data1, data2, data3, data4)

/* DEBUG_INFO */
#define DEBUG_INFO1(fmt) if (LogLevel & DEBUG_LEVEL_INFO) Log1(PCSC_LOG_INFO, fmt)

#define DEBUG_INFO2(fmt, data) if (LogLevel & DEBUG_LEVEL_INFO) Log2(PCSC_LOG_INFO, fmt, data)

#define DEBUG_INFO3(fmt, data1, data2) if (LogLevel & DEBUG_LEVEL_INFO) Log3(PCSC_LOG_INFO, fmt, data1, data2)

#define DEBUG_INFO4(fmt, data1, data2, data3) if (LogLevel & DEBUG_LEVEL_INFO) Log4(PCSC_LOG_INFO, fmt, data1, data2, data3)

#define DEBUG_INFO5(fmt, data1, data2, data3, data4) if (LogLevel & DEBUG_LEVEL_INFO) Log5(PCSC_LOG_INFO, fmt, data1, data2, data3, data4)

#define DEBUG_INFO_XXD(msg, buffer, size) if (LogLevel & DEBUG_LEVEL_INFO) LogXxd(PCSC_LOG_INFO, msg, buffer, size)

/* DEBUG_PERIODIC */
#define DEBUG_PERIODIC(fmt) if (LogLevel & DEBUG_LEVEL_PERIODIC) Log1(PCSC_LOG_DEBUG, fmt)

#define DEBUG_PERIODIC2(fmt, data) if (LogLevel & DEBUG_LEVEL_PERIODIC) Log2(PCSC_LOG_DEBUG, fmt, data)

#define DEBUG_PERIODIC3(fmt, data1, data2) if (LogLevel & DEBUG_LEVEL_PERIODIC) Log3(PCSC_LOG_DEBUG, fmt, data1, data2)

/* DEBUG_COMM */
#define DEBUG_COMM(fmt) if (LogLevel & DEBUG_LEVEL_COMM) Log1(PCSC_LOG_DEBUG, fmt)

#define DEBUG_COMM2(fmt, data) if (LogLevel & DEBUG_LEVEL_COMM) Log2(PCSC_LOG_DEBUG, fmt, data)

#define DEBUG_COMM3(fmt, data1, data2) if (LogLevel & DEBUG_LEVEL_COMM) Log3(PCSC_LOG_DEBUG, fmt, data1, data2)

#define DEBUG_COMM4(fmt, data1, data2, data3) if (LogLevel & DEBUG_LEVEL_COMM) Log4(PCSC_LOG_DEBUG, fmt, data1, data2, data3)

/* DEBUG_XXD */
#define DEBUG_XXD(msg, buffer, size) if (LogLevel & DEBUG_LEVEL_COMM) LogXxd(PCSC_LOG_DEBUG, msg, buffer, size)

#endif


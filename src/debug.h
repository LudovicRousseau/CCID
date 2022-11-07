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
 *	log "text" if (LogLevel & DEBUG_LEVEL_CRITICAL) is true
 *
 * DEBUG_CRITICAL2("text: %d", 1234);
 *  log "text: 1234" if (DEBUG_LEVEL_CRITICAL & DEBUG_LEVEL_CRITICAL) is true
 * the format string can be anything printf() can understand
 *
 * same thing for DEBUG_INFO, DEBUG_COMM and DEBUG_PERIODIC
 *
 * DEBUG_XXD(msg, buffer, size);
 *  log a dump of buffer if (LogLevel & DEBUG_LEVEL_COMM) is true
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

#ifdef USE_OS_LOG

#define LOG_STRING "%{public}s"
#define LOG_SENSIBLE_STRING "%s"

#include <os/log.h>

#define DEBUG_CRITICAL(fmt) os_log_fault(OS_LOG_DEFAULT, fmt)
#define DEBUG_CRITICAL2(fmt, data1) os_log_fault(OS_LOG_DEFAULT, fmt, data1)
#define DEBUG_CRITICAL3(fmt, data1, data2) os_log_fault(OS_LOG_DEFAULT, fmt, data1, data2)
#define DEBUG_CRITICAL4(fmt, data1, data2, data3) os_log_fault(OS_LOG_DEFAULT, fmt, data1, data2, data3)
#define DEBUG_CRITICAL5(fmt, data1, data2, data3, data4) os_log_fault(OS_LOG_DEFAULT, fmt, data1, data2, data3, data4)

#define DEBUG_INFO1(fmt) os_log_info(OS_LOG_DEFAULT, fmt)
#define DEBUG_INFO2(fmt, data1) os_log_info(OS_LOG_DEFAULT, fmt, data1)
#define DEBUG_INFO3(fmt, data1, data2) os_log_info(OS_LOG_DEFAULT, fmt, data1, data2)
#define DEBUG_INFO4(fmt, data1, data2, data3) os_log_info(OS_LOG_DEFAULT, fmt, data1, data2, data3)
#define DEBUG_INFO5(fmt, data1, data2, data3, data4) os_log_info(OS_LOG_DEFAULT, fmt, data1, data2, data3, data4)

#define DEBUG_PERIODIC(fmt) os_log_debug(OS_LOG_DEFAULT, fmt)
#define DEBUG_PERIODIC2(fmt, data1) os_log_debug(OS_LOG_DEFAULT, fmt, data1)
#define DEBUG_PERIODIC3(fmt, data1, data2) os_log_debug(OS_LOG_DEFAULT, fmt, data1, data2)

#define DEBUG_COMM(fmt) os_log_info(OS_LOG_DEFAULT, fmt)
#define DEBUG_COMM2(fmt, data1) os_log_info(OS_LOG_DEFAULT, fmt, data1)
#define DEBUG_COMM3(fmt, data1, data2) os_log_info(OS_LOG_DEFAULT, fmt, data1, data2)
#define DEBUG_COMM4(fmt, data1, data2, data3) os_log_info(OS_LOG_DEFAULT, fmt, data1, data2, data3)

#define DEBUG_INFO_XXD(msg, buffer, size) do { if (LogLevel & DEBUG_LEVEL_INFO) log_xxd(PCSC_LOG_INFO, msg, buffer, size); } while (0)
#define DEBUG_XXD(msg, buffer, size) do { if (LogLevel & DEBUG_LEVEL_COMM) log_xxd(PCSC_LOG_DEBUG, msg, buffer, size); } while (0)

#else

#define LOG_STRING "%s"
#define LOG_SENSIBLE_STRING "%s"

#define TO_PCSCD_LOG(fmt, CCID_LEVEL, PCSCD_LEVEL)  do { if (LogLevel & DEBUG_LEVEL_ ## CCID_LEVEL) Log1(PCSC_LOG_ ## PCSCD_LEVEL, fmt); } while (0)
#define TO_PCSCD_LOG2(fmt, data, CCID_LEVEL, PCSCD_LEVEL)  do { if (LogLevel & DEBUG_LEVEL_ ## CCID_LEVEL) Log2(PCSC_LOG_ ## PCSCD_LEVEL, fmt, data); } while (0)
#define TO_PCSCD_LOG3(fmt, data1, data2, CCID_LEVEL, PCSCD_LEVEL)  do { if (LogLevel & DEBUG_LEVEL_ ## CCID_LEVEL) Log3(PCSC_LOG_ ## PCSCD_LEVEL, fmt, data1, data2); } while (0)
#define TO_PCSCD_LOG4(fmt, data1, data2, data3, CCID_LEVEL, PCSCD_LEVEL)  do { if (LogLevel & DEBUG_LEVEL_ ## CCID_LEVEL) Log4(PCSC_LOG_ ## PCSCD_LEVEL, fmt, data1, data2, data3); } while (0)
#define TO_PCSCD_LOG5(fmt, data1, data2, data3, data4, CCID_LEVEL, PCSCD_LEVEL)  do { if (LogLevel & DEBUG_LEVEL_ ## CCID_LEVEL) Log5(PCSC_LOG_ ## PCSCD_LEVEL, fmt, data1, data2, data3, data4); } while (0)

/* DEBUG_CRITICAL */
#define DEBUG_CRITICAL(fmt) TO_PCSCD_LOG(fmt, CRITICAL, CRITICAL)
#define DEBUG_CRITICAL2(fmt, data) TO_PCSCD_LOG2(fmt, data, CRITICAL, CRITICAL)
#define DEBUG_CRITICAL3(fmt, data1, data2) TO_PCSCD_LOG3(fmt, data1, data2, CRITICAL, CRITICAL)
#define DEBUG_CRITICAL4(fmt, data1, data2, data3) TO_PCSCD_LOG4(fmt, data1, data2, data3, CRITICAL, CRITICAL)
#define DEBUG_CRITICAL5(fmt, data1, data2, data3, data4) TO_PCSCD_LOG5(fmt, data1, data2, data3, data4, CRITICAL, CRITICAL)

/* DEBUG_INFO */
#define DEBUG_INFO1(fmt) TO_PCSCD_LOG(fmt, INFO, INFO)
#define DEBUG_INFO2(fmt, data) TO_PCSCD_LOG2(fmt, data, INFO, INFO)
#define DEBUG_INFO3(fmt, data1, data2) TO_PCSCD_LOG3(fmt, data1, data2, INFO, INFO)
#define DEBUG_INFO4(fmt, data1, data2, data3) TO_PCSCD_LOG4(fmt, data1, data2, data3, INFO, INFO)
#define DEBUG_INFO5(fmt, data1, data2, data3, data4) TO_PCSCD_LOG5(fmt, data1, data2, data3, data4, INFO, INFO)

#define DEBUG_INFO_XXD(msg, buffer, size) do { if (LogLevel & DEBUG_LEVEL_INFO) log_xxd(PCSC_LOG_INFO, msg, buffer, size); } while (0)

/* DEBUG_PERIODIC */
#define DEBUG_PERIODIC(fmt) TO_PCSCD_LOG(fmt, PERIODIC, DEBUG)
#define DEBUG_PERIODIC2(fmt, data) TO_PCSCD_LOG2(fmt, data, PERIODIC, DEBUG)
#define DEBUG_PERIODIC3(fmt, data1, data2) TO_PCSCD_LOG3(fmt, data1, data2, PERIODIC, DEBUG)

/* DEBUG_COMM */
#define DEBUG_COMM(fmt) TO_PCSCD_LOG(fmt, COMM, DEBUG)
#define DEBUG_COMM2(fmt, data) TO_PCSCD_LOG2(fmt, data, COMM, DEBUG)
#define DEBUG_COMM3(fmt, data1, data2) TO_PCSCD_LOG3(fmt, data1, data2, COMM, DEBUG)
#define DEBUG_COMM4(fmt, data1, data2, data3) TO_PCSCD_LOG4(fmt, data1, data2, data3, COMM, DEBUG)

/* DEBUG_XXD */
#define DEBUG_XXD(msg, buffer, size) do { if (LogLevel & DEBUG_LEVEL_COMM) log_xxd(PCSC_LOG_DEBUG, msg, buffer, size); } while (0)

#endif

#endif


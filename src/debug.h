/*
    debug.h: log (or not) messages using syslog
    Copyright (C) 2003   Ludovic Rousseau

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

/*
 * DEBUG_CRITICAL("text");
 * 	print "text" is DEBUG_LEVEL_CRITICAL and DEBUG_STDERR is defined
 * 	send "text" to syslog if DEBUG_LEVEL_CRITICAL is defined
 *
 * DEBUG_CRITICAL2("text: %d", 1234)
 *  print "text: 1234" is DEBUG_LEVEL_CRITICAL and DEBUG_STDERR is defined
 *  send "text: 1234" to syslog if DEBUG_LEVEL_CRITICAL is defined
 * the format string can be anything printf() can understand
 * 
 * same thing for DEBUG_INFO and DEBUG_COMM
 *
 * DEBUG_XXD(msg, buffer, size) is only defined if DEBUG_LEVEL_COMM if defined
 *
 */
 
#ifndef PACKAGE		/* PACKAGE is defined in ../config.h by ./configure */
#error "file config.h NOT included"
#endif

#ifndef _GCDEBUG_H_
#define  _GCDEBUG_H_

/* You can't do #ifndef __FUNCTION__ */
#if !defined(__GNUC__) && !defined(__IBMC__)
#define __FUNCTION__ ""
#endif

#ifdef DEBUG_LEVEL_CRITICAL
#define DEBUG_CRITICAL(fmt) debug_msg("%s:%d:%s " fmt, __FILE__, __LINE__, __FUNCTION__)
#define DEBUG_CRITICAL2(fmt, data) debug_msg("%s:%d:%s " fmt, __FILE__, __LINE__, __FUNCTION__, data)
#define DEBUG_CRITICAL3(fmt, data1, data2) debug_msg("%s:%d:%s " fmt, __FILE__, __LINE__, __FUNCTION__, data1, data2)
#define DEBUG_CRITICAL4(fmt, data1, data2, data3) debug_msg("%s:%d:%s " fmt, __FILE__, __LINE__, __FUNCTION__, data1, data2, data3)
#define DEBUG
#else
#define DEBUG_CRITICAL(fmt)
#define DEBUG_CRITICAL2(fmt, data)
#define DEBUG_CRITICAL3(fmt, data1, data2)
#define DEBUG_CRITICAL4(fmt, data1, data2, data3)
#endif

#ifdef DEBUG_LEVEL_INFO
#define DEBUG_INFO(fmt) debug_msg("%s:%d:%s " fmt, __FILE__, __LINE__, __FUNCTION__)
#define DEBUG_INFO2(fmt, data) debug_msg("%s:%d:%s " fmt, __FILE__, __LINE__, __FUNCTION__, data)
#define DEBUG_INFO3(fmt, data1, data2) debug_msg("%s:%d:%s " fmt, __FILE__, __LINE__, __FUNCTION__, data1, data2)
#define DEBUG_INFO4(fmt, data1, data2, data3) debug_msg("%s:%d:%s " fmt, __FILE__, __LINE__, __FUNCTION__, data1, data2, data3)
#define DEBUG
#else
#define DEBUG_INFO(fmt)
#define DEBUG_INFO2(fmt, data)
#define DEBUG_INFO3(fmt, data1, data2)
#define DEBUG_INFO4(fmt, data1, data2, data3)
#endif

#ifdef DEBUG_LEVEL_PERIODIC
#define DEBUG_PERIODIC(fmt) debug_msg("%s:%d:%s " fmt, __FILE__, __LINE__, __FUNCTION__)
#define DEBUG_PERIODIC2(fmt, data) debug_msg("%s:%d:%s " fmt, __FILE__, __LINE__, __FUNCTION__, data)
#define DEBUG
#else
#define DEBUG_PERIODIC(fmt)
#define DEBUG_PERIODIC2(fmt, data)
#endif

#ifdef DEBUG_LEVEL_COMM
#define DEBUG_COMM(fmt) debug_msg("%s:%d:%s " fmt, __FILE__, __LINE__, __FUNCTION__)
#define DEBUG_COMM2(fmt, data) debug_msg("%s:%d:%s " fmt, __FILE__, __LINE__, __FUNCTION__, data)
#define DEBUG_COMM3(fmt, data1, data2) debug_msg("%s:%d:%s " fmt, __FILE__, __LINE__, __FUNCTION__, data1, data2)
#define DEBUG_COMM4(fmt, data1, data2, data3) debug_msg("%s:%d:%s " fmt, __FILE__, __LINE__, __FUNCTION__, data1, data2, data3)
#define DEBUG
#define DEBUG_XXD(msg, buffer, size) debug_xxd(msg, buffer, size)
#else
#define DEBUG_COMM(fmt)
#define DEBUG_COMM2(fmt, data)
#define DEBUG_COMM3(fmt, data1, data2)
#define DEBUG_COMM4(fmt, data1, data2, data3)
#endif

#ifndef DEBUG_XXD
#define DEBUG_XXD(msg, buffer, size)
#endif

void debug_msg(char *fmt, ...);
void debug_xxd(const char *msg, const unsigned char *buffer, const int size);

#endif


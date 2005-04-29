/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


//
// logging - message log support
//
#ifdef __MWERKS__
# define _CPP_LOGGING
#endif

#include <security_utilities/logging.h>
#include <security_utilities/globalizer.h>
#include <cstdarg>
#include <syslog.h>

namespace Security
{

namespace Syslog
{

//
// Open and initialize logging
//
void open(const char *ident, int facility, int options /*= 0*/)
{
	::openlog(ident, options, facility);
}


//
// General output method
//
static void output(int priority, const char *format, va_list args)
{
	::vsyslog(priority, format, args);
}


//
// Priority-specific wrappers
//
void syslog(int priority, const char *format, ...)
{ va_list args; va_start(args, format); output(priority, format, args); va_end(args); }

void emergency(const char *format, ...)
{ va_list args; va_start(args, format); output(LOG_EMERG, format, args); va_end(args); }
void alert(const char *format, ...)
{ va_list args; va_start(args, format); output(LOG_ALERT, format, args); va_end(args); }
void critical(const char *format, ...)
{ va_list args; va_start(args, format); output(LOG_CRIT, format, args); va_end(args); }
void error(const char *format, ...)
{ va_list args; va_start(args, format); output(LOG_ERR, format, args); va_end(args); }
void warning(const char *format, ...)
{ va_list args; va_start(args, format); output(LOG_WARNING, format, args); va_end(args); }
void notice(const char *format, ...)
{ va_list args; va_start(args, format); output(LOG_NOTICE, format, args); va_end(args); }
void info(const char *format, ...)
{ va_list args; va_start(args, format); output(LOG_INFO, format, args); va_end(args); }
void debug(const char *format, ...)
{ va_list args; va_start(args, format); output(LOG_DEBUG, format, args); va_end(args); }


//
// Enable mask operation
//
int mask()
{
	int mask;
	::setlogmask(mask = ::setlogmask(0));
	return mask;
}
	
void upto(int priority)
{
	::setlogmask(LOG_UPTO(priority));
}

void enable(int priority)
{
	::setlogmask(::setlogmask(0) | LOG_MASK(priority));
}

void disable(int priority)
{
	::setlogmask(::setlogmask(0) & ~LOG_MASK(priority));
}

} // end namespace Syslog

} // end namespace Security

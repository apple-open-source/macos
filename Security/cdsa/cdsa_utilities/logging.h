/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// logging - message log support
//
#ifndef _H_LOGGING
#define _H_LOGGING

#include <Security/utilities.h>
#include <syslog.h>

#ifdef _CPP_LOGGING
#pragma export on
#endif

namespace Security
{

//
// Log destination object
//
namespace Syslog
{

void syslog(int priority, const char *format, ...);

void emergency(const char *format, ...);
void alert(const char *format, ...);
void critical(const char *format, ...);
void error(const char *format, ...);
void warning(const char *format, ...);
void notice(const char *format, ...);
void info(const char *format, ...);
void debug(const char *format, ...);

void open(const char *ident, int facility, int options = 0);
	
int mask();
void upto(int priority);
void enable(int priority);
void disable(int priority);

} // end namespace Syslog

} // end namespace Security

#ifdef _CPP_LOGGING
#pragma export off
#endif

#endif //_H_LOGGING

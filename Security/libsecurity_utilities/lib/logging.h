/*
 * Copyright (c) 2000-2001,2003-2004 Apple Computer, Inc. All Rights Reserved.
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
#ifndef _H_LOGGING
#define _H_LOGGING

#include <security_utilities/utilities.h>
#include <sys/cdefs.h>

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

void syslog(int priority, const char *format, ...) __printflike(2, 3);

void emergency(const char *format, ...) __printflike(1, 2);
void alert(const char *format, ...) __printflike(1, 2);
void critical(const char *format, ...) __printflike(1, 2);
void error(const char *format, ...) __printflike(1, 2);
void warning(const char *format, ...) __printflike(1, 2);
void notice(const char *format, ...) __printflike(1, 2);
void info(const char *format, ...) __printflike(1, 2);
void debug(const char *format, ...) __printflike(1, 2);

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

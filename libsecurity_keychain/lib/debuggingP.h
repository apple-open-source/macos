/*
 * Copyright (c) 2006-2007,2009-2010 Apple Inc. All Rights Reserved.
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

/* 
 * debugging.h - non-trivial debug support
 */
#ifndef _SECURITY_UTILITIES_DEBUGGING_H_
#define _SECURITY_UTILITIES_DEBUGGING_H_

#ifdef __cplusplus
extern "C" {
#endif

extern void __security_debug(const char *scope, const char *function,
    const char *file, int line, const char *format, ...);

#if !defined(NDEBUG)
# define secdebug(scope,...)	__security_debug(scope, __FUNCTION__, \
    __FILE__, __LINE__, __VA_ARGS__)
#else
# define secdebug(scope, format...)	/* nothing */
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SECURITY_UTILITIES_DEBUGGING_H_ */

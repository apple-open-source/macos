/*
 * Copyright (c) 2003-2004,2014 Apple Inc. All Rights Reserved.
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
 *
 * security.h
 */

#ifndef _SECURITY_H_
#define _SECURITY_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

typedef int(*command_func)(int argc, char * const *argv);

/* If 1 attempt to be as quiet as possible. */
extern int do_quiet;

/* If 1 attempt to be as verbose as possible. */
extern int do_verbose;

const char *sec_errstr(int err);
void sec_error(const char *msg, ...);
void sec_perror(const char *msg, int err);

#ifdef __cplusplus
}
#endif

#endif /*  _SECURITY_H_ */

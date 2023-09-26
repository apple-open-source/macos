/*
 * Copyright (c) 1999-2008 Apple Inc. All rights reserved.
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

#include "lib_fsck_hfs.h"

#define     EEXIT           8   /* Standard error exit. */

char*       unrawname(char *name);
char*       rawname(char *name);
void        cleanup_fs_fd(void);
void		catch __P((int));
void        ckfini ();
void        pfatal(char *fmt, va_list ap);
void        pwarn(char *fmt, va_list ap);
void		logstring(void *, const char *) __printflike(2, 0);     // write to log file 
void		outstring(void *, const char *) __printflike(2, 0);     // write to standard out
void 		llog(const char *fmt, ...) __printflike(1, 2);          // write to log file
void 		olog(const char *fmt, ...) __printflike(1, 2);          // write to standard out
void        plog(const char *fmt, ...) __printflike(1, 2);          // printf replacement that writes to both log file and standard out
void        vplog(const char *fmt, va_list ap) __printflike(1, 0);  // vprintf replacement that writes to both log file and standard out
void        fplog(FILE *stream, const char *fmt, va_list ap) __printflike(2, 3);    // fprintf replacement that writes to both log file and standard out
#define printf  plog      // just in case someone tries to use printf/fprint
#define fprintf fplog

void		DumpData(const void *ptr, size_t sz, char *label);


/*
 * Copyright (c) 1999-2006 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * Portions of this software were developed for the FreeBSD Project by
 * ThinkSec AS and NAI Labs, the Security Research Division of Network
 * Associates, Inc.  under DARPA/SPAWAR contract N66001-01-C-8035
 * ("CBOSS"), as part of the DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)chpass.h	8.4 (Berkeley) 4/2/94
 * $FreeBSD: src/usr.bin/chpass/chpass.h,v 1.7 2004/01/18 21:46:39 charnier Exp $
 */

#ifdef OPEN_DIRECTORY
#include "open_directory.h"

extern char* progname;
extern CFStringRef DSPath;
#endif /* OPEN_DIRECTORY */

struct passwd;

typedef struct _entry {
	const char *prompt;
#ifdef OPEN_DIRECTORY
	void (*display)();
#endif
	int (*func)(char *, struct passwd *, struct _entry *);
	int restricted;
	size_t len;
#if OPEN_DIRECTORY
	char *except;
	CFStringRef attrName;
#else /* OPEN_DIRECTORY */
	char *except, *save;
#endif /* OPEN_DIRECTORY */
} ENTRY;

/* Field numbers. */
#define	E_BPHONE	8
#define	E_HPHONE	9
#define	E_LOCATE	10
#define	E_NAME		7
#define	E_OTHER		11
#define	E_SHELL		13

extern ENTRY list[];
extern int master_mode;

#ifdef OPEN_DIRECTORY
/* edit.c */
void	display_time(CFDictionaryRef, CFStringRef, const char*, FILE *);
void	display_string(CFDictionaryRef, CFStringRef, const char*, FILE *);
CFDictionaryRef edit(const char *tfn, CFDictionaryRef pw);

/* util.c */
int	cfprintf(FILE* file, const char* format, ...);
int	editfile(const char* tfn);
#endif /* OPEN_DIRECTORY */

int	 atot(char *, time_t *);
#ifndef OPEN_DIRECTORY
struct passwd *edit(const char *, struct passwd *);
#endif /* OPEN_DIRECTORY */
int      ok_shell(char *);
char    *dup_shell(char *);
int	 p_change(char *, struct passwd *, ENTRY *);
int	 p_class(char *, struct passwd *, ENTRY *);
int	 p_expire(char *, struct passwd *, ENTRY *);
int	 p_gecos(char *, struct passwd *, ENTRY *);
int	 p_gid(char *, struct passwd *, ENTRY *);
int	 p_hdir(char *, struct passwd *, ENTRY *);
int	 p_login(char *, struct passwd *, ENTRY *);
int	 p_passwd(char *, struct passwd *, ENTRY *);
int	 p_shell(char *, struct passwd *, ENTRY *);
int	 p_uid(char *, struct passwd *, ENTRY *);
#ifdef OPEN_DIRECTORY
int	 p_uuid(char *, struct passwd *, ENTRY *);
#endif /* OPEN_DIRECTORY */
char    *ttoa(time_t);

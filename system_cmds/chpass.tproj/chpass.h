/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 *      The Regents of the University of California.  All rights reserved.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 *      @(#)chpass.h    8.4 (Berkeley) 4/2/94
 */

#ifdef DIRECTORY_SERVICE
#include <stdio.h>
#include <sys/types.h>

struct display {
	struct passwd *pw;
	char *fullname;
	char *location;
	char *officephone;
	char *homephone;
};
#endif /* DIRECTORY_SERVICE */

struct passwd;

typedef struct _entry {
	char *prompt;
#ifdef DIRECTORY_SERVICE
	void (*display)();
#endif /* DIRECTORY_SERVICE */
	int (*func)(), restricted, len;
	char *except, *save;
} ENTRY;

/* Field numbers. */
#ifdef DIRECTORY_SERVICE
#define	E_LOGIN		0
#define	E_PASSWD	1
#define	E_UID		2
#define	E_GID		3
#define	E_CHANGE	4
#define	E_EXPIRE	5
#define	E_CLASS		6
#define	E_HOME		7
#define	E_SHELL		8
#define	E_NAME		9
#define	E_LOCATE	10
#define	E_BPHONE	11
#define	E_HPHONE	12
#else /* DIRECTORY_SERVICE */
#define	E_BPHONE	8
#define	E_HPHONE	9
#define	E_LOCATE	10
#define	E_NAME		7
#define	E_SHELL		12
#endif /* DIRECTORY_SERVICE */

extern ENTRY list[];
extern uid_t uid;

int	 atot __P((char *, time_t *));
#ifdef DIRECTORY_SERVICE
void	 d_change __P((struct display *, FILE *));
void	 d_class __P((struct display *, FILE *));
void	 d_expire __P((struct display *, FILE *));
void	 d_fullname __P((struct display *, FILE *));
void	 d_gid __P((struct display *, FILE *));
void	 d_hdir __P((struct display *, FILE *));
void	 d_homephone __P((struct display *, FILE *));
void	 d_login __P((struct display *, FILE *));
void	 d_location __P((struct display *, FILE *));
void	 d_officephone __P((struct display *, FILE *));
void	 d_passwd __P((struct display *, FILE *));
void	 d_shell __P((struct display *, FILE *));
void	 d_uid __P((struct display *, FILE *));
#endif /* DIRECTORY_SERVICE */
void	 display __P((int, struct passwd *));
void	 edit __P((struct passwd *));
char    *ok_shell __P((char *));
int	 p_change __P((char *, struct passwd *, ENTRY *));
int	 p_class __P((char *, struct passwd *, ENTRY *));
int	 p_expire __P((char *, struct passwd *, ENTRY *));
int	 p_gecos __P((char *, struct passwd *, ENTRY *));
int	 p_gid __P((char *, struct passwd *, ENTRY *));
int	 p_hdir __P((char *, struct passwd *, ENTRY *));
int	 p_login __P((char *, struct passwd *, ENTRY *));
int	 p_login __P((char *, struct passwd *, ENTRY *));
int	 p_passwd __P((char *, struct passwd *, ENTRY *));
int	 p_shell __P((char *, struct passwd *, ENTRY *));
int	 p_uid __P((char *, struct passwd *, ENTRY *));
char    *ttoa __P((time_t));
int	 verify __P((struct passwd *));

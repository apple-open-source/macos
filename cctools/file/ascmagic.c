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
/*	$OpenBSD: ascmagic.c,v 1.3 1997/02/09 23:58:18 millert Exp $	*/

/*
 * ASCII magic -- file types that we know based on keywords
 * that can appear anywhere in the file.
 *
 * Copyright (c) Ian F. Darwin, 1987.
 * Written by Ian F. Darwin.
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or of the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. The author is not responsible for the consequences of use of this
 *    software, no matter how awful, even if they arise from flaws in it.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Since few users ever read sources,
 *    credits must appear in the documentation.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.  Since few users
 *    ever read sources, credits must appear in the documentation.
 *
 * 4. This notice may not be removed or altered.
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include "file.h"
#include "names.h"

#ifndef	lint
#if 0
static char *moduleid = "$OpenBSD: ascmagic.c,v 1.3 1997/02/09 23:58:18 millert Exp $";
#endif
#endif	/* lint */

			/* an optimisation over plain strcmp() */
#define	STREQ(a, b)	(*(a) == *(b) && strcmp((a), (b)) == 0)

int
ascmagic(buf, nbytes)
unsigned char *buf;
int nbytes;	/* size actually read */
{
	int i, has_escapes = 0;
	unsigned char *s;
	char nbuf[HOWMANY+1];	/* one extra for terminating '\0' */
	char *token;
	register struct names *p;

	/*
	 * Do the tar test first, because if the first file in the tar
	 * archive starts with a dot, we can confuse it with an nroff file.
	 */
	switch (is_tar(buf, nbytes)) {
	case 1:
		ckfputs("tar archive", stdout);
		return 1;
	case 2:
		ckfputs("POSIX tar archive", stdout);
		return 1;
	}

	/*
	 * for troff, look for . + letter + letter or .\";
	 * this must be done to disambiguate tar archives' ./file
	 * and other trash from real troff input.
	 */
	if (*buf == '.') {
		unsigned char *tp = buf + 1;

		while (isascii(*tp) && isspace(*tp))
			++tp;	/* skip leading whitespace */
		if ((isascii(*tp) && (isalnum(*tp) || *tp=='\\') &&
		    isascii(tp[1]) && (isalnum(tp[1]) || tp[1] == '"'))) {
			ckfputs("troff or preprocessor input text", stdout);
			return 1;
		}
	}
	if ((*buf == 'c' || *buf == 'C') && 
	    isascii(buf[1]) && isspace(buf[1])) {
		ckfputs("fortran program text", stdout);
		return 1;
	}


	/* Make sure we are dealing with ascii text before looking for tokens */
	for (i = 0; i < nbytes; i++) {
		if (!isascii(buf[i]))
			return 0;	/* not all ASCII */
	}

	/* look for tokens from names.h - this is expensive! */
	/* make a copy of the buffer here because strtok() will destroy it */
	s = (unsigned char*) memcpy(nbuf, buf, nbytes);
	s[nbytes] = '\0';
	has_escapes = (memchr(s, '\033', nbytes) != NULL);
	while ((token = strtok((char *) s, " \t\n\r\f")) != NULL) {
		s = NULL;	/* make strtok() keep on tokin' */
		for (p = names; p < names + NNAMES; p++) {
			if (STREQ(p->name, token)) {
				ckfputs(types[p->type], stdout);
				if (has_escapes)
					ckfputs(" (with escape sequences)", 
						stdout);
				return 1;
			}
		}
	}

	/* all else fails, but it is ASCII... */
	ckfputs("ASCII text", stdout);
	if (has_escapes) {
		ckfputs(" (with escape sequences)", stdout);
	}
	return 1;
}



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
#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$Id: nsap_addr.c,v 1.4 2002/05/17 21:00:31 bbraun Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <ctype.h>
#include <resolv.h>
#include <stdlib.h>

#if !defined(isxdigit)	/* XXX - could be a function */
static int
isxdigit(c)
	register int c;
{
	return ((c >= '0') && (c <= '9')) || ((c >= 'A') && (c <= 'F'));
}
#endif

static char
xtob(c)
	register int c;
{
	return (c - (((c >= '0') && (c <= '9')) ? '0' : '7'));
}

u_int
inet_nsap_addr(ascii, binary, maxlen)
	const char *ascii;
	u_char *binary;
	int maxlen;
{
	register u_char c, nib;
	u_int len = 0;

	while ((c = *ascii++) != '\0' && len < maxlen) {
		if (c == '.' || c == '+' || c == '/')
			continue;
		if (!isascii(c))
			return (0);
		if (islower(c))
			c = toupper(c);
		if (isxdigit(c)) {
			nib = xtob(c);
			if (c = *ascii++) {
				c = toupper(c);
				if (isxdigit(c)) {
					*binary++ = (nib << 4) | xtob(c);
					len++;
				} else
					return (0);
			}
			else
				return (0);
		}
		else
			return (0);
	}
	return (len);
}

char *
inet_nsap_ntoa(binlen, binary, ascii)
	int binlen;
	register const u_char *binary;
	register char *ascii;
{
	register int nib;
	int i;
	static char *tmpbuf = NULL;
	char *start;

	if( tmpbuf == NULL ) {
		tmpbuf = malloc(255*3);
		if( tmpbuf == NULL )
			return NULL;
	}

	if (ascii)
		start = ascii;
	else {
		ascii = tmpbuf;
		start = tmpbuf;
	}

	if (binlen > 255)
		binlen = 255;

	for (i = 0; i < binlen; i++) {
		nib = *binary >> 4;
		*ascii++ = nib + (nib < 10 ? '0' : '7');
		nib = *binary++ & 0x0f;
		*ascii++ = nib + (nib < 10 ? '0' : '7');
		if (((i % 2) == 0 && (i + 1) < binlen))
			*ascii++ = '.';
	}
	*ascii = '\0';
	return (start);
}

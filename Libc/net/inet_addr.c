/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1983, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */


#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>

/*
 * Ascii internet address interpretation routine.
 * The value returned is in network order.
 */
u_long
inet_addr(cp)
	register const char *cp;
{
	struct in_addr val;

	if (inet_aton(cp, &val))
		return (val.s_addr);
	return (INADDR_NONE);
}

/* 
 * Check whether "cp" is a valid ascii representation
 * of an Internet address and convert to a binary address.
 * Returns 1 if the address is valid, 0 if not.
 * This replaces inet_addr, the return value from which
 * cannot distinguish between failure and a local broadcast address.
 */
int
inet_aton(cp, addr)
	register const char *cp;
	struct in_addr *addr;
{
	u_long parts[4];
	u_int32_t val;
	char *c;
	char *endptr;
	int gotend, n;

	c = (char *)cp;
	n = 0;
	/*
	 * Run through the string, grabbing numbers until
	 * the end of the string, or some error
	 */
	gotend = 0;
	while (!gotend) {
		errno = 0;
		val = strtoul(c, &endptr, 0);

		if (errno == ERANGE)	/* Fail completely if it overflowed. */
			return (0);
		
		/* 
		 * If the whole string is invalid, endptr will equal
		 * c.. this way we can make sure someone hasn't
		 * gone '.12' or something which would get past
		 * the next check.
		 */
		if (endptr == c)
			return (0);
		parts[n] = val;
		c = endptr;

		/* Check the next character past the previous number's end */
		switch (*c) {
		case '.' :
			/* Make sure we only do 3 dots .. */
			if (n == 3)	/* Whoops. Quit. */
				return (0);
			n++;
			c++;
			break;

		case '\0':
			gotend = 1;
			break;

		default:
			if (isspace((unsigned char)*c)) {
				gotend = 1;
				break;
			} else
				return (0);	/* Invalid character, so fail */
		}

	}

	/*
	 * Concoct the address according to
	 * the number of parts specified.
	 */

	switch (n) {
	case 0:				/* a -- 32 bits */
		/*
		 * Nothing is necessary here.  Overflow checking was
		 * already done in strtoul().
		 */
		break;
	case 1:				/* a.b -- 8.24 bits */
		if (val > 0xffffff || parts[0] > 0xff)
			return (0);
		val |= parts[0] << 24;
		break;

	case 2:				/* a.b.c -- 8.8.16 bits */
		if (val > 0xffff || parts[0] > 0xff || parts[1] > 0xff)
			return (0);
		val |= (parts[0] << 24) | (parts[1] << 16);
		break;

	case 3:				/* a.b.c.d -- 8.8.8.8 bits */
		if (val > 0xff || parts[0] > 0xff || parts[1] > 0xff ||
		    parts[2] > 0xff)
			return (0);
		val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
		break;
	}

	if (addr != NULL)
		addr->s_addr = htonl(val);
	return (1);
}

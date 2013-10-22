/*
 * Copyright (c) 1988, 1990, 1993
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

#include <sys/types.h>
#include <sys/socket.h>

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/*
 * 5/10/12: Imported from telnet project
 *
 * Source route is handed in as
 *	[!]@hop1@hop2...[@|:]dst
 * If the leading ! is present, it is a
 * strict source route, otherwise it is
 * assmed to be a loose source route.
 *
 * We fill in the source route option as
 *	hop1,hop2,hop3...dest
 * and return a pointer to hop1, which will
 * be the address to connect() to.
 *
 * Arguments:
 *
 *	res:	ponter to addrinfo structure which contains sockaddr to
 *		the host to connect to.
 *
 *	arg:	pointer to route list to decipher
 *
 *	cpp:	If *cpp is not equal to NULL, this is a
 *		pointer to a pointer to a character array
 *		that should be filled in with the option.
 *
 *	lenp:	pointer to an integer that contains the
 *		length of *cpp if *cpp != NULL.
 *
 *	protop: pointer to an integer that should be filled in with
 *		appropriate protocol for setsockopt, as socket 
 *		protocol family.
 *
 *	optp:	pointer to an integer that should be filled in with
 *		appropriate option for setsockopt, as socket protocol
 *		family.
 *
 * Return values:
 *
 *	If the return value is 1, then all operations are
 *	successful. If the
 *	return value is -1, there was a syntax error in the
 *	option, either unknown characters, or too many hosts.
 *	If the return value is 0, one of the hostnames in the
 *	path is unknown, and *cpp is set to point to the bad
 *	hostname.
 *
 *	*cpp:	If *cpp was equal to NULL, it will be filled
 *		in with a pointer to our static area that has
 *		the option filled in.  This will be 32bit aligned.
 *
 *	*lenp:	This will be filled in with how long the option
 *		pointed to by *cpp is.
 *
 *	*protop: This will be filled in with appropriate protocol for
 *		 setsockopt, as socket protocol family.
 *
 *	*optp:	This will be filled in with appropriate option for
 *		setsockopt, as socket protocol family.
 */
int
sourceroute(struct addrinfo *ai, char *arg, char **cpp, int *lenp, int *protop, int *optp)
{
	static char buf[1024 + ALIGNBYTES];	/*XXX*/
	char *cp, *cp2, *lsrp, *ep;
	struct sockaddr_in *_sin;
#ifdef INET6
	struct sockaddr_in6 *sin6;
	struct cmsghdr *cmsg = NULL;
#endif
	struct addrinfo hints, *res;
	int error;
	char c;
	
	/*
	 * Verify the arguments, and make sure we have
	 * at least 7 bytes for the option.
	 */
	if (cpp == NULL || lenp == NULL)
		return -1;
	if (*cpp != NULL) {
		switch (ai->ai_family) {
			case AF_INET:
				if (*lenp < 7)
					return -1;
				break;
#ifdef INET6
			case AF_INET6:
				if (*lenp < (int)CMSG_SPACE(sizeof(struct ip6_rthdr) +
											sizeof(struct in6_addr)))
					return -1;
				break;
#endif
		}
	}
	/*
	 * Decide whether we have a buffer passed to us,
	 * or if we need to use our own static buffer.
	 */
	if (*cpp) {
		lsrp = *cpp;
		ep = lsrp + *lenp;
	} else {
		*cpp = lsrp = (char *)ALIGN(buf);
		ep = lsrp + 1024;
	}
	
	cp = arg;
	
#ifdef INET6
	if (ai->ai_family == AF_INET6) {
		cmsg = inet6_rthdr_init(*cpp, IPV6_RTHDR_TYPE_0);
		if (*cp != '@')
			return -1;
		*protop = IPPROTO_IPV6;
		*optp = IPV6_PKTOPTIONS;
	} else
#endif
	{
		/*
		 * Next, decide whether we have a loose source
		 * route or a strict source route, and fill in
		 * the begining of the option.
		 */
		if (*cp == '!') {
			cp++;
			*lsrp++ = IPOPT_SSRR;
		} else
			*lsrp++ = IPOPT_LSRR;
		
		if (*cp != '@')
			return -1;
		
		lsrp++;		/* skip over length, we'll fill it in later */
		*lsrp++ = 4;
		*protop = IPPROTO_IP;
		*optp = IP_OPTIONS;
	}
	
	cp++;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = ai->ai_family;
	hints.ai_socktype = SOCK_STREAM;
	for (c = 0;;) {
		if (
#ifdef INET6
			ai->ai_family != AF_INET6 &&
#endif
			c == ':')
			cp2 = 0;
		else for (cp2 = cp; (c = *cp2); cp2++) {
			if (c == ',') {
				*cp2++ = '\0';
				if (*cp2 == '@')
					cp2++;
			} else if (c == '@') {
				*cp2++ = '\0';
			} else if (
#ifdef INET6
					   ai->ai_family != AF_INET6 &&
#endif
					   c == ':') {
				*cp2++ = '\0';
			} else
				continue;
			break;
		}
		if (!c)
			cp2 = 0;
		
		hints.ai_flags = AI_NUMERICHOST;
		error = getaddrinfo(cp, NULL, &hints, &res);
#ifdef EAI_NODATA
		if ((error == EAI_NODATA) || (error == EAI_NONAME))
#else
			if (error == EAI_NONAME)
#endif
			{
				hints.ai_flags = 0;
				error = getaddrinfo(cp, NULL, &hints, &res);
			}
		if (error != 0) {
			fprintf(stderr, "%s: %s\n", cp, gai_strerror(error));
			if (error == EAI_SYSTEM)
				fprintf(stderr, "%s: %s\n", cp,
						strerror(errno));
			*cpp = cp;
			return(0);
		}
#ifdef INET6
		if (res->ai_family == AF_INET6) {
			sin6 = (struct sockaddr_in6 *)res->ai_addr;
			inet6_rthdr_add(cmsg, &sin6->sin6_addr, IPV6_RTHDR_LOOSE);
		} else
#endif
		{
			_sin = (struct sockaddr_in *)res->ai_addr;
			memcpy(lsrp, (char *)&_sin->sin_addr, 4);
			lsrp += 4;
		}
		if (cp2)
			cp = cp2;
		else
			break;
		/*
		 * Check to make sure there is space for next address
		 */
#ifdef INET6
		if (res->ai_family == AF_INET6) {
			if (((char *)CMSG_DATA(cmsg) +
				 sizeof(struct ip6_rthdr) +
				 ((inet6_rthdr_segments(cmsg) + 1) *
				  sizeof(struct in6_addr))) > ep)
				return -1;
		} else
#endif
			if (lsrp + 4 > ep)
				return -1;
		freeaddrinfo(res);
	}
#ifdef INET6
	if (res->ai_family == AF_INET6) {
		inet6_rthdr_lasthop(cmsg, IPV6_RTHDR_LOOSE);
		*lenp = cmsg->cmsg_len;
	} else
#endif
	{
		if ((*(*cpp+IPOPT_OLEN) = lsrp - *cpp) <= 7) {
			*cpp = 0;
			*lenp = 0;
			return -1;
		}
		*lsrp++ = IPOPT_NOP; /* 32 bit word align it */
		*lenp = lsrp - *cpp;
	}
	freeaddrinfo(res);
	return 1;
}
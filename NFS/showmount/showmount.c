/*
 * Copyright (c) 1999-2010 Apple Inc. All rights reserved.
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
 * Copyright (c) 1989, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <netdb.h>
#include <oncrpc/rpc.h>
#include <oncrpc/pmap_clnt.h>
#include <oncrpc/pmap_prot.h>
#include <nfs/rpcv2.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/queue.h>

#include "showmount.h"

struct mountlist *mntdump;
TAILQ_HEAD(exportslisthead, exportslist) exports;
int mounttype = MOUNTSHOWHOSTS;
int rpcs = 0, mntvers = 1, ipvers = 0;

void	usage(void);
int	xdr_mntdump(XDR *, struct mountlist **);
int	xdr_exports(XDR *, struct exportslisthead *);

/*
 * This command queries the NFS mount daemon for it's mount list and/or
 * it's exports list and prints them out.
 * See "NFS: Network File System Protocol Specification, RFC1094, Appendix A"
 * and the "Network File System Protocol XXX.."
 * for detailed information on the protocol.
 */
int
main(int argc, char *argv[])
{
	int ch, rv, do_browse = 0;

	while ((ch = getopt(argc, argv, "Aade36")) != EOF)
		switch((char)ch) {
		case 'A':
			do_browse = 1;
			break;
		case 'a':
			if (mounttype)
				usage();
			mounttype = MOUNTSHOWALL;
			rpcs |= DODUMP;
			break;
		case 'd':
			if (mounttype)
				usage();
			mounttype = MOUNTSHOWDIRS;
			rpcs |= DODUMP;
			break;
		case 'e':
			rpcs |= DOEXPORTS;
			break;
		case '3':
			mntvers = 3;
			break;
		case '6':
			ipvers = 6;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (rpcs == 0)
		rpcs = DODUMP;

	if (do_browse)
		rv = browse();
	else
		rv = do_print((argc > 0) ? argv[0] : "localhost");

	exit(rv);
}

/*
 * Compare the addresses in two addrinfo structures.
 */
static int
addrinfo_cmp(struct addrinfo *a1, struct addrinfo *a2)
{
	if (a1->ai_family != a2->ai_family)
		return (a1->ai_family - a2->ai_family);
	if (a1->ai_addrlen != a2->ai_addrlen)
		return (a1->ai_addrlen - a2->ai_addrlen);
	return bcmp(a1->ai_addr, a2->ai_addr, a1->ai_addrlen);
}

/*
 * Resolve the given host name to a list of usable addresses
 */
int
getaddresslist(const char *name, struct addrinfo **ailistp)
{
	struct addrinfo aihints, *ailist = NULL, *ai, *aiprev, *ainext, *aidiscard;
	char namebuf[NI_MAXHOST];
	const char *hostname;

	hostname = name;
	if ((hostname[0] == '[') && (hostname[strlen(hostname)-1] == ']')) {
		/* Looks like an IPv6 literal in brackets */
		strlcpy(namebuf, hostname+1, sizeof(namebuf));
		namebuf[strlen(namebuf)-1] = '\0';
		hostname = namebuf;
	}

	bzero(&aihints, sizeof(aihints));
	aihints.ai_flags = AI_ADDRCONFIG;
	if (getaddrinfo(hostname, NULL, &aihints, &ailist)) {
		warnx("can't resolve host: %s", hostname);
		return (ENOENT);
        }

	/* strip out addresses that don't match the options given */
	aidiscard = NULL;
	aiprev = NULL;

	for (ai = ailist; ai; ai = ainext) {
		ainext = ai->ai_next;

		/* If ipvers is set, eliminate addresses that aren't of that IP version. */
		if (ipvers) {
			if ((ipvers == 6) && (ai->ai_family != AF_INET6))
				goto discard;
			if ((ipvers == 4) && (ai->ai_family != AF_INET))
				goto discard;
		}

		/* eliminate unknown protocol families */
		if ((ai->ai_family != AF_INET) && (ai->ai_family != AF_INET6))
			goto discard;

		/* Eliminate duplicate addresses with different socktypes. */
		if (aiprev && (ai->ai_socktype != aiprev->ai_socktype) &&
		    !addrinfo_cmp(aiprev, ai))
			goto discard;

		aiprev = ai;
		continue;
discard:
		/* Add ai to the discard list */
		if (aiprev)
			aiprev->ai_next = ai->ai_next;
		else
			ailist = ai->ai_next;
		ai->ai_next = aidiscard;
		aidiscard = ai;
	}

	/* free up any discarded addresses */
	if (aidiscard)
		freeaddrinfo(aidiscard);

	*ailistp = ailist;
	return (0);
}

/*
 * Xdr routine for retrieving the mount dump list
 */
int
xdr_mntdump(XDR *xdrsp, struct mountlist **mlp)
{
	struct mountlist *mp, **otp, *tp;
	int bool, val, val2;
	char *strp;

	*mlp = (struct mountlist *)0;
	if (!xdr_bool(xdrsp, &bool))
		return (0);
	while (bool) {
		mp = (struct mountlist *)malloc(sizeof(struct mountlist));
		if (mp == NULL)
			return (0);
		mp->ml_left = mp->ml_right = (struct mountlist *)0;
		strp = mp->ml_host;
		if (!xdr_string(xdrsp, &strp, RPCMNT_NAMELEN))
			return (0);
		strp = mp->ml_dirp;
		if (!xdr_string(xdrsp, &strp, RPCMNT_PATHLEN))
			return (0);

		/*
		 * Build a binary tree on sorted order of either host or dirp.
		 * Drop any duplications.
		 */
		tp = *mlp;
		if (tp == NULL) {
			*mlp = mp;
		} else {
			while (tp) {
				val = strcmp(mp->ml_host, tp->ml_host);
				val2 = strcmp(mp->ml_dirp, tp->ml_dirp);
				switch (mounttype) {
				case MOUNTSHOWALL:
					if (val == 0) {
						if (val2 == 0) {
							free((caddr_t)mp);
							goto next;
						}
						val = val2;
					}
					break;
				case MOUNTSHOWDIRS:
					if (val2 == 0) {
						free((caddr_t)mp);
						goto next;
					}
					val = val2;
					break;
				default:
					if (val == 0) {
						free((caddr_t)mp);
						goto next;
					}
					break;
				}
				if (val < 0) {
					otp = &tp->ml_left;
					tp = tp->ml_left;
				} else {
					otp = &tp->ml_right;
					tp = tp->ml_right;
				}
			}
			*otp = mp;
		}
next:
		if (!xdr_bool(xdrsp, &bool))
			return (0);
	}
	return (1);
}

/*
 * Xdr routine to retrieve exports list
 */
int
xdr_exports(XDR *xdrsp, struct exportslisthead *exphead)
{
	struct exportslist *ep;
	struct grouplist *gp;
	int bool, grpbool;
	char *strp;

	TAILQ_INIT(exphead);
	if (!xdr_bool(xdrsp, &bool))
		return (0);
	while (bool) {
		ep = (struct exportslist *)malloc(sizeof(struct exportslist));
		if (ep == NULL)
			return (0);
		TAILQ_INIT(&ep->ex_groups);
		strp = ep->ex_dirp;
		if (!xdr_string(xdrsp, &strp, RPCMNT_PATHLEN))
			return (0);
		if (!xdr_bool(xdrsp, &grpbool))
			return (0);
		while (grpbool) {
			gp = (struct grouplist *)malloc(sizeof(struct grouplist));
			if (gp == NULL)
				return (0);
			strp = gp->gr_name;
			if (!xdr_string(xdrsp, &strp, RPCMNT_NAMELEN))
				return (0);
			TAILQ_INSERT_TAIL(&ep->ex_groups, gp, gr_link);
			if (!xdr_bool(xdrsp, &grpbool))
				return (0);
		}
		TAILQ_INSERT_TAIL(exphead, ep, ex_link);
		if (!xdr_bool(xdrsp, &bool))
			return (0);
	}
	return (1);
}

void
usage(void)
{
	fprintf(stderr, "usage: showmount [-Ae36] [-a|-d] host\n");
	exit(1);
}

/*
 * Print the binary tree in inorder so that output is sorted.
 */
void
print_dump(struct mountlist *mp)
{
	if (mp == NULL)
		return;
	if (mp->ml_left)
		print_dump(mp->ml_left);
	switch (mounttype) {
	case MOUNTSHOWALL:
		printf("%s:%s\n", mp->ml_host, mp->ml_dirp);
		break;
	case MOUNTSHOWDIRS:
		printf("%s\n", mp->ml_dirp);
		break;
	default:
		printf("%s\n", mp->ml_host);
		break;
	}
	if (mp->ml_right)
		print_dump(mp->ml_right);
}

/*
 * free the binary tree
 */
void
free_dump(struct mountlist *mp)
{
	if (!mp)
		return;
	free_dump(mp->ml_left);
	free_dump(mp->ml_right);
	free(mp);
}

int
do_print(const char *host)
{
	struct exportslist *exp, *expnext;
	struct grouplist *grp, *grpnext;
	int so, estat, success = 0;
	CLIENT *clp;
	struct timeval try;
	struct addrinfo *ailist = NULL, *ai;
	char *spcerr = NULL;
	char *sperr = NULL;
	char serr[MAXPATHLEN], *s;
	size_t slen;

	/* get the list of addresses for the host */
	if (getaddresslist(host, &ailist))
		return (1);
	if (!ailist) {
		warnx("no usable addresses for host: %s", host);
		return (1);
	}

	/* try each address until we find one that works */
	for (success=0, ai=ailist; !success && ai; ai=ai->ai_next) {

		try.tv_sec = 10;
		try.tv_usec = 0;
		so = RPC_ANYSOCK;

		/* Try to connect to TCP, otherwise fall back to UDP */
		clp = clnttcp_create_sa(ai->ai_addr, RPCPROG_MNT, mntvers, &so, 0, 0);
		if (clp == NULL)
			clp = clntudp_create_sa(ai->ai_addr, RPCPROG_MNT, mntvers, try, &so);
		if (clp == NULL) {
			if (spcerr)
				free(spcerr);
			spcerr = strdup(clnt_spcreateerror(host));
			continue;
		}
		clp->cl_auth = authunix_create_default();

		if (rpcs & DODUMP) {
			estat = clnt_call(clp, RPCMNT_DUMP, (xdrproc_t)xdr_void, NULL,
						(xdrproc_t)xdr_mntdump, &mntdump, try);
			if (estat != 0) {
				if (sperr)
					free(sperr);
				snprintf(serr, sizeof(serr), "%s: RPC failed:", host);
				sperr = strdup(clnt_sperror(clp, serr));
			} else {
				success++;
			}
		}
		if (rpcs & DOEXPORTS) {
			estat = clnt_call(clp, RPCMNT_EXPORT, (xdrproc_t)xdr_void, NULL,
						(xdrproc_t)xdr_exports, &exports, try);
			if (estat != 0) {
				if (sperr)
					free(sperr);
				snprintf(serr, sizeof(serr), "%s: RPC failed:", host);
				sperr = strdup(clnt_sperror(clp, serr));
			} else {
				success++;
			}
		}
		auth_destroy(clp->cl_auth);
		clnt_destroy(clp);
	}

	if (!success) {
		s = sperr ? sperr : spcerr;
		if (s) {
			slen = strlen(s);
			if ((slen > 0) && (s[slen-1] == '\n'))
				s[slen-1] = '\0';
		}
		warnx("Cannot retrieve info from host: %s", s ? s : host);
		if (spcerr)
			free(spcerr);
		if (sperr)
			free(sperr);
		return (1);
	}

	/* Now just print out the results */
	if (rpcs & DODUMP) {
		switch (mounttype) {
		case MOUNTSHOWALL:
			printf("All mounts on %s:\n", host);
			break;
		case MOUNTSHOWDIRS:
			printf("Directories on %s:\n", host);
			break;
		default:
			printf("Hosts on %s:\n", host);
			break;
		}
		print_dump(mntdump);
		free_dump(mntdump);
	}
	if (rpcs & DOEXPORTS) {
		printf("Exports list on %s:\n", host);
		TAILQ_FOREACH_SAFE(exp, &exports, ex_link, expnext) {
			printf("%-35s", exp->ex_dirp);
			if (TAILQ_EMPTY(&exp->ex_groups)) {
				printf(" Everyone\n"); // allow space
			} else {
				TAILQ_FOREACH_SAFE(grp, &exp->ex_groups, gr_link, grpnext) {
					printf(" %s", grp->gr_name);
					free(grp);
				}
				printf("\n");
			}
			free(exp);
		}
	}

	if (spcerr)
		free(spcerr);
	if (sperr)
		free(sperr);
	return (0);
}

/*
 * Copyright (c) 2008-2009 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*-
 * Copyright (c) 1983, 1988, 1993
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

/*
 * Display protocol blocks in the unix domain.
 */
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>
#include <sys/un.h>
#include <sys/unpcb.h>

#include <netinet/in.h>

#include <errno.h>
#include <err.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "netstat.h"

#include <TargetConditionals.h>

#define ROUNDUP64(a) \
((a) > 0 ? (1 + (((a) - 1) | (sizeof(uint64_t) - 1))) : sizeof(uint64_t))
#define ADVANCE64(x, n) (((char *)x) += ROUNDUP64(n))


#define ALL_XGN_KIND_UNPCB (XSO_SOCKET | XSO_RCVBUF | XSO_SNDBUF | XSO_STATS | XSO_UNPCB)

struct xgen_n {
	u_int32_t	xgn_len;	/* length of this structure */
	u_int32_t	xgn_kind;	/* type of structure */
};

static	const char *const socktype[] =
{ "#0", "stream", "dgram", "raw" };

static void
unixdomainpr_n(struct xunpcb_n *xunp,
	       struct xsocket_n *so,
	       struct xsockbuf_n *so_rcv,
	       struct xsockbuf_n *so_snd,
	       struct xsockstat_n *so_stat)
{
	struct sockaddr_un *sa;
	static int first = 1;

	sa = &xunp->xu_addr;

	if (first) {
		printf("Active LOCAL (UNIX) domain sockets\n");
		printf(
		       "%-16.16s %-6.6s %-6.6s %-6.6s %16.16s %16.16s %16.16s %16.16s",
		       "Address", "Type", "Recv-Q", "Send-Q",
		       "Inode", "Conn", "Refs", "Nextref");
		if (bflag > 0 || vflag > 0)
			printf(" %10.10s %10.10s",
			       "rxbytes", "txbytes");
		if (vflag > 0) {
			printf(" %7.7s %7.7s %6.6s %6.6s %5.5s %8.8s",
			       "rhiwat", "shiwat", "pid", "epid", "state", "options");
			printf(" %16.16s %8.8s %8.8s %6s %6s %5s",
			       "gencnt", "flags", "flags1", "usecnt", "rtncnt", "fltrs");
		}
		printf(" Addr\n");
		first = 0;
	}

	printf("%16llx %-6.6s %6u %6u %16llx %16llx %16llx %16llx",
	       xunp->xunp_unpp, socktype[so->so_type], so_rcv->sb_cc,
	       so_snd->sb_cc,
	       xunp->xunp_vnode, xunp->xunp_conn,
	       xunp->xunp_refs, xunp->xunp_reflink);
	if (bflag > 0 || vflag > 0) {
		int i;
		u_int64_t rxbytes = 0;
		u_int64_t txbytes = 0;

		for (i = 0; i < SO_TC_STATS_MAX; i++) {
			rxbytes += so_stat->xst_tc_stats[i].rxbytes;
			txbytes += so_stat->xst_tc_stats[i].txbytes;
		}
		printf(" %10llu %10llu", rxbytes, txbytes);
	}
	if (vflag > 0) {
		printf(" %7u %7u %6u %6u %05x %08x",
		       so_rcv->sb_hiwat,
		       so_snd->sb_hiwat,
		       so->so_last_pid,
		       so->so_e_pid,
		       so->so_state,
		       so->so_options);
		printf(" %016llx %08x %08x %6d %6d %06x",
		       so->so_gencnt,
		       so->so_flags,
		       so->so_flags1,
		       so->so_usecount,
		       so->so_retaincnt,
		       so->xso_filter_flags);
	}

	if (sa->sun_len)
		printf(" %.*s",
		       (int)(sa->sun_len - offsetof(struct sockaddr_un, sun_path)),
		       sa->sun_path);
	putchar('\n');
}

void
unixpr_n(void)
{
	int    type;

	for (type = SOCK_STREAM; type <= SOCK_DGRAM; type++) {
		int which = 0;
		char *buf = NULL, *next;
		size_t len;
		struct xunpgen *xug, *oxug;
		struct xgen_n *xgn;
		struct xunpcb_n *xunp = NULL;
		struct xsocket_n *so = NULL;
		struct xsockbuf_n *so_rcv = NULL;
		struct xsockbuf_n *so_snd = NULL;
		struct xsockstat_n *so_stat = NULL;
		char mibvar[sizeof "net.local.xxxxxxxxxxxxxx.pcblist_n"];

		snprintf(mibvar, sizeof(mibvar), "net.local.%s.pcblist_n", socktype[type]);

		len = 0;
		if (sysctlbyname(mibvar, 0, &len, 0, 0) < 0) {
			warn("sysctl: %s", mibvar);
			return;
		}
		if ((buf = malloc(len)) == 0) {
			warn("malloc %lu bytes", (u_long)len);
			return;
		}
		if (sysctlbyname(mibvar, buf, &len, 0, 0) < 0) {
			warn("sysctl: %s", mibvar);
			free(buf);
			return;
		}

		oxug = xug = (struct xunpgen *)buf;

		for (next = buf + ROUNDUP64(xug->xug_len); next < buf + len; next += ROUNDUP64(xgn->xgn_len)) {
			xgn = (struct xgen_n*)next;

			if (xgn->xgn_len <= sizeof(struct xgen_n))
				break;

			if ((which & xgn->xgn_kind) == 0) {
				which |= xgn->xgn_kind;
				switch (xgn->xgn_kind) {
					case XSO_SOCKET:
						so = (struct xsocket_n *)xgn;
						break;
					case XSO_RCVBUF:
						so_rcv = (struct xsockbuf_n *)xgn;
						break;
					case XSO_SNDBUF:
						so_snd = (struct xsockbuf_n *)xgn;
						break;
					case XSO_STATS:
						so_stat = (struct xsockstat_n *)xgn;
						break;
					case XSO_UNPCB:
						xunp = (struct xunpcb_n *)xgn;
						break;
					default:
						/* It's OK to have some extra bytes at the end */
						if (which != 0) {
							printf("unexpected kind %d which 0x%x\n", xgn->xgn_kind, which);
						}
						break;
				}
			} else {
				if (vflag)
					printf("got %d twice\n", xgn->xgn_kind);
			}
			if (which == ALL_XGN_KIND_UNPCB) {
				which = 0;

				if (xunp->xunp_gencnt > oxug->xug_gen)
					continue;
				unixdomainpr_n(xunp, so, so_rcv, so_snd, so_stat);

			}
		}

		if (xug != oxug && xug->xug_gen != oxug->xug_gen) {
			if (oxug->xug_count > xug->xug_count) {
				printf("Some %s sockets may have been deleted.\n",
				       socktype[type]);
			} else if (oxug->xug_count < xug->xug_count) {
				printf("Some %s sockets may have been created.\n",
				       socktype[type]);
			} else {
				printf("Some %s sockets may have been created or deleted\n",
				       socktype[type]);
			}
		}
		free(buf);
	}
}

void
unixpr(uint32_t proto, char *name, int af)
{
#pragma unused(proto, name, af)
	unixpr_n();
}

void
unixstats(uint32_t proto, char *name, int af)
{
#pragma unused(proto, name, af)
	uint32_t pcbcount;
	size_t len;

	printf("local (UNIX):\n");

	len = sizeof(pcbcount);
	if (sysctlbyname("net.local.pcbcount", &pcbcount, &len, 0, 0) < 0) {
		warn("sysctl: net.local.pcbcount");
		return;
	}

	printf("\t%u open local socket%s\n", pcbcount, plural(pcbcount));
}

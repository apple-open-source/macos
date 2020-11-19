/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

/*
 * Display protocol blocks in the vsock domain.
 */
#include <sys/proc_info.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/vsock.h>

#include <errno.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include "netstat.h"

#ifdef AF_VSOCK

static void vsockdomainpr __P((struct xvsockpcb *));

void
vsockpr(uint32_t proto,
char *name, int af)
{
	char   *buf, *next;
	size_t len;
	struct xvsockpgen *xvg, *oxvg;
	struct xvsockpcb *xpcb;

	const char* mibvar = "net.vsock.pcblist";
	len = 0;
	if (sysctlbyname(mibvar, 0, &len, 0, 0) < 0) {
		if (errno != ENOENT)
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

	/*
	 * Bail-out to avoid logic error in the loop below when
	 * there is in fact no more control block to process
	 */
	if (len <= 2 * sizeof(struct xvsockpgen)) {
		free(buf);
		return;
	}

	oxvg = (struct xvsockpgen *)buf;

	// Save room for the last xvsockpgen.
	len -= oxvg->xvg_len;

	for (next = buf + oxvg->xvg_len; next < buf + len; next += xpcb->xv_len) {
		xpcb = (struct xvsockpcb *)next;

		/* Ignore PCBs which were freed during copyout. */
		if (xpcb->xvp_gencnt > oxvg->xvg_gen)
			continue;
		vsockdomainpr(xpcb);
	}
	xvg = (struct xvsockpgen *)next;
	if (xvg != oxvg && xvg->xvg_gen != oxvg->xvg_gen) {
		if (oxvg->xvg_count > xvg->xvg_count) {
			printf("Some vsock sockets may have been deleted.\n");
		} else if (oxvg->xvg_count < xvg->xvg_count) {
			printf("Some vsock sockets may have been created.\n");
		} else {
			printf("Some vsock sockets may have been created or deleted.\n");
		}
	}
	free(buf);
}

static void
vsock_print_addr(buf, cid, port)
	char *buf;
	uint32_t cid;
	uint32_t port;
{
	if (cid == VMADDR_CID_ANY && port == VMADDR_PORT_ANY) {
		(void) sprintf(buf, "*:*");
	} else if (cid == VMADDR_CID_ANY) {
		(void) sprintf(buf, "*:%u", port);
	} else if (port == VMADDR_PORT_ANY) {
		(void) sprintf(buf, "%u:*", cid);
	} else {
		(void) sprintf(buf, "%u:%u", cid, port);
	}
}

static void
vsockdomainpr(xpcb)
	struct xvsockpcb *xpcb;
{
	static int first = 1;

	if (first) {
		printf("Active VSock sockets\n");
		printf("%-5.5s %-6.6s %-6.6s %-6.6s %-18.18s %-18.18s %-11.11s",
			   "Proto", "Type",
			   "Recv-Q", "Send-Q",
			   "Local Address", "Foreign Address",
			   "State");
		if (vflag > 0)
			printf(" %10.10s %10.10s %10.10s %10.10s %6.6s %6.6s %6.6s %6s %10s",
				   "rxcnt", "txcnt", "peer_rxcnt", "peer_rxhiwat",
				   "rxhiwat", "txhiwat", "pid", "state", "options");
		printf("\n");
		first = 0;
	}

	struct xsocket *so = &xpcb->xv_socket;

	char srcAddr[50];
	char dstAddr[50];

	vsock_print_addr(srcAddr, xpcb->xvp_local_cid, xpcb->xvp_local_port);
	vsock_print_addr(dstAddr, xpcb->xvp_remote_cid, xpcb->xvp_remote_port);

	// Determine the vsock socket state.
	char *state;
	if (so->so_state & SOI_S_ISCONNECTING) {
		state = "CONNECTING";
	} else if (so->so_state & SOI_S_ISCONNECTED) {
		state = "ESTABLISHED";
	} else if (so->so_state & SOI_S_ISDISCONNECTING) {
		state = "CLOSING";
	} else if (so->so_options & SO_ACCEPTCONN) {
		state = "LISTEN";
	} else {
		state = "CLOSED";
	}

	printf("%-5.5s %-6.6s %6u %6u %-18s %-18s %-11s",
	       "vsock", "stream",
		   so->so_rcv.sb_cc, so->so_snd.sb_cc,
		   srcAddr, dstAddr,
	       state);
	if (vflag > 0)
		printf(" %10u %10u %10u %10u %6u %6u %6u 0x%04x 0x%08x",
			   xpcb->xvp_rxcnt,
			   xpcb->xvp_txcnt,
			   xpcb->xvp_peer_rxcnt,
			   xpcb->xvp_peer_rxhiwat,
			   so->so_rcv.sb_hiwat,
			   so->so_snd.sb_hiwat,
			   xpcb->xvp_last_pid,
			   so->so_state,
			   so->so_options);
	printf("\n");
}

#endif /* AF_VSOCK */

/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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


#include <sys/param.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>

#include <stdio.h>
#include "netstat.h"
#include <netat/sysglue.h>    /* To get Appletalk message/mbuf types */

#define	YES	1
typedef int bool;

struct	mbstat mbstat;

static struct mbtypes {
	int	mt_type;
	char	*mt_name;
} mbtypes[] = {
	{ MT_DATA,	"data" },
	{ MT_OOBDATA,	"oob data" },
	{ MT_CONTROL,	"ancillary data" },
	{ MT_HEADER,	"packet headers" },
	{ MT_SOCKET,	"socket structures" },			/* XXX */
	{ MT_PCB,	"protocol control blocks" },		/* XXX */
	{ MT_RTABLE,	"routing table entries" },		/* XXX */
	{ MT_HTABLE,	"IMP host table entries" },		/* XXX */
	{ MT_ATABLE,	"address resolution tables" },
	{ MT_FTABLE,	"fragment reassembly queue headers" },	/* XXX */
	{ MT_SONAME,	"socket names and addresses" },
	{ MT_SOOPTS,	"socket options" },
	{ MT_RIGHTS,	"access rights" },
	{ MT_IFADDR,	"interface addresses" },		/* XXX */
	{ MSG_DATA,	"Appletalk data blocks"},
	{ MSG_PROTO,	"Appletalk internal msgs"},
	{ MSG_IOCTL,	"Appletalk ioctl requests"},
	{ MSG_ERROR,	"Appletalk error indicators"},
	{ MSG_HANGUP,	"Appletalk termination requests"},
	{ MSG_IOCACK,	"Appletalk ioctl acks"},
	{ MSG_IOCNAK,	"Appletalk ioctl failure indicators"},
	{ MSG_CTL,	"Appletalk control msgs"},
	{ 0, 0 }
};

int nmbtypes = sizeof(mbstat.m_mtypes) / sizeof(short);
bool seen[256];			/* "have we seen this type yet?" */

/*
 * Print mbuf statistics.
 */
void
mbpr(void)
{
	int totmem, totfree, totmbufs;
	int i;
	struct mbtypes *mp;
	size_t len;

	if (nmbtypes != 256) {
		fprintf(stderr,
		    "netstat: unexpected change to mbstat; check source\n");
		return;
	}
	len = sizeof(mbstat);
	if (sysctlbyname("kern.ipc.mbstat", &mbstat, &len, 0, 0) == -1)
		return;

	totmbufs = 0;
	for (mp = mbtypes; mp->mt_name; mp++)
		totmbufs += mbstat.m_mtypes[mp->mt_type];
	printf("%u mbufs in use:\n", totmbufs);
	for (mp = mbtypes; mp->mt_name; mp++)
		if (mbstat.m_mtypes[mp->mt_type]) {
			seen[mp->mt_type] = YES;
			printf("\t%u mbufs allocated to %s\n",
			    mbstat.m_mtypes[mp->mt_type], mp->mt_name);
		}
	seen[MT_FREE] = YES;
	for (i = 0; i < nmbtypes; i++)
		if (!seen[i] && mbstat.m_mtypes[i]) {
			printf("\t%u mbufs allocated to <mbuf type %d>\n",
			    mbstat.m_mtypes[i], i);
		}
	printf("%u/%u mbuf clusters in use\n",
	       (unsigned int)(mbstat.m_clusters - mbstat.m_clfree),
	       (unsigned int)mbstat.m_clusters);
	printf("%u/%u mbuf 4KB clusters in use\n",
	       (unsigned int)(mbstat.m_bigclusters - mbstat.m_bigclfree),
	       (unsigned int)mbstat.m_bigclusters);
	totmem = totmbufs * MSIZE + mbstat.m_clusters * MCLBYTES + mbstat.m_bigclusters * mbstat.m_bigmclbytes;
	totfree = mbstat.m_clfree * MCLBYTES + mbstat.m_bigclfree * mbstat.m_bigmclbytes;
	printf("%u Kbytes allocated to network (%d%% in use)\n",
		totmem / 1024, (totmem - totfree) * 100 / totmem);
	printf("%u requests for memory denied\n",
	       (unsigned int)mbstat.m_drops);
	printf("%u requests for memory delayed\n", (unsigned int)mbstat.m_wait);
	printf("%u calls to protocol drain routines\n",
	       (unsigned int)mbstat.m_drain);
}

/*
 * Copyright (c) 1999-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* 
 * Copyright (c) 1997 Apple Computer, Inc. All Rights Reserved
 *
 * Copyright (c) 1983, 1989, 1993
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
 *	@(#)nfsstat.c	8.2 (Berkeley) 3/31/95
 */


#include <sys/param.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <kvm.h>
#include <nlist.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <err.h>

#define SHOW_SERVER 0x01
#define SHOW_CLIENT 0x02
#define SHOW_ALL (SHOW_SERVER | SHOW_CLIENT)

struct nlist nl[] = {
#define	N_NFSSTAT	0
	{ "_nfsstats" },
	{""},
};
kvm_t *kd;

static int deadkernel = 0;

void intpr __P((u_long, u_int));
void printhdr __P((void));
void sidewaysintpr __P((u_int, u_long, u_int));
void usage __P((void));

int
main(argc, argv)
	int argc;
	char **argv;
{
	extern int optind;
	extern char *optarg;
	u_int interval;
	u_int display = SHOW_ALL;
	int ch;
	char *memf, *nlistf;
	char errbuf[80];

	interval = 0;
	memf = nlistf = NULL;
	while ((ch = getopt(argc, argv, "M:N:w:sc")) != EOF)
		switch(ch) {
		case 'M':
			memf = optarg;
			break;
		case 'N':
			nlistf = optarg;
			break;
		case 'w':
			interval = atoi(optarg);
			break;
		case 's':
			display = SHOW_SERVER;
			break;
		case 'c':
			display = SHOW_CLIENT;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

#define	BACKWARD_COMPATIBILITY
#ifdef	BACKWARD_COMPATIBILITY
	if (*argv) {
		interval = atoi(*argv);
		if (*++argv) {
			nlistf = *argv;
			if (*++argv)
				memf = *argv;
		}
	}
#endif
	/*
	 * Discard setgid privileges if not the running kernel so that bad
	 * guys can't print interesting stuff from kernel memory.
	 */
	if (nlistf != NULL || memf != NULL) {
		setegid(getgid());
		setgid(getgid());
		deadkernel = 1;

		if ((kd = kvm_openfiles(nlistf, memf, NULL, O_RDONLY,
					errbuf)) == 0) {
			errx(1, "kvm_openfiles: %s", errbuf);
		}
		if (kvm_nlist(kd, nl) != 0) {
			errx(1, "kvm_nlist: can't get names");
		}
	}

	if (interval)
		sidewaysintpr(interval, nl[N_NFSSTAT].n_value, display);
	else
		intpr(nl[N_NFSSTAT].n_value, display);
	exit(0);
}

/*
 * Read the nfs stats using sysctl(3) for live kernels, or kvm_read
 * for dead ones.
 */
void
readstats(stp)
	struct nfsstats *stp;
{
	if(deadkernel) {
		if(kvm_read(kd, (u_long)nl[N_NFSSTAT].n_value, stp,
			    sizeof *stp) < 0) {
			err(1, "kvm_read");
		}
	} else {
		int name[3];
		size_t buflen = sizeof *stp;
		struct vfsconf vfc;

		if (getvfsbyname("nfs", &vfc) < 0)
			err(1, "getvfsbyname: NFS not compiled into kernel");
		name[0] = CTL_VFS;
		name[1] = vfc.vfc_typenum;
		name[2] = NFS_NFSSTATS;
		if (sysctl(name, 3, stp, &buflen, (void *)0, (size_t)0) < 0) {
			err(1, "sysctl");
		}
	}
}

/*
 * Print a description of the nfs stats.
 */
void
intpr(nfsstataddr, display)
	u_long nfsstataddr;
	u_int display;
{
	struct nfsstats nfsstats;

	readstats(&nfsstats);

	if (display & SHOW_CLIENT) {
		printf("Client Info:\n");
		printf("Rpc Counts:\n");
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
		       "Getattr", "Setattr", "Lookup", "Readlink", "Read",
		       "Write", "Create", "Remove");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		       nfsstats.rpccnt[NFSPROC_GETATTR],
		       nfsstats.rpccnt[NFSPROC_SETATTR],
		       nfsstats.rpccnt[NFSPROC_LOOKUP],
		       nfsstats.rpccnt[NFSPROC_READLINK],
		       nfsstats.rpccnt[NFSPROC_READ],
		       nfsstats.rpccnt[NFSPROC_WRITE],
		       nfsstats.rpccnt[NFSPROC_CREATE],
		       nfsstats.rpccnt[NFSPROC_REMOVE]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
		       "Rename", "Link", "Symlink", "Mkdir", "Rmdir",
		       "Readdir", "RdirPlus", "Access");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		       nfsstats.rpccnt[NFSPROC_RENAME],
		       nfsstats.rpccnt[NFSPROC_LINK],
		       nfsstats.rpccnt[NFSPROC_SYMLINK],
		       nfsstats.rpccnt[NFSPROC_MKDIR],
		       nfsstats.rpccnt[NFSPROC_RMDIR],
		       nfsstats.rpccnt[NFSPROC_READDIR],
		       nfsstats.rpccnt[NFSPROC_READDIRPLUS],
		       nfsstats.rpccnt[NFSPROC_ACCESS]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s\n",
		       "Mknod", "Fsstat", "Fsinfo", "PathConf", "Commit");
		printf("%9d %9d %9d %9d %9d\n",
		       nfsstats.rpccnt[NFSPROC_MKNOD],
		       nfsstats.rpccnt[NFSPROC_FSSTAT],
		       nfsstats.rpccnt[NFSPROC_FSINFO],
		       nfsstats.rpccnt[NFSPROC_PATHCONF],
		       nfsstats.rpccnt[NFSPROC_COMMIT]);
		printf("Rpc Info:\n");
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s\n",
		       "TimedOut", "Invalid", "X Replies", "Retries", "Requests");
		printf("%9d %9d %9d %9d %9d\n",
		       nfsstats.rpctimeouts,
		       nfsstats.rpcinvalid,
		       nfsstats.rpcunexpected,
		       nfsstats.rpcretries,
		       nfsstats.rpcrequests);
		printf("Cache Info:\n");
		printf("%9.9s %9.9s %9.9s %9.9s",
		       "Attr Hits", "Misses", "Lkup Hits", "Misses");
		printf(" %9.9s %9.9s %9.9s %9.9s\n",
		       "BioR Hits", "Misses", "BioW Hits", "Misses");
		printf("%9d %9d %9d %9d",
		       nfsstats.attrcache_hits, nfsstats.attrcache_misses,
		       nfsstats.lookupcache_hits, nfsstats.lookupcache_misses);
		printf(" %9d %9d %9d %9d\n",
		       nfsstats.biocache_reads-nfsstats.read_bios,
		       nfsstats.read_bios,
		       nfsstats.biocache_writes-nfsstats.write_bios,
		       nfsstats.write_bios);
		printf("%9.9s %9.9s %9.9s %9.9s",
		       "BioRLHits", "Misses", "BioD Hits", "Misses");
		printf(" %9.9s %9.9s\n", "DirE Hits", "Misses");
		printf("%9d %9d %9d %9d",
		       nfsstats.biocache_readlinks-nfsstats.readlink_bios,
		       nfsstats.readlink_bios,
		       nfsstats.biocache_readdirs-nfsstats.readdir_bios,
		       nfsstats.readdir_bios);
		printf(" %9d %9d\n",
		       nfsstats.direofcache_hits, nfsstats.direofcache_misses);
        }
	if (display & SHOW_SERVER) {
		printf("\nServer Info:\n");
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
		       "Getattr", "Setattr", "Lookup", "Readlink", "Read",
		       "Write", "Create", "Remove");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		       nfsstats.srvrpccnt[NFSPROC_GETATTR],
		       nfsstats.srvrpccnt[NFSPROC_SETATTR],
		       nfsstats.srvrpccnt[NFSPROC_LOOKUP],
		       nfsstats.srvrpccnt[NFSPROC_READLINK],
		       nfsstats.srvrpccnt[NFSPROC_READ],
		       nfsstats.srvrpccnt[NFSPROC_WRITE],
		       nfsstats.srvrpccnt[NFSPROC_CREATE],
		       nfsstats.srvrpccnt[NFSPROC_REMOVE]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s %9.9s\n",
		       "Rename", "Link", "Symlink", "Mkdir", "Rmdir",
		       "Readdir", "RdirPlus", "Access");
		printf("%9d %9d %9d %9d %9d %9d %9d %9d\n",
		       nfsstats.srvrpccnt[NFSPROC_RENAME],
		       nfsstats.srvrpccnt[NFSPROC_LINK],
		       nfsstats.srvrpccnt[NFSPROC_SYMLINK],
		       nfsstats.srvrpccnt[NFSPROC_MKDIR],
		       nfsstats.srvrpccnt[NFSPROC_RMDIR],
		       nfsstats.srvrpccnt[NFSPROC_READDIR],
		       nfsstats.srvrpccnt[NFSPROC_READDIRPLUS],
		       nfsstats.srvrpccnt[NFSPROC_ACCESS]);
		printf("%9.9s %9.9s %9.9s %9.9s %9.9s\n",
		       "Mknod", "Fsstat", "Fsinfo", "PathConf", "Commit");
		printf("%9d %9d %9d %9d %9d\n",
		       nfsstats.srvrpccnt[NFSPROC_MKNOD],
		       nfsstats.srvrpccnt[NFSPROC_FSSTAT],
		       nfsstats.srvrpccnt[NFSPROC_FSINFO],
		       nfsstats.srvrpccnt[NFSPROC_PATHCONF],
		       nfsstats.srvrpccnt[NFSPROC_COMMIT]);
		printf("Server Ret-Failed\n");
		printf("%17d\n", nfsstats.srvrpc_errs);
		printf("Server Faults\n");
		printf("%13d\n", nfsstats.srv_errs);
		printf("Server Cache Stats:\n");
		printf("%9.9s %9.9s %9.9s %9.9s\n",
		       "Inprog", "Idem", "Non-idem", "Misses");
		printf("%9d %9d %9d %9d\n",
		       nfsstats.srvcache_inproghits,
		       nfsstats.srvcache_idemdonehits,
		       nfsstats.srvcache_nonidemdonehits,
		       nfsstats.srvcache_misses);
		printf("Server Write Gathering:\n");
		printf("%9.9s %9.9s %9.9s\n",
		       "WriteOps", "WriteRPC", "Opsaved");
		printf("%9d %9d %9d\n",
		       nfsstats.srvvop_writes,
		       nfsstats.srvrpccnt[NFSPROC_WRITE],
		       nfsstats.srvrpccnt[NFSPROC_WRITE] - nfsstats.srvvop_writes);
	}
}

u_char	signalled;			/* set if alarm goes off "early" */

/*
 * Print a running summary of nfs statistics.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed at top of screen is always cumulative.
 */
void
sidewaysintpr(interval, off, display)
	u_int interval;
	u_long off;
	u_int display;
{
	struct nfsstats nfsstats, lastst;
	int hdrcnt, oldmask;
	void catchalarm();

	(void)signal(SIGALRM, catchalarm);
	signalled = 0;
	(void)alarm(interval);
	bzero((caddr_t)&lastst, sizeof(lastst));

	for (hdrcnt = 1;;) {
		if (!--hdrcnt) {
			printhdr();
			hdrcnt = 20;
		}
		readstats(&nfsstats);
		if (display & SHOW_CLIENT)
		  printf("Client: %8d %8d %8d %8d %8d %8d %8d %8d\n",
		    nfsstats.rpccnt[NFSPROC_GETATTR]-lastst.rpccnt[NFSPROC_GETATTR],
		    nfsstats.rpccnt[NFSPROC_LOOKUP]-lastst.rpccnt[NFSPROC_LOOKUP],
		    nfsstats.rpccnt[NFSPROC_READLINK]-lastst.rpccnt[NFSPROC_READLINK],
		    nfsstats.rpccnt[NFSPROC_READ]-lastst.rpccnt[NFSPROC_READ],
		    nfsstats.rpccnt[NFSPROC_WRITE]-lastst.rpccnt[NFSPROC_WRITE],
		    nfsstats.rpccnt[NFSPROC_RENAME]-lastst.rpccnt[NFSPROC_RENAME],
		    nfsstats.rpccnt[NFSPROC_ACCESS]-lastst.rpccnt[NFSPROC_ACCESS],
		    (nfsstats.rpccnt[NFSPROC_READDIR]-lastst.rpccnt[NFSPROC_READDIR])
		    +(nfsstats.rpccnt[NFSPROC_READDIRPLUS]-lastst.rpccnt[NFSPROC_READDIRPLUS]));
		if (display & SHOW_SERVER)
		  printf("Server: %8d %8d %8d %8d %8d %8d %8d %8d\n",
		    nfsstats.srvrpccnt[NFSPROC_GETATTR]-lastst.srvrpccnt[NFSPROC_GETATTR],
		    nfsstats.srvrpccnt[NFSPROC_LOOKUP]-lastst.srvrpccnt[NFSPROC_LOOKUP],
		    nfsstats.srvrpccnt[NFSPROC_READLINK]-lastst.srvrpccnt[NFSPROC_READLINK],
		    nfsstats.srvrpccnt[NFSPROC_READ]-lastst.srvrpccnt[NFSPROC_READ],
		    nfsstats.srvrpccnt[NFSPROC_WRITE]-lastst.srvrpccnt[NFSPROC_WRITE],
		    nfsstats.srvrpccnt[NFSPROC_RENAME]-lastst.srvrpccnt[NFSPROC_RENAME],
		    nfsstats.srvrpccnt[NFSPROC_ACCESS]-lastst.srvrpccnt[NFSPROC_ACCESS],
		    (nfsstats.srvrpccnt[NFSPROC_READDIR]-lastst.srvrpccnt[NFSPROC_READDIR])
		    +(nfsstats.srvrpccnt[NFSPROC_READDIRPLUS]-lastst.srvrpccnt[NFSPROC_READDIRPLUS]));
		lastst = nfsstats;
		fflush(stdout);
		oldmask = sigblock(sigmask(SIGALRM));
		if (!signalled)
			sigpause(0);
		sigsetmask(oldmask);
		signalled = 0;
		(void)alarm(interval);
	}
	/*NOTREACHED*/
}

void
printhdr()
{
	printf("        %8.8s %8.8s %8.8s %8.8s %8.8s %8.8s %8.8s %8.8s\n",
	    "Getattr", "Lookup", "Readlink", "Read", "Write", "Rename",
	    "Access", "Readdir");
	fflush(stdout);
}

/*
 * Called if an interval expires before sidewaysintpr has completed a loop.
 * Sets a flag to not wait for the alarm.
 */
void
catchalarm()
{
	signalled = 1;
}

void
usage()
{
	(void)fprintf(stderr,
	    "usage: nfsstat [-cs] [-M core] [-N system] [-w interval]\n");
	exit(1);
}

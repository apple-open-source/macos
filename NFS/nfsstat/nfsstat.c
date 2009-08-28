/*
 * Copyright (c) 1999-2008 Apple Inc. All rights reserved.
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
#include <sys/time.h>
#include <sys/socket.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <pwd.h>
#include <sys/queue.h>
#include <err.h>

#define SHOW_SERVER 0x01
#define SHOW_CLIENT 0x02
#define SHOW_ALL (SHOW_SERVER | SHOW_CLIENT)

#define CLIENT_SERVER_MODE 0
#define EXPORTS_MODE 1
#define ACTIVE_USER_MODE 2

#define NUMERIC_NET 1
#define NUMERIC_USER 2

#define DEFAULT_EXPORT_STATS_BUFLEN 32768
#define DEFAULT_ACTIVE_USER_STATS_BUFLEN 131072

LIST_HEAD(nfs_active_user_node_head, nfs_active_user_node);
LIST_HEAD(nfs_export_node_head, nfs_export_node);

struct nfs_active_user_node {
	LIST_ENTRY(nfs_active_user_node)	user_next;
        struct nfs_user_stat_user_rec		*rec;
};

struct nfs_export_node {
	LIST_ENTRY(nfs_export_node)		export_next;
	struct nfs_active_user_node_head	nodes;
        struct nfs_user_stat_path_rec		*rec;
};

struct nfs_active_user_list {
	char				*buf;
	struct nfs_export_node_head	export_list;
};

void intpr(u_int);
void printhdr(void);
void sidewaysintpr(u_int, u_int);
void usage(void);
void do_exports_interval(u_int interval);
void do_exports_normal(void);
void do_active_users_interval(u_int interval, u_int flags);
void do_active_users_normal(u_int flags);
int  read_export_stats(char **buf, uint *buflen);
int  read_active_user_stats(char **buf, uint *buflen);
void display_export_diffs(char *newb, char *oldb);
void display_active_user_diffs(char *newb, char *oldb, u_int flags);
void displayActiveUserRec(struct nfs_user_stat_user_rec *rec, u_int flags);
struct nfs_export_stat_rec *findExport(char *path, char *buf);
struct nfs_user_stat_user_rec *findActiveUser(char *path, struct nfs_user_stat_user_rec *rec, char *buf);
int cmp_active_user(struct nfs_user_stat_user_rec *rec1, struct nfs_user_stat_user_rec *rec2);
uint64_t serialdiff_64(uint64_t new, uint64_t old);
struct nfs_export_node_head *get_sorted_active_user_list(char *buf);
void free_nfs_export_list(struct nfs_export_node_head *export_list);
void catchalarm(int);

int
main(int argc, char *argv[])
{
	extern int optind;
	extern char *optarg;
	u_int interval;
	u_int display = SHOW_ALL;
	u_int usermode_flags = 0;
	u_int mode = CLIENT_SERVER_MODE;
	int ch;

	interval = 0;
	while ((ch = getopt(argc, argv, "w:sceun:")) != EOF)
		switch(ch) {
		case 'w':
			interval = atoi(optarg);
			break;
		case 's':
			mode = CLIENT_SERVER_MODE;
			display = SHOW_SERVER;
			break;
		case 'c':
			mode = CLIENT_SERVER_MODE;
			display = SHOW_CLIENT;
			break;
		case 'e':
			mode = EXPORTS_MODE;
			break;
		case 'u':
			mode = ACTIVE_USER_MODE;
			break;
		case 'n':
			if (!strcmp(optarg, "net"))
				usermode_flags |= NUMERIC_NET;
			else if (!strcmp(optarg, "user"))
				usermode_flags |= NUMERIC_USER;
			else
				printf("unsupported 'n' argument\n");
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/* Determine the mode and display the stats */
	if (mode == EXPORTS_MODE) {
		if (interval)
			do_exports_interval(interval);
		else
			do_exports_normal();
	} else if (mode == ACTIVE_USER_MODE) {
		if (interval)
			do_active_users_interval(interval, usermode_flags);
		else
			do_active_users_normal(usermode_flags);
	} else {
		if (interval)
			sidewaysintpr(interval, display);
		else
			intpr(display);
	}
	exit(0);
}

/*
 * Read the nfs stats using sysctl(3)
 */
void
readstats(struct nfsstats *stp)
{
	int name[3];
	size_t buflen = sizeof(*stp);
	struct vfsconf vfc;

	if (getvfsbyname("nfs", &vfc) < 0)
		err(1, "getvfsbyname: NFS not compiled into kernel");
	name[0] = CTL_VFS;
	name[1] = vfc.vfc_typenum;
	name[2] = NFS_NFSSTATS;
	if (sysctl(name, 3, stp, &buflen, NULL, 0) < 0)
		err(1, "sysctl");
}

/*
 * Read the nfs export stats from the kernel. 
 *
 * If a valid buffer (non-NULL) is passed in parameter buf, then
 * the parameter buflen must also be valid. In this case an attempt will
 * be made to read the export stat records from the kernel using the passed buffer.
 * However if the passed buffer is of insufficient size to hold all available
 * export stat records, then the passed buffer will be freed and a new one
 * will be allocated.  Thus the returned buffer may not be the same as
 * the buffer originally passed in the function call.
 *
 * If a NULL buffer is passed in parameter buf, a new buffer will
 * be allocated. 
 * 
 * Returns:
 *  0	if successful,  valid buffer containing export stat records returned in buf, and buflen is valid.
 *  1	if not enough memory was available.  NULL buffer returned in buf,  buflen is undefined.
 */
int
read_export_stats(char **buf, uint *buflen)
{
	struct nfs_export_stat_desc *hdr;
	int	name[3];
	uint	statlen;
	struct	vfsconf vfc;

	/* check if we need to allocate a buffer */
	if (*buf == NULL) {
		*buflen = DEFAULT_EXPORT_STATS_BUFLEN;
		*buf = malloc(*buflen);
		if (*buf == NULL) {
			warnx("No memory for reading export stat data");
			return 1;
		}
	}

	if (getvfsbyname("nfs", &vfc) < 0)
	{
		free(*buf);
		err(1, "getvfsbyname: NFS not compiled into kernel");
	}

	name[0] = CTL_VFS;
	name[1] = vfc.vfc_typenum;
	name[2] = NFS_EXPORTSTATS;

	/* fetch the export stats */
	statlen = *buflen;
	if (sysctl(name, 3, *buf, (size_t *)&statlen, NULL, 0) < 0) {
		/* sysctl failed */
		free(*buf);
		err(1, "sysctl failed");
	}

	/* check if buffer was large enough */
	if (statlen > *buflen) {
		/* Didn't get all the stat records, try again with a larger buffer */
		/* Alloc a larger buffer than the sysctl indicated, in case more exports */
		/* get added before we make the next sysctl */
		free(*buf);
		*buflen = statlen + (4 * sizeof(struct nfs_export_stat_rec));
		*buf = malloc(*buflen);
		if (*buf == NULL) {
			warnx("No mem for reading export statistics");
			return 1;
		}

		/* fetch export stats one more time */
		statlen = *buflen;
		if (sysctl(name, 3, *buf, (size_t *)&statlen, NULL, 0) < 0) {
			free(*buf);
			err(1, "sysctl failed");
		}
	}

	/* Check export stat record version */
	hdr = (struct nfs_export_stat_desc *)*buf;
	if (hdr->rec_vers != NFS_EXPORT_STAT_REC_VERSION) {
		free(*buf);
		errx(1, "NFS export stat version mismatch");
	}

	return 0;
}

/*
 * Read nfs active user stats from the kernel.
 * 
 * If a valid buffer (non-NULL) is passed in parameter buf, then
 * the parameter buflen must also be valid. In this case an attempt will
 * be made to read the user stat records from the kernel using the passed buffer.
 * However if the passed buffer is of insufficient size to hold all available
 * user stat records, then the passed buffer will be freed and a new one
 * will be allocated.  Thus the returned buffer may not be the same as 
 * the buffer originally passed in the function call.
 *
 * If a NULL buffer is passed in parameter buf, a new buffer will
 * be allocated.
 *
 * Returns:
 *  0   if successful,  valid buffer containing export active user stat records returned in buf, and buflen is valid.
 *  1   if not enough memory was available.  NULL buffer returned in buf,  buflen is undefined.
 */
int
read_active_user_stats(char **buf, uint *buflen)
{
	struct nfs_user_stat_desc	*hdr;
	int				name[3];
	uint				statlen;
	struct				vfsconf vfc;

	/* check if we need to allocate a buffer */ 
	if (*buf == NULL) {
		*buflen = DEFAULT_ACTIVE_USER_STATS_BUFLEN;
		*buf = malloc(*buflen);
		if (*buf == NULL) {
			warnx("Non mem for reading active user statistics");
			return 1;
		}
	}

	if (getvfsbyname("nfs", &vfc) < 0)
	{
		free(*buf);
		err(1, "getvfsbyname: NFS not compiled into kernel");
	}

	name[0] = CTL_VFS;
	name[1] = vfc.vfc_typenum;
	name[2] = NFS_USERSTATS;

	/* fetch the user stats */
	statlen = *buflen;
	if (sysctl(name, 3, *buf, (size_t *)&statlen, NULL, 0) < 0) {
		/* sysctl failed */
		free(*buf);
		err(1, "sysctl failed");
	}
 
	/* check if buffer was large enough */
	if (statlen > *buflen) {
		/* Didn't get all the stat records, try again with a larger buffer. */
		free(*buf);

		/* Allocate a little extra than indicated in case more exports and/or users */
		/* show up before the next sysctl */
		*buflen = statlen + sizeof(struct nfs_user_stat_path_rec);
		*buflen += (4 * sizeof(struct nfs_user_stat_user_rec));
		*buf = malloc(*buflen);
		if (*buf == NULL) {
			warnx("No mem for reading active user statistics\n");
			return 1;
		}

		/* fetch user stats one more time */
		statlen = *buflen;
		if (sysctl(name, 3, *buf, (size_t *)&statlen, NULL, 0) < 0) {
			free(*buf);
			err(1, "sysctl failed");
		}
	}

	/* Check record version */
	hdr = (struct nfs_user_stat_desc *)*buf;
	if (hdr->rec_vers != NFS_USER_STAT_REC_VERSION) {
		free(*buf);
		errx(1, "NFS user stat version mismatch");
	}

	return 0;
}

/*
 * Print a description of the nfs stats.
 */
void
intpr(u_int display)
{
	struct nfsstats nfsstats;

	readstats(&nfsstats);

	if (display & SHOW_CLIENT) {
		printf("Client Info:\n");
		printf("RPC Counts:\n");
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
		printf("RPC Info:\n");
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
		printf("RPC Counts:\n");
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
sidewaysintpr(u_int interval, u_int display)
{
	struct nfsstats nfsstats, lastst;
	int hdrcnt;
	sigset_t sigset, oldsigset;

	signal(SIGALRM, catchalarm);
	signalled = 0;
	alarm(interval);
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
		sigemptyset(&sigset);
		sigaddset(&sigset, SIGALRM);
		if (sigprocmask(SIG_BLOCK, &sigset, &oldsigset) == -1)
			err(1, "sigprocmask failed");
		if (!signalled) {
			sigemptyset(&sigset);
			sigsuspend(&sigset);
		}
		if (sigprocmask(SIG_SETMASK, &oldsigset, NULL) == -1)
			err(1, "sigprocmask failed");
		signalled = 0;
		alarm(interval);
	}
	/*NOTREACHED*/
}


/* ************************ */
/* *** Per-export Stats *** */
/* ************************ */

/*
 * Print the stats of all nfs exported directories
 */
void
do_exports_normal(void)
{
	struct nfs_export_stat_desc *stat_desc;
	struct nfs_export_stat_rec  *rec;
	char  *buf;
	uint  bufLen, i, recs;

	/* Read in export stats from the kernel */
	buf = NULL;

	/* check for failure  */
	if (read_export_stats(&buf, &bufLen))
		return;

	/* check for empty export table */
	stat_desc = (struct nfs_export_stat_desc *)buf;
	recs = stat_desc->rec_count;
	if (!recs) {
		printf("No export stat data found\n");
		free(buf);
		return;
	}

	/* init record pointer to position following stat descriptor */
	rec = (struct nfs_export_stat_rec *)(buf + sizeof(struct nfs_export_stat_desc));

	/* print out a header */
	printf("Exported Directory Info:\n");
	printf("%12s  %12s  %12s\n", "Requests", "Read Bytes", "Write Bytes");

	/* loop through, printing out stats of each export */
	for(i = 0; i < recs; i++)
		printf("%12llu  %12llu  %12llu  %s\n", rec[i].ops,
		rec[i].bytes_read, rec[i].bytes_written, rec[i].path);

	/* clean up */
	free(buf);
}

/*
 * Print a running summary of nfs export statistics.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed at top of screen is always cumulative.
 */
void
do_exports_interval(u_int interval)
{
	char *oldExportBuf, *newExportBuf, *tmpBuf;
	uint oldLen, newLen, tmpLen;
	int hdrcnt;
	sigset_t sigset, oldsigset;

	oldExportBuf = newExportBuf = NULL;
	oldLen = newLen = 0;

	signal(SIGALRM, catchalarm);
	signalled = 0;
	alarm(interval);

	for (hdrcnt = 1;;) {
		if (!--hdrcnt) {
			printf("%12s  %12s  %12s\n", "Requests", "Read Bytes", "Write Bytes");
			fflush(stdout);
			hdrcnt = 20;
		}

		if (read_export_stats(&newExportBuf, &newLen) == 0) {
			display_export_diffs(newExportBuf, oldExportBuf);
			tmpBuf = oldExportBuf;
			tmpLen = oldLen;
			oldExportBuf = newExportBuf;
			oldLen = newLen;
			newExportBuf = tmpBuf;
			newLen = tmpLen;
		}

		fflush(stdout);
		sigemptyset(&sigset);
		sigaddset(&sigset, SIGALRM);
		if (sigprocmask(SIG_BLOCK, &sigset, &oldsigset) == -1)
			err(1, "sigprocmask failed");
		if (!signalled) {
			sigemptyset(&sigset);
			sigsuspend(&sigset);
		}
		if (sigprocmask(SIG_SETMASK, &oldsigset, NULL) == -1)
			err(1, "sigprocmask failed");
		signalled = 0;
		alarm(interval);
	}
}

void
display_export_diffs(char *newb, char *oldb)
{
	struct nfs_export_stat_desc *hdr;
	struct nfs_export_stat_rec  *rec, *oldRec;
	uint i, recs;

	if (newb == NULL)
		return;

	/* Determine how many records in newb */
	hdr = (struct nfs_export_stat_desc *)newb;
	recs = hdr->rec_count;

	/* check for empty export table */
	if (!recs) {
		printf("No exported directories found\n");
		return;
	}

	/* initialize rec pointer */
	rec = (struct nfs_export_stat_rec *)(newb + sizeof(struct nfs_export_stat_desc));

	for(i = 0; i < recs; i++) {
		/* find old export record for this path */
		oldRec = findExport(rec[i].path, oldb);
		if (oldRec != NULL) {
			printf("%12llu %12llu %12llu %s\n",
			(rec[i].ops >= oldRec->ops) ?
				rec[i].ops - oldRec->ops : oldRec->ops - rec[i].ops,
			(rec[i].bytes_read >= oldRec->bytes_read) ?
				rec[i].bytes_read - oldRec->bytes_read : oldRec->bytes_read - rec[i].bytes_read,
			(rec[i].bytes_written >= oldRec->bytes_written) ?
				rec[i].bytes_written - oldRec->bytes_written : oldRec->bytes_written - rec[i].bytes_written,
			rec[i].path);
		}
		else 
			printf("%12llu %12llu %12llu %s\n",
				rec[i].ops, rec[i].bytes_read, rec[i].bytes_written, rec[i].path);
	}
}

struct nfs_export_stat_rec *
findExport(char *path, char *buf)
{
	struct nfs_export_stat_desc *hdr;
	struct nfs_export_stat_rec  *rec, *retRec;
	uint    i, recs;

	retRec = NULL;

	if (buf == NULL)
		return retRec;

	/* determine how many records in buf */
	hdr = (struct nfs_export_stat_desc *)buf;
	recs = hdr->rec_count;

	/* check if no records to compare */
	if (!recs)
		return retRec;

	/* initialize our rec pointer */
	rec = (struct nfs_export_stat_rec *)(buf + sizeof(struct nfs_export_stat_desc));

	for(i = 0; i < recs; i++) {
		if (strcmp(path, rec[i].path) == 0) {
			/* found a match */
			retRec = &rec[i];
			break;
		}
	}

	return retRec;
}


/* ****************** */
/* *** User Stats *** */
/* ****************** */

/*
 * Print active user stats for each nfs exported directory
 */
void
do_active_users_normal(u_int flags)
{
	struct nfs_user_stat_desc	*stat_desc;
	struct nfs_export_node_head	*export_list;
	struct nfs_export_node		*export_node;
	struct nfs_active_user_node	*unode;
	char				*buf;
	uint				bufLen, recs;

	/* Read in user stats from the kernel */
	buf = NULL;
	if (read_active_user_stats(&buf, &bufLen))
		return;

	/* check for empty user list */
	stat_desc = (struct nfs_user_stat_desc *)buf;
	recs = stat_desc->rec_count;
	if (!recs) {
		printf("No NFS active user statistics found.\n");
		free(buf);
		return;
	}

	/* get a sorted list */
	export_list = get_sorted_active_user_list(buf);
	if (!export_list) {
		printf("Not enough  memory for displaying active user statistics\n");
		free(buf);
		return;
	}

	/* print out a header */
	printf("NFS Active User Info:\n");

	LIST_FOREACH(export_node, export_list, export_next) {
		printf("%s\n", export_node->rec->path);
		printf("%12s  %12s  %12s  %-7s  %-8s %s\n",
		    "Requests", "Read Bytes", "Write Bytes",
		    "Idle", "User", "IP Address");

		LIST_FOREACH(unode, &export_node->nodes, user_next)
			displayActiveUserRec(unode->rec, flags);
	}

	/* clean up */
	free_nfs_export_list(export_list);
	free(export_list);
	free(buf);
}

/*
 * Print a running summary of nfs active user statistics.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed at top of screen is always cumulative.
 */
void
do_active_users_interval(u_int interval, u_int flags)
{
	char		*oldBuf, *newBuf, *tmpBuf;
	uint		oldLen, newLen, tmpLen;
	int		hdrcnt; 
	sigset_t	sigset, oldsigset;
        
	oldBuf = newBuf = NULL;
	oldLen = newLen = 0;
        
	signal(SIGALRM, catchalarm);
	signalled = 0;
	alarm(interval);
        
	for (hdrcnt = 1;;) {
		if (!--hdrcnt) {
			printf("%12s  %12s  %12s  %-7s  %-8s %s\n",
			    "Requests", "Read Bytes", "Write Bytes",
			    "Idle", "User", "IP Address");
			fflush(stdout);
			hdrcnt = 20;
		}
                
		if (read_active_user_stats(&newBuf, &newLen) == 0) {
			display_active_user_diffs(newBuf, oldBuf, flags);
			tmpBuf = oldBuf;
			tmpLen = oldLen;
			oldBuf = newBuf;
			oldLen = newLen;
			newBuf = tmpBuf;
			newLen = tmpLen;
		}

		fflush(stdout);
		sigemptyset(&sigset);
		sigaddset(&sigset, SIGALRM);
		if (sigprocmask(SIG_BLOCK, &sigset, &oldsigset) == -1)
			err(1, "sigprocmask failed");
		if (!signalled) {
			sigemptyset(&sigset);
			sigsuspend(&sigset);
		}
		if (sigprocmask(SIG_SETMASK, &oldsigset, NULL) == -1)
			err(1, "sigprocmask failed");
		signalled = 0;
		alarm(interval);
	}
}

void    
display_active_user_diffs(char *newb, char *oldb, u_int flags)
{
	struct nfs_user_stat_desc	*stat_desc;
	struct nfs_export_node_head	*export_list;
	struct nfs_export_node		*export_node;
	struct nfs_active_user_node	*unode;

	struct nfs_user_stat_user_rec	*rec, *oldrec, diffrec;
	struct nfs_user_stat_path_rec	*pathrec;

	if (newb == NULL)
		return;

	/* check for empty export table */
	stat_desc = (struct nfs_user_stat_desc *)newb;
	if (!stat_desc->rec_count) {
		printf("No NFS active user statistics found.\n");
		return;
	}

	/* get a sorted list from newb */
	export_list = get_sorted_active_user_list(newb);
	if (!export_list) {
		printf("Not enough  memory for displaying active user statistics\n");
		return;
	}

	LIST_FOREACH(export_node, export_list, export_next) {
		pathrec = export_node->rec;
		printf("%s\n", export_node->rec->path);

		LIST_FOREACH(unode, &export_node->nodes, user_next) {
			rec = unode->rec;
			/* check for old record */
			oldrec = findActiveUser(pathrec->path, rec, oldb);

			if (oldrec != NULL) {
				/* setup diff rec */
				diffrec.uid = rec->uid;
				diffrec.tm_start = rec->tm_start;
				diffrec.tm_last = rec->tm_last;
				diffrec.ops = serialdiff_64(rec->ops, oldrec->ops);
				diffrec.bytes_read = serialdiff_64(rec->bytes_read, oldrec->bytes_read);
				diffrec.bytes_written = serialdiff_64(rec->bytes_written, oldrec->bytes_written);
				bcopy(&rec->sock, &diffrec.sock, rec->sock.ss_len);

				/* display differential record */
				displayActiveUserRec(&diffrec, flags);
			}
			else
				displayActiveUserRec(rec, flags);
		}
	}

	/* clean up */
	free_nfs_export_list(export_list);
	free(export_list);
}

struct nfs_user_stat_user_rec *
findActiveUser(char *path, struct nfs_user_stat_user_rec *rec, char *buf)
{
	struct nfs_user_stat_desc	*stat_desc;
	struct nfs_user_stat_user_rec	*tmpRec, *retRec;
	struct nfs_user_stat_path_rec	*pathrec;
	char				*bufp;
	uint				i, recs, scan_state;

#define FIND_EXPORT 0
#define FIND_USER 1

	scan_state = FIND_EXPORT;

	retRec = NULL;
        
	if (buf == NULL)
		return retRec;          
        
	/* determine how many records in buf */
	stat_desc = (struct nfs_user_stat_desc *)buf;
	recs = stat_desc->rec_count;          
        
	/* check if no records to compare */
	if (!recs)
		return retRec;
        
	/* initialize buf pointer */
	bufp = buf + sizeof(struct nfs_user_stat_desc);

	for(i = 0; i < recs; i++) {
		switch(*bufp) {
			case NFS_USER_STAT_PATH_REC:
				if(scan_state == FIND_EXPORT) {
					pathrec = (struct nfs_user_stat_path_rec *)bufp;
					if(!strcmp(path, pathrec->path))
						scan_state = FIND_USER;
				}
				else {
					/* encountered the next export, didn't find user. */
					goto done;
				}
				bufp += sizeof(struct nfs_user_stat_path_rec);
				break;
			case NFS_USER_STAT_USER_REC:
				if(scan_state == FIND_USER) {
					tmpRec = (struct nfs_user_stat_user_rec *)bufp;
					if (!cmp_active_user(rec, tmpRec)) {
						/* found a match */
						retRec = tmpRec;
						goto done;
					}
				}
				bufp += sizeof(struct nfs_user_stat_user_rec);
				break;
			default:
				goto done;
		}
	}

done:
	return retRec;
}

void
displayActiveUserRec(struct nfs_user_stat_user_rec *rec, u_int flags)
{
	struct sockaddr_in	*in; 
	struct sockaddr_in6	*in6;
	struct hostent		*hp = NULL;
	struct passwd		*pw;
	struct timeval		now;
	struct timezone		tz; 
	uint32_t		now32, hr, min, sec;
	char			addrbuf[NI_MAXHOST];
	char			unknown[] = "* * * *";
	char			*addr = unknown;

	/* get current time for calculating idle time */
	gettimeofday(&now, &tz);

	/* calculate idle hour, min sec */
	now32 = (uint32_t)now.tv_sec;
	if (now32 >= rec->tm_last)
		sec = now32 - rec->tm_last;
	else    
		sec = ~(rec->tm_last - now32) + 1;
	hr = sec / 3600;
	sec %= 3600;
	min = sec / 60;
	sec %= 60;
                                
	/* setup ip address string */
	if (rec->sock.ss_family == AF_INET) {
		/* ipv4 */
		in = (struct sockaddr_in *)&rec->sock;
		if (!(flags & NUMERIC_NET))
			hp = gethostbyaddr((char *)&in->sin_addr, sizeof(in->sin_addr), AF_INET);
		if (hp && hp->h_name)
			addr = hp->h_name;
		else if (inet_ntop(AF_INET, &in->sin_addr, addrbuf, sizeof(addrbuf)))
			addr = addrbuf;
	} else if (rec->sock.ss_family == AF_INET6) {
		/* ipv6 */
		in6 = (struct sockaddr_in6 *)&rec->sock;
		if (!(flags & NUMERIC_NET))
			hp = gethostbyaddr((char *)&in6->sin6_addr, sizeof(in6->sin6_addr), AF_INET6);
		if (hp && hp->h_name)
			addr = hp->h_name;
		else if (inet_ntop(AF_INET6, &in6->sin6_addr, addrbuf, sizeof(addrbuf)))
			addr = addrbuf;
	}
                                        
	if ((flags & NUMERIC_USER) || !(pw = getpwuid(rec->uid))) {
		/* print uid */
		printf("%12llu  %12llu  %12llu  %1u:%02u:%02u  %-8u %s\n",
		    rec->ops, rec->bytes_read, rec->bytes_written,
		    hr, min, sec, rec->uid, addr);
	} else {
		/* print user name */
		printf("%12llu  %12llu  %12llu  %1u:%02u:%02u  %-8.8s %s\n",
		    rec->ops, rec->bytes_read, rec->bytes_written,
		    hr, min, sec, pw->pw_name, addr);
	}
}

/* Returns zero if both uid and IP address fields match */
int
cmp_active_user(struct nfs_user_stat_user_rec *rec1, struct nfs_user_stat_user_rec *rec2)
{
	struct sockaddr_in	*ipv4_sock1, *ipv4_sock2;
	struct sockaddr_in6	*ipv6_sock1, *ipv6_sock2;
	int			retVal = 1;

	/* check uid */
	if (rec1->uid != rec2->uid)
		return retVal;

	/* check address length */
	if (rec1->sock.ss_len != rec2->sock.ss_len)
		return retVal;

	/* Check address family */
	if (rec1->sock.ss_family != rec2->sock.ss_family)
		return retVal;

	if (rec1->sock.ss_family == AF_INET) {
		/* IPv4 */ 
		ipv4_sock1 = (struct sockaddr_in *)&rec1->sock;
		ipv4_sock2 = (struct sockaddr_in *)&rec2->sock;
                
		if (!bcmp(&ipv4_sock1->sin_addr, &ipv4_sock2->sin_addr, sizeof(struct in_addr)))
			retVal = 0;
 
	}
	else {
		/* IPv6 */
		ipv6_sock1 = (struct sockaddr_in6 *)&rec1->sock;
		ipv6_sock2 = (struct sockaddr_in6 *)&rec2->sock;

		if (!bcmp(&ipv6_sock1->sin6_addr, &ipv6_sock2->sin6_addr, sizeof(struct in6_addr)))
			retVal = 0;
	}

	return retVal;
}

struct nfs_export_node_head *
get_sorted_active_user_list(char *buf)
{
	struct nfs_user_stat_desc	*stat_desc;
	struct nfs_export_node_head	*export_list;
	struct nfs_export_node		*export_node;
	struct nfs_active_user_node	*unode, *unode_before, *unode_after;
	char				*bufp;
	uint				i, recs, err;

	/* first check for empty user list */
	stat_desc = (struct nfs_user_stat_desc *)buf;
	recs = stat_desc->rec_count;
	if (!recs) 
		return NULL;

	export_list = (struct nfs_export_node_head *)malloc(sizeof(struct nfs_export_node_head));
	if (export_list == NULL) 
		return NULL;
	LIST_INIT(export_list);
		export_node = NULL;
	err = 0;

	/* init record pointer to position following the stat descriptor */
	bufp = buf + sizeof(struct nfs_user_stat_desc);
                
	/* loop through, printing out each record */
	for(i = 0; i < recs; i++) {
		switch(*bufp) {
			case NFS_USER_STAT_PATH_REC:
				/* create a new export node */
				export_node  = (struct nfs_export_node *)malloc(sizeof(struct nfs_export_node));
				if (export_node == NULL) {
					err = 1;
					goto done_err;
				}
				LIST_INIT(&export_node->nodes);
				export_node->rec = (struct nfs_user_stat_path_rec *)bufp;
				LIST_INSERT_HEAD(export_list, export_node, export_next);

				bufp += sizeof(struct nfs_user_stat_path_rec);
				break;
			case NFS_USER_STAT_USER_REC:
				if (export_node == NULL) {
					err = 1;
					goto done_err;
				}
				/* create a new user node */
				unode = (struct nfs_active_user_node *)malloc(sizeof(struct nfs_active_user_node));
				if (unode == NULL) {
					err = 1;
					goto done_err;
				}
				unode->rec = (struct nfs_user_stat_user_rec *)bufp;

				/* insert in decending order */
				unode_before = NULL;
				LIST_FOREACH(unode_after, &export_node->nodes, user_next) {
					if (unode->rec->tm_last > unode_after->rec->tm_last)
						break;
					unode_before = unode_after;
				}
				if (unode_after)
					LIST_INSERT_BEFORE(unode_after, unode, user_next);
				else if (unode_before)
					LIST_INSERT_AFTER(unode_before, unode, user_next);
				else
					LIST_INSERT_HEAD(&export_node->nodes, unode, user_next);

				bufp += sizeof(struct nfs_user_stat_user_rec);
				break;
        
			default:
				printf("nfsstat: unexpected record type 0x%02x in active user data stream\n", *bufp);
				err = 1;
				goto done_err;
		}
	}

done_err:
	if (err) {
		free_nfs_export_list(export_list);
		free(export_list);
		export_list = NULL;
	}
	return(export_list);
}

void
free_nfs_export_list(struct nfs_export_node_head *export_list)
{
	struct nfs_export_node		*exp_node;
	struct nfs_active_user_node	*unode;

	while ((exp_node = LIST_FIRST(export_list))) {
		LIST_REMOVE(exp_node, export_next);
		while((unode = LIST_FIRST(&exp_node->nodes))) {
			LIST_REMOVE(unode, user_next);
			free(unode);
		}
		free(exp_node);
	}
}

uint64_t
serialdiff_64(uint64_t new, uint64_t old)
{
        if(new > old)
                return (new - old);
        else
                return ( ~(old - new) +  1);
}


void
printhdr(void)
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
catchalarm(__unused int dummy)
{
	signalled = 1;
}

void
usage(void)
{
	fprintf(stderr, "usage: nfsstat [-cseu] [-w interval] [-n user|net]\n");
	exit(1);
}

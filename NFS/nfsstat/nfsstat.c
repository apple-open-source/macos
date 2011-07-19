/*
 * Copyright (c) 1999-2011 Apple Inc. All rights reserved.
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

#define _NFS_XDR_SUBS_FUNCS_ /* define this to get xdrbuf function definitions */
#include <nfs/xdr_subs.h>

int verbose = 0;

#define SHOW_SERVER 0x01
#define SHOW_CLIENT 0x02
#define SHOW_ALL (SHOW_SERVER | SHOW_CLIENT)

#define CLIENT_SERVER_MODE 0
#define EXPORTS_MODE 1
#define ACTIVE_USER_MODE 2
#define MOUNT_MODE 3

#define NUMERIC_NET 1
#define NUMERIC_USER 2

#define DEFAULT_EXPORT_STATS_BUFLEN 32768
#define DEFAULT_ACTIVE_USER_STATS_BUFLEN 131072
#define DEFAULT_MOUNT_INFO_BUFLEN 4096

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
void do_mountinfo(char *);
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
	while ((ch = getopt(argc, argv, "w:sceun:mv")) != EOF)
		switch(ch) {
		case 'v':
			verbose++;
			break;
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
		case 'm':
			mode = MOUNT_MODE;
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
	if (mode == MOUNT_MODE) {
		do_mountinfo(argc ? argv[0] : NULL);
	} else if (mode == EXPORTS_MODE) {
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
		/* make sure to touch/init the entire buffer */
		bzero(*buf, *buflen);
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
			warnx("No memory for reading export statistics");
			return 1;
		}
		/* make sure to touch/init the entire buffer */
		bzero(*buf, *buflen);

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
			warnx("No memory for reading active user statistics");
			return 1;
		}
		/* make sure to touch/init the entire buffer */
		bzero(*buf, *buflen);
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
		*buflen = statlen + (5 * sizeof(struct nfs_user_stat_path_rec));
		*buf = malloc(*buflen);
		if (*buf == NULL) {
			warnx("No memory for reading active user statistics\n");
			return 1;
		}
		/* make sure to touch/init the entire buffer */
		bzero(*buf, *buflen);

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
 * Read nfs mount info from the kernel.
 * 
 * If a valid buffer (non-NULL) is passed in parameter buf, then
 * the parameter buflen must also be valid. In this case an attempt will
 * be made to read the mount info from the kernel using the passed buffer.
 * However if the passed buffer is of insufficient size to hold the entire
 * mount info record, then the passed buffer will be freed and a new one
 * will be allocated.  Thus the returned buffer may not be the same as 
 * the buffer originally passed in the function call.
 *
 * If a NULL buffer is passed in parameter buf, a new buffer will
 * be allocated.
 *
 * Returns:
 *  0   if successful, valid buffer containing mount info returned in buf, and buflen is valid.
 *  1   if not enough memory was available.  NULL buffer returned in buf,  buflen is undefined.
 */
int
read_mountinfo(fsid_t *fsid, char **buf, uint *buflen)
{
	uint32_t vers, *bufxdr;
	int name[3];
	size_t infolen;
	struct vfsconf vfc;

	/* check if we need to allocate a buffer */ 
	if (*buf == NULL) {
		*buflen = DEFAULT_MOUNT_INFO_BUFLEN;
		*buf = malloc(*buflen);
		if (*buf == NULL) {
			warnx("No memory for reading mount information");
			return (ENOMEM);
		}
		/* make sure to touch/init the entire buffer */
		bzero(*buf, *buflen);
	}

	if (getvfsbyname("nfs", &vfc) < 0)
		err(1, "getvfsbyname: NFS not compiled into kernel");

	name[0] = CTL_VFS;
	name[1] = vfc.vfc_typenum;
	name[2] = NFS_MOUNTINFO;

	/* copy fsid to buffer to tell kernel which fs to read info for */
	bufxdr = (uint32_t*)*buf;
	bufxdr[0] = htonl(fsid->val[0]);
	bufxdr[1] = htonl(fsid->val[1]);

	/* fetch the mount info */
	infolen = *buflen;
	if (sysctl(name, 3, *buf, &infolen, NULL, 0) < 0) {
		/* sysctl failed */
		warn("sysctl failed");
		return (errno);
	}
 
	/* check if buffer was large enough */
	if (infolen > *buflen) {
		/* Didn't have large enough buffer, try again with a larger buffer. */
		free(*buf);

		*buflen = infolen;
		*buf = malloc(*buflen);
		if (*buf == NULL) {
			warnx("No memory for reading mount information\n");
			return (ENOMEM);
		}
		/* make sure to touch/init the entire buffer */
		bzero(*buf, *buflen);

		/* copy fsid to buffer to tell kernel which fs to read info for */
		bufxdr = (uint32_t*)*buf;
		bufxdr[0] = htonl(fsid->val[0]);
		bufxdr[1] = htonl(fsid->val[1]);

		/* fetch mount information one more time */
		infolen = *buflen;
		if (sysctl(name, 3, *buf, (size_t *)&infolen, NULL, 0) < 0) {
			warn("sysctl failed");
			return (errno);
		}
	}

	/* Check mount information version */
	vers = ntohl(*(uint32_t*)*buf);
	if (vers != NFS_MOUNT_INFO_VERSION) {
		warnx("NFS mount information version mismatch");
		return (EBADRPC);
	}

	return (0);
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

/* ****************** */
/* *** Mount Info *** */
/* ****************** */

struct nfs_fs_server {
	char *		name;					/* server name */
	char **		addrs;					/* array of addresses */
	uint32_t	addrcount;				/* # of addresses */
};

struct nfs_fs_location {
	struct nfs_fs_server *servers;				/* array of servers */
	char **		components;				/* array of path components */
	uint32_t	servcount;				/* # of servers */
	uint32_t	compcount;				/* # of path components */
};

struct mountargs {
	uint32_t	margsvers;				/* what mount args version was used */
	int		mntflags;				/* MNT_* flags */
	uint32_t	mattrs[NFS_MATTR_BITMAP_LEN];		/* what attrs are set */
	uint32_t	mflags_mask[NFS_MFLAG_BITMAP_LEN];	/* what flags are set */
	uint32_t	mflags[NFS_MFLAG_BITMAP_LEN];		/* set flag values */
	uint32_t	nfs_version, nfs_minor_version;		/* NFS version */
	uint32_t	rsize, wsize, readdirsize, readahead;	/* I/O values */
	struct timespec	acregmin, acregmax, acdirmin, acdirmax;	/* attrcache values */
	uint32_t	lockmode;				/* advisory file locking mode */
	struct nfs_sec	sec;					/* security flavors */
	uint32_t	maxgrouplist;				/* max AUTH_SYS groups */
	char		sotype[6];				/* socket type */
	uint32_t	nfs_port, mount_port;			/* port info */
	struct timespec	request_timeout;			/* NFS request timeout */
	uint32_t	soft_retry_count;			/* soft retrans count */
	struct timespec	dead_timeout;				/* dead timeout value */
	fhandle_t	fh;					/* initial file handle */
	uint32_t	numlocs;				/* # of fs locations */
	struct nfs_fs_location *locs;				/* array of fs locations */
	char *		mntfrom;				/* mntfrom mount arg */
};

void
mountargs_cleanup(struct mountargs *margs)
{
	uint32_t loc, serv, addr, comp;

	for (loc=0; loc < margs->numlocs; loc++) {
		if (!margs->locs)
			break;
		for (serv=0; serv < margs->locs[loc].servcount; serv++) {
			if (!margs->locs[loc].servers)
				break;
			for (addr=0; addr < margs->locs[loc].servers[serv].addrcount; addr++) {
				if (!margs->locs[loc].servers[serv].addrs || !margs->locs[loc].servers[serv].addrs[addr])
					continue;
				free(margs->locs[loc].servers[serv].addrs[addr]);
				margs->locs[loc].servers[serv].addrs[addr] = NULL;
			}
			if (margs->locs[loc].servers[serv].addrs) {
				free(margs->locs[loc].servers[serv].addrs);
				margs->locs[loc].servers[serv].addrs = NULL;
			}
			margs->locs[loc].servers[serv].addrcount = 0;
			if (margs->locs[loc].servers[serv].name) {
				free(margs->locs[loc].servers[serv].name);
				margs->locs[loc].servers[serv].name = NULL;
			}
		}
		if (margs->locs[loc].servers) {
			free(margs->locs[loc].servers);
			margs->locs[loc].servers = NULL;
		}
		margs->locs[loc].servcount = 0;
		for (comp=0; comp < margs->locs[loc].compcount; comp++) {
			if (!margs->locs[loc].components || !margs->locs[loc].components[comp])
				continue;
			if (margs->locs[loc].components[comp]) {
				free(margs->locs[loc].components[comp]);
				margs->locs[loc].components[comp] = NULL;
			}
		}
		if (margs->locs[loc].components) {
			free(margs->locs[loc].components);
			margs->locs[loc].components = NULL;
		}
		margs->locs[loc].compcount = 0;
	}
	if (margs->locs) {
		free(margs->locs);
		margs->locs = NULL;
	}
	margs->numlocs = 0;
	if (margs->mntfrom) {
		free(margs->mntfrom);
		margs->mntfrom = NULL;
	}
}

int
parse_mountargs(struct xdrbuf *xb, int margslen, struct mountargs *margs)
{
	uint32_t val, attrslength, loc, serv, addr, comp;
	int error = 0, i;

	if (margslen <= XDRWORD*2)
		return (EBADRPC);
	xb_get_32(error, xb, margs->margsvers);			/* mount args version */
	if (margs->margsvers > NFS_ARGSVERSION_XDR)
		return (EBADRPC);
	xb_get_32(error, xb, val);				/* mount args length */
	if (val != (uint32_t)margslen)
		return (EBADRPC);
	xb_get_32(error, xb, val);				/* xdr args version */
	if (val != NFS_XDRARGS_VERSION_0)
		return (EINVAL);
	val = NFS_MATTR_BITMAP_LEN;
	xb_get_bitmap(error, xb, margs->mattrs, val);
	xb_get_32(error, xb, attrslength);
	if (attrslength > ((uint32_t)margslen - ((4+NFS_MATTR_BITMAP_LEN+1)*XDRWORD)))
		return (EINVAL);
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_FLAGS)) {
		val = NFS_MFLAG_BITMAP_LEN;
		xb_get_bitmap(error, xb, margs->mflags_mask, val);
		val = NFS_MFLAG_BITMAP_LEN;
		xb_get_bitmap(error, xb, margs->mflags, val);
	}
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_NFS_VERSION))
		xb_get_32(error, xb, margs->nfs_version);
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_NFS_MINOR_VERSION))
		xb_get_32(error, xb, margs->nfs_minor_version);
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_READ_SIZE))
		xb_get_32(error, xb, margs->rsize);
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_WRITE_SIZE))
		xb_get_32(error, xb, margs->wsize);
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_READDIR_SIZE))
		xb_get_32(error, xb, margs->readdirsize);
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_READAHEAD))
		xb_get_32(error, xb, margs->readahead);
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_ATTRCACHE_REG_MIN)) {
		xb_get_32(error, xb, margs->acregmin.tv_sec);
		xb_get_32(error, xb, margs->acregmin.tv_nsec);
	}
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_ATTRCACHE_REG_MAX)) {
		xb_get_32(error, xb, margs->acregmax.tv_sec);
		xb_get_32(error, xb, margs->acregmax.tv_nsec);
	}
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_ATTRCACHE_DIR_MIN)) {
		xb_get_32(error, xb, margs->acdirmin.tv_sec);
		xb_get_32(error, xb, margs->acdirmin.tv_nsec);
	}
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_ATTRCACHE_DIR_MAX)) {
		xb_get_32(error, xb, margs->acdirmax.tv_sec);
		xb_get_32(error, xb, margs->acdirmax.tv_nsec);
	}
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_LOCK_MODE))
		xb_get_32(error, xb, margs->lockmode);
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_SECURITY)) {
		xb_get_32(error, xb, margs->sec.count);
		for (i=0; i < margs->sec.count; i++)
			xb_get_32(error, xb, margs->sec.flavors[i]);
	}
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_MAX_GROUP_LIST))
		xb_get_32(error, xb, margs->maxgrouplist);
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_SOCKET_TYPE)) {
		xb_get_32(error, xb, val);		/* socket type length */
		if (val >= sizeof(margs->sotype))
			error = EBADRPC;
		if (!error)
			error = xb_get_bytes(xb, margs->sotype, val, 0);
		margs->sotype[val] = '\0';
	}
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_NFS_PORT))
		xb_get_32(error, xb, margs->nfs_port);
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_MOUNT_PORT))
		xb_get_32(error, xb, margs->mount_port);
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_REQUEST_TIMEOUT)) {
		xb_get_32(error, xb, margs->request_timeout.tv_sec);
		xb_get_32(error, xb, margs->request_timeout.tv_nsec);
	}
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_SOFT_RETRY_COUNT))
		xb_get_32(error, xb, margs->soft_retry_count);
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_DEAD_TIMEOUT)) {
		xb_get_32(error, xb, margs->dead_timeout.tv_sec);
		xb_get_32(error, xb, margs->dead_timeout.tv_nsec);
	}
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_FH)) {
		xb_get_32(error, xb, margs->fh.fh_len);
		if (!error)
			error = xb_get_bytes(xb, (char*)&margs->fh.fh_data[0], margs->fh.fh_len, 0);
	}
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_FS_LOCATIONS)) {
		xb_get_32(error, xb, margs->numlocs);                        /* FS_LOCATIONS */
		if (!error && (margs->numlocs > 255))
			error = EINVAL;
		if (!error && margs->numlocs) {
			margs->locs = calloc(margs->numlocs, sizeof(struct nfs_fs_location));
			if (!margs->locs)
				error = ENOMEM;
		}
		for (loc = 0; !error && (loc < margs->numlocs); loc++) {
			xb_get_32(error, xb, margs->locs[loc].servcount);
			if (!error && (margs->locs[loc].servcount > 255))
				error = EINVAL;
			if (!error && margs->locs[loc].servcount) {
				margs->locs[loc].servers = calloc(margs->locs[loc].servcount, sizeof(struct nfs_fs_server));
				if (!margs->locs[loc].servers)
					error = ENOMEM;
			}
			for (serv = 0; !error && (serv < margs->locs[loc].servcount); serv++) {
				xb_get_32(error, xb, val);
				if (!error && (val > MAXPATHLEN))
					error = EINVAL;
				if (!error && val) {
					margs->locs[loc].servers[serv].name = calloc(1, val+1);
					if (!margs->locs[loc].servers[serv].name)
						error = ENOMEM;
				}
				if (!error)
					error = xb_get_bytes(xb, margs->locs[loc].servers[serv].name, val, 0);
				xb_get_32(error, xb, margs->locs[loc].servers[serv].addrcount);
				if (!error && (margs->locs[loc].servers[serv].addrcount > 255))
					error = EINVAL;
				if (!error && margs->locs[loc].servers[serv].addrcount) {
					margs->locs[loc].servers[serv].addrs = calloc(margs->locs[loc].servers[serv].addrcount, sizeof(char*));
					if (!margs->locs[loc].servers[serv].addrs)
						error = ENOMEM;
				}
				for (addr = 0; !error && (addr < margs->locs[loc].servers[serv].addrcount); addr++) {
					xb_get_32(error, xb, val);
					if (!error && (val > 255))
						error = EINVAL;
					if (!error && val) {
						margs->locs[loc].servers[serv].addrs[addr] = calloc(1, val+1);
						if (!margs->locs[loc].servers[serv].addrs[addr])
							error = ENOMEM;
					}
					if (!error)
						error = xb_get_bytes(xb, margs->locs[loc].servers[serv].addrs[addr], val, 0);
				}
				xb_get_32(error, xb, val);
				xb_skip(error, xb, val);	/* skip server info */
			}
			xb_get_32(error, xb, margs->locs[loc].compcount);
			if (!error && (val > MAXPATHLEN))
				error = EINVAL;
			if (!error && margs->locs[loc].compcount) {
				margs->locs[loc].components = calloc(margs->locs[loc].compcount, sizeof(char*));
				if (!margs->locs[loc].components)
					error = ENOMEM;
			}
			for (comp = 0; !error && (comp < margs->locs[loc].compcount); comp++) {
				xb_get_32(error, xb, val);
				if (!error && (val > MAXPATHLEN))
					error = EINVAL;
				if (!error && val) {
					margs->locs[loc].components[comp] = calloc(1, val+1);
					if (!margs->locs[loc].components[comp])
						error = ENOMEM;
				}
				if (!error)
					error = xb_get_bytes(xb, margs->locs[loc].components[comp], val, 0);
			}
			xb_get_32(error, xb, val);
			xb_skip(error, xb, val);	/* skip fs loction info */
		}
	}
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_MNTFLAGS))
		xb_get_32(error, xb, margs->mntflags);
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_MNTFROM)) {
		xb_get_32(error, xb, val);
		if (!error)
			margs->mntfrom = calloc(1, val+1);
		if (!margs->mntfrom)
			error = ENOMEM;
		if (!error)
			error = xb_get_bytes(xb, margs->mntfrom, val, 0);
	}

	if (error)
		mountargs_cleanup(margs);
	return (error);
}

/* Map from mount options to printable formats. */
struct opt {
	int o_opt;
	const char *o_name;
};
static struct opt optnames[] =
{
	{ MNT_RDONLY,		"ro" },
	{ MNT_ASYNC,		"async" },
	{ MNT_NODEV,		"nodev" },
	{ MNT_NOEXEC,		"noexec" },
	{ MNT_NOSUID,		"nosuid" },
	{ MNT_SYNCHRONOUS,	"sync" },
	{ MNT_UNION,		"union" },
	{ MNT_AUTOMOUNTED,	"automounted" },
	{ MNT_DEFWRITE, 	"defwrite" },
	{ MNT_IGNORE_OWNERSHIP,	"noowners" },
	{ MNT_NOATIME,		"noatime" },
	{ MNT_QUARANTINE,	"quarantine" },
	{ MNT_DONTBROWSE,	"nobrowse" },
	{ MNT_CPROTECT,		"protect"},
	{ MNT_NOUSERXATTR,	"nouserxattr"},
	{ 0, 			NULL }
};
static struct opt optnames2[] =
{
	{ MNT_ROOTFS,		"rootfs"},
	{ MNT_LOCAL,		"local" },
	{ MNT_JOURNALED,	"journaled" },
	{ MNT_DOVOLFS,		"dovolfs"},
	{ MNT_QUOTA,		"with quotas" },
	{ MNT_EXPORTED,		"NFS exported" },
	{ MNT_MULTILABEL,	"multilabel"},
	{ 0, 			NULL }
};

static const char *
sec_flavor_name(uint32_t flavor)
{
	switch(flavor) {
	case RPCAUTH_NONE:	return ("none");
	case RPCAUTH_SYS:	return ("sys");
	case RPCAUTH_KRB5:	return ("krb5");
	case RPCAUTH_KRB5I:	return ("krb5i");
	case RPCAUTH_KRB5P:	return ("krb5p");
	default:		return ("?");
	}
}

const char *
socket_type(char *sotype)
{
	if (!strcmp(sotype, "tcp"))
		return ("tcp");
	if (!strcmp(sotype, "udp"))
		return ("udp");
	if (!strcmp(sotype, "tcp4"))
		return ("proto=tcp");
	if (!strcmp(sotype, "udp4"))
		return ("proto=udp");
	if (!strcmp(sotype, "tcp6"))
		return ("proto=tcp6");
	if (!strcmp(sotype, "udp6"))
		return ("proto=udp6");
	if (!strcmp(sotype, "inet4"))
		return ("inet4");
	if (!strcmp(sotype, "inet6"))
		return ("inet6");
	if (!strcmp(sotype, "inet"))
		return ("inet");
	return (sotype);
}

void
print_mountargs(struct mountargs *margs, uint32_t origmargsvers)
{
	int i, flags;
	uint32_t loc, serv, addr, comp;
	struct opt *o;
	char sep;

	/* option separator is space for first option printed and comma for rest */
	sep = ' ';
	/* call this macro after printing to update separator */
#define SEP	sep=','

	flags = margs->mntflags;
	printf("     General mount flags: 0x%x", flags);
	for (o = optnames; flags && o->o_opt; o++)
		if (flags & o->o_opt) {
			printf("%c%s", sep, o->o_name);
			flags &= ~o->o_opt;
			SEP;
		}
	sep = ' ';
	for (o = optnames2; flags && o->o_opt; o++)
		if (flags & o->o_opt) {
			printf("%c%s", sep, o->o_name);
			flags &= ~o->o_opt;
			SEP;
		}
	printf("\n");

	sep = ' ';
	printf("     NFS parameters:");
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_NFS_VERSION)) {
		printf("%cvers=%d", sep, margs->nfs_version);
		if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_NFS_MINOR_VERSION))
			printf(".%d", margs->nfs_minor_version);
		SEP;
	}
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_SOCKET_TYPE))
		printf("%c%s", sep, socket_type(margs->sotype)), SEP;
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_NFS_PORT))
		printf("%cport=%d", sep, margs->nfs_port), SEP;
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_MOUNT_PORT))
		printf("%cmountport=%d", sep, margs->mount_port), SEP;
	if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_MNTUDP))
		printf("%c%smntudp", sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_MNTUDP) ? "" : "no"), SEP;
	if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_SOFT))
		printf("%c%s", sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_SOFT) ? "soft" : "hard"), SEP;
	if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_INTR))
		printf("%c%sintr", sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_INTR) ? "" : "no"), SEP;
	if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_RESVPORT))
		printf("%c%sresvport", sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_RESVPORT) ? "" : "no"), SEP;
	if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_NOCONNECT))
		printf("%c%sconn", sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_NOCONNECT) ? "no" : ""), SEP;
	if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_NOCALLBACK))
		printf("%c%scallback", sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_NOCALLBACK) ? "no" : ""), SEP;
	if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_NONEGNAMECACHE))
		printf("%c%snegnamecache", sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_NONEGNAMECACHE) ? "no" : ""), SEP;
	if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_NONAMEDATTR))
		printf("%c%snamedattr", sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_NONAMEDATTR) ? "no" : ""), SEP;
	if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_NOACL))
		printf("%c%sacl", sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_NOACL) ? "no" : ""), SEP;
	if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_ACLONLY))
		printf("%c%saclonly", sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_ACLONLY) ? "" : "no"), SEP;
	if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_CALLUMNT))
		printf("%c%scallumnt", sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_CALLUMNT) ? "" : "no"), SEP;
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_LOCK_MODE))
		switch(margs->lockmode) {
		case NFS_LOCK_MODE_ENABLED:
			printf("%clocks", sep);
			SEP;
			break;
		case NFS_LOCK_MODE_DISABLED:
			printf("%cnolocks", sep);
			SEP;
			break;
		case NFS_LOCK_MODE_LOCAL:
			printf("%clocallocks", sep);
			SEP;
			break;
		}
	if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_NOQUOTA))
		printf("%c%squota", sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_NOQUOTA) ? "no" : ""), SEP;
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_READ_SIZE))
		printf("%crsize=%d", sep, margs->rsize), SEP;
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_WRITE_SIZE))
		printf("%cwsize=%d", sep, margs->wsize), SEP;
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_READAHEAD))
		printf("%creadahead=%d", sep, margs->readahead), SEP;
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_READDIR_SIZE))
		printf("%cdsize=%d", sep, margs->readdirsize), SEP;
	if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_RDIRPLUS))
		printf("%c%srdirplus", sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_RDIRPLUS) ? "" : "no"), SEP;
	if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_DUMBTIMER))
		printf("%c%sdumbtimr", sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_DUMBTIMER) ? "" : "no"), SEP;
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_REQUEST_TIMEOUT))
		printf("%ctimeo=%ld", sep, ((margs->request_timeout.tv_sec * 10) + (margs->request_timeout.tv_nsec % 100000000))), SEP;
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_SOFT_RETRY_COUNT))
		printf("%cretrans=%d", sep, margs->soft_retry_count), SEP;
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_MAX_GROUP_LIST))
		printf("%cmaxgroups=%d", sep, margs->maxgrouplist), SEP;
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_ATTRCACHE_REG_MIN))
		printf("%cacregmin=%ld", sep, margs->acregmin.tv_sec), SEP;
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_ATTRCACHE_REG_MAX))
		printf("%cacregmax=%ld", sep, margs->acregmax.tv_sec), SEP;
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_ATTRCACHE_DIR_MIN))
		printf("%cacdirmin=%ld", sep, margs->acdirmin.tv_sec), SEP;
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_ATTRCACHE_DIR_MAX))
		printf("%cacdirmax=%ld", sep, margs->acdirmax.tv_sec), SEP;
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_DEAD_TIMEOUT))
		printf("%cdeadtimeout=%ld", sep, margs->dead_timeout.tv_sec), SEP;
	if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_MUTEJUKEBOX))
		printf("%c%smutejukebox", sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_MUTEJUKEBOX) ? "" : "no"), SEP;
	if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_EPHEMERAL))
		printf("%c%sephemeral", sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_EPHEMERAL) ? "" : "no"), SEP;
	if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_NFC))
		printf("%c%snfc", sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_NFC) ? "" : "no"), SEP;
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_SECURITY)) {
		printf("%csec=%s", sep, sec_flavor_name(margs->sec.flavors[0]));
		for (i=1; i < margs->sec.count; i++)
			printf(":%s", sec_flavor_name(margs->sec.flavors[i]));
		SEP;
	}
	printf("\n");

	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_FS_LOCATIONS)) {
		printf("     File system locations:\n");
		if (origmargsvers < NFS_ARGSVERSION_XDR) {
			printf("       %s", margs->mntfrom ? margs->mntfrom : "???");
			if (margs->numlocs && margs->locs[0].servcount &&
			    margs->locs[0].servers[0].addrcount &&
			    margs->locs[0].servers[0].addrs[0])
				printf(" (%s)", margs->locs[0].servers[0].addrs[0]);
			printf("\n");
		}
		if ((origmargsvers == NFS_ARGSVERSION_XDR) || verbose) {
			for (loc=0; loc < margs->numlocs; loc++) {
				printf("       ");
				if (!margs->locs[loc].compcount)
					printf("/");
				for (comp=0; comp < margs->locs[loc].compcount; comp++)
					printf("/%s", margs->locs[loc].components[comp]);
				printf(" @");
				for (serv=0; serv < margs->locs[loc].servcount; serv++) {
					printf(" %s (", margs->locs[loc].servers[serv].name);
					for (addr=0; addr < margs->locs[loc].servers[serv].addrcount; addr++)
						printf("%s%s", !addr ? "" : ",", margs->locs[loc].servers[serv].addrs[addr]);
					printf(")");
				}
				printf("\n");
			}
		}
	}
	if (verbose && NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_FH)) {
		printf("     fh %d ", margs->fh.fh_len);
		for (i=0; i < margs->fh.fh_len; i++)
			printf("%02x", margs->fh.fh_data[i] & 0xff);
		printf("\n");
	}
}

/*
 * Print the given mount info.
 */
void
print_mountinfo(struct statfs *mnt, char *buf, uint buflen)
{
	struct xdrbuf xb;
	uint32_t val, miattrs[NFS_MIATTR_BITMAP_LEN], miflags[NFS_MIFLAG_BITMAP_LEN];
	int error = 0, len;
	struct mountargs origargs, curargs;
	uint32_t flags, loc, serv, addr, comp;

	NFS_BITMAP_ZERO(miattrs, NFS_MIATTR_BITMAP_LEN);
	NFS_BITMAP_ZERO(miflags, NFS_MIFLAG_BITMAP_LEN);
	bzero(&origargs, sizeof(origargs));
	bzero(&curargs, sizeof(curargs));

	xb_init_buffer(&xb, buf, buflen);
	xb_get_32(error, &xb, val);			/* NFS_MOUNT_INFO_VERSION */
	if (error)
		goto out;
	if (val != NFS_MOUNT_INFO_VERSION) {
		printf("%s unknown mount info version %d\n\n", mnt->f_mntonname, val);
		return;
	}
	xb_get_32(error, &xb, val);			/* mount info length */
	if (error)
		goto out;
	if (val > buflen) {
		printf("%s bogus mount info length %d > %d\n\n", mnt->f_mntonname, val, buflen);
		return;
	}
	len = NFS_MIATTR_BITMAP_LEN;
	xb_get_bitmap(error, &xb, miattrs, len);
	if (NFS_BITMAP_ISSET(miattrs, NFS_MIATTR_FLAGS)) {
		len = NFS_MIFLAG_BITMAP_LEN;
		xb_get_bitmap(error, &xb, miflags, len);
	}
	if (error)
		goto out;

	printf("%s from %s\n", mnt->f_mntonname, mnt->f_mntfromname);

	if (NFS_BITMAP_ISSET(miattrs, NFS_MIATTR_ORIG_ARGS)) {
		xb_get_32(error, &xb, val);			/* original mount args length */
		if (!error)
			error = parse_mountargs(&xb, val, &origargs);
	}
	if (NFS_BITMAP_ISSET(miattrs, NFS_MIATTR_CUR_ARGS)) {
		xb_get_32(error, &xb, val);			/* current mount args length */
		if (!error)
			error = parse_mountargs(&xb, val, &curargs);
	}
	if (NFS_BITMAP_ISSET(miattrs, NFS_MIATTR_CUR_LOC_INDEX)) {
		xb_get_32(error, &xb, flags);
		xb_get_32(error, &xb, loc);
		xb_get_32(error, &xb, serv);
		xb_get_32(error, &xb, addr);
	}
	if (error)
		goto out;

	if (NFS_BITMAP_ISSET(miattrs, NFS_MIATTR_ORIG_ARGS)) {
		printf("  -- Original mount options:\n");
		print_mountargs(&origargs, origargs.margsvers);
	}
	if (NFS_BITMAP_ISSET(miattrs, NFS_MIATTR_CUR_ARGS)) {
		printf("  -- Current mount parameters:\n");
		print_mountargs(&curargs, origargs.margsvers);
		if (NFS_BITMAP_ISSET(miattrs, NFS_MIATTR_CUR_LOC_INDEX) &&
		    (verbose || (curargs.numlocs > 1) || (curargs.locs[0].servcount > 1) ||
		     (curargs.locs[0].servers[0].addrcount > 1))) {
			printf("     Current location: 0x%x %d %d %d: ", flags, loc, serv, addr);
			if ((loc >= curargs.numlocs) || (serv >= curargs.locs[loc].servcount) ||
			    (addr >= curargs.locs[loc].servers[serv].addrcount)) {
				printf("<invalid>\n");
			} else {
				printf("\n       ");
				if (!curargs.locs[loc].compcount)
					printf("/");
				for (comp=0; comp < curargs.locs[loc].compcount; comp++)
					printf("/%s", curargs.locs[loc].components[comp]);
				printf(" @ %s (%s)\n", curargs.locs[loc].servers[serv].name,
					curargs.locs[loc].servers[serv].addrs[addr]);
			}
		}
	}
	if (NFS_BITMAP_ISSET(miattrs, NFS_MIATTR_FLAGS)) {
		printf("     Status flags: 0x%x", miflags[0]);
		if (NFS_BITMAP_ISSET(miflags, NFS_MIFLAG_DEAD))
			printf(",dead");
		if (NFS_BITMAP_ISSET(miflags, NFS_MIFLAG_NOTRESP))
			printf(",not responding");
		if (NFS_BITMAP_ISSET(miflags, NFS_MIFLAG_RECOVERY))
			printf(",recovery");
		printf("\n");
	}
out:
	if (error)
		printf("%s error parsing mount info (%d)\n", mnt->f_mntonname, error);
	printf("\n");
	mountargs_cleanup(&origargs);
	mountargs_cleanup(&curargs);
}

/*
 * Print mount info for given mount (or all NFS mounts)
 */
void
do_mountinfo(char *mountpath)
{
	struct statfs *mntbuf;
	int i, mntsize;
	char *buf = NULL, *p;
	uint buflen = 0;

	if (mountpath) {
		/* be nice and strip any trailing slashes */
		p = mountpath + strlen(mountpath) - 1;
		while ((p > mountpath) && (*p == '/'))
			*p-- = '\0';
	}

	if ((mntsize = getmntinfo(&mntbuf, MNT_NOWAIT)) == 0)
		err(1, "getmntinfo");
	for (i = 0; i < mntsize; i++) {
		/* check if this mount is one we want */
		if (mountpath && (strcmp(mountpath, mntbuf[i].f_mntonname) || !strcmp(mntbuf[i].f_fstypename, "autofs")))
			continue;
		if (!mountpath && strcmp(mntbuf[i].f_fstypename, "nfs"))
			continue;
		/* Get the mount information. */
		if (read_mountinfo(&mntbuf[i].f_fsid, &buf, &buflen)) {
			warnx("Unable to get mount info for %s", mntbuf[i].f_mntonname);
			continue;
		}
		/* Print the mount information. */
		print_mountinfo(&mntbuf[i], buf, buflen);
	}

	/* clean up */
	if (buf)
		free(buf);
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
	fprintf(stderr, "usage: nfsstat [-cseuv] [-w interval] [-n user|net]\n");
	exit(1);
}

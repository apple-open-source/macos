/*
 * Copyright (c) 1999-2018 Apple Inc. All rights reserved.
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
#include <sys/un.h>
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

#include "printer.h"

#define _NFS_XDR_SUBS_FUNCS_ /* define this to get xdrbuf function definitions */
#include <nfs/xdr_subs.h>

#define MAX_AF_LOCAL_PATH sizeof (((struct sockaddr_un *)0)->sun_path)

int verbose = 0;

#define AOK	(void *)	// assert alignment is OK

#define SHOW_SERVER 0x01
#define SHOW_CLIENT 0x02
#define SHOW_ALL (SHOW_SERVER | SHOW_CLIENT)

#define CLIENT_SERVER_MODE 0
#define EXPORTS_MODE 1
#define ACTIVE_USER_MODE 2
#define MOUNT_MODE 3
#define ZEROSTATS_MODE 4

#define NUMERIC_NET 1
#define NUMERIC_USER 2

#define DEFAULT_EXPORT_STATS_BUFLEN 32768
#define DEFAULT_ACTIVE_USER_STATS_BUFLEN 131072
#define DEFAULT_MOUNT_INFO_BUFLEN 4096

LIST_HEAD(nfs_active_user_node_head, nfs_active_user_node);
LIST_HEAD(nfs_export_node_head, nfs_export_node);

const struct nfsstats_printer *printer = &printf_printer;

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
void zerostats(void);
void printhdr(void);
void sidewaysintpr(u_int, u_int);
void usage(void);
void do_mountinfo(char *);
void do_exports_interval(u_int interval);
void do_exports_normal(void);
void do_active_users_interval(u_int interval, u_int flags);
void do_active_users_normal(u_int flags);
int  read_export_stats(char **buf, size_t *buflen);
int  read_active_user_stats(char **buf, size_t *buflen);
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
	while ((ch = getopt(argc, argv, "w:sceun:mvzf:")) != EOF)
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
		case 'z':
			mode = ZEROSTATS_MODE;
			break;
		case 'f':
			if (!strcmp(optarg, "JSON") || !strcmp(optarg, "Json") || !strcmp(optarg, "json"))
				printer = &json_printer;
				printer->open(PRINTER_NO_PREFIX, NULL);
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
	} else if (mode == ZEROSTATS_MODE) {
		zerostats();
	} else {
		if (interval)
			sidewaysintpr(interval, display);
		else
			intpr(display);
	}
	printer->dump();
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
 * Zero the nfs stats using sysctl(3)
 */
void
zerostats(void)
{
	int name[3];
	struct vfsconf vfc;

	if (getvfsbyname("nfs", &vfc) < 0)
		err(1, "getvfsbyname: NFS not compiled into kernel");
	name[0] = CTL_VFS;
	name[1] = vfc.vfc_typenum;
	name[2] = NFS_NFSZEROSTATS;
	if (sysctl(name, 3, NULL, 0, NULL, 0) < 0)
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
read_export_stats(char **buf, size_t *buflen)
{
	struct nfs_export_stat_desc *hdr;
	int	name[3];
	size_t	statlen;
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
read_active_user_stats(char **buf, size_t *buflen)
{
	struct nfs_user_stat_desc	*hdr;
	int				name[3];
	size_t				statlen;
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
read_mountinfo(fsid_t *fsid, char **buf, size_t *buflen)
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
	bufxdr = (uint32_t*) AOK *buf;
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
		bufxdr = (uint32_t*) AOK *buf;
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
	vers = ntohl(*(uint32_t*) AOK *buf);
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
		printer->open(PRINTER_NO_PREFIX, "Client Info");
		printer->open(PRINTER_NO_PREFIX, "RPC Counts");
		printer->intpr("%12.12s %12.12s %12.12s %12.12s %12.12s %12.12s\n",
							  "Getattr", nfsstats.rpccnt[NFSPROC_GETATTR],
							  "Setattr", nfsstats.rpccnt[NFSPROC_SETATTR],
							  "Lookup", nfsstats.rpccnt[NFSPROC_LOOKUP],
							  "Readlink", nfsstats.rpccnt[NFSPROC_READLINK],
							  "Read", nfsstats.rpccnt[NFSPROC_READ],
							  "Write", nfsstats.rpccnt[NFSPROC_WRITE]);
		printer->intpr("%12.12s %12.12s %12.12s %12.12s %12.12s %12.12s\n",
							  "Create", nfsstats.rpccnt[NFSPROC_CREATE],
							  "Remove", nfsstats.rpccnt[NFSPROC_REMOVE],
							  "Rename", nfsstats.rpccnt[NFSPROC_RENAME],
							  "Link", nfsstats.rpccnt[NFSPROC_LINK],
							  "Symlink", nfsstats.rpccnt[NFSPROC_SYMLINK],
							  "Mkdir", nfsstats.rpccnt[NFSPROC_MKDIR]);
		printer->intpr("%12.12s %12.12s %12.12s %12.12s %12.12s %12.12s\n",
							  "Rmdir", nfsstats.rpccnt[NFSPROC_RMDIR],
							  "Readdir", nfsstats.rpccnt[NFSPROC_READDIR],
							  "RdirPlus", nfsstats.rpccnt[NFSPROC_READDIRPLUS],
							  "Access", nfsstats.rpccnt[NFSPROC_ACCESS],
							  "Mknod", nfsstats.rpccnt[NFSPROC_MKNOD],
							  "Fsstat", nfsstats.rpccnt[NFSPROC_FSSTAT]);
		printer->intpr("%12.12s %12.12s %12.12s\n",
							  "Fsinfo", nfsstats.rpccnt[NFSPROC_FSINFO],
							  "PathConf", nfsstats.rpccnt[NFSPROC_PATHCONF],
							  "Commit", nfsstats.rpccnt[NFSPROC_COMMIT],
							  NULL, 0, NULL, 0, NULL, 0);
		printer->close();
		printer->open(PRINTER_NO_PREFIX, "RPC Info");
		printer->intpr("%12.12s %12.12s %12.12s %12.12s %12.12s %12.12s\n",
							  "TimedOut", nfsstats.rpctimeouts,
							  "Invalid", nfsstats.rpcinvalid,
							  "X Replies", nfsstats.rpcunexpected,
							  "Retries", nfsstats.rpcretries,
							  "Requests", nfsstats.rpcrequests,
							  NULL, 0);
		printer->close();
		printer->open(PRINTER_NO_PREFIX, "Cache Info");
		printer->intpr("%12.12s %12.12s %12.12s %12.12s %12.12s %12.12s\n",
							  "Attr Hits", nfsstats.attrcache_hits,
							  "Attr Misses", nfsstats.attrcache_misses,
							  "Lkup Hits", nfsstats.lookupcache_hits,
							  "Lkup Misses", nfsstats.lookupcache_misses,
							  "BioR Hits", nfsstats.biocache_reads-nfsstats.read_bios,
							  "BioR Misses", nfsstats.read_bios);
		printer->intpr("%12.12s %12.12s %12.12s %12.12s %12.12s %12.12s\n",
							  "BioW Hits", nfsstats.biocache_writes-nfsstats.write_bios,
							  "BioW Misses", nfsstats.write_bios,
							  "BioRL Hits", nfsstats.biocache_readlinks-nfsstats.readlink_bios,
							  "BioRL Misses", nfsstats.readlink_bios,
							  "BioD Hits", nfsstats.biocache_readdirs-nfsstats.readdir_bios,
							  "BioD Misses", nfsstats.readdir_bios);
		printer->intpr("%12.12s %12.12s %12.12s\n",
							  "DirE Hits", nfsstats.direofcache_hits,
							  "DirE Misses", nfsstats.direofcache_misses,
							  NULL, 0, NULL, 0, NULL, 0, NULL, 0);
		printer->close();
		printer->close();
	}
	if (display & SHOW_SERVER) {
		printer->newline();
		printer->open(PRINTER_NO_PREFIX, "Server Info");
		printer->open(PRINTER_NO_PREFIX, "RPC Counts");
		printer->intpr("%12.12s %12.12s %12.12s %12.12s %12.12s %12.12s\n",
							  "Getattr", nfsstats.srvrpccnt[NFSPROC_GETATTR],
							  "Setattr", nfsstats.srvrpccnt[NFSPROC_SETATTR],
							  "Lookup", nfsstats.srvrpccnt[NFSPROC_LOOKUP],
							  "Readlink", nfsstats.srvrpccnt[NFSPROC_READLINK],
							  "Read", nfsstats.srvrpccnt[NFSPROC_READ],
							  "Write", nfsstats.srvrpccnt[NFSPROC_WRITE]);
		printer->intpr("%12.12s %12.12s %12.12s %12.12s %12.12s %12.12s\n",
							  "Create", nfsstats.srvrpccnt[NFSPROC_CREATE],
							  "Remove", nfsstats.srvrpccnt[NFSPROC_REMOVE],
							  "Rename", nfsstats.srvrpccnt[NFSPROC_RENAME],
							  "Link", nfsstats.srvrpccnt[NFSPROC_LINK],
							  "Symlink", nfsstats.srvrpccnt[NFSPROC_SYMLINK],
							  "Mkdir", nfsstats.srvrpccnt[NFSPROC_MKDIR]);
		printer->intpr("%12.12s %12.12s %12.12s %12.12s %12.12s %12.12s\n",
							  "Rmdir", nfsstats.srvrpccnt[NFSPROC_RMDIR],
							  "Readdir", nfsstats.srvrpccnt[NFSPROC_READDIR],
							  "RdirPlus", nfsstats.srvrpccnt[NFSPROC_READDIRPLUS],
							  "Access", nfsstats.srvrpccnt[NFSPROC_ACCESS],
							  "Mknod", nfsstats.srvrpccnt[NFSPROC_MKNOD],
							  "Fsstat", nfsstats.srvrpccnt[NFSPROC_FSSTAT]);
		printer->intpr("%12.12s %12.12s %12.12s\n",
							  "Fsinfo", nfsstats.srvrpccnt[NFSPROC_FSINFO],
							  "PathConf", nfsstats.srvrpccnt[NFSPROC_PATHCONF],
							  "Commit", nfsstats.srvrpccnt[NFSPROC_COMMIT],
							  NULL, 0, NULL, 0, NULL, 0);
		printer->close();
		printer->open(PRINTER_NO_PREFIX, "Server Faults");
		printer->intpr("%12.12s %12.12s\n",
							  "RPC Errors", nfsstats.srvrpc_errs,
							  "Errors", nfsstats.srv_errs,
							  NULL, 0, NULL, 0, NULL, 0, NULL, 0);
		printer->close();
		printer->open(PRINTER_NO_PREFIX, "Server Cache Stats");
		printer->intpr("%12.12s %12.12s %12.12s %12.12s\n",
							  "Inprog", nfsstats.srvcache_inproghits,
							  "Idem", nfsstats.srvcache_idemdonehits,
							  "Non-idem", nfsstats.srvcache_nonidemdonehits,
							  "Misses", nfsstats.srvcache_misses,
							  NULL, 0, NULL, 0);
		printer->close();
		printer->open(PRINTER_NO_PREFIX, "Server Write Gathering");
		printer->intpr("%12.12s %12.12s %12.12s\n",
							  "WriteOps", nfsstats.srvvop_writes,
							  "WriteRPC", nfsstats.srvrpccnt[NFSPROC_WRITE],
							  "Opsaved", nfsstats.srvrpccnt[NFSPROC_WRITE] - nfsstats.srvvop_writes,
							  NULL, 0, NULL, 0, NULL, 0);
		printer->close();
		printer->close();
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
		  printf("Client: %8llu %8llu %8llu %8llu %8llu %8llu %8llu %8llu\n",
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
		  printf("Server: %8llu %8llu %8llu %8llu %8llu %8llu %8llu %8llu\n",
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
	char *		realm;					/* realm to use for acquiring creds */
	char *		principal;				/* principal to use on initial mount */
	char *		sprinc;					/* server's kerberos principal */
	uint32_t	nfs_version, nfs_minor_version;		/* NFS version */
	uint32_t	nfs_min_vers, nfs_max_vers;		/* NFS packed version range */
	uint32_t	rsize, wsize, readdirsize, readahead;	/* I/O values */
	struct timespec	acregmin, acregmax, acdirmin, acdirmax;	/* attrcache values */
	uint32_t	lockmode;				/* advisory file locking mode */
	struct nfs_sec	sec;					/* security flavors */
	struct nfs_etype etype;					/* Supported kerberos encryption types */
	uint32_t	maxgrouplist;				/* max AUTH_SYS groups */
	char		sotype[16];				/* socket type */
	uint32_t	nfs_port, mount_port;			/* port info */
	char		*nfs_localport, *mount_localport;	/* AF_LOCAL address (port) info */
	struct timespec	request_timeout;			/* NFS request timeout */
	uint32_t	soft_retry_count;			/* soft retrans count */
	struct timespec	dead_timeout;				/* dead timeout value */
	fhandle_t	fh;					/* initial file handle */
	uint32_t	numlocs;				/* # of fs locations */
	struct nfs_fs_location *locs;				/* array of fs locations */
	char *		mntfrom;				/* mntfrom mount arg */
	uint32_t	owner;					/* mount owner's uid */
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

	if (margs->realm) {
		free(margs->realm);
		margs->realm = NULL;
	}
	if (margs->principal) {
		free(margs->principal);
		margs->principal = NULL;
	}
	if (margs->sprinc) {
		free(margs->sprinc);
		margs->sprinc = NULL;
	}
	if (margs->nfs_localport) {
		free(margs->nfs_localport);
		margs->nfs_localport = NULL;
	}
	if (margs->mount_localport) {
		free(margs->mount_localport);
		margs->mount_localport = NULL;
	}
}

int
parse_mountargs(struct xdrbuf *xb, int margslen, struct mountargs *margs)
{
	uint32_t val = 0, attrslength = 0, loc, serv, addr, comp;
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
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_NFS_VERSION_RANGE)) {
		xb_get_32(error, xb, margs->nfs_min_vers);
		xb_get_32(error, xb, margs->nfs_max_vers);
	}
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
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_KERB_ETYPE)) {
		xb_get_32(error, xb, margs->etype.count);
		xb_get_32(error, xb, margs->etype.selected);
		for (uint32_t j = 0; j < margs->etype.count; j++)
			xb_get_32(error, xb, margs->etype.etypes[j]);
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

	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_REALM)) {
		xb_get_32(error, xb, val);
		if (!error && ((val < 1) || (val > MAXPATHLEN)))
			error=EINVAL;
		margs->realm = calloc(val+1, sizeof(char));
		if (!margs->realm)
			error = ENOMEM;
		if (!error)
			error = xb_get_bytes(xb, margs->realm, val, 0);
	}

	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_PRINCIPAL)) {
		xb_get_32(error, xb, val);
		if (!error && ((val < 1) || (val > MAXPATHLEN)))
			error=EINVAL;
		margs->principal = calloc(val+1, sizeof(char));
		if (!margs->principal)
			error = ENOMEM;
		if (!error)
			error = xb_get_bytes(xb, margs->principal, val, 0);
	}

	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_SVCPRINCIPAL)) {
		xb_get_32(error, xb, val);
		if (!error && ((val < 1) || (val > MAXPATHLEN)))
			error=EINVAL;
		margs->sprinc = calloc(val+1, sizeof(char));
		if (!margs->sprinc)
			error = ENOMEM;
		if (!error)
			error = xb_get_bytes(xb, margs->sprinc, val, 0);
	}

	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_LOCAL_NFS_PORT)) {
		xb_get_32(error, xb, val);
		if (!error && ((val < 1) || (val > MAX_AF_LOCAL_PATH)))
			error = EINVAL;
		margs->nfs_localport = calloc(val+1, sizeof(char));
		if (!margs->nfs_localport)
			error = ENOMEM;
		if (!error)
			error = xb_get_bytes(xb, margs->nfs_localport, val, 0);
	}

	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_LOCAL_MOUNT_PORT)) {
		xb_get_32(error, xb, val);
		if (!error && ((val < 1) || (val > MAX_AF_LOCAL_PATH)))
			error = EINVAL;
		margs->mount_localport = calloc(val+1, sizeof(char));
		if (!margs->mount_localport)
			error = ENOMEM;
		if (!error)
			error = xb_get_bytes(xb, margs->mount_localport, val, 0);
	}

	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_SET_MOUNT_OWNER))
		xb_get_32(error, xb, margs->owner);

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

static const char *
etype_name(uint32_t etype)
{
	switch(etype) {
	case NFS_DES3_CBC_SHA1_KD:		return ("des3-cbc-sha1-kd");
	case NFS_AES128_CTS_HMAC_SHA1_96:	return ("aes128-cts-hmac-sha1-96");
	case NFS_AES256_CTS_HMAC_SHA1_96:	return ("aes256-cts-hmac-sha1-96");
	default:				return ("?");
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
	if (!strcmp(sotype, "ticlts"))
		return ("ticlts");
	if (!strcmp(sotype, "ticotsord"))
		return ("ticotsord");
	return (sotype);
}

void
print_mountargs(struct mountargs *margs, uint32_t origmargsvers)
{
	int i, n, flags;
	uint32_t loc, serv, comp;
	struct opt *o;
	char buf[1024], *location = NULL;
	char sep;

	/* option separator is space for first option printed and comma for rest */
	sep = ' ';
	/* call this macro after printing to update separator */
#define SEP	sep=','

	flags = margs->mntflags;
	printer->open_array("     ", "General mount flags", &flags);
	for (o = optnames; flags && o->o_opt; o++)
		if (flags & o->o_opt) {
			printer->add_array_str(sep, o->o_name, "");
			flags &= ~o->o_opt;
			SEP;
		}
	for (o = optnames2; flags && o->o_opt; o++)
		if (flags & o->o_opt) {
			printer->add_array_str(sep, o->o_name, "");
			flags &= ~o->o_opt;
			SEP;
		}
	printer->close_array(1);

	sep = ' ';
	printer->open_array("     ", "NFS parameters", NULL);
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_NFS_VERSION)) {
		n = sprintf(buf, "vers=%d", margs->nfs_version);

		if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_NFS_MINOR_VERSION))
			n+= sprintf(buf + n, ".%d", margs->nfs_minor_version);
		buf[n] = '\0';
		printer->add_array_str(sep, buf, "");
		SEP;
	} else if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_NFS_VERSION_RANGE)) {
		uint32_t maj, min;

		maj = PVER2MAJOR(margs->nfs_min_vers);
		min = PVER2MINOR(margs->nfs_min_vers);
		n = sprintf(buf, "vers=%d", maj);
		if (min)
			n+= sprintf(buf + n, ".%d", min);
		maj = PVER2MAJOR(margs->nfs_max_vers);
		min = PVER2MINOR(margs->nfs_max_vers);
		n+= sprintf(buf + n, "-%d", maj);
		if (min)
			n+= sprintf(buf + n, ".%d", min);
		buf[n] = '\0';
		printer->add_array_str(sep, buf, "");
		SEP;
	}
    if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_SOCKET_TYPE)) {
        printer->add_array_str(sep, socket_type(margs->sotype), "");
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_NFS_PORT)) {
        printer->add_array_num(sep, "port=", margs->nfs_port);
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_LOCAL_NFS_PORT)) {
        printer->add_array_str(sep, "port=", margs->nfs_localport);
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_MOUNT_PORT)) {
        printer->add_array_num(sep, "mountport=", margs->mount_port);
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_LOCAL_MOUNT_PORT)) {
        printer->add_array_str(sep, "mountport=", margs->mount_localport);
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_MNTUDP)) {
        printer->add_array_str(sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_MNTUDP) ? "" : "no", "mntudp");
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_SOFT)) {
        printer->add_array_str(sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_SOFT) ? "soft" : "hard", "");
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_INTR)) {
        printer->add_array_str(sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_INTR) ? "" : "no", "intr");
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_RESVPORT)) {
        printer->add_array_str(sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_RESVPORT) ? "" : "no", "resvport");
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_NOCONNECT)) {
        printer->add_array_str(sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_NOCONNECT) ? "no" : "", "conn");
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_NOCALLBACK)) {
        printer->add_array_str(sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_NOCALLBACK) ? "no" : "", "callback");
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_NONEGNAMECACHE)) {
        printer->add_array_str(sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_NONEGNAMECACHE) ? "no" : "", "negnamecache");
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_NAMEDATTR)) {
        printer->add_array_str(sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_NAMEDATTR) ? "" : "no", "namedattr");
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_NOACL)) {
        printer->add_array_str(sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_NOACL) ? "no" : "", "acl");
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_ACLONLY)) {
        printer->add_array_str(sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_ACLONLY) ? "" : "no", "aclonly");
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_CALLUMNT)) {
        printer->add_array_str(sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_CALLUMNT) ? "" : "no", "callumnt");
        SEP;
    }
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_LOCK_MODE))
		switch(margs->lockmode) {
		case NFS_LOCK_MODE_ENABLED:
			printer->add_array_str(sep, "locks", "");
			SEP;
			break;
		case NFS_LOCK_MODE_DISABLED:
			printer->add_array_str(sep, "nolocks", "");
			SEP;
			break;
		case NFS_LOCK_MODE_LOCAL:
			printer->add_array_str(sep, "locallocks", "");
			SEP;
			break;
		}
    if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_NOQUOTA)) {
        printer->add_array_str(sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_NOQUOTA) ? "no" : "", "quota");
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_READ_SIZE)) {
        printer->add_array_num(sep, "rsize=", margs->rsize);
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_WRITE_SIZE)) {
        printer->add_array_num(sep, "wsize=", margs->wsize);
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_READAHEAD)) {
        printer->add_array_num(sep, "readahead=", margs->readahead);
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_READDIR_SIZE)) {
        printer->add_array_num(sep, "dsize=", margs->readdirsize);
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_RDIRPLUS)) {
        printer->add_array_str(sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_RDIRPLUS) ? "" : "no", "rdirplus");
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_DUMBTIMER)) {
        printer->add_array_str(sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_DUMBTIMER) ? "" : "no", "dumbtimr");
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_REQUEST_TIMEOUT)) {
        printer->add_array_num(sep, "timeo=", ((margs->request_timeout.tv_sec * 10) + (margs->request_timeout.tv_nsec % 100000000)));
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_SOFT_RETRY_COUNT)) {
        printer->add_array_num(sep, "retrans=", margs->soft_retry_count);
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_MAX_GROUP_LIST)) {
        printer->add_array_num(sep, "maxgroups=", margs->maxgrouplist);
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_ATTRCACHE_REG_MIN)) {
        printer->add_array_num(sep, "acregmin=", margs->acregmin.tv_sec);
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_ATTRCACHE_REG_MAX)) {
        printer->add_array_num(sep, "acregmax=", margs->acregmax.tv_sec);
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_ATTRCACHE_DIR_MIN)) {
        printer->add_array_num(sep, "acdirmin=", margs->acdirmin.tv_sec);
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_ATTRCACHE_DIR_MAX)) {
        printer->add_array_num(sep, "acdirmax=", margs->acdirmax.tv_sec);
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_DEAD_TIMEOUT)) {
        printer->add_array_num(sep, "deadtimeout=", margs->dead_timeout.tv_sec);
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_MUTEJUKEBOX)) {
        printer->add_array_str(sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_MUTEJUKEBOX) ? "" : "no", "mutejukebox");
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_EPHEMERAL)) {
        printer->add_array_str(sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_EPHEMERAL) ? "" : "no", "ephemeral");
        SEP;
    }
    if (NFS_BITMAP_ISSET(margs->mflags_mask, NFS_MFLAG_NFC)) {
        printer->add_array_str(sep, NFS_BITMAP_ISSET(margs->mflags, NFS_MFLAG_NFC) ? "" : "no", "nfc");
        SEP;
    }
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_SECURITY)) {
		n = sprintf(buf, "sec=%s", sec_flavor_name(margs->sec.flavors[0]));
		for (i=1; i < margs->sec.count; i++)
			n+= sprintf(buf + n, ":%s", sec_flavor_name(margs->sec.flavors[i]));
		buf[n] = '\0';
		printer->add_array_str(sep, buf, "");
		SEP;
	}
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_KERB_ETYPE)) {
		n = sprintf(buf, "etype=%s%s", margs->etype.selected == 0 ? "*" : "",  etype_name(margs->etype.etypes[0]));
		for (uint32_t j = 1; j < margs->etype.count; j++)
			n += sprintf(buf + n, ":%s%s", margs->etype.selected == j ? "*" : "", etype_name(margs->etype.etypes[j]));
		buf[n] = '\0';
		printer->add_array_str(sep, buf, "");
		SEP;
	}
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_REALM))
		printer->add_array_str(sep, "realm=", margs->realm);
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_PRINCIPAL))
		printer->add_array_str(sep, "principal=", margs->principal);
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_SVCPRINCIPAL))
		printer->add_array_str(sep, "sprincipalm=", margs->sprinc);
	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_SET_MOUNT_OWNER))
		printer->add_array_num(sep, "owner=", margs->owner);
	printer->close_array(0);

	if (NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_FS_LOCATIONS)) {
		printer->open_array("     ", "File system locations", NULL);
		printer->newline();
		if (origmargsvers < NFS_ARGSVERSION_XDR) {
			printer->open_inside_array("       ", NULL);
			if (margs->numlocs && margs->locs[0].servcount &&
			    margs->locs[0].servers[0].addrcount &&
			    margs->locs[0].servers[0].addrs[0])
			        location = margs->locs[0].servers[0].addrs[0];
			printer->add_locations(margs->mntfrom ? margs->mntfrom : "???", "???", location ? 1 : 0, &location);
			printer->close();
			printer->newline();
		}
		if ((origmargsvers == NFS_ARGSVERSION_XDR) || verbose) {
			for (loc=0; loc < margs->numlocs; loc++) {
				printer->open_inside_array("       ", NULL);
				n = 0;
				if (!margs->locs[loc].compcount)
					n += sprintf(buf, "/");
				for (comp=0; comp < margs->locs[loc].compcount; comp++)
					n += sprintf(buf + n, "/%s", margs->locs[loc].components[comp]);
				buf[n] = '\0';
				for (serv=0; serv < margs->locs[loc].servcount; serv++) {
					if (margs->locs[loc].servers[serv].name == NULL)
						printer->add_locations(buf, "<local domaim>", 0, NULL);
					else {
						printer->add_locations(buf, margs->locs[loc].servers[serv].name, margs->locs[loc].servers[serv].addrcount, margs->locs[loc].servers[serv].addrs);
					}
				}
				printer->close();
			}
		}
		printer->close_array(0);
	}
	if (verbose && NFS_BITMAP_ISSET(margs->mattrs, NFS_MATTR_FH)) {
		printer->mount_fh(margs->fh.fh_len, margs->fh.fh_data);
	}
}

/*
 * Print the given mount info.
 */
void
print_mountinfo(struct statfs *mnt, char *buf, size_t buflen)
{
	struct xdrbuf xb;
	uint32_t val = -1, miattrs[NFS_MIATTR_BITMAP_LEN], miflags[NFS_MIFLAG_BITMAP_LEN];
	int error = 0, len, n, invalid = 0;
	char addrbuf[1024];
	struct mountargs origargs, curargs;
	uint32_t flags = 0, loc = 0, serv = 0, addr = 0, comp;

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
		printf("%s bogus mount info length %u > %zu\n\n", mnt->f_mntonname, val, buflen);
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

	printer->mount_header(mnt->f_mntonname, mnt->f_mntfromname);

	if (NFS_BITMAP_ISSET(miattrs, NFS_MIATTR_ORIG_ARGS)) {
		printer->open("  -- ", "Original mount options");
		print_mountargs(&origargs, origargs.margsvers);
		printer->close();
	}
	if (NFS_BITMAP_ISSET(miattrs, NFS_MIATTR_CUR_ARGS)) {
		printer->open("  -- ", "Current mount parameters");
		print_mountargs(&curargs, origargs.margsvers);
		if (NFS_BITMAP_ISSET(miattrs, NFS_MIATTR_CUR_LOC_INDEX) &&
		    (verbose || (curargs.numlocs > 1) || (curargs.locs[0].servcount > 1) ||
		     (curargs.locs[0].servers[0].addrcount > 1))) {
			if ((loc >= curargs.numlocs) || (serv >= curargs.locs[loc].servcount) ||
			    (addr >= curargs.locs[loc].servers[serv].addrcount)) {
				invalid = 1;
			}
			printer->open_locations("     ", "Current location", flags, loc, serv, addr, invalid);
			if (!invalid) {
				printer->newline();
				printer->title("       ");
				n = 0;
				if (!curargs.locs[loc].compcount)
					n += sprintf(addrbuf, "/");
				for (comp=0; comp < curargs.locs[loc].compcount; comp++)
					n += sprintf(addrbuf + n, "/%s", curargs.locs[loc].components[comp]);
				addrbuf[n] = '\0';
				if (curargs.locs[loc].servers[serv].name) {
					char *addrstr = curargs.locs[loc].servers[serv].addrs[addr];
					printer->add_locations(addrbuf, curargs.locs[loc].servers[serv].name, addrstr ? 1 : 0, &addrstr);
				} else {
					printer->add_locations(addrbuf, "<local domain>", 0, NULL);
				}
				printer->newline();
			}
			printer->close();
		}
		printer->close();
	}
	if (NFS_BITMAP_ISSET(miattrs, NFS_MIATTR_FLAGS)) {
		printer->open_array("     ", "Status flags", (int *)&miflags[0]);
		if (NFS_BITMAP_ISSET(miflags, NFS_MIFLAG_DEAD))
			printer->add_array_str(',', "dead", "");
		if (NFS_BITMAP_ISSET(miflags, NFS_MIFLAG_NOTRESP))
			printer->add_array_str(',', "not responding", "");
		if (NFS_BITMAP_ISSET(miflags, NFS_MIFLAG_RECOVERY))
			printer->add_array_str(',', "recovery", "");
		printer->close_array(1);
	}

	printer->close();
out:
	if (error)
		printf("%s error parsing mount info (%d)\n", mnt->f_mntonname, error);
	printer->newline();
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
	char realpathbuf[PATH_MAX];
	char *buf = NULL;
	size_t buflen = 0;

	if (realpath(mountpath, realpathbuf)) {
		mountpath = realpathbuf;
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
	uint  i;
	uint64_t recs;
    size_t bufLen;

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
	printer->open(PRINTER_NO_PREFIX, "Exported Directory Info");
	printer->title("%12s  %12s  %12s\n", "Requests", "Read Bytes", "Write Bytes");
	/* loop through, printing out stats of each export */
	for(i = 0; i < recs; i++)
		printer->exports(&rec[i]);

	printer->close();

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
	size_t oldLen, newLen, tmpLen;
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
	uint i;
	uint64_t recs;

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
	uint    i;
	uint64_t recs;

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
    size_t				bufLen;
    uint                recs;

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
	printer->open(PRINTER_NO_PREFIX, "NFS Active User Info");

	LIST_FOREACH(export_node, export_list, export_next) {
		printer->open(PRINTER_NO_PREFIX, export_node->rec->path);
		printer->title("%12s  %12s  %12s  %-7s  %-8s %s\n", "Requests", "Read Bytes", "Write Bytes", "Idle", "User", "IP Address");
		LIST_FOREACH(unode, &export_node->nodes, user_next)
			displayActiveUserRec(unode->rec, flags);
		printer->close();
	}

	printer->close();

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
	size_t		oldLen, newLen, tmpLen;
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
	struct sockaddr_in	in; 
	struct sockaddr_in6	in6;
	struct hostent		*hp = NULL;
	struct passwd		*pw = NULL;
	struct timeval		now;
	struct timezone		tz; 
	time_t			hr, min, sec;
	char			addrbuf[NI_MAXHOST];
	char			unknown[] = "* * * *";
	char			*addr = unknown;
	char			printuuid = 0;

	/* get current time for calculating idle time */
	gettimeofday(&now, &tz);

	/* calculate idle hour, min sec */
	if (now.tv_sec >= rec->tm_last)
		sec = now.tv_sec - rec->tm_last;
	else    
		sec = ~(rec->tm_last - now.tv_sec) + 1;
	hr = sec / 3600;
	sec %= 3600;
	min = sec / 60;
	sec %= 60;
                                
	/* setup ip address string */
	if (rec->sock.ss_family == AF_INET) {
		/* ipv4 */
        /* copy out the data due to alignment issues */
        memcpy(&in, &rec->sock, sizeof(struct sockaddr_in));
        
		if (!(flags & NUMERIC_NET))
			hp = gethostbyaddr((char *)&in.sin_addr, sizeof(in.sin_addr), AF_INET);
		if (hp && hp->h_name)
			addr = hp->h_name;
		else if (inet_ntop(AF_INET, &in.sin_addr, addrbuf, sizeof(addrbuf)))
			addr = addrbuf;
        
	} else if (rec->sock.ss_family == AF_INET6) {
		/* ipv6 */
        /* copy out the data due to alignment issues */
        memcpy(&in6, &rec->sock, sizeof(struct sockaddr_in6));

		if (!(flags & NUMERIC_NET))
			hp = gethostbyaddr((char *)&in6.sin6_addr, sizeof(in6.sin6_addr), AF_INET6);
		if (hp && hp->h_name)
			addr = hp->h_name;
		else if (inet_ntop(AF_INET6, &in6.sin6_addr, addrbuf, sizeof(addrbuf)))
			addr = addrbuf;
	}
                                        
	if ((flags & NUMERIC_USER) || !(pw = getpwuid(rec->uid))) {
		printuuid = 1;
	}

	printer->active_users(rec, addr, pw, printuuid, hr, min, sec);
}

/* Returns zero if both uid and IP address fields match */
int
cmp_active_user(struct nfs_user_stat_user_rec *rec1, struct nfs_user_stat_user_rec *rec2)
{
	struct sockaddr_in	ipv4_sock1, ipv4_sock2;
	struct sockaddr_in6	ipv6_sock1, ipv6_sock2;
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
        /* Copy out the data due to alignment issues */
        memcpy(&ipv4_sock1, &rec1->sock, sizeof(struct sockaddr_in));
        memcpy(&ipv4_sock2, &rec2->sock, sizeof(struct sockaddr_in));

		if (!bcmp(&ipv4_sock1.sin_addr, &ipv4_sock2.sin_addr, sizeof(struct in_addr)))
			retVal = 0;
 
	}
	else {
		/* IPv6 */
        /* Copy out the data due to alignment issues */
        memcpy(&ipv6_sock1, &rec1->sock, sizeof(struct sockaddr_in6));
        memcpy(&ipv6_sock2, &rec2->sock, sizeof(struct sockaddr_in6));

		if (!bcmp(&ipv6_sock1.sin6_addr, &ipv6_sock2.sin6_addr, sizeof(struct in6_addr)))
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
	fprintf(stderr, "usage: nfsstat [-cseuvmz] [-f JSON] [-w interval] [-n user|net]\n");
	exit(1);
}

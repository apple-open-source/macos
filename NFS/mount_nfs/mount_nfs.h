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
 * Copyright (c) 1992, 1993, 1994
 *    The Regents of the University of California.  All rights reserved.
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
 *    This product includes software developed by the University of
 *    California, Berkeley and its contributors.
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

#ifndef mount_nfs_h
#define mount_nfs_h

#include <sys/socket.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>

struct nfs_conf_client {
	int access_cache_timeout;
	int access_for_getattr;
	int allow_async;
	int callback_port;
	int initialdowndelay;
	int iosize;
	int nextdowndelay;
	int nfsiod_thread_max;
	int statfs_rate_limit;
	int is_mobile;
	int squishy_flags;
	int root_steals_gss_context;
	int mount_timeout;
	int mount_quick_timeout;
	char *default_nfs4domain;
};

extern struct nfs_conf_client config;

struct nfs_options_client {
	int             mntflags;                               /* MNT_* flags */
	uint32_t        mattrs[NFS_MATTR_BITMAP_LEN];           /* what attrs are set */
	uint32_t        mflags_mask[NFS_MFLAG_BITMAP_LEN];      /* what flags are set */
	uint32_t        mflags[NFS_MFLAG_BITMAP_LEN];           /* set flag values */
	uint32_t        nfs_version, nfs_minor_version;         /* NFS version */
	uint32_t        nfs_max_vers, nfs_min_vers;             /* NFS min and max packed version */
	uint32_t        rsize, wsize, readdirsize, readahead;   /* I/O values */
	struct timespec acregmin, acregmax;                     /* attrcache file values */
	struct timespec acdirmin, acdirmax;                     /* attrcache dir values */
	struct timespec acrootdirmin, acrootdirmax;             /* attrcache root dir values */
	uint32_t        lockmode;                               /* advisory file locking mode */
	struct nfs_sec  sec;                                    /* security flavors */
	struct nfs_etype etype;                                 /* Kerberos session key encryption types to use */
	uint32_t        maxgrouplist;                           /* max AUTH_SYS groups */
	int             socket_type, socket_family;             /* socket info */
	uint32_t        nfs_port, mount_port;                   /* port info */
	struct timespec request_timeout;                        /* NFS request timeout */
	uint32_t        soft_retry_count;                       /* soft retrans count */
	int             readlink_nocache;                       /* readlink no-cache mode */
	int             access_cache;                           /* access cache size */
	struct timespec dead_timeout;                           /* dead timeout value */
	fhandle_t       fh;                                     /* initial file handle */
	char *          realm;                                  /* realm of the client. use for setting up kerberos creds */
	char *          principal;                              /* inital GSS pincipal */
	char *          sprinc;                                 /* principal of the server */
	char *          local_nfs_port;                         /* unix domain address "port" for nfs protocol */
	char *          local_mount_port;                       /* unix domain address "port" for mount protocol */
	int             force_localhost;                        /* force connection to localhost */
};

extern struct nfs_options_client options;

/*
 * NFS file system location structures
 */
struct nfs_fs_server {
	struct nfs_fs_server *    ns_next;
	char *                    ns_name;/* name of server */
	struct addrinfo *         ns_ailist;/* list of addresses for server */
};

struct nfs_fs_location {
	struct nfs_fs_location    *nl_next;
	struct nfs_fs_server      *nl_servers;/* list of server pointers */
	char *                    nl_path;/* file system path */
	uint32_t                  nl_servcnt;/* # servers in list */
};

int config_read(const char *confpath, struct nfs_conf_client *);
int parse_fs_locations(const char *, struct nfs_fs_location **);
void getaddresslists(struct nfs_fs_location *, int *);
int mount_nfs_imp(int argc, char *argv[]);
void handle_mntopts(char *opts);
void setup_switches(void);
int  hexstr2fh(const char *, fhandle_t *);

#endif /* mount_nfs_h */

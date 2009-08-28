/*
 * Copyright (c) 1999-2009 Apple Inc.  All rights reserved.
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
 * Copyright (c) 1989, 1993
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
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>
#include <sys/ucred.h>
#include <sys/wait.h>
#include <sys/queue.h>
#include <sys/types.h>

#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int bindresvport_sa(int sd, struct sockaddr *sa);

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <CoreFoundation/CoreFoundation.h>
#include <DiskArbitration/DiskArbitration.h>

#ifdef __LP64__
typedef int		xdr_long_t;
#else
typedef long		xdr_long_t;
#endif

#include "pathnames.h"
#include "common.h"

/*
 * Structure for maintaining list of export IDs for exported volumes
 */
struct expidlist {
	LIST_ENTRY(expidlist)	xid_list;
	char			xid_path[MAXPATHLEN];	/* exported sub-directory */
	u_int32_t		xid_id;			/* export ID */
};

/*
 * Structure for maintaining list of UUIDs for exported volumes
 */
struct uuidlist {
	TAILQ_ENTRY(uuidlist)		ul_list;
	char				ul_mntfromname[MAXPATHLEN];
	char				ul_mntonname[MAXPATHLEN];
	u_char				ul_uuid[16];	/* UUID used */
	u_char				ul_dauuid[16];	/* DiskArb UUID */
	char				ul_davalid;	/* DiskArb UUID valid */
	char				ul_exported;	/* currently exported? */
	u_int32_t			ul_fsid;	/* exported FS ID */
	LIST_HEAD(expidhead,expidlist)	ul_exportids;	/* export ID list */
};
TAILQ_HEAD(,uuidlist) ulhead;
#define UL_CHECK_MNTFROM	0x1
#define UL_CHECK_MNTON		0x2
#define UL_CHECK_ALL		0x3

/*
 * Default FSID is just a "hash" of UUID
 */
#define UUID2FSID(U) \
	(*((u_int32_t*)(U))     ^ *(((u_int32_t*)(U))+1) ^ \
	 *(((u_int32_t*)(U))+2) ^ *(((u_int32_t*)(U))+3))

/*
 * Structure for keeping the (outstanding) mount list
 */
struct mountlist {
	struct mountlist	*ml_next;
	char			*ml_host;	/* NFS client name or address */
	char			*ml_dir;	/* mounted directory */
};

/*
 * Structure used to hold a list of names
 */
struct namelist {
	TAILQ_ENTRY(namelist)	nl_next;
	char			*nl_name;
};
TAILQ_HEAD(namelisttqh, namelist);

/*
 * Structure used to hold a list of directories
 */
struct dirlist {
	struct dirlist		*dl_next;
	char			*dl_dir;
};

/*
 * Structure used to hold an export error message.
 */
struct errlist {
	LIST_ENTRY(errlist)	el_next;
	uint32_t		el_linenum;
	char			*el_msg;
};

/*
 * Structures for keeping the export lists
 */

TAILQ_HEAD(expfstqh, expfs);
TAILQ_HEAD(expdirtqh, expdir);
TAILQ_HEAD(hosttqh, host);

/*
 * Structure to hold the exports for each exported file system.
 */
struct expfs {
	TAILQ_ENTRY(expfs)	xf_next;
	struct expdirtqh	xf_dirl;	/* list of exported directories */
	int			xf_flag;	/* internal flags for this struct */
	u_char			xf_uuid[16];	/* file system's UUID */
	u_int32_t		xf_fsid;	/* exported FS ID */
	char			*xf_fsdir;	/* mount point of this file system */
};
/* xf_flag bits */
#define	XF_LINKED	0x1

/*
 * Structure to hold info about an exported directory
 */
struct expdir {
	TAILQ_ENTRY(expdir)	xd_next;
	struct hosttqh		xd_hosts;	/* List of hosts this dir exported to */
	struct expdirtqh	xd_mountdirs;	/* list of mountable sub-directories */
	int			xd_iflags;	/* internal flags for this structure */
	int			xd_flags;	/* default export flags */
	struct xucred		xd_cred;	/* default export mapped credential */
	struct nfs_sec		xd_sec;		/* default security flavors */
	int			xd_oflags;	/* old default export flags */
	struct xucred		xd_ocred;	/* old default export mapped credential */
	struct nfs_sec		xd_osec;	/* old default security flavors */
	struct nfs_sec		xd_ssec;	/* security flavors for showmount */
	char			*xd_dir;	/* pathname of exported directory */
	struct expidlist 	*xd_xid;	/* corresponding export ID */
};

/*
 * Structures for holding sets of exported-to hosts/nets/addresses
 */

/* holds a network/mask and name */
struct netmsk {
	uint32_t	nt_net;		/* network address */
	uint32_t	nt_mask;	/* network mask */
	char 		*nt_name;	/* network name */
};

/* holds either a host or network */
union grouptypes {
	struct hostent *gt_hostent;
	struct netmsk	gt_net;
	char *		gt_netgroup;
};

/* host/network list entry */
struct grouplist {
	struct grouplist *gr_cache;	/* linked list in cache */
	struct grouplist *gr_next;	/* linked list in get_exportlist() */
	int gr_refcnt;			/* #references on this group */
	int16_t gr_type;		/* type of group */
	int16_t gr_flags;		/* group flags */
	union grouptypes gr_u;		/* the host/network */
};
/* Group types */
#define	GT_NULL		0x0		/* not fully-initialized yet */
#define	GT_NETGROUP	0x1		/* this is a netgroup */
#define	GT_NET		0x2		/* this is a network */
#define	GT_HOST		0x3		/* this is a single host address */
/* Group flags */
#define	GF_SHOW		0x1		/* show this entry in export list */

/*
 * host/network flags list entry
 */
struct host {
	TAILQ_ENTRY(host) ht_next;
	int		 ht_flags;	/* export options for these hosts */
	struct xucred	 ht_cred;	/* mapped credential for these hosts */
	struct grouplist *ht_grp;	/* host/network flags applies to */
	struct nfs_sec	 ht_sec;	/* security flavors for these hosts */
};

struct fhreturn {
	int		fhr_flags;
	int		fhr_vers;
	struct nfs_sec	fhr_sec;
	fhandle_t	fhr_fh;
};

/* Global defs */
int	add_name(struct namelisttqh *, char *);
void	free_namelist(struct namelisttqh *);
int	add_dir(struct dirlist **, char *);
char *	add_expdir(struct expdir **, char *, int);
int	add_grp(struct grouplist **, struct grouplist *);
int	add_host(struct hosttqh *, struct host *);
void	add_mlist(char *, char *);
int	check_dirpath(char *);
int	check_options(int);
void	clear_export_error(uint32_t);
int	cmp_secflavs(struct nfs_sec *, struct nfs_sec *);
void	merge_secflavs(struct nfs_sec *, struct nfs_sec *);
void	del_mlist(char *, char *);
int	expdir_search(struct expfs *, char *, uint32_t, int *, struct nfs_sec *);
int	do_export(int, struct expfs *, struct expdir *, struct grouplist *, int,
		struct xucred *, struct nfs_sec *);
int	do_opt(char **, char **, struct grouplist *, int *,
		int *, int *, struct xucred *, struct nfs_sec *, char *, u_char *);
struct expfs *ex_search(u_char *);
void	export_error(int, const char *, ...);
void	export_error_cleanup(struct expfs *);
struct host *find_group_address_match_in_host_list(struct hosttqh *, struct grouplist *);
struct host *find_host(struct hosttqh *, uint32_t);
void	free_dirlist(struct dirlist *dl);
void	free_expdir(struct expdir *);
void	free_expfs(struct expfs *);
void	free_grp(struct grouplist *);
void	free_hosts(struct hosttqh *);
void	free_host(struct host *);
struct expdir *get_expdir(void);
struct expfs *get_expfs(void);
int	get_host_addresses(char *, struct grouplist *);
struct host *get_host(void);
int	get_export_entry(void);
void	get_mountlist(void);
int	get_net(char *, struct netmsk *, int);
int	get_sec_flavors(char *flavorlist, struct nfs_sec *);
struct grouplist *get_grp(struct grouplist *);
char *	grp_name(struct grouplist *);
int	hang_options_setup(struct expdir *, int, struct xucred *, struct grouplist *, struct nfs_sec *, int *);
void	hang_options_finalize(struct expdir *);
void	hang_options_cleanup(struct expdir *);
int	hang_options_mountdir(struct expdir *, char *, int, struct grouplist *, struct nfs_sec *);
void	mntsrv(struct svc_req *, SVCXPRT *);
void	nextfield(char **, char **);
void	out_of_mem(const char *);
int	parsecred(char *, struct xucred *);
int	put_exlist(struct expdir *, XDR *);
int	subdir_check(char *, char *);
int	xdr_dir(XDR *, char *);
int	xdr_explist(XDR *, caddr_t);
int	xdr_fhs(XDR *, caddr_t);
int	xdr_mlist(XDR *, caddr_t);

int	get_uuid_from_diskarb(const char *, u_char *);
struct uuidlist * get_uuid_from_list(const struct statfs *, u_char *, const int);
struct uuidlist * add_uuid_to_list(const struct statfs *, u_char *, u_char *);
struct uuidlist * get_uuid(const struct statfs *, u_char *);
struct uuidlist * find_uuid(u_char *);
struct uuidlist * find_uuid_by_fsid(u_int32_t);
void	uuidlist_clearexport(void);
char *	uuidstring(u_char *, char *);
void	uuidlist_save(void);
void	uuidlist_restore(void);

struct expidlist * find_export_id(struct uuidlist *, u_int32_t);
struct expidlist * get_export_id(struct uuidlist *, char *);

void dump_exports(void);
void snprintf_cred(char *buf, int, struct xucred *cr);

pthread_mutex_t export_mutex;		/* lock for mountd/export globals */
struct expfstqh xfshead;		/* list of exported file systems */
struct dirlist *xpaths;			/* list of exported paths */
int xpaths_complete = 1;
struct mountlist *mlhead;		/* remote mount list */
struct grouplist *grpcache;		/* host/net group cache */
struct xucred def_anon = {		/* default map credential: "nobody" */
	XUCRED_VERSION,
	(uid_t) -2,
	1,
	{ (gid_t) -2 },
};

LIST_HEAD(,errlist) xerrs;		/* list of export errors */
int export_errors, hostnamecount, hostnamegoodcount;
SVCXPRT *udptransp, *tcptransp;
int mounttcpsock, mountudpsock;

/* export options */
#define	OP_MAPROOT	0x00000001	/* map root credentials */
#define	OP_MAPALL	0x00000002	/* map all credentials */
#define	OP_SECFLAV	0x00000004	/* security flavor(s) specified */
#define	OP_MASK		0x00000008	/* network mask specified */
#define	OP_NET		0x00000010	/* network address specified */
#define	OP_ALLDIRS	0x00000040	/* allow mounting subdirs */
#define	OP_READONLY	0x00000080	/* export read-only */
#define	OP_32BITCLIENTS	0x00000100	/* use 32-bit directory cookies */
#define	OP_FSPATH	0x00000200	/* file system path specified */
#define	OP_FSUUID	0x00000400	/* file system UUID specified */
#define	OP_OFFLINE	0x00000800	/* export is offline */
#define	OP_ONLINE	0x04000000	/* export is online */
#define	OP_SHOW		0x08000000	/* show this entry in export list */
#define	OP_MISSING	0x10000000	/* export is missing */
#define	OP_DEFEXP	0x20000000	/* default export for everyone (else) */
#define	OP_ADD		0x40000000	/* tag export for potential addition */
#define	OP_DEL		0x80000000	/* tag export for potential deletion */
#define	OP_EXOPTMASK	0x100009C3	/* export options mask */
#define	OP_EXOPTS(X)	((X) & OP_EXOPTMASK)

#define RECHECKEXPORTS_TIMEOUT			600
#define RECHECKEXPORTS_DELAYED_STARTUP_TIMEOUT	120
#define RECHECKEXPORTS_DELAYED_STARTUP_INTERVAL	5

/*
 * Mountd server for NFS mount protocol as described in:
 * NFS: Network File System Protocol Specification, RFC1094, Appendix A
 * The optional arguments are the exports file name
 * default: _PATH_EXPORTS
 * and "-n" to allow nonroot mount.
 */

/*
 * The incredibly complex mountd thread function
 */
void *
mountd_thread(__unused void *arg)
{
	set_thread_sigmask();
	svc_run();
	log(LOG_ERR, "mountd died");
	exit(1);
}

void
mountd_init(void)
{
	int error;

	TAILQ_INIT(&xfshead);
	xpaths = NULL;
	mlhead = NULL;
	grpcache = NULL;
	TAILQ_INIT(&ulhead);
	LIST_INIT(&xerrs);

	error = pthread_mutex_init(&export_mutex, NULL);
	if (error) {
		log(LOG_ERR, "export mutex init failed: %s (%d)", strerror(error), error);
		exit(1);
	}

	uuidlist_restore();
}

void
mountd(void)
{
	struct sockaddr_in inetaddr;
	socklen_t socklen;
	struct nfs_export_args nxa;
	int error, on, init_retry;
	pthread_t thd;
	time_t init_start;

	/* global initialization */
	mountd_init();
	check_for_mount_changes();

	/*
	 * mountd needs to start from a clean slate.
	 */

	/* clear the table of mounts at startup. */
	unlink(_PATH_RMOUNTLIST);

	/* Delete all exports that are in the kernel. */
	bzero(&nxa, sizeof(nxa));
	nxa.nxa_flags = NXA_DELETE_ALL;
	error = nfssvc(NFSSVC_EXPORT, &nxa);
	if (error && (errno != ENOENT))
		log(LOG_ERR, "Can't delete all exports: %s (%d)", strerror(errno), errno);

	/* set up the export and mount lists */

	/*
	 * Note that the recheckexports functionality will allow us to retry exports for
	 * a while if we have problems resolving any host names.  However, these problems
	 * may not affect all exports (e.g. default exports) and that could result in some
	 * hosts getting the wrong access until their export options are set up properly.
	 * Since this could result in some hosts receiving errors or erroneous processing,
	 * we check if the first get_exportlist() check resulted in any successful host
	 * name lookups.  If there were names and none of them were looked up successfully,
	 * then we'll delay startup for a *short* while in an attempt to avoid any problems
	 * if the problem clears up shortly.
	 */
	init_start = time(NULL);
	init_retry = 0;
	while (1) {
		DEBUG(1, "Getting export list.");
		get_exportlist();
		if (!hostnamecount || hostnamegoodcount) {
			if (init_retry)
				log(LOG_WARNING, "host name resolution seems to be working now... continuing initialization");
			break;
		}
		if (!init_retry) {
			log(LOG_WARNING, "host name resolution seems to be having problems... delaying initialization");
			init_retry = 1;
		} else if (time(NULL) > (init_start + RECHECKEXPORTS_DELAYED_STARTUP_TIMEOUT)) {
			log(LOG_WARNING, "giving up on host name resolution... continuing initialization");
			break;
		}
		sleep(RECHECKEXPORTS_DELAYED_STARTUP_INTERVAL);
	}

	DEBUG(1, "Getting mount list.");
	get_mountlist();
	DEBUG(1, "Here we go.");

	/* create mountd service sockets */
	if (!config.udp && !config.tcp) {
		log(LOG_WARNING, "No network transport(s) configured.  mountd thread not starting.");
		return;
	}

	/* If we are serving UDP, set up the MOUNT/UDP socket. */
	if (config.udp) {
		if ((mountudpsock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			log(LOG_ERR, "can't create MOUNT/UDP socket: %s (%d)", strerror(errno), errno);
			exit(1);
		}
		inetaddr.sin_family = AF_INET;
		inetaddr.sin_addr.s_addr = INADDR_ANY;
		inetaddr.sin_port = htons(config.mount_port);
		inetaddr.sin_len = sizeof(inetaddr);
		if (bindresvport_sa(mountudpsock, (struct sockaddr*)&inetaddr) < 0) {
			/* socket may still be lingering from previous incarnation */
			/* wait a few seconds and try again */
			sleep(6);
			if (bindresvport_sa(mountudpsock, (struct sockaddr*)&inetaddr) < 0) {
				log(LOG_ERR, "can't bind MOUNT/UDP addr: %s (%d)", strerror(errno), errno);
				exit(1);
			}
		}
		socklen = sizeof(inetaddr);
		if (getsockname(mountudpsock, (struct sockaddr*)&inetaddr, &socklen)) {
			log(LOG_ERR, "can't getsockname on MOUNT/UDP socket: %s (%d)", strerror(errno), errno);
		} else {
			mountudpport = ntohs(inetaddr.sin_port);
		}
		if ((udptransp = svcudp_create(mountudpsock)) == NULL)
			log(LOG_ERR, "Can't create MOUNT/UDP service");
		else {
			if (!svc_register(udptransp, RPCPROG_MNT, 1, mntsrv, 0))
				log(LOG_ERR, "Can't register MOUNT/UDP v1 service");
			if (!svc_register(udptransp, RPCPROG_MNT, 3, mntsrv, 0))
				log(LOG_ERR, "Can't register MOUNT/UDP v3 service");
		}
	}

	/* If we are serving TCP, set up the MOUNT/TCP socket. */
	on = 1;
	if (config.tcp) {
		if ((mounttcpsock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			log(LOG_ERR, "can't create MOUNT/TCP socket: %s (%d)", strerror(errno), errno);
			exit(1);
		}
		if (setsockopt(mounttcpsock, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
			log(LOG_WARNING, "setsockopt SO_REUSEADDR: %s (%d)", strerror(errno), errno);
		inetaddr.sin_family = AF_INET;
		inetaddr.sin_addr.s_addr = INADDR_ANY;
		inetaddr.sin_port = htons(config.mount_port);
		inetaddr.sin_len = sizeof(inetaddr);
		if (bindresvport_sa(mounttcpsock, (struct sockaddr*)&inetaddr) < 0) {
			log(LOG_ERR, "can't bind MOUNT/TCP addr: %s (%d)", strerror(errno), errno);
			exit(1);
		}
		if (listen(mounttcpsock, 128) < 0) {
			log(LOG_ERR, "MOUNT listen failed: %s (%d)", strerror(errno), errno);
			exit(1);
		}
		socklen = sizeof(inetaddr);
		if (getsockname(mounttcpsock, (struct sockaddr*)&inetaddr, &socklen)) {
			log(LOG_ERR, "can't getsockname on MOUNT/TCP socket: %s (%d)", strerror(errno), errno);
		} else {
			mounttcpport = ntohs(inetaddr.sin_port);
		}
		if ((tcptransp = svctcp_create(mounttcpsock, 0, 0)) == NULL)
			log(LOG_ERR, "Can't create MOUNT/TCP service");
		else {
			if (!svc_register(tcptransp, RPCPROG_MNT, 1, mntsrv, 0))
				log(LOG_ERR, "Can't register MOUNT/TCP v1 service");
			if (!svc_register(tcptransp, RPCPROG_MNT, 3, mntsrv, 0))
				log(LOG_ERR, "Can't register MOUNT/TCP v3 service");
		}
	}
	if ((udptransp == NULL) && (tcptransp == NULL)) {
		log(LOG_ERR, "Can't create MOUNT sockets");
		exit(1);
	}

	/* launch mountd pthread */
	error = pthread_create(&thd, &pattr, mountd_thread, NULL);
	if (error) {
		log(LOG_ERR, "mountd pthread_create: %s (%d)", strerror(error), error);
		exit(1);
	}
}

/*
 * functions for locking/unlocking the exports list (and other export-related globals)
 */
void
lock_exports(void)
{
	int error;

	if (checkexports)
		return;

	error = pthread_mutex_lock(&export_mutex);
	if (error)
		log(LOG_ERR, "export mutex lock failed: %s (%d)", strerror(error), error);
}
void
unlock_exports(void)
{
	int error;

	if (checkexports)
		return;

	error = pthread_mutex_unlock(&export_mutex);
	if (error)
		log(LOG_ERR, "export mutex unlock failed: %s (%d)", strerror(error), error);
}

/*
 * The mount rpc service
 */
void
mntsrv(struct svc_req *rqstp, SVCXPRT *transp)
{
	struct expfs *xf;
	struct fhreturn fhr;
	struct stat stb;
	struct statfs fsb;
	struct hostent *hp;
	struct nfs_sec secflavs;
	uint32_t saddr;
	u_short sport;
	char rpcpath[RPCMNT_PATHLEN + 1], dirpath[MAXPATHLEN], *mhost;
	int bad = ENOENT, options;
	u_char uuid[16];

	saddr = transp->xp_raddr.sin_addr.s_addr;
	sport = ntohs(transp->xp_raddr.sin_port);
	hp = NULL;
	switch (rqstp->rq_proc) {
	case NULLPROC:
		if (!svc_sendreply(transp, (xdrproc_t)xdr_void, (caddr_t)NULL))
			log(LOG_ERR, "Can't send NULL MOUNT reply");
		return;
	case RPCMNT_MOUNT:
		if ((sport >= IPPORT_RESERVED) && config.mount_require_resv_port) {
			svcerr_weakauth(transp);
			return;
		}
		if (!svc_getargs(transp, xdr_dir, rpcpath)) {
			svcerr_decode(transp);
			return;
		}

		lock_exports();

		/*
		 * Get the real pathname and make sure it is a directory
		 * or a regular file if the -r option was specified
		 * and it exists.
		 */
		if (realpath(rpcpath, dirpath) == 0 ||
		    stat(dirpath, &stb) < 0 ||
		    (!S_ISDIR(stb.st_mode) &&
		     (!config.mount_regular_files || !S_ISREG(stb.st_mode))) ||
		    statfs(dirpath, &fsb) < 0) {
			unlock_exports();
			chdir("/");	/* Just in case realpath doesn't */
			DEBUG(1, "stat failed on %s", dirpath);
			if (!svc_sendreply(transp, (xdrproc_t)xdr_long, (caddr_t)&bad))
				log(LOG_ERR, "Can't send reply for failed mount");
			if (config.verbose) {
				mhost = inet_ntoa(transp->xp_raddr.sin_addr);
				log(LOG_NOTICE, "Mount failed: %s %s", mhost, dirpath);
			}
			return;
		}

		/* get UUID for volume */
		if (!get_uuid_from_list(&fsb, uuid, UL_CHECK_ALL)) {
			unlock_exports();
			DEBUG(1, "no exported volume uuid for %s", dirpath);
			if (!svc_sendreply(transp, (xdrproc_t)xdr_long, (caddr_t)&bad))
				log(LOG_ERR, "Can't send reply for failed mount");
			if (config.verbose) {
				mhost = inet_ntoa(transp->xp_raddr.sin_addr);
				log(LOG_NOTICE, "Mount failed: %s %s", mhost, dirpath);
			}
			return;
		}

		/* Check in the exports list */
		xf = ex_search(uuid);
		if (xf && expdir_search(xf, dirpath, saddr, &options, &secflavs)) {
			fhr.fhr_flags = options;
			fhr.fhr_vers = rqstp->rq_vers;
			bcopy(&secflavs, &fhr.fhr_sec, sizeof(struct nfs_sec));
			/* Get the file handle */
			memset(&fhr.fhr_fh, 0, sizeof(fhandle_t));
			if (getfh(dirpath, (fhandle_t *)&fhr.fhr_fh) < 0) {
				DEBUG(1, "Can't get fh for %s: %s (%d)", dirpath,
					strerror(errno), errno);
				bad = EACCES; /* path must not be exported */
				if (!svc_sendreply(transp, (xdrproc_t)xdr_long, (caddr_t)&bad))
					log(LOG_ERR, "Can't send reply for failed mount");
				unlock_exports();
				return;
			}
			if (!svc_sendreply(transp, (xdrproc_t)xdr_fhs, (caddr_t)&fhr))
				log(LOG_ERR, "Can't send mount reply");
			hp = gethostbyaddr((caddr_t)&transp->xp_raddr.sin_addr,
				sizeof(transp->xp_raddr.sin_addr), AF_INET);
			mhost = hp ? hp->h_name : inet_ntoa(transp->xp_raddr.sin_addr);
			add_mlist(mhost, dirpath);
			log(LOG_INFO, "Mount successful: %s %s", mhost, dirpath);
		} else {
			bad = EACCES;
			if (config.verbose) {
				mhost = inet_ntoa(transp->xp_raddr.sin_addr);
				log(LOG_NOTICE, "Mount failed: %s %s", mhost, dirpath);
			}
			if (!svc_sendreply(transp, (xdrproc_t)xdr_long, (caddr_t)&bad))
				log(LOG_ERR, "Can't send reply for failed mount");
		}
		unlock_exports();
		return;
	case RPCMNT_DUMP:
		if (!svc_sendreply(transp, (xdrproc_t)xdr_mlist, (caddr_t)NULL))
			log(LOG_ERR, "Can't send MOUNT dump reply");
		return;
	case RPCMNT_UMOUNT:
		if ((sport >= IPPORT_RESERVED) && config.mount_require_resv_port) {
			svcerr_weakauth(transp);
			return;
		}
		if (!svc_getargs(transp, xdr_dir, dirpath)) {
			svcerr_decode(transp);
			return;
		}
		if (!svc_sendreply(transp, (xdrproc_t)xdr_void, (caddr_t)NULL))
			log(LOG_ERR, "Can't send UMOUNT reply");
		hp = gethostbyaddr((caddr_t)&saddr, sizeof(saddr), AF_INET);
		if (hp)
			del_mlist(hp->h_name, dirpath);
		del_mlist(inet_ntoa(transp->xp_raddr.sin_addr), dirpath);
		mhost = hp ? hp->h_name : inet_ntoa(transp->xp_raddr.sin_addr);
		log(LOG_INFO, "umount: %s %s", mhost, dirpath);
		return;
	case RPCMNT_UMNTALL:
		if ((sport >= IPPORT_RESERVED) && config.mount_require_resv_port) {
			svcerr_weakauth(transp);
			return;
		}
		if (!svc_sendreply(transp, (xdrproc_t)xdr_void, (caddr_t)NULL))
			log(LOG_ERR, "Can't send UMNTALL reply");
		hp = gethostbyaddr((caddr_t)&saddr, sizeof(saddr), AF_INET);
		if (hp)
			del_mlist(hp->h_name, (char *)NULL);
		del_mlist(inet_ntoa(transp->xp_raddr.sin_addr), (char *)NULL);
		mhost = hp ? hp->h_name : inet_ntoa(transp->xp_raddr.sin_addr);
		log(LOG_INFO, "umount all: %s", mhost);
		return;
	case RPCMNT_EXPORT:
		lock_exports();
		if (!svc_sendreply(transp, (xdrproc_t)xdr_explist, (caddr_t)NULL))
			log(LOG_ERR, "Can't send EXPORT reply");
		unlock_exports();
		DEBUG(1, "export: %s", inet_ntoa(transp->xp_raddr.sin_addr));
		return;
	default:
		svcerr_noproc(transp);
		return;
	}
}

/*
 * Xdr conversion for a dirpath string
 */
int
xdr_dir(XDR *xdrsp, char *dirp)
{
	return (xdr_string(xdrsp, &dirp, RPCMNT_PATHLEN));
}

/*
 * Xdr routine to generate file handle reply
 */
int
xdr_fhs(XDR *xdrsp, caddr_t cp)
{
	struct fhreturn *fhrp = (struct fhreturn *)cp;
	xdr_long_t ok = 0, len, auth;
	int32_t i;

	if (!xdr_long(xdrsp, &ok))
		return (0);
	switch (fhrp->fhr_vers) {
	case 1:
		return (xdr_opaque(xdrsp, (caddr_t)fhrp->fhr_fh.fh_data, NFSX_V2FH));
	case 3:
		len = fhrp->fhr_fh.fh_len;
		if (!xdr_long(xdrsp, &len))
			return (0);
		if (!xdr_opaque(xdrsp, (caddr_t)fhrp->fhr_fh.fh_data, fhrp->fhr_fh.fh_len))
			return (0);
		/* security flavors */
		if (fhrp->fhr_sec.count == 0) {
			auth = RPCAUTH_SYS;
			len = 1;
			if (!xdr_long(xdrsp, &len))
				return (0);
			return (xdr_long(xdrsp, &auth));
		}

		len = fhrp->fhr_sec.count;
		if (!xdr_long(xdrsp, &len))
			return (0);
		for (i = 0; i < fhrp->fhr_sec.count; i++) {
			auth = (xdr_long_t)fhrp->fhr_sec.flavors[i];
			if (!xdr_long(xdrsp, &auth))
				return (0);
		}
		return (TRUE);
	};
	return (0);
}

int
xdr_mlist(XDR *xdrsp, __unused caddr_t cp)
{
	struct mountlist *mlp;
	int trueval = 1;
	int falseval = 0;

	mlp = mlhead;
	while (mlp) {
		if (!xdr_bool(xdrsp, &trueval))
			return (0);
		if (!xdr_string(xdrsp, &mlp->ml_host, RPCMNT_NAMELEN))
			return (0);
		if (!xdr_string(xdrsp, &mlp->ml_dir, RPCMNT_PATHLEN))
			return (0);
		mlp = mlp->ml_next;
	}
	if (!xdr_bool(xdrsp, &falseval))
		return (0);
	return (1);
}

/*
 * Xdr conversion for export list
 */
int
xdr_explist(XDR *xdrsp, __unused caddr_t cp)
{
	struct expfs *xf;
	struct expdir *xd;
	int falseval = 0;

	TAILQ_FOREACH(xf, &xfshead, xf_next) {
		TAILQ_FOREACH(xd, &xf->xf_dirl, xd_next) {
			if (put_exlist(xd, xdrsp))
				goto errout;
		}
	}
	if (!xdr_bool(xdrsp, &falseval))
		return (0);
	return (1);
errout:
	return (0);
}

/*
 * Called from xdr_explist() to output the mountable exported
 * directory paths.
 */
int
put_exlist(struct expdir *xd, XDR *xdrsp)
{
	struct expdir *mxd;
	struct grouplist *grp;
	struct host *hp;
	int trueval = 1;
	int falseval = 0;
	char *strp;
	char offline_all[] = "<offline>";
	char offline_some[] = "<offline*>";
	char everyone[] = "(Everyone)";
	char abuf[RPCMNT_NAMELEN+1];
	int offline = 0, auth = 0, i;

	if (!xd)
		return (0);

	if (!xdr_bool(xdrsp, &trueval))
		return (1);
	strp = xd->xd_dir;
	if (!xdr_string(xdrsp, &strp, RPCMNT_PATHLEN))
		return (1);
	if (xd->xd_iflags & OP_OFFLINE) {
		/* report if export is offline for all or some* hosts */
		if (!xdr_bool(xdrsp, &trueval))
			return (1);
		strp = (xd->xd_iflags & OP_ONLINE) ? offline_some : offline_all;
		if (!xdr_string(xdrsp, &strp, RPCMNT_NAMELEN))
			return (1);
		offline = 1;
	}
	if ((xd->xd_ssec.count > 1) || (xd->xd_ssec.flavors[0] != RPCAUTH_SYS)) {
		/* report non-default auth flavors */
		if (!xdr_bool(xdrsp, &trueval))
			return (1);
		abuf[0] = '\0';
		strlcpy(abuf, "<", sizeof(abuf));
		for (i=0; i < xd->xd_ssec.count; i++) {
			if (xd->xd_ssec.flavors[i] == RPCAUTH_SYS)
				strlcat(abuf, "sys", sizeof(abuf));
			else if (xd->xd_ssec.flavors[i] == RPCAUTH_KRB5)
				strlcat(abuf, "krb5", sizeof(abuf));
			else if (xd->xd_ssec.flavors[i] == RPCAUTH_KRB5I)
				strlcat(abuf, "krb5i", sizeof(abuf));
			else if (xd->xd_ssec.flavors[i] == RPCAUTH_KRB5P)
				strlcat(abuf, "krb5p", sizeof(abuf));
			else
				continue;
			if (i < xd->xd_ssec.count-1)
				strlcat(abuf, ":", sizeof(abuf));
		}
		strlcat(abuf, ">", sizeof(abuf));
		strp = abuf;
		if (!xdr_string(xdrsp, &strp, RPCMNT_NAMELEN))
			return (1);
		auth = 1;
	}
	if (!(xd->xd_flags & OP_DEFEXP)) {
		TAILQ_FOREACH(hp, &xd->xd_hosts, ht_next) {
			if (!(hp->ht_flags & OP_SHOW))
				continue;
			grp = hp->ht_grp;
			switch (grp->gr_type) {
			case GT_HOST:
			case GT_NET:
			case GT_NETGROUP:
				if (!xdr_bool(xdrsp, &trueval))
					return (1);
				strp = grp_name(grp);
				if (!xdr_string(xdrsp, &strp, RPCMNT_NAMELEN))
					return (1);
			}
		}
	} else if (offline || auth) {
		if (!xdr_bool(xdrsp, &trueval))
			return (1);
		strp = everyone;
		if (!xdr_string(xdrsp, &strp, RPCMNT_NAMELEN))
			return (1);
	}
	if (!xdr_bool(xdrsp, &falseval))
		return (1);

	TAILQ_FOREACH(mxd, &xd->xd_mountdirs, xd_next) {
		if (put_exlist(mxd, xdrsp))
			return (1);
	}

	return (0);
}


/*
 * Clean up a pathname.  Removes quotes around quoted strings,
 * strips escaped characters, removes trailing slashes.
 */
char *
clean_pathname(char *line)
{
	int len, esc;
	char c, *p, *s;

	if (line == NULL)
		return NULL;
	len = strlen(line);
	s = malloc(len + 1);
	if (s == NULL)
		return NULL;

	len = 0;
	esc = 0;
	c = '\0';

	p = line;

	if (*p == '\'' || *p == '"') {
		c = *p;
		p++;
	}

	for (;*p != '\0'; p++) {
		if (esc == 1) {
			s[len++] = *p;
			esc = 0;
		} else if (*p == c)
			break;
		else if (*p == '\\')
			esc = 1;
		else if (c == '\0' && (*p == ' ' || *p == '\t'))
			break;
		else s[len++] = *p;
	}

	/* strip trailing slashes */
	for (; len > 1 && s[len-1] == '/'; len--)
		;

	s[len] = '\0';

	return (s);
}


/*
 * Query DiskArb for a volume's UUID
 */
int
get_uuid_from_diskarb(const char *path, u_char *uuid)
{
	DASessionRef session;
	DADiskRef disk;
	CFDictionaryRef dd;
	CFTypeRef val;
	CFUUIDBytes uuidbytes;
	int rv = 1;

	session = NULL;
	disk = NULL;
	dd = NULL;

	session = DASessionCreate(NULL);
	if (!session) {
		log(LOG_ERR, "can't create DiskArb session");
		rv = 0;
		goto out;
	}
	disk = DADiskCreateFromBSDName(NULL, session, path);
	if (!disk) {
		DEBUG(1, "DADiskCreateFromBSDName(%s) failed", path);
		rv = 0;
		goto out;
	}
	dd = DADiskCopyDescription(disk);
	if (!dd) {
		DEBUG(1, "DADiskCopyDescription(%s) failed", path);
		rv = 0;
		goto out;
	}

	if (!CFDictionaryGetValueIfPresent(dd, (kDADiskDescriptionVolumeUUIDKey), &val)) {
		DEBUG(1, "unable to get UUID for volume %s", path);
		rv = 0;
		goto out;
	}
	uuidbytes = CFUUIDGetUUIDBytes(val);
	bcopy(&uuidbytes, uuid, sizeof(uuidbytes));

out:
	if (session) CFRelease(session);
	if (disk) CFRelease(disk);
	if (dd) CFRelease(dd);
	return (rv);
}

/*
 * find the UUID for this volume in the UUID list
 */
struct uuidlist *
get_uuid_from_list(const struct statfs *fsb, u_char *uuid, const int flags)
{
	struct uuidlist *ulp;

	if (!(flags & UL_CHECK_ALL))
		return (NULL);

	TAILQ_FOREACH(ulp, &ulhead, ul_list) {
		if ((flags & UL_CHECK_MNTFROM) &&
		    strcmp(fsb->f_mntfromname, ulp->ul_mntfromname))
			continue;
		if ((flags & UL_CHECK_MNTON) &&
		    strcmp(fsb->f_mntonname, ulp->ul_mntonname))
			continue;
		if (uuid)
			bcopy(&ulp->ul_uuid, uuid, sizeof(ulp->ul_uuid));
		break;
	}
	return (ulp);
}

/*
 * find UUID list entry with the given UUID
 */
struct uuidlist *
find_uuid(u_char *uuid)
{
	struct uuidlist *ulp;

	TAILQ_FOREACH(ulp, &ulhead, ul_list) {
		if (!bcmp(ulp->ul_uuid, uuid, sizeof(ulp->ul_uuid)))
			break;
	}
	return (ulp);
}

/*
 * find UUID list entry with the given FSID
 */
struct uuidlist *
find_uuid_by_fsid(u_int32_t fsid)
{
	struct uuidlist *ulp;

	TAILQ_FOREACH(ulp, &ulhead, ul_list) {
		if (ulp->ul_fsid == fsid)
			break;
	}
	return (ulp);
}

/*
 * add a UUID to the UUID list
 */
struct uuidlist *
add_uuid_to_list(const struct statfs *fsb, u_char *dauuid, u_char *uuid)
{
	struct uuidlist *ulpnew;
	u_int32_t xfsid;

	ulpnew = malloc(sizeof(struct uuidlist));
	if (!ulpnew) {
		log(LOG_ERR, "add_uuid_to_list: out of memory");
		return (NULL);
	}
	bzero(ulpnew, sizeof(*ulpnew));
	LIST_INIT(&ulpnew->ul_exportids);
	if (dauuid) {
		bcopy(dauuid, ulpnew->ul_dauuid, sizeof(ulpnew->ul_dauuid));
		ulpnew->ul_davalid = 1;
	}
	bcopy(uuid, ulpnew->ul_uuid, sizeof(ulpnew->ul_uuid));
	strlcpy(ulpnew->ul_mntfromname, fsb->f_mntfromname, sizeof(ulpnew->ul_mntfromname));
	strlcpy(ulpnew->ul_mntonname, fsb->f_mntonname, sizeof(ulpnew->ul_mntonname));

	/* make sure exported FS ID is unique */
	xfsid = UUID2FSID(uuid);
	ulpnew->ul_fsid = xfsid;
	while (find_uuid_by_fsid(ulpnew->ul_fsid))
		if (++ulpnew->ul_fsid == xfsid) {
			/* exhausted exported FS ID values! */
			log(LOG_ERR, "exported FS ID values exhausted, can't add %s",
				ulpnew->ul_mntonname);
			free(ulpnew);
			return (NULL);
		}

	TAILQ_INSERT_TAIL(&ulhead, ulpnew, ul_list);
	return (ulpnew);
}

/*
 * get the UUID to use for this volume's file handles
 * and add it to the UUID list if it isn't there yet.
 */
struct uuidlist *
get_uuid(const struct statfs *fsb, u_char *uuid)
{
	CFUUIDRef cfuuid;
	CFUUIDBytes uuidbytes;
	struct uuidlist *ulp;
	u_char dauuid[16];
	int davalid, uuidchanged, reportuuid = 0;
	char buf[64], buf2[64];

	/* get DiskArb's idea of the UUID (if any) */
	davalid = get_uuid_from_diskarb(fsb->f_mntfromname, dauuid);

	if (davalid) {
		DEBUG(2, "get_uuid: %s %s DiskArb says: %s",
			fsb->f_mntfromname, fsb->f_mntonname,
			uuidstring(dauuid, buf));
	}

	/* try to get UUID out of UUID list */
	if ((ulp = get_uuid_from_list(fsb, uuid, UL_CHECK_MNTON))) {
		DEBUG(2, "get_uuid: %s %s found: %s",
			fsb->f_mntfromname, fsb->f_mntonname,
			uuidstring(uuid, buf));
		/*
		 * Check against any DiskArb UUID.
		 * If diskarb UUID is different then drop the uuid entry.
		 */
		if (davalid) {
			if (!ulp->ul_davalid)
				uuidchanged = 1;
			else if (bcmp(ulp->ul_dauuid, dauuid, sizeof(dauuid)))
				uuidchanged = 1;
			else
				uuidchanged = 0;
		} else {
			if (ulp->ul_davalid) {
				/*
				 * We had a UUID before, but now we don't?
				 * Assume this is just a transient error,
				 * issue a warning, and stick with the old UUID.
				 */
				uuidstring(ulp->ul_dauuid, buf);
				log(LOG_WARNING, "lost UUID for %s, was %s, keeping old UUID",
					fsb->f_mntonname, buf);
				uuidchanged = 0;
			} else
				uuidchanged = 0;
		}
		if (uuidchanged) {
			uuidstring(ulp->ul_dauuid, buf);
			if (davalid)
				uuidstring(dauuid, buf2);
			else
				strlcpy(buf2, "------------------------------------", sizeof(buf2));
			if (ulp->ul_exported) {
				/*
				 * Woah!  We already have this file system exported with
				 * a different UUID (UUID changed while processing the
				 * exports list).  Ignore the UUID change for now so that
				 * all the exports for this file system will be registered
				 * using the same UUID/FSID.
				 *
				 * XXX Should we do something like set gothup=1 so that
				 * we will reregister all the exports (with the new UUID)?
				 * If so, what's to prevent an infinite loop if we always
				 * seem to be hitting this problem?
				 */
				log(LOG_WARNING, "ignoring UUID change for already exported file system %s, was %s now %s",
					fsb->f_mntonname, buf, buf2);
				uuidchanged = 0;
			}
		}
		if (uuidchanged) {
			log(LOG_WARNING, "UUID changed for %s, was %s now %s",
				fsb->f_mntonname, buf, buf2);
			bcopy(dauuid, uuid, sizeof(dauuid));
			/* remove old UUID from list */
			TAILQ_REMOVE(&ulhead, ulp, ul_list);
			free(ulp);
			ulp = NULL;
		} else {
			ulp->ul_exported = 1;
		}
	} else if (davalid) {
		/*
		 * The UUID wasn't in the list, but DiskArb has a UUID for it.
		 * (If the DiskArb UUID conflicts with something already in the
		 * list, we'll need to create a new UUID for it below.)
		 */
		bcopy(dauuid, uuid, sizeof(dauuid));
	} else {
		/*
		 * We need to create a UUID to use for this volume.
		 * This is because it wasn't already in the list, and
		 * either DiskArb didn't have a UUID for the volume or
		 * the UUID DiskArb has for the volume conflicts with
		 * a UUID for a volume already in the list.
		 */
		reportuuid = 1;
		cfuuid = CFUUIDCreate(NULL);
		uuidbytes = CFUUIDGetUUIDBytes(cfuuid);
		bcopy(&uuidbytes, uuid, sizeof(uuidbytes));
		CFRelease(cfuuid);
	}

	if (!ulp) {
		/*
		 * Add the UUID to the list, but make sure it is unique first.
		 */
		while ((ulp = find_uuid(uuid))) {
			reportuuid = 1;
			uuidstring(uuid, buf);
			log(LOG_WARNING, "%s UUID conflict with %s, %s",
				fsb->f_mntonname, ulp->ul_mntonname, buf);
			cfuuid = CFUUIDCreate(NULL);
			uuidbytes = CFUUIDGetUUIDBytes(cfuuid);
			bcopy(&uuidbytes, uuid, sizeof(uuidbytes));
			CFRelease(cfuuid);
			/* double check that the UUID is unique */
		}
		ulp = add_uuid_to_list(fsb, (davalid ? dauuid : NULL), uuid);
		if (!ulp) {
			log(LOG_ERR, "error adding %s", fsb->f_mntonname);
		} else {
			ulp->ul_exported = 1;
		}
	} else if (!ulp->ul_mntfromname[0]) {
		/*
		 * If the volume didn't exist when mountd read the
		 * mountdexptab, it's possible this ulp doesn't
		 * have a copy of it's mntfromname.  So, we make
		 * sure to grab a copy here before the volume gets
		 * exported.
		 */
		strlcpy(ulp->ul_mntfromname, fsb->f_mntfromname, sizeof(ulp->ul_mntfromname));
	}

	if (reportuuid)
		log(LOG_WARNING, "%s using UUID %s", fsb->f_mntonname, uuidstring(uuid, buf));
	else
		DEBUG(1, "%s using UUID %s", fsb->f_mntonname, uuidstring(uuid, buf));

	return (ulp);
}

/*
 * clear export flags on all UUID entries
 */
void
uuidlist_clearexport(void)
{
	struct uuidlist *ulp;

	TAILQ_FOREACH(ulp, &ulhead, ul_list) {
		ulp->ul_exported = 0;
	}
}

/* convert UUID bytes to UUID string */
#define HEXTOC(c) \
	((c) >= 'a' ? ((c) - ('a' - 10)) : \
	((c) >= 'A' ? ((c) - ('A' - 10)) : ((c) - '0')))
#define HEXSTRTOI(p) \
	((HEXTOC(p[0]) << 4) + HEXTOC(p[1]))
char *
uuidstring(u_char *uuid, char *string)
{
	snprintf(string, (16*2)+4+1, /* XXX silly, yes. But at least we're not using sprintf. [sigh] */
		"%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
		uuid[0] & 0xff, uuid[1] & 0xff, uuid[2] & 0xff, uuid[3] & 0xff,
		uuid[4] & 0xff, uuid[5] & 0xff,
		uuid[6] & 0xff, uuid[7] & 0xff,
		uuid[8] & 0xff, uuid[9] & 0xff,
		uuid[10] & 0xff, uuid[11] & 0xff, uuid[12] & 0xff,
		uuid[13] & 0xff, uuid[14] & 0xff, uuid[15] & 0xff);
	return (string);
}

/*
 * save the exported volume UUID list to the mountdexptab file
 *
 * We have the option of saving all UUIDs in the list, or just
 * saving the ones that are currently exported.  However, if we
 * have a volume exported, then removed from the export list, and
 * then added back to the export list, it may be expected that the
 * file handles/UUIDs will be the same.  But if we don't save what
 * the UUIDs were before, we risk the chance of using a different
 * UUID for the second export.  This can happen if the volume's
 * DiskArb UUID is not used for export (because DiskArb doesn't have
 * a UUID for it, or because there was a UUID conflict and we needed
 * to use a different UUID).
 */
void
uuidlist_save(void)
{
	FILE *ulfile;
	struct uuidlist *ulp;
	struct expidlist *xid;
	char buf[64], buf2[64];

	if ((ulfile = fopen(_PATH_MOUNTEXPLIST, "w")) == NULL) {
		log(LOG_ERR, "Can't write %s: %s (%d)", _PATH_MOUNTEXPLIST,
			strerror(errno), errno);
		return;
	}
	TAILQ_FOREACH(ulp, &ulhead, ul_list) {
#ifdef SAVE_ONLY_EXPORTED_UUIDS
		if (!ulp->ul_exported)
			continue;
#endif
		if (ulp->ul_davalid)
			uuidstring(ulp->ul_dauuid, buf);
		else
			strlcpy(buf, "------------------------------------", sizeof(buf));
		uuidstring(ulp->ul_uuid, buf2);
		fprintf(ulfile, "%s %s 0x%08X %s\n", buf, buf2, ulp->ul_fsid, ulp->ul_mntonname);
		LIST_FOREACH(xid, &ulp->ul_exportids, xid_list) {
			fprintf(ulfile, "XID 0x%08X %s\n", xid->xid_id,
				((xid->xid_path[0] == '\0') ? "." : xid->xid_path));
		}
	}
	fclose(ulfile);
}

/*
 * read in the exported volume UUID list from the mountdexptab file
 */
void
uuidlist_restore(void)
{
	struct uuidlist *ulp;
	struct expidlist *xid;
	char *cp, str[2*MAXPATHLEN];
	FILE *ulfile;
	int i, slen, davalid, uuidchanged;
	uint32_t linenum;
	struct statfs fsb;
	u_char dauuid[16];
	char buf[64], buf2[64];

	if ((ulfile = fopen(_PATH_MOUNTEXPLIST, "r")) == NULL) {
		if (errno != ENOENT)
			log(LOG_WARNING, "Can't open %s: %s (%d)", _PATH_MOUNTEXPLIST,
				strerror(errno), errno);
		else
			DEBUG(1, "Can't open %s, %s (%d)", _PATH_MOUNTEXPLIST,
				strerror(errno), errno);
		return;
	}
	ulp = NULL;
	linenum = 0;
	while (fgets(str, 2*MAXPATHLEN, ulfile) != NULL) {
		linenum++;
		slen = strlen(str);
		if (str[slen-1] == '\n')
			str[slen-1] = '\0';
		if (!strncmp(str, "XID ", 4)) {
			/* we have an export ID line for the current UUID */
			if (!ulp) {
				log(LOG_ERR, "ignoring orphaned export ID at line %d of %s",
					linenum, _PATH_MOUNTEXPLIST);
				continue;
			}
			/* parse XID and add to current UUID */
			xid = malloc(sizeof(*xid));
			if (xid == NULL)
				out_of_mem("uuidlist_restore");
			cp = str + 4;
			slen -= 4;
			if (sscanf(cp, "%i", &xid->xid_id) != 1) {
				log(LOG_ERR, "invalid export ID at line %d of %s",
					linenum, _PATH_MOUNTEXPLIST);
				free(xid);
				continue;
			}
			while (*cp && (*cp != ' ')) {
				cp++;
				slen--;
			}
			cp++;
			slen--;
			if (slen >= (int)sizeof(xid->xid_path)) {
				log(LOG_ERR, "export ID path too long at line %d of %s",
					linenum, _PATH_MOUNTEXPLIST);
				free(xid);
				continue;
			}
			if ((cp[0] == '.') && (cp[1] == '\0'))
				xid->xid_path[0] = '\0';
			else
				strlcpy(xid->xid_path, cp, sizeof(xid->xid_path));
			LIST_INSERT_HEAD(&ulp->ul_exportids, xid, xid_list);
			continue;
		}
		ulp = malloc(sizeof(*ulp));
		if (ulp == NULL)
			out_of_mem("uuidlist_restore");
		bzero(ulp, sizeof(*ulp));
		LIST_INIT(&ulp->ul_exportids);
		cp = str;
		if (*cp == '-') {
			/* DiskArb UUID not present */
			ulp->ul_davalid = 0;
			bzero(ulp->ul_dauuid, sizeof(ulp->ul_dauuid));
			while (*cp && (*cp != ' '))
				cp++;
		} else {
			ulp->ul_davalid = 1;
			for (i=0; i < (int)sizeof(ulp->ul_dauuid); i++, cp+=2) {
				if (*cp == '-')
					cp++;
				if (!isxdigit(*cp) || !isxdigit(*(cp+1))) {
					log(LOG_ERR, "invalid UUID at line %d of %s",
						linenum, _PATH_MOUNTEXPLIST);
					free(ulp);
					ulp = NULL;
					break;
				}
				ulp->ul_dauuid[i] = HEXSTRTOI(cp);
			}
		}
		if (ulp == NULL)
			continue;
		cp++;
		for (i=0; i < (int)sizeof(ulp->ul_uuid); i++, cp+=2) {
			if (*cp == '-')
				cp++;
			if (!isxdigit(*cp) || !isxdigit(*(cp+1))) {
				log(LOG_ERR, "invalid UUID at line %d of %s",
					linenum, _PATH_MOUNTEXPLIST);
				free(ulp);
				ulp = NULL;
				break;
			}
			ulp->ul_uuid[i] = HEXSTRTOI(cp);
		}
		if (ulp == NULL)
			continue;
		if (*cp != ' ') {
			log(LOG_ERR, "invalid entry at line %d of %s",
				linenum, _PATH_MOUNTEXPLIST);
			free(ulp);
			continue;
		}
		cp++;
		if (sscanf(cp, "%i", &ulp->ul_fsid) != 1) {
			log(LOG_ERR, "invalid entry at line %d of %s",
				linenum, _PATH_MOUNTEXPLIST);
			free(ulp);
			continue;
		}
		while (*cp && (*cp != ' '))
			cp++;
		if (*cp != ' ') {
			log(LOG_ERR, "invalid entry at line %d of %s",
				linenum, _PATH_MOUNTEXPLIST);
			free(ulp);
			continue;
		}
		cp++;
		strncpy(ulp->ul_mntonname, cp, MAXPATHLEN);
		ulp->ul_mntonname[MAXPATHLEN-1] = '\0';

		/* verify the path exists and that it is a mount point */
		if (!check_dirpath(ulp->ul_mntonname) ||
		    (statfs(ulp->ul_mntonname, &fsb) < 0) ||
		    strcmp(ulp->ul_mntonname, fsb.f_mntonname)) {
			/* don't drop the UUID record if the volume isn't currently mounted! */
			/* If it's mounted/exported later, we want to use the same record. */
			DEBUG(1, "export entry for non-existent file system %s at line %d of %s",
				ulp->ul_mntonname, linenum, _PATH_MOUNTEXPLIST);
			ulp->ul_mntfromname[0] = '\0';
			TAILQ_INSERT_TAIL(&ulhead, ulp, ul_list);
			continue;
		}

		/* grab the path's mntfromname */
		strncpy(ulp->ul_mntfromname, fsb.f_mntfromname, MAXPATHLEN);
		ulp->ul_mntfromname[MAXPATHLEN-1] = '\0';

		/*
		 * Grab DiskArb's UUID for this volume (if any) and
		 * see if it has changed.
		 */
		davalid = get_uuid_from_diskarb(ulp->ul_mntfromname, dauuid);
		if (davalid) {
			if (!ulp->ul_davalid)
				uuidchanged = 1;
			else if (bcmp(ulp->ul_dauuid, dauuid, sizeof(dauuid)))
				uuidchanged = 1;
			else
				uuidchanged = 0;
		} else {
			if (ulp->ul_davalid) {
				/*
				 * We had a UUID before, but now we don't?
				 * Assume this is just a transient error,
				 * issue a warning, and stick with the old UUID.
				 */
				uuidstring(ulp->ul_dauuid, buf);
				log(LOG_WARNING, "lost UUID for %s, was %s, keeping old UUID",
					fsb.f_mntonname, buf);
				uuidchanged = 0;
			} else
				uuidchanged = 0;
		}
		if (uuidchanged) {
			/* The UUID changed, so we'll drop any entry */
			uuidstring(ulp->ul_dauuid, buf);
			if (davalid)
				uuidstring(dauuid, buf2);
			else
				strlcpy(buf2, "------------------------------------", sizeof(buf2));
			log(LOG_WARNING, "UUID changed for %s, was %s now %s",
				ulp->ul_mntonname, buf, buf2);
			free(ulp);
			continue;
		}

		TAILQ_INSERT_TAIL(&ulhead, ulp, ul_list);
	}
	fclose(ulfile);
}

struct expidlist *
find_export_id(struct uuidlist *ulp, u_int32_t id)
{
	struct expidlist *xid;

	LIST_FOREACH(xid, &ulp->ul_exportids, xid_list) {
		if (xid->xid_id == id)
			break;
	}

	return (xid);
}

struct expidlist *
get_export_id(struct uuidlist *ulp, char *path)
{
	struct expidlist *xid;
	u_int32_t maxid = 0;

	LIST_FOREACH(xid, &ulp->ul_exportids, xid_list) {
		if (!strcmp(xid->xid_path, path))
			break;
		if (maxid < xid->xid_id)
			maxid = xid->xid_id;
	}
	if (xid)
		return (xid);
	/* add it */
	xid = malloc(sizeof(*xid));
	if (!xid) {
		log(LOG_ERR, "get_export_id: out of memory");
		return (NULL);
	}
	bzero(xid, sizeof(*xid));
	strlcpy(xid->xid_path, path, sizeof(xid->xid_path));
	xid->xid_id = maxid + 1;
	while (find_export_id(ulp, xid->xid_id)) {
		xid->xid_id++;
		if (xid->xid_id == maxid) {
			/* exhausted export id values! */
			log(LOG_ERR, "export ID values exhausted for %s",
				ulp->ul_mntonname);
			free(xid);
			return (NULL);
		}
	}
	LIST_INSERT_HEAD(&ulp->ul_exportids, xid, xid_list);
	return (xid);
}

/*
 * find_exported_fs_by_path_and_uuid()
 *
 * Given a path and a uuid, find the exported file system that
 * best matches both.
 */
struct uuidlist *
find_exported_fs_by_path_and_uuid(char *fspath, u_char *fsuuid)
{
	struct uuidlist *ulp;

	ulp = TAILQ_FIRST(&ulhead);
	while (ulp) {
		/* find next matching uuid */
		while (ulp && fsuuid) {
			if (ulp->ul_davalid && !bcmp(ulp->ul_dauuid, fsuuid, sizeof(ulp->ul_dauuid)))
				break;
			if (!bcmp(ulp->ul_uuid, fsuuid, sizeof(ulp->ul_uuid)))
				break;
			ulp = TAILQ_NEXT(ulp, ul_list);
		}
		if (!ulp)
			break;
		/* we're done if fspath ommitted or matches */
		if (!fspath || !strcmp(ulp->ul_mntonname, fspath))
			break;
		ulp = TAILQ_NEXT(ulp, ul_list);
	}

	return (ulp);
}

/*
 * find_exported_fs_by_dirlist()
 *
 * Given a list of directories, find the common parent directory and
 * the exported file system that directory should be located on.
 */
struct uuidlist *
find_exported_fs_by_dirlist(struct dirlist *dirhead)
{
	struct dirlist *dirl;
	char *path, *p;
	int cmp, bestlen, len;
	struct uuidlist *ulp, *bestulp;

	if (!dirhead)
		return (NULL);

	path = strdup(dirhead->dl_dir);

	dirl = dirhead->dl_next;
	while (dirl) {
		cmp = subdir_check(path, dirl->dl_dir);
		if (cmp >= 0) {
			/* same or subdir, so skip */
			dirl = dirl->dl_next;
			continue;
		}
		p = strrchr(path, '/');
		if (p == path) {
			/* hit root */
			p[1] = '\0';
			break;
		}
		p[0] = '\0';
	}
	DEBUG(4, "find_exported_fs: %s", path);

	/*
	 * Now search uuid list for best match.
	 * We're looking for the longest mntonname that this path matches.
	 */
	bestulp = NULL;
	bestlen = -1;
	TAILQ_FOREACH(ulp, &ulhead, ul_list) {
		if (subdir_check(ulp->ul_mntonname, path) < 0)
			continue;
		len = strlen(ulp->ul_mntonname);
		if (len > bestlen) {
			bestulp = ulp;
			bestlen = strlen(ulp->ul_mntonname);
		}
	}

	free(path);
	DEBUG(4, "find_exported_fs: best exported fs: %s", bestulp ? bestulp->ul_mntonname : "<none>");
	return (bestulp);
}

/*
 * subdir_check()
 *
 * Compares two pathname strings to see if one is a subdir of the other.
 * Returns:
 * 1   if second path is a subdir of the first path
 * 0   if the paths are the same
 * -1  if the paths have just a substring match
 * -2  if the paths do not match at all
 * -3  if second path could not be a subdir of the first path (due to length)
 */
int
subdir_check(char *s1, char *s2)
{
	int len1, len2, rv;
	len1 = strlen(s1);
	len2 = strlen(s2);
	if (len1 > len2)
		rv = -3;
	else if (strncmp(s1, s2, len1))
		rv = -2;
	else if (len1 == len2)
		rv = 0;
	else if ((s2[len1] == '/') || (len1 == 1))
		rv = 1;
	else
		rv = -1;
	DEBUG(4, "subdir_check: %s %s %d", s1, s2, rv);
	return rv;
}

char *line = NULL;
uint32_t linesize = 0;
FILE *exp_file;
uint32_t linenum, entrylines;

/*
 * Get the export list
 */
int
get_exportlist(void)
{
	struct expfs *xf, *xf2, *xf3;
	struct grouplist ngrp, *grp, *tgrp, tmpgrp;
	struct expdir *xd, *xd2, *xd3;
	struct expidlist *xid;
	struct dirlist *dirhead, *dirl, *dirl2;
	struct namelisttqh names, netgroups;
	struct namelist *nl;
	struct host *ht, *ht2, *ht3;
	struct hosttqh htfree;
	struct statfs fsb;
	struct xucred anon;
	struct nfs_sec secflavs;
	char *cp, *endcp, *name, *word, *hst, *usr, *dom, savedc, *savedcp, *subdir, *mntonname, *word2;
	int len, dlen, hostcount, badhostcount, exflags, got_nondir, netgrp, cmp, show;
	char fspath[MAXPATHLEN];
	u_char uuid[16], fsuuid[16];
	struct uuidlist *ulp, *bestulp;
	char buf[64], buf2[64];
	int error, opt_flags, need_export, saved_errors;

	lock_exports();

	/*
	 * First, tag all existing export/group structures for deletion
	 */
	TAILQ_FOREACH(xf, &xfshead, xf_next) {
		TAILQ_FOREACH(xd, &xf->xf_dirl, xd_next) {
			xd->xd_iflags &= ~(OP_ADD|OP_OFFLINE|OP_ONLINE);
			xd->xd_ssec.count = 0;
			xd->xd_oflags = xd->xd_flags;
			xd->xd_ocred = xd->xd_cred;
			xd->xd_osec = xd->xd_sec;
			xd->xd_flags |= OP_DEL;
			TAILQ_FOREACH(ht, &xd->xd_hosts, ht_next)
				ht->ht_flags |= OP_DEL;
			TAILQ_FOREACH(xd2, &xd->xd_mountdirs, xd_next) {
				xd2->xd_flags |= OP_DEL;
				TAILQ_FOREACH(ht, &xd2->xd_hosts, ht_next)
					ht->ht_flags |= OP_DEL;
			}
		}
	}
	uuidlist_clearexport();

	if (xpaths) {
		free_dirlist(xpaths);
		xpaths = NULL;
	}
	xpaths_complete = 1;

	TAILQ_INIT(&names);
	TAILQ_INIT(&netgroups);
	hostnamecount = hostnamegoodcount = 0;
	export_errors = 0;
	linenum = 0;

	if ((exp_file = fopen(exportsfilepath, "r")) == NULL) {
		log(LOG_WARNING, "Can't open %s", exportsfilepath);
		export_errors = 1;
		goto exports_read;
	}

	/*
	 * Read in the exports and build the list, calling nfssvc(NFSSVC_EXPORT)
	 * as we go along to push the NEW export rules into the kernel.
	 */

	dirhead = NULL;
	while (get_export_entry()) {
		/*
		 * Create new exports list entry
		 */
		DEBUG(1, "---> Got line: %s", line);

		/*
		 * Set defaults.
		 */
		saved_errors = export_errors;
		hostcount = badhostcount = 0;
		anon = def_anon;
		exflags = 0;
		opt_flags = 0;
		got_nondir = 0;
		xf = NULL;
		ulp = NULL;
		fspath[0] = '\0';
		bzero(fsuuid, sizeof(fsuuid));

		/* init group list and net group */
		tgrp = NULL;
		bzero(&ngrp, sizeof(ngrp));

		/* init default security flavor */
		bzero(&secflavs, sizeof(secflavs));
		secflavs.flavors[0] = RPCAUTH_SYS;
		secflavs.count = 1;

		/*
		 * process all the fields in this line
		 * (we loop until nextfield finds nothing)
		 */
		cp = line;
		nextfield(&cp, &endcp);
		if (*cp == '#')
			goto nextline;

		while (endcp > cp) {
			DEBUG(2, "got field: %.*s", endcp-cp, cp);
			if (*cp == '-') {
				/*
				 * looks like we have some options
				 */
				if (dirhead == NULL) {
					export_error(LOG_ERR, "got options with no exported directory: %s", cp);
					export_error_cleanup(xf);
					goto nextline;
				}
				DEBUG(3, "processing option: %.*s", endcp-cp, cp);
				got_nondir = 1;
				savedcp = cp;
				if (do_opt(&cp, &endcp, &ngrp, &hostcount, &opt_flags,
					   &exflags, &anon, &secflavs, fspath, fsuuid)) {
					export_error(LOG_ERR, "error processing options: %s", savedcp);
					export_error_cleanup(xf);
					goto nextline;
				}
			} else if ((*cp == '/') || ((*cp == '\'' || *cp == '"') && (*(cp+1) == '/'))) {
				/*
				 * looks like we have a pathname
				 */
				DEBUG(2, "processing pathname: %.*s", endcp-cp, cp);
				word = clean_pathname(cp);
				DEBUG(3, "   cleaned pathname: %s", word);
				if (word == NULL) {
					export_error(LOG_ERR, "error processing pathname (out of memory)");
					export_error_cleanup(xf);
					goto nextline;
				}
				if (got_nondir) {
					export_error(LOG_ERR, "Directories must be first: %s", word);
					export_error_cleanup(xf);
					free(word);
					goto nextline;
				}
				if (strlen(word) > RPCMNT_NAMELEN) {
					export_error(LOG_ERR, "pathname too long (%d > %d): %s",
					    strlen(word), RPCMNT_NAMELEN, word);
					export_error_cleanup(xf);
					free(word);
					goto nextline;
				}
				/* Add path to this global exported path list */
				word2 = strdup(word);
				if ((error = add_dir(&xpaths, word2)))
					free(word2);
				if (error == ENOMEM) {
					log(LOG_WARNING, "Can't allocate memory to add export path: %s", word);
					xpaths_complete = 0;
				}
				/* Add path to this line's directory list */
				error = add_dir(&dirhead, word);
				if (error == EEXIST) {
					export_error(LOG_WARNING, "duplicate directory: %s", word);
					free(word);
				} else if (error == ENOMEM) {
					export_error(LOG_ERR, "Can't allocate memory to add directory: %s", word);
					export_error_cleanup(xf);
					free(word);
					goto nextline;
				}
			} else {
				/*
				 * looks like we have a host/netgroup
				 */
				savedc = *endcp;
				*endcp = '\0';
				DEBUG(2, "got host/netgroup: %s", cp);
				got_nondir = 1;
				if (dirhead == NULL) {
					export_error(LOG_ERR, "got host/group with no directory?: %s", cp);
					export_error_cleanup(xf);
					goto nextline;
				}

				/* add it to the name list */
				error = add_name(&names, cp);
				if (error == ENOMEM) {
					export_error(LOG_ERR, "Can't allocate memory to add host/group: %s", cp);
					export_error_cleanup(xf);
					goto nextline;
				}
				/* iterate name list until it's empty (first name gets GF_SHOW) */
				show = 1;
				while ((nl = TAILQ_FIRST(&names))) {
					TAILQ_REMOVE(&names, nl, nl_next);
					name = nl->nl_name;
					free(nl);
					/* check if name is a netgroup */
					setnetgrent(name);
					netgrp = getnetgrent(&hst, &usr, &dom);
					if (netgrp) {
						DEBUG(3, "got netgroup: %s", name);
						error = add_name(&netgroups, name);
						if (error == ENOMEM) {
							export_error(LOG_ERR, "Can't allocate memory to add host/group: %s", cp);
							export_error_cleanup(xf);
							endnetgrent();
							free(name);
							goto nextline;
						}
						if (show) {
							/* add an entry for the netgroup (w/GF_SHOW) */
							DEBUG(3, "add netgroup w/show: %s", name);
							show = 0;
							bzero(&tmpgrp, sizeof(tmpgrp));
							tmpgrp.gr_type = GT_NETGROUP;
							tmpgrp.gr_flags = GF_SHOW;
							tmpgrp.gr_u.gt_netgroup = strdup(name);
							if (!tmpgrp.gr_u.gt_netgroup || !(grp = get_grp(&tmpgrp))) {
								export_error(LOG_ERR, "Can't allocate memory to add netgroup - %s", name);
								export_error_cleanup(xf);
								endnetgrent();
								free(name);
								goto nextline;
							} else if (!add_grp(&tgrp, grp)) {
								DEBUG(3, "duplicate netgroup: %s", name);
								free_grp(grp);
							}
						}
						if (error == -1) {
							/* already in the netgroup list, skip it */
							DEBUG(3, "netgroup already in netgroup list: %s", name);
							endnetgrent();
							free(name);
							continue;
						}
						/* enumerate the netgroup, adding each name to the name list */
						do {
							DEBUG(3, "add netgroup member: %s", hst);
							error = add_name(&names, hst);
							if (error == ENOMEM) {
								export_error(LOG_ERR, "Can't allocate memory to add netgroup:host - %s", name, hst);
								export_error_cleanup(xf);
								endnetgrent();
								free(name);
								goto nextline;
							}
						} while (getnetgrent(&hst, &usr, &dom));
						endnetgrent();
						free(name);
						continue;
					}
					endnetgrent();
					/* not a netgroup, add a host/net entry */
					bzero(&tmpgrp, sizeof(tmpgrp));
					if (get_host_addresses(name, &tmpgrp)) {
						export_error(LOG_WARNING, "couldn't get address for host: %s", name);
						badhostcount++;
					} else {
						DEBUG(3, "got host: %s", name);
						if (show) {
							show = 0;
							tmpgrp.gr_flags |= GF_SHOW;
						}
						grp = get_grp(&tmpgrp);
						if (!grp) {
							export_error(LOG_ERR, "Can't allocate memory to add host - %s", name);
						} else if (!add_grp(&tgrp, grp)) {
							DEBUG(3, "duplicate host: %s", name);
							free_grp(grp);
						} else {
							hostcount++;
						}
					}
					free(name);
				}

				*endcp = savedc;
			}
			cp = endcp;
			nextfield(&cp, &endcp);
		}

		/*
		 * Done parsing through export entry fields.
		 */

		/* check options and hosts */
		if (!(opt_flags & (OP_MAPROOT|OP_MAPALL))) {
			/* If no mapping option specified, map root by default */
			exflags |= NX_MAPROOT;
			opt_flags |= OP_MAPROOT;
		}
		if (check_options(opt_flags)) {
			export_error(LOG_ERR, "bad export options");
			export_error_cleanup(xf);
			goto nextline;
		}
		if (!hostcount) {
			if (badhostcount) {
				export_error(LOG_ERR, "no valid hosts found for export");
				export_error_cleanup(xf);
				goto nextline;
			}
			DEBUG(1, "default export");
		} else if (opt_flags & OP_NET) {
			if (tgrp) {
				/*
				 * Don't allow a network export coincide with a list of
				 * host(s) on the same line.
				 */
				export_error(LOG_ERR, "can't specify both network and hosts on same line");
				export_error_cleanup(xf);
				goto nextline;
			}
			ngrp.gr_flags |= GF_SHOW;
			tgrp = get_grp(&ngrp);
			if (!tgrp) {
				export_error(LOG_ERR, "Can't allocate memory to add network - %s",
					grp_name(&ngrp));
				export_error_cleanup(xf);
				goto nextline;
			}
			ngrp.gr_type = GT_NULL;
		}

		/* check directory list */
		if (dirhead == NULL) { /* sanity check */
			export_error(LOG_ERR, "no directories for export entry?");
			export_error_cleanup(xf);
			goto nextline;
		}

		mntonname = NULL;
		if (opt_flags & (OP_FSPATH|OP_FSUUID))
			bestulp = find_exported_fs_by_path_and_uuid(
					(opt_flags & OP_FSPATH) ? fspath : NULL,
					(opt_flags & OP_FSUUID) ? fsuuid : NULL);
		else
			bestulp = find_exported_fs_by_dirlist(dirhead);

		dirl = dirhead;
		/* Look for an exported directory that passes check_dirpath() */
		while (dirl && !check_dirpath(dirl->dl_dir)) {
			export_error(LOG_WARNING, "path contains non-directory or non-existent components: %s", dirl->dl_dir);
			/* skip subdirectories */
			dirl2 = dirl->dl_next;
			while (dirl2 && (subdir_check(dirl->dl_dir, dirl2->dl_dir) == 1))
				dirl2 = dirl2->dl_next;
			dirl = dirl2;
		}
		if (!dirl) {
			if (!bestulp) {
				export_error(LOG_ERR, "no usable directories in export entry and no fallback");
				export_error_cleanup(xf);
				goto nextline;
			}
			export_error(LOG_WARNING, "no usable directories in export entry");
			goto prepare_offline_export;
		}
		if (statfs(dirl->dl_dir, &fsb) < 0) {
			export_error(LOG_ERR, "statfs failed (%s (%d)) for path: %s",
				strerror(errno), errno, dirl->dl_dir);
			export_error_cleanup(xf);
			goto nextline;
		}
		if ((opt_flags & OP_FSPATH) && strcmp(fsb.f_mntonname, fspath)) {
			/* fspath doesn't match export fs path? */
			if (!bestulp) {
				export_error(LOG_ERR, "file system path (%s) does not match fspath (%s) and no fallback",
					fsb.f_mntonname, fspath);
				export_error_cleanup(xf);
				goto nextline;
			}
			export_error(LOG_WARNING, "file system path (%s) does not match fspath (%s)",
				fsb.f_mntonname, fspath);
			goto prepare_offline_export;
		}
		if (bestulp && (subdir_check(fsb.f_mntonname, bestulp->ul_mntonname) > 0)) {
			export_error(LOG_WARNING, "Exported file system (%s) doesn't match best guess (%s).",
				fsb.f_mntonname, bestulp->ul_mntonname);
			if (!(opt_flags & (OP_FSUUID|OP_FSPATH)))
				export_error(LOG_WARNING, "Suggest using fspath=/path and/or fsuuid=uuid to disambiguate.");
			goto prepare_offline_export;
		}
		if (!(ulp = get_uuid(&fsb, uuid))) {
			export_error(LOG_ERR, "couldn't get UUID for volume: %s", fsb.f_mntonname);
			export_error_cleanup(xf);
			goto nextline;
		}
		if ((opt_flags & OP_FSUUID) && bcmp(uuid, fsuuid, sizeof(fsuuid))) {
			/* fsuuid doesn't match export fs uuid? */
			if (!bestulp) {
				export_error(LOG_ERR, "file system UUID (%s) does not match fsuuid (%s) and no fallback",
					uuidstring(uuid, buf), uuidstring(fsuuid, buf2));
				export_error_cleanup(xf);
				goto nextline;
			}
			export_error(LOG_WARNING, "file system UUID (%s) does not match fsuuid (%s)",
				uuidstring(uuid, buf), uuidstring(fsuuid, buf2));
			goto prepare_offline_export;
		}

		if (bestulp && (opt_flags & OP_MISSING)) {
prepare_offline_export:
			export_error(LOG_WARNING, "using fallback (marked offline): %s", bestulp->ul_mntonname);
			exflags |= NX_OFFLINE;
			opt_flags |= OP_MISSING;
			ulp = bestulp;
			mntonname = ulp->ul_mntonname;
			bcopy(ulp->ul_uuid, uuid, sizeof(uuid));
		} else {
			mntonname = fsb.f_mntonname;
		}

		/* See if this directory is already in the export list. */
		xf = ex_search(uuid);
		if (xf == NULL) {
			xf = get_expfs();
			if (xf)
				xf->xf_fsdir = malloc(strlen(mntonname) + 1);
			if (!xf || !xf->xf_fsdir) {
				export_error(LOG_ERR, "Can't allocate memory to export volume: %s",
					mntonname);
				export_error_cleanup(xf);
				goto nextline;
			}
			bcopy(uuid, xf->xf_uuid, sizeof(uuid));
			xf->xf_fsid = ulp->ul_fsid;
			strlcpy(xf->xf_fsdir, mntonname, strlen(mntonname)+1);
			DEBUG(2, "New expfs uuid=%s", uuidstring(uuid, buf));
		} else {
			DEBUG(2, "Found expfs uuid=%s", uuidstring(uuid, buf));
		}

		/* verify the rest of the directories in the list are kosher */
		if (dirl)
			dirl = dirl->dl_next;
		for (; dirl; dirl = dirl->dl_next) {
			DEBUG(2, "dir: %s", dirl->dl_dir);
			if (!check_dirpath(dirl->dl_dir)) {
				export_error(LOG_WARNING, "path contains non-directory or non-existent components: %s", dirl->dl_dir);
				continue;
			}
			if (statfs(dirl->dl_dir, &fsb) < 0) {
				export_error(LOG_WARNING, "statfs failed (%s (%d)) for path: %s",
					strerror(errno), errno, dirl->dl_dir);
				continue;
			}
			if (strcmp(xf->xf_fsdir, fsb.f_mntonname)) {
				export_error(LOG_WARNING, "Volume mismatch for: %s\ndirectories must be on same volume", dirl->dl_dir);
				continue;
			}
		}

		/*
		 * Done processing exports line fields.
		 */

		/*
		 * Walk the dirlist.
		 * Verify the next dir is (or can be) an exported directory.
		 * Check subsequent dirs to see if they are mount subdirs of that dir.
		 */
		dirl = dirhead;
		while (dirl) {
			DEBUG(2, "dir: %s", dirl->dl_dir);
			/*
			 * Check for nesting conflicts with any existing entries.
			 * Note we skip any entries that are NOT marked OP_ADD because
			 * we don't care about directories that are tagged for deletion.
			 */
			TAILQ_FOREACH(xd2, &xf->xf_dirl, xd_next) {
				if ((xd2->xd_iflags & OP_ADD) &&
				    ((subdir_check(xd2->xd_dir, dirl->dl_dir) == 1) ||
				     (subdir_check(dirl->dl_dir, xd2->xd_dir) == 1))) {
					export_error(LOG_ERR, "%s conflicts with existing export %s",
						dirl->dl_dir, xd2->xd_dir);
					export_error_cleanup(xf);
					goto nextline;
				}
			}
			/*
			 * Scan exported file system for a matching exported directory
			 * or at least the insertion point of a new one.
			 */
			len = strlen(dirl->dl_dir);
			xd3 = NULL;
			cmp = 1;
			TAILQ_FOREACH(xd2, &xf->xf_dirl, xd_next) {
				dlen = strlen(xd2->xd_dir);
				cmp = strncmp(dirl->dl_dir, xd2->xd_dir, dlen);
				DEBUG(3, "     %s compare %s %d", dirl->dl_dir,
					xd2->xd_dir, cmp);
				if (!cmp) {
					if (len == dlen) /* found an exact match */
						break;
					/* dirl was actually longer than xd2 */
					cmp = 1;
				}
				if (cmp > 0)
					break;
				xd3 = xd2;
			}

			if (!cmp) {
				xd = xd2;
				DEBUG(2, "     %s xd is %s", dirl->dl_dir, xd->xd_dir);
			} else {
				/* go ahead and create a new expdir structure */
				if (strncmp(dirl->dl_dir, mntonname, strlen(mntonname))) {
					export_error(LOG_ERR, "exported dir/fs mismatch: %s %s",
						dirl->dl_dir, mntonname);
					export_error_cleanup(xf);
					goto nextline;
				}
				/* first, get export path and ID */
				/* point subdir beyond mount path string */
				subdir = dirl->dl_dir + strlen(mntonname);
				/* skip "/" between mount and subdir */
				while (*subdir && (*subdir == '/'))
					subdir++;
				xid = get_export_id(ulp, subdir);
				if (!xid) {
					export_error(LOG_ERR, "unable to get export ID for %s", dirl->dl_dir);
					export_error_cleanup(xf);
					goto nextline;
				}
				xd = get_expdir();
				if (xd)
					xd->xd_dir = strdup(dirl->dl_dir);
				if (!xd || !xd->xd_dir) {
					if (xd)
						free_expdir(xd);
					export_error(LOG_ERR, "can't allocate memory for export %s", dirl->dl_dir);
					export_error_cleanup(xf);
					goto nextline;
				}
				xd->xd_xid = xid;
				DEBUG(2, "     %s new xd", xd->xd_dir);
			}

			/* preflight the addition of these new export options */
			if (hang_options_setup(xd, opt_flags, &anon, tgrp, &secflavs, &need_export)) {
				export_error(LOG_ERR, "export option conflict for %s", xd->xd_dir);
				/* XXX what to do about already successful exports? */
				hang_options_cleanup(xd);
				if (cmp)
					free_expdir(xd);
				export_error_cleanup(xf);
				goto nextline;
			}

			/*
			 * Send list of hosts to do_export for pushing the exports into
			 * the kernel (unless checkexports and the export is missing).
			 */
			if (need_export && !(checkexports && (opt_flags & OP_MISSING))) {
				int expcmd = checkexports ? NXA_CHECK : NXA_REPLACE;
				if (do_export(expcmd, xf, xd, tgrp, exflags, &anon, &secflavs)) {
					if ((errno == ENOTSUP) || (errno == EISDIR)) {
						/* if ENOTSUP report lack of NFS export support */
						/* if EISDIR report lack of extended readdir support */
						export_error(LOG_ERR, "kernel export registration failed: "
							"NFS exporting not supported by fstype \"%s\" (%s)",
							statfs(xf->xf_fsdir, &fsb) ? "?" : fsb.f_fstypename,
							(errno == EISDIR) ? "readdir" : "fh");
					} else {
						export_error(LOG_ERR, "kernel export registration failed");
					}
					hang_options_cleanup(xd);
					if (cmp)
						free_expdir(xd);
					export_error_cleanup(xf);
					goto nextline;
				}
				/* Success. Update the data structures.  */
				DEBUG(1, "kernel export registered for %s/%s", xf->xf_fsdir, xd->xd_xid->xid_path);
			} else {
				DEBUG(2, "kernel export already registered for %s/%s", xf->xf_fsdir, xd->xd_xid->xid_path);
			}

			/* add mount subdirectories of this directory */
			dirl2 = dirl->dl_next;
			while (dirl2) {
				if (subdir_check(dirl->dl_dir, dirl2->dl_dir) != 1)
					break;
				error = hang_options_mountdir(xd, dirl2->dl_dir, opt_flags, tgrp, &secflavs);
				if (error == EEXIST) {
					export_error(LOG_WARNING, "mount subdirectory export option conflict for %s",
						dirl2->dl_dir);
				} else if (error == ENOMEM) {
					export_error(LOG_WARNING, "unable to add mount subdirectory for %s, %s",
						xd->xd_dir, dirl2->dl_dir);
				}
				dirl2 = dirl2->dl_next;
			}
			dirl = dirl2;

			/* finalize export option additions */
			hang_options_finalize(xd);

			/* mark that we've added exports to this xd */
			xd->xd_iflags |= OP_ADD;

			if (cmp) {
				/* add new expdir to xf */
				if (xd3) {
					TAILQ_INSERT_AFTER(&xf->xf_dirl, xd3, xd, xd_next);
				} else {
					TAILQ_INSERT_HEAD(&xf->xf_dirl, xd, xd_next);
				}
			}

		}

		if ((xf->xf_flag & XF_LINKED) == 0) {
			/* Insert in the list in alphabetical order. */
			xf3 = NULL;
			TAILQ_FOREACH(xf2, &xfshead, xf_next) {
				if (strcmp(xf->xf_fsdir, xf2->xf_fsdir) < 0)
					break;
				xf3 = xf2;
			}
			if (xf3) {
				TAILQ_INSERT_AFTER(&xfshead, xf3, xf, xf_next);
			} else {
				TAILQ_INSERT_HEAD(&xfshead, xf, xf_next);
			}
			xf->xf_flag |= XF_LINKED;
		}

		if (export_errors == saved_errors) {
			/* no errors, clear any previous errors for this entry */
			if (clear_export_errors(linenum))
				log(LOG_WARNING, "exports:%d: export entry OK (previous errors cleared)", linenum);
		}
nextline:
		if (!TAILQ_EMPTY(&netgroups))
			free_namelist(&netgroups);
		if (!TAILQ_EMPTY(&names))
			free_namelist(&names);
		if (dirhead) {
			free_dirlist(dirhead);
			dirhead = NULL;
		}
		/* release groups */
		switch (ngrp.gr_type) {
		case GT_NET:
			if (ngrp.gr_u.gt_net.nt_name)
				free(ngrp.gr_u.gt_net.nt_name);
			break;
		}
		while (tgrp) {
			grp = tgrp;
			tgrp = tgrp->gr_next;
			grp->gr_flags &= ~GF_SHOW;
			free_grp(grp);
		}
	}

	fclose(exp_file);

	if (config.verbose >= 5) {
		DEBUG(3, "========> get_exportlist() CURRENT EXPORTS UPDATED:");
		dump_exports();
	}

exports_read:
	/*
	 * Now, find all existing structures still tagged for deletion.
	 * For each tagged group found, call nfssvc(NXA_DELETE) to delete
	 * the exports for the addresses that haven't had new/replacement
	 * exports registered.  We simply scan the current exports for
	 * an untagged match for each address.  If an exported directory
	 * loses all of its exports, we delete that expdir.
	 */
	xf = TAILQ_FIRST(&xfshead);
	while (xf && !checkexports) {
		xd = TAILQ_FIRST(&xf->xf_dirl);
		while (xd) {
			/*
			 * First check the list of mountdirs.  Since the kernel
			 * is not aware of these and they are not registered,
			 * we merely need to delete the data structures.
			 */
			TAILQ_FOREACH_SAFE(xd2, &xd->xd_mountdirs, xd_next, xd3) {
				TAILQ_FOREACH_SAFE(ht, &xd2->xd_hosts, ht_next, ht2)
					if (ht->ht_flags & OP_DEL) {
						TAILQ_REMOVE(&xd2->xd_hosts, ht, ht_next);
						free_host(ht);
					}
				if (xd2->xd_flags & OP_DEL)
					xd2->xd_flags = xd2->xd_oflags = 0;
				if (!(xd2->xd_flags & OP_DEFEXP) && TAILQ_EMPTY(&xd2->xd_hosts)) {
					/* No more exports here, delete */
					TAILQ_REMOVE(&xd->xd_mountdirs, xd2, xd_next);
					free_expdir(xd2);
				}
			}
			/*
			 * Now scan the xd_hosts list for hosts that are still
			 * tagged for deletion and move those to the htfree list.
			 */
			TAILQ_INIT(&htfree);
			TAILQ_FOREACH_SAFE(ht, &xd->xd_hosts, ht_next, ht2)
				if (ht->ht_flags & OP_DEL) {
					TAILQ_REMOVE(&xd->xd_hosts, ht, ht_next);
					TAILQ_INSERT_TAIL(&htfree, ht, ht_next);
				}
			/*
			 * Go through htfree and find the groups/addresses
			 * that are no longer being exported to and place
			 * those groups/addresses in the list tgrp.
			 * The hosts and groups/addresses that have been
			 * replaced with newer exports (and thus don't require
			 * deletion from the kernel) will be freed as we go.
			 */
			tgrp = NULL;
			TAILQ_FOREACH_SAFE(ht, &htfree, ht_next, ht2) {
				grp = ht->ht_grp;
				if (!find_group_address_match_in_host_list(&xd->xd_hosts, grp)) {
					/* no conflicts, so we can safely delete these */
					/* steal the grp from the host */
					ht->ht_grp = NULL;
					if (!add_grp(&tgrp, grp)) {
						/* shouldn't happen...  */
						log(LOG_ERR, "failure to queue group for export deletion");
						/* ... but try to recover anyway */
						grp->gr_next = tgrp;
						tgrp = grp;
					}
				} else if (grp->gr_type == GT_HOST) {
					uint32_t **a1, **a2, **a3, *atmp;
					/*
					 * Some or all of the addresses are still exported.
					 * Find any addresses whose exports haven't been replaced.
					 * a3 points to the location of the next address we will
					 * want to delete the export for.  a1 walks the array
					 * and a3 follows along/behind essentially compacting the
					 * array of addresses into only those that we want to
					 * delete exports for.  The effect is that addresses which
					 * are still exported will be "squeezed" out of the array.
					 */
					a1 = (uint32_t **) grp->gr_u.gt_hostent->h_addr_list;
					a3 = a1;
					while (*a1) {
						/* scan exports host list for GT_HOSTs w/matching address */
						TAILQ_FOREACH(ht3, &xd->xd_hosts, ht_next) {
							if (ht3->ht_grp->gr_type != GT_HOST)
								continue;
							a2 = (uint32_t **) ht->ht_grp->gr_u.gt_hostent->h_addr_list;
							while (*a2 && (**a1 != **a2))
								a2++;
							if (*a2) /* we found a match */
								break;
						}
						if (!ht3) {
							/* didn't find address, so "add" it to the array */
							if (*a3 != *a1) {
								atmp = *a3;
								*a3 = *a1;
								*a1 = atmp;
							}
							a3++;
						}
						a1++;
					}
					if (a3 == (uint32_t **)grp->gr_u.gt_hostent->h_addr_list) {
						/* a3 hasn't moved, so we know that all of */
						/* the addresses are being exported again */
						/* so we shouldn't delete any of the exports */
					} else {
						/* some of the addresses are being exported again */
						/* we've compacted the list of addresses that aren't */
						/* and here we will free up the rest of them */
						while (*a3) {
							free(*a3);
							*a3++ = NULL;
						}
						/* steal the grp from the host */
						ht->ht_grp = NULL;
						if (!add_grp(&tgrp, grp)) {
							/* shouldn't happen...  */
							log(LOG_ERR, "failure to queue group for export deletion");
							/* ... but try to recover anyway */
							grp->gr_next = tgrp;
							tgrp = grp;
						}
					}
				}
				TAILQ_REMOVE(&htfree, ht, ht_next);
				free_host(ht);
			}
			if ((config.verbose >= 3) && tgrp) {
				struct grouplist *g;
				DEBUG(1, "deleting export for %s/%s", xf->xf_fsdir, xd->xd_xid->xid_path);
				g = tgrp;
				while (g) {
					DEBUG(3, "    0x%x %d %s %s",
						g, g->gr_refcnt, grp_name(g),
						(g->gr_type == GT_HOST) ? 
						inet_ntoa(*(struct in_addr *)*g->gr_u.gt_hostent->h_addr_list) :
						(g->gr_type == GT_NET) ? 
						inet_ntoa(*(struct in_addr *)&g->gr_u.gt_net.nt_net) :
						(g->gr_type == GT_NETGROUP) ? "" : "???");
					g = g->gr_cache;
				}
			}
			if (tgrp && do_export(NXA_DELETE, xf, xd, tgrp, 0, NULL, NULL)) {
				log(LOG_ERR, "kernel export unregistration failed for %s, %s%s",
					xd->xd_dir, grp_name(tgrp), tgrp->gr_next ? ", ..." : "");
			}
			while (tgrp) {
				grp = tgrp;
				tgrp = tgrp->gr_next;
				free_grp(grp);
			}
			if ((xd->xd_flags & (OP_DEL|OP_DEFEXP)) == (OP_DEL|OP_DEFEXP)) {
				DEBUG(1, "deleting default export for %s/%s", xf->xf_fsdir, xd->xd_xid->xid_path);
				/* we need to delete this default export */
				xd->xd_flags = xd->xd_oflags = 0;
				if (do_export(NXA_DELETE, xf, xd, NULL, 0, NULL, NULL)) {
					log(LOG_ERR, "kernel export unregistration failed for %s,"
						" default export", xd->xd_dir);
				}
			}
			xd3 = TAILQ_NEXT(xd, xd_next);
			if (!(xd->xd_flags & OP_DEFEXP) && TAILQ_EMPTY(&xd->xd_hosts)) {
				TAILQ_REMOVE(&xf->xf_dirl, xd, xd_next);
				free_expdir(xd);
			} else {
				xd->xd_iflags &= ~OP_ADD;
				xd->xd_flags &= ~OP_DEL;
			}
			xd = xd3;
		}

		xf2 = TAILQ_NEXT(xf, xf_next);
		if (TAILQ_EMPTY(&xf->xf_dirl)) {
			/* No more exports here, delete */
			TAILQ_REMOVE(&xfshead, xf, xf_next);
			free_expfs(xf);
		}
		xf = xf2;
	}

	if (config.verbose >= 4) {
		DEBUG(2, "========> get_exportlist() NEW EXPORTS LIST:");
		dump_exports();
	}

	if (!checkexports)
		uuidlist_save();

	/*
	 * If we appear to be having problems resolving host names on startup,
	 * then we'll want to automatically recheck exports for a while.
	 *
	 * First time through, we make sure to set recheckexports.
	 * If we have problems then set the recheck timer - otherwise disable it.
	 * On subsequent export checks, turn off the recheck timer once we no
	 * longer need it or the timer expires.
	 */
	if (!checkexports && (recheckexports == 0)) {	/* first time through... */
		/* did we have any host names and were any of them problematic? */
		if (hostnamegoodcount != hostnamecount) {
			log(LOG_WARNING, "There seem to be problems resolving host names...");
			log(LOG_WARNING, "...will periodically recheck exports for a while.");
			recheckexports = time(NULL) + RECHECKEXPORTS_TIMEOUT; /* set the recheck timer */
		} else {
			recheckexports = -1; /* turn it off */
		}
	} else if (recheckexports > 0) {
		/* if we don't need to recheck any more, turn it off */
		if (hostnamegoodcount == hostnamecount) {
			recheckexports = -1;
		} else if (recheckexports < time(NULL)) {
			log(LOG_WARNING, "Giving up on automatic rechecking of exports.");
			recheckexports = -1;
		}
	}

	unlock_exports();

	return (export_errors);
}

/*
 * Allocate an exported file system structure
 */
struct expfs *
get_expfs(void)
{
	struct expfs *xf;

	xf = malloc(sizeof(*xf));
	if (xf == NULL)
		return (NULL);
	memset(xf, 0, sizeof(*xf));
	TAILQ_INIT(&xf->xf_dirl);
	return (xf);
}

/*
 * Allocate an exported directory structure
 */
struct expdir *
get_expdir(void)
{
	struct expdir *xd;

	xd = malloc(sizeof(*xd));
	if (xd == NULL)
		return (NULL);
	memset(xd, 0, sizeof(*xd));
	TAILQ_INIT(&xd->xd_hosts);
	TAILQ_INIT(&xd->xd_mountdirs);
	return (xd);
}

/*
 * Return the "name" of the given group
 */
static char unknown_group[] = "unknown group";
char *
grp_name(struct grouplist *grp)
{
	if (grp->gr_type == GT_NETGROUP)
		return (grp->gr_u.gt_netgroup);
	if (grp->gr_type == GT_NET)
		return (grp->gr_u.gt_net.nt_name);
	if (grp->gr_type == GT_HOST)
		return (grp->gr_u.gt_hostent->h_name);
	return (unknown_group);
}

/*
 * compare two group list elements
 */
int
grpcmp(struct grouplist *g1, struct grouplist *g2)
{
	struct hostent *h1, *h2;
	struct netmsk *n1, *n2;
	uint32_t **a1, **a2;
	int rv;

	rv = g1->gr_type - g2->gr_type;
	if (rv)
		return (rv);
	switch (g1->gr_type) {
	case GT_NETGROUP:
		rv = strcmp(g1->gr_u.gt_netgroup, g2->gr_u.gt_netgroup);
		break;
	case GT_NET:
		n1 = &g1->gr_u.gt_net;
		n2 = &g2->gr_u.gt_net;
		rv = strcmp(n1->nt_name, n2->nt_name);
		if (rv)
			break;
		rv = n1->nt_net - n2->nt_net;
		if (rv)
			break;
		rv = n1->nt_mask - n2->nt_mask;
		break;
	case GT_HOST:
		h1 = g1->gr_u.gt_hostent;
		h2 = g2->gr_u.gt_hostent;
		rv = strcmp(h1->h_name, h2->h_name);
		if (rv)
			break;
		a1 = (uint32_t **)h1->h_addr_list;
		a2 = (uint32_t **)h2->h_addr_list;
		while (*a1 && *a2) {
			rv = **a1 - **a2;
			if (rv)
				break;
			a1++;
			a2++;
		}
		if (!rv && !(!*a1 && !*a2)) {
			if (*a1)
				rv = 1;
			else
				rv = -1;
		}
		break;
	}
	return (rv);
}

/*
 * Return a group in the group cache that matches the given group.
 * If the group isn't yet in the cache, a new group will be allocated,
 * populated with the info from the given group, and added to the cache.
 * In any event, any memory referred to within grptmp->gr_u has been
 * either used in a new group cache entry or freed.
 */
struct grouplist *
get_grp(struct grouplist *grptmp)
{
	struct grouplist *g, *g2;
	char **addrp;
	int cmp = 1, clean_up_gr_u = 1;

	if (config.verbose >= 7) {
		DEBUG(5, "get_grp: %s %s",
			grp_name(grptmp),
			(grptmp->gr_type == GT_HOST) ? 
			inet_ntoa(*(struct in_addr *)*grptmp->gr_u.gt_hostent->h_addr_list) :
			(grptmp->gr_type == GT_NET) ? 
			inet_ntoa(*(struct in_addr *)&grptmp->gr_u.gt_net.nt_net) :
			(grptmp->gr_type == GT_NETGROUP) ? "" : "???");
		g = grpcache;
		while (g) {
			DEBUG(6, "grpcache: 0x%x %d %s %s",
				g, g->gr_refcnt, grp_name(g),
				(g->gr_type == GT_HOST) ? 
				inet_ntoa(*(struct in_addr *)*g->gr_u.gt_hostent->h_addr_list) :
				(g->gr_type == GT_NET) ? 
				inet_ntoa(*(struct in_addr *)&g->gr_u.gt_net.nt_net) :
				(g->gr_type == GT_NETGROUP) ? "" : "???");
			g = g->gr_cache;
		}
	}

	g2 = NULL;
	g = grpcache;
	while (g && ((cmp = grpcmp(grptmp, g)) > 0)) {
		g2 = g;
		g = g->gr_cache;
	}

	if (!cmp) {
		g->gr_refcnt++;
		if (config.verbose >= 7)
			DEBUG(5, "get_grp: found 0x%x %d %s %s",
				g, g->gr_refcnt, grp_name(g),
				(g->gr_type == GT_HOST) ? 
				inet_ntoa(*(struct in_addr *)*g->gr_u.gt_hostent->h_addr_list) :
				(g->gr_type == GT_NET) ? 
				inet_ntoa(*(struct in_addr *)&g->gr_u.gt_net.nt_net) :
				(g->gr_type == GT_NETGROUP) ? "" : "???");
		g->gr_flags |= grptmp->gr_flags;
		goto out;
	}

	g = malloc(sizeof(*g));
	if (g == NULL)
		goto out;
	memset(g, 0, sizeof(*g));
	g->gr_refcnt = 1;
	g->gr_type = grptmp->gr_type;
	g->gr_flags = grptmp->gr_flags;
	g->gr_u = grptmp->gr_u; /* memory allocations in *grptmp->gr_u are now owned by g->gr_u */
	clean_up_gr_u = 0;

	if (g2) {
		g->gr_cache = g2->gr_cache;
		g2->gr_cache = g;
	} else {
		g->gr_cache = grpcache;
		grpcache = g;
	}

	if (config.verbose >= 7) {
		DEBUG(5, "get_grp: ----- NEW 0x%x %d %s %s",
			g, g->gr_refcnt, grp_name(g),
			(g->gr_type == GT_HOST) ? 
			inet_ntoa(*(struct in_addr *)*g->gr_u.gt_hostent->h_addr_list) :
			(g->gr_type == GT_NET) ? 
			inet_ntoa(*(struct in_addr *)&g->gr_u.gt_net.nt_net) :
			(g->gr_type == GT_NETGROUP) ? "" : "???");
		g2 = grpcache;
		while (g2) {
			DEBUG(6, "grpcache: 0x%x %d %s %s",
				g2, g2->gr_refcnt, grp_name(g2),
				(g2->gr_type == GT_HOST) ? 
				inet_ntoa(*(struct in_addr *)*g2->gr_u.gt_hostent->h_addr_list) :
				(g2->gr_type == GT_NET) ? 
				inet_ntoa(*(struct in_addr *)&g2->gr_u.gt_net.nt_net) :
				(g2->gr_type == GT_NETGROUP) ? "" : "???");
			g2 = g2->gr_cache;
		}
	}

out:
	if (clean_up_gr_u) {
		/* free up the contents of grptmp->gr_u */
		switch (grptmp->gr_type) {
		case GT_HOST:
			if (grptmp->gr_u.gt_hostent) {
				addrp = grptmp->gr_u.gt_hostent->h_addr_list;
				while (addrp && *addrp)
					free(*addrp++);
				if (grptmp->gr_u.gt_hostent->h_addr_list)
					free(grptmp->gr_u.gt_hostent->h_addr_list);
				if (grptmp->gr_u.gt_hostent->h_name)
					free(grptmp->gr_u.gt_hostent->h_name);
				free(grptmp->gr_u.gt_hostent);
			}
			break;
		case GT_NET:
			if (grptmp->gr_u.gt_net.nt_name)
				free(grptmp->gr_u.gt_net.nt_name);
			break;
		case GT_NETGROUP:
			if (grptmp->gr_u.gt_netgroup)
				free(grptmp->gr_u.gt_netgroup);
			break;
		}
	}

	return (g);
}

/*
 * Free up a group list.
 */
void
free_grp(struct grouplist *grp)
{
	struct grouplist **g;
	char **addrp;

	/* decrement reference count */
	grp->gr_refcnt--;

	if (config.verbose >= 7)
		DEBUG(5, "free_grp: 0x%x %d %s %s",
			grp, grp->gr_refcnt, grp_name(grp),
			(grp->gr_type == GT_HOST) ? 
			inet_ntoa(*(struct in_addr *)*grp->gr_u.gt_hostent->h_addr_list) :
			(grp->gr_type == GT_NET) ? 
			inet_ntoa(*(struct in_addr *)&grp->gr_u.gt_net.nt_net) :
			(grp->gr_type == GT_NETGROUP) ? "" : "???");

	if (grp->gr_refcnt > 0)
		return;

	/* remove group from grpcache list */
	g = &grpcache;
	while (*g && (*g != grp))
		g = &(*g)->gr_cache;
	*g = (*g)->gr_cache;

	/* free up the memory */
	if (grp->gr_type == GT_HOST) {
		if (grp->gr_u.gt_hostent) {
			addrp = grp->gr_u.gt_hostent->h_addr_list;
			while (addrp && *addrp)
				free(*addrp++);
			if (grp->gr_u.gt_hostent->h_addr_list)
				free(grp->gr_u.gt_hostent->h_addr_list);
			if (grp->gr_u.gt_hostent->h_name)
				free(grp->gr_u.gt_hostent->h_name);
			free(grp->gr_u.gt_hostent);
		}
	} else if (grp->gr_type == GT_NET) {
		if (grp->gr_u.gt_net.nt_name)
			free(grp->gr_u.gt_net.nt_name);
	} else if (grp->gr_type == GT_NETGROUP) {
		if (grp->gr_u.gt_netgroup)
			free(grp->gr_u.gt_netgroup);
	}
	free((caddr_t)grp);
}

/*
 * insert a group list element into a group list
 */
int
add_grp(struct grouplist **glp, struct grouplist *newg)
{
	struct grouplist *g1, *g2;
	int cmp = 1;

	g2 = NULL;
	g1 = *glp;
	while (g1 && ((cmp = grpcmp(newg, g1)) > 0)) {
		g2 = g1;
		g1 = g1->gr_next;
	}
	if (!cmp) {
		/* already in list, make sure SHOW bit is set */
		if (newg->gr_flags & GF_SHOW)
			g1->gr_flags |= GF_SHOW;
		return (0);
	}
	if (g2) {
		newg->gr_next = g2->gr_next;
		g2->gr_next = newg;
	} else {
		newg->gr_next = *glp;
		*glp = newg;
	}
	return (1);
}

/*
 * insert a host list element into a host list
 * (identical to add_grp(), but takes a host list and
 *  allows "duplicates" if one is tagged for deletion)
 */
int
add_host(struct hosttqh *head, struct host *newht)
{
	struct host *ht;
	int cmp = 1;

	TAILQ_FOREACH(ht, head, ht_next)
		if ((cmp = grpcmp(newht->ht_grp, ht->ht_grp)) <= 0)
			break;
	if (!cmp && !(ht->ht_flags & OP_DEL)) {
		/* already in list, make sure SHOW bit is set */
		if (newht->ht_flags & OP_SHOW)
			ht->ht_flags |= OP_SHOW;
		return (0);
	}
	if (ht)
		TAILQ_INSERT_BEFORE(ht, newht, ht_next);
	else
		TAILQ_INSERT_TAIL(head, newht, ht_next);
	return (1);
}

/*
 * report/record an export error message
 */
void
export_error(int level, const char *fmt, ...)
{
	struct errlist *elnew, *el, *elp;
	char *s = NULL;
	va_list ap;

	export_errors++;

	va_start(ap, fmt);
	vasprintf(&s, fmt, ap);
	va_end(ap);

	/* check if we've already logged this error */
	LIST_FOREACH(el, &xerrs, el_next) {
		if (linenum < el->el_linenum)
			continue;
		if (linenum > el->el_linenum) {
			el = NULL;
			break;
		}
		if (!strcmp(el->el_msg, s))
			break;
	}

	/* log the message if we haven't already */
	if (!el)
		log(level, "exports:%d: %s", linenum, s);

	/* add this error to the list */
	elnew = malloc(sizeof(*elnew));
	if (!elnew) {
		free(s);
		return;
	}
	elnew->el_linenum = linenum;
	elnew->el_msg = s;
	elp = NULL;
	LIST_FOREACH(el, &xerrs, el_next) {
		if (linenum < el->el_linenum)
			break;
		elp = el;
	}
	if (elp)
		LIST_INSERT_AFTER(elp, elnew, el_next);
	else
		LIST_INSERT_HEAD(&xerrs, elnew, el_next);
}

/*
 * clear export errors on the give line# (or all if line#=0)
 */
int
clear_export_errors(uint32_t linenum)
{
	struct errlist *el, *elnext;
	int cleared = 0;

	LIST_FOREACH_SAFE(el, &xerrs, el_next, elnext) {
		if (linenum) {
			if (linenum < el->el_linenum)
				break;
			if (linenum > el->el_linenum)
				continue;
		}
		cleared = 1;
		LIST_REMOVE(el, el_next);
		if (el->el_msg)
			free(el->el_msg);
		free(el);
	}

	return (cleared);
}

/*
 * Clean up upon an error in get_exportlist().
 */
void
export_error_cleanup(struct expfs *xf)
{
	if (xf && (xf->xf_flag & XF_LINKED) == 0)
		free_expfs(xf);
}

/*
 * Search the export list for a matching fs.
 */
struct expfs *
ex_search(u_char *uuid)
{
	struct expfs *xf;

	TAILQ_FOREACH(xf, &xfshead, xf_next) {
		if (!bcmp(xf->xf_uuid, uuid, 16))
			return (xf);
	}
	return (xf);
}

/*
 * add a directory to a dirlist (sorted)
 *
 * Note that the list is sorted to place subdirectories
 * after the entry for their matching parent directory.
 * This isn't strictly sorted because other directories may
 * have similar names with characters that sort lower than '/'.
 * For example: /export /export.test /export/subdir
 */
int
add_dir(struct dirlist **dlpp, char *cp)
{
	struct dirlist *newdl, *dl, *dl2, *dl3, *dlstop;
	int cplen, dlen, cmp;

	dlstop = NULL;
	dl2 = NULL;
	dl = *dlpp;
	cplen = strlen(cp);

	while (dl && (dl != dlstop)) {
		dlen = strlen(dl->dl_dir);
		cmp = strncmp(cp, dl->dl_dir, dlen);
		DEBUG(3, "add_dir: %s compare %s %d", cp, dl->dl_dir, cmp);
		if (cmp < 0)
			break;
		if (cmp == 0) {
			if (cplen == dlen) /* duplicate */
				return (EEXIST);
			if (cp[dlen] == '/') {
				/*
				 * Find the next entry that isn't a
				 * subdirectory of this directory so
				 * we know when to stop looking for
				 * the insertion point.
				 */
				DEBUG(3, "add_dir: %s compare %s %d subdir match",
					cp, dl->dl_dir, cmp);
				dlstop = dl->dl_next;
				while (dlstop && (subdir_check(dl->dl_dir, dlstop->dl_dir) == 1))
					dlstop = dlstop->dl_next;
			} else {
				/*
				 * The new dir should go after this directory and
				 * its subdirectories.  So, skip subdirs of this dir.
				 */
				DEBUG(3, "add_dir: %s compare %s %d partial match",
					cp, dl->dl_dir, cmp);
				dl3 = dl;
				dl2 = dl;
				dl = dl->dl_next;
				while (dl && (subdir_check(dl3->dl_dir, dl->dl_dir) == 1)) {
					dl2 = dl;
					dl = dl->dl_next;
					}
				continue;
			}
		}
		dl2 = dl;
		dl = dl->dl_next;
	}
	if (dl && (dl == dlstop))
		DEBUG(3, "add_dir: %s stopped before %s", cp, dlstop->dl_dir);
	newdl = malloc(sizeof(*dl));
	if (newdl == NULL) {
		log(LOG_ERR, "can't allocate memory to add dir %s", cp);
		return (ENOMEM);
	}
	newdl->dl_dir = cp;
	if (dl2 == NULL) {
		newdl->dl_next = *dlpp;
		*dlpp = newdl;
	} else {
		newdl->dl_next = dl;
		dl2->dl_next = newdl;
	}
	if (config.verbose >= 6) {
		dl = *dlpp;
		while (dl) {
			DEBUG(4, "DIRLIST: %s", dl->dl_dir);
			dl = dl->dl_next;
		}
	}
	return (0);
}

/*
 * free up all the elements in a dirlist
 */
void
free_dirlist(struct dirlist *dl)
{
	struct dirlist *dl2;

	while (dl) {
		dl2 = dl->dl_next;
		if (dl->dl_dir)
			free(dl->dl_dir);
		free(dl);
		dl = dl2;
	}
}

/*
 * add a name to a namelist
 *
 * returns 0 on success, -1 on duplicate, or error
 */
int
add_name(struct namelisttqh *names, char *name)
{
	struct namelist *nl;

	TAILQ_FOREACH(nl, names, nl_next)
		if (!strcmp(nl->nl_name, name))
			return (-1);
	nl = malloc(sizeof(*nl));
	if (!nl)
		return (ENOMEM);
	nl->nl_name = strdup(name);
	if (!nl->nl_name) {
		free(nl);
		return (ENOMEM);
	}
	TAILQ_INSERT_TAIL(names, nl, nl_next);
	return (0);
}

/*
 * free up all the elements in a namelist
 */
void
free_namelist(struct namelisttqh *names)
{
	struct namelist *nl, *nlnext;

	TAILQ_FOREACH_SAFE(nl, names, nl_next, nlnext) {
		TAILQ_REMOVE(names, nl, nl_next);
		if (nl->nl_name)
			free(nl->nl_name);
		free(nl);
	}
}

/*
 * find a host list entry that has the same group
 */
struct host *
find_group_match_in_host_list(struct hosttqh *head, struct grouplist *grp)
{
	struct host *ht;

	TAILQ_FOREACH(ht, head, ht_next)
		if (!grpcmp(grp, ht->ht_grp))
			break;
	return (ht);
}

/*
 * find a host list entry that has the same address
 */
struct host *
find_group_address_match_in_host_list(struct hosttqh *head, struct grouplist *grp)
{
	struct host *ht;
	struct netmsk *n1, *n2;
	uint32_t **a1, **a2;

	switch (grp->gr_type) {
	case GT_HOST:
		a1 = (uint32_t **) grp->gr_u.gt_hostent->h_addr_list;
		while (*a1) {
			TAILQ_FOREACH(ht, head, ht_next) {
				if (ht->ht_grp->gr_type != GT_HOST)
					continue;
				if (ht->ht_flags & OP_DEL)
					continue;
				a2 = (uint32_t **) ht->ht_grp->gr_u.gt_hostent->h_addr_list;
				while (*a2) {
					if (**a1 == **a2)
						return (ht);
					a2++;
				}
			}
			a1++;
		}
		break;
	case GT_NET:
		n1 = &grp->gr_u.gt_net;
		TAILQ_FOREACH(ht, head, ht_next) {
			if (ht->ht_grp->gr_type != GT_NET)
				continue;
			if (ht->ht_flags & OP_DEL)
				continue;
			n2 = &ht->ht_grp->gr_u.gt_net;
			if ((n1->nt_net & n1->nt_mask) == (n2->nt_net & n2->nt_mask))
				return (ht);
		}
		break;
	}

	return (NULL);
}

/*
 * compare two credentials
 */
int
crcmp(struct xucred *cr1, struct xucred *cr2)
{
	int i;

	if (cr1 == cr2)
		return 0;
	if (cr1 == NULL || cr2 == NULL)
		return 1;
	if (cr1->cr_uid != cr2->cr_uid)
		return 1;
	if (cr1->cr_ngroups != cr2->cr_ngroups)
		return 1;
	/* XXX assumes groups will always be listed in some order */
	for (i=0; i < cr1->cr_ngroups; i++)
		if (cr1->cr_groups[i] != cr2->cr_groups[i])
			return 1;
	return (0);
}

/*
 * tentatively hang export options for a list of groups off of an exported directory
 */
int
hang_options_setup(struct expdir *xd, int opt_flags, struct xucred *cr, struct grouplist *grp,
		   struct nfs_sec *secflavs, int *need_export)
{
	struct host *ht;

	*need_export = 0;

	if (!grp) {
		/* default export */
		if (xd->xd_flags & OP_DEFEXP) {
			/* exported directory already has default export! */
			if ((OP_EXOPTS(xd->xd_flags) == OP_EXOPTS(opt_flags)) &&
			    (!(opt_flags & (OP_MAPALL|OP_MAPROOT)) || !crcmp(&xd->xd_cred, cr)) &&
			    (!cmp_secflavs(&xd->xd_sec, secflavs))) {
				if (!(xd->xd_flags & OP_DEL))
					log(LOG_WARNING, "duplicate default export for %s", xd->xd_dir);
				xd->xd_flags &= ~OP_EXOPTMASK;
				xd->xd_flags |= opt_flags | OP_DEFEXP | OP_ADD;
				return (0);
			} else if (!(xd->xd_flags & OP_DEL)) {
				log(LOG_ERR, "multiple/conflicting default exports for %s", xd->xd_dir);
				return (EEXIST);
			}
		}
		xd->xd_flags &= ~OP_EXOPTMASK;
		xd->xd_flags |= opt_flags | OP_DEFEXP | OP_ADD;
		if (cr)
			xd->xd_cred = *cr;
		bcopy(secflavs, &xd->xd_sec, sizeof(struct nfs_sec));
		DEBUG(3, "hang_options_setup: %s default 0x%x", xd->xd_dir, xd->xd_flags);
		*need_export = 1;
		return (0);
	}

	while (grp) {
		/* first check for an existing entry for this group */
		ht = find_group_match_in_host_list(&xd->xd_hosts, grp);
		if (ht) {
			/* found a match... */
			if ((OP_EXOPTS(ht->ht_flags) == OP_EXOPTS(opt_flags)) &&
			    (!(opt_flags & (OP_MAPALL|OP_MAPROOT)) || !crcmp(&ht->ht_cred, cr)) &&
			    (!cmp_secflavs(&ht->ht_sec, secflavs))) {
				/* options match, OK, it's the same export */
				if (!(ht->ht_flags & OP_ADD) && !(grp->gr_flags & GF_SHOW))
					ht->ht_flags &= ~OP_SHOW;
				ht->ht_flags |= OP_ADD;
				if (grp->gr_flags & GF_SHOW)
					ht->ht_flags |= OP_SHOW;
				grp = grp->gr_next;
				continue;
			}
			/* options don't match... */
			if (!(ht->ht_flags & OP_DEL)) {
				/* this is a new entry, so this is a conflict */
				log(LOG_ERR, "conflicting exports for %s, %s", xd->xd_dir, grp_name(grp));
				return (EEXIST);
			}
			/* this entry is marked for deletion, so there is no conflict */
			/* go ahead and add a new entry with the new options */
		}
		/* also check for an existing entry for any addresses in this group */
		ht = find_group_address_match_in_host_list(&xd->xd_hosts, grp);
		if (ht) {
			/* found a match... */
			if ((OP_EXOPTS(ht->ht_flags) != OP_EXOPTS(opt_flags)) ||
			    ((opt_flags & (OP_MAPALL|OP_MAPROOT)) && crcmp(&ht->ht_cred, cr)) ||
			    (cmp_secflavs(&ht->ht_sec, secflavs))) {
				/* ...with different options */
				log(LOG_ERR, "conflicting exports for %s, %s", xd->xd_dir, grp_name(grp));
				return (EEXIST);
			}
			/* ... with same options */
			log(LOG_WARNING, "duplicate export for %s, %s vs. %s", xd->xd_dir,
				grp_name(grp), grp_name(ht->ht_grp));
			grp = grp->gr_next;
			continue;
		}
		/* OK to add a new host */
		ht = get_host();
		if (!ht) {
			log(LOG_ERR, "Can't allocate memory for host: %s", grp_name(grp));
			return(ENOMEM);
		}
		ht->ht_flags = opt_flags | OP_ADD;
		ht->ht_cred = *cr;
		ht->ht_grp = grp;
		grp->gr_refcnt++;
		bcopy(secflavs, &ht->ht_sec, sizeof(struct nfs_sec));
		if (grp->gr_flags & GF_SHOW)
			ht->ht_flags |= OP_SHOW;
		if (config.verbose >= 6)
			DEBUG(4, "grp2host: 0x%x %d %s %s",
				grp, grp->gr_refcnt, grp_name(grp),
				(grp->gr_type == GT_HOST) ? 
				inet_ntoa(*(struct in_addr *)*grp->gr_u.gt_hostent->h_addr_list) :
				(grp->gr_type == GT_NET) ? 
				inet_ntoa(*(struct in_addr *)&grp->gr_u.gt_net.nt_net) :
				(grp->gr_type == GT_NETGROUP) ? "" : "???");
		if (!add_host(&xd->xd_hosts, ht)) {
			/* This shouldn't happen given the above checks */
			log(LOG_ERR, "duplicate host in export list: %s", grp_name(grp));
			free_host(ht);
			return (EEXIST);
		}
		*need_export = 1;
		DEBUG(3, "hang_options_setup: %s %s 0x%x", xd->xd_dir, grp_name(grp), opt_flags);
		grp = grp->gr_next;
	}

	return (0);
}

/*
 * make permanent the export options added via hang_options_setup()
 */
void
hang_options_finalize(struct expdir *xd)
{
	struct host *ht;
	struct expdir *mxd;

	if (xd->xd_flags & OP_ADD) {
		xd->xd_iflags |= (xd->xd_flags & (OP_OFFLINE|OP_MISSING)) ? OP_OFFLINE : OP_ONLINE;
		merge_secflavs(&xd->xd_ssec, &xd->xd_sec);
		xd->xd_flags &= ~(OP_ADD|OP_DEL);
		/* update old options in case subsequent export fails */
		xd->xd_oflags = xd->xd_flags;
		xd->xd_ocred = xd->xd_cred;
		bcopy(&xd->xd_sec, &xd->xd_osec, sizeof(struct nfs_sec));
	}

	TAILQ_FOREACH(ht, &xd->xd_hosts, ht_next) {
		if (!(ht->ht_flags & OP_ADD))
			continue;
		ht->ht_flags &= ~(OP_ADD|OP_DEL);
		xd->xd_iflags |= (ht->ht_flags & (OP_OFFLINE|OP_MISSING)) ? OP_OFFLINE : OP_ONLINE;
		merge_secflavs(&xd->xd_ssec, &ht->ht_sec);
	}

	TAILQ_FOREACH(mxd, &xd->xd_mountdirs, xd_next) {
		hang_options_finalize(mxd);
	}
}

/*
 * cleanup/undo the export options added via hang_options_setup()
 */
void
hang_options_cleanup(struct expdir *xd)
{
	struct host *ht, *htnext;

	if (xd->xd_flags & OP_ADD) {
		xd->xd_flags = xd->xd_oflags;
		xd->xd_cred = xd->xd_ocred;
		bcopy(&xd->xd_osec, &xd->xd_sec, sizeof(struct nfs_sec));
	}

	TAILQ_FOREACH_SAFE(ht, &xd->xd_hosts, ht_next, htnext) {
		if (!(ht->ht_flags & OP_ADD))
			continue;
		if (ht->ht_flags & OP_DEL) {
			ht->ht_flags &= ~OP_ADD;
			continue;
		}
		TAILQ_REMOVE(&xd->xd_hosts, ht, ht_next);
		free_host(ht);
	}

	/*
	 * Note: currently cleanup isn't called after handling mountdirs,
	 * so we don't have to bother cleaning up any of the mountdirs.
	 */
}

/*
 * hang export options for mountable subdirectories of an exported directory
 */
int
hang_options_mountdir(struct expdir *xd, char *dir, int opt_flags, struct grouplist *grp, struct nfs_sec *secflavs)
{
	struct host *ht;
	struct expdir *mxd, *mxd2, *mxd3;
	int cmp;

	/* check for existing mountdir */
	mxd = mxd3 = NULL;
	TAILQ_FOREACH(mxd2, &xd->xd_mountdirs, xd_next) {
		cmp = strcmp(dir, mxd2->xd_dir);
		if (!cmp) {
			/* found it */
			mxd = mxd2;
			break;
		} else if (cmp < 0) {
			/* found where it needs to be inserted */
			break;
		}
		mxd3 = mxd2;
	}
	if (!mxd) {
		mxd = get_expdir();
		if (mxd)
			mxd->xd_dir = strdup(dir);
		if (!mxd || !mxd->xd_dir) {
			if (mxd)
				free_expdir(mxd);
			log(LOG_ERR, "can't allocate memory for mountable sub-directory; %s", dir);
			return (ENOMEM);
		}
		if (mxd3) {
			TAILQ_INSERT_AFTER(&xd->xd_mountdirs, mxd3, mxd, xd_next);
		} else {
			TAILQ_INSERT_HEAD(&xd->xd_mountdirs, mxd, xd_next);
		}
	}

	if (!grp) {
		/* default export */
		if (mxd->xd_flags & OP_DEFEXP) {
			/* exported directory already has default export! */
			if ((OP_EXOPTS(mxd->xd_flags) == OP_EXOPTS(opt_flags)) &&
			   (!cmp_secflavs(&mxd->xd_sec, secflavs))) {
				if (!(mxd->xd_flags & OP_DEL))
					log(LOG_WARNING, "duplicate default export for %s", mxd->xd_dir);
				mxd->xd_flags &= ~OP_EXOPTMASK;
				mxd->xd_flags |= opt_flags | OP_DEFEXP | OP_ADD;
				return (0);
			} else if (!(mxd->xd_flags & OP_DEL)) {
				log(LOG_ERR, "multiple/conflicting default exports for %s", mxd->xd_dir);
				return (EEXIST);
			}
		}
		mxd->xd_flags &= ~OP_EXOPTMASK;
		mxd->xd_flags |= opt_flags | OP_DEFEXP | OP_ADD;
		bcopy(secflavs, &mxd->xd_sec, sizeof(struct nfs_sec));
		DEBUG(3, "hang_options_mountdir: %s default 0x%x",
			mxd->xd_dir, mxd->xd_flags);
		return (0);
	}

	while (grp) {
		/* first check for an existing entry for this group */
		ht = find_group_match_in_host_list(&mxd->xd_hosts, grp);
		if (ht) {
			/* found a match... */
			if ((OP_EXOPTS(ht->ht_flags) == OP_EXOPTS(opt_flags)) &&
			   (!cmp_secflavs(&ht->ht_sec, secflavs))) {
				/* options match, OK, it's the same export */
				ht->ht_flags |= OP_ADD;
				grp = grp->gr_next;
				continue;
			}
			/* options don't match... */
			if (!(ht->ht_flags & OP_DEL)) {
				/* this is a new entry, so this is a conflict */
				log(LOG_ERR, "conflicting mountdir exports for %s, %s",
					mxd->xd_dir, grp_name(grp));
				return (EEXIST);
			}
			/* this entry is marked for deletion, so there is no conflict */
			/* go ahead and add a new entry with the new options */
		}
		/* also check for an existing entry for any addresses in this group */
		ht = find_group_address_match_in_host_list(&mxd->xd_hosts, grp);
		if (ht) {
			/* found a match... */
			if ((OP_EXOPTS(ht->ht_flags) != OP_EXOPTS(opt_flags)) ||
			   (cmp_secflavs(&ht->ht_sec, secflavs))) {
				/* ...with different options */
				log(LOG_ERR, "conflicting mountdir exports for %s, %s",
					mxd->xd_dir, grp_name(grp));
				return (EEXIST);
			}
			/* ... with same options */
			log(LOG_WARNING, "duplicate mountdir export for %s, %s vs. %s",
				mxd->xd_dir, grp_name(grp), grp_name(ht->ht_grp));
			grp = grp->gr_next;
			continue;
		}
		/* OK to add a new host */
		ht = get_host();
		if (!ht) {
			log(LOG_ERR, "Can't allocate memory for host: %s", grp_name(grp));
			return(ENOMEM);
		}
		ht->ht_flags = opt_flags | OP_ADD;
		ht->ht_grp = grp;
		grp->gr_refcnt++;
		bcopy(secflavs, &ht->ht_sec, sizeof(struct nfs_sec));
		if (grp->gr_flags & GF_SHOW)
			ht->ht_flags |= OP_SHOW;
		if (config.verbose >= 6)
			DEBUG(4, "grp2host: 0x%x %d %s %s",
				grp, grp->gr_refcnt, grp_name(grp),
				(grp->gr_type == GT_HOST) ? 
				inet_ntoa(*(struct in_addr *)*grp->gr_u.gt_hostent->h_addr_list) :
				(grp->gr_type == GT_NET) ? 
				inet_ntoa(*(struct in_addr *)&grp->gr_u.gt_net.nt_net) :
				(grp->gr_type == GT_NETGROUP) ? "" : "???");
		if (!add_host(&mxd->xd_hosts, ht)) {
			/* This shouldn't happen given the above checks */
			log(LOG_ERR, "Can't add host to mountdir export list: %s", grp_name(grp));
			free_host(ht);
			return (EEXIST);
		}
		DEBUG(3, "hang_options_mountdir: %s %s 0x%x",
			mxd->xd_dir, grp_name(grp), opt_flags);
		grp = grp->gr_next;
	}

	return (0);
}

/*
 * Search for an exported directory on an exported file system that
 * a given host can mount and return the export options.
 *
 * Search order:
 * an exact match on exported directory path
 * an exact match on exported directory mountdir path
 * a subdir match on exported directory mountdir path with ALLDIRS
 * a subdir match on exported directory path with ALLDIRS
 */
int
expdir_search(struct expfs *xf, char *dirpath, uint32_t saddr, int *options, struct nfs_sec *secflavs)
{
	struct expdir *xd, *mxd;
	struct host *hp;
	int cmp = 1, chkalldirs = 0;

	TAILQ_FOREACH(xd, &xf->xf_dirl, xd_next) {
		if ((cmp = subdir_check(xd->xd_dir, dirpath)) >= 0)
			break;
	}
	if (!xd) {
		DEBUG(1, "expdir_search: no matching export: %s", dirpath);
		return (0);
	}

	DEBUG(1, "expdir_search: %s -> %s", dirpath, xd->xd_dir);

	if (cmp == 0) {
		/* exact match on exported directory path */
check_xd_hosts:
		/* find options for this host */
		hp = find_host(&xd->xd_hosts, saddr);
		if (hp && (!chkalldirs || (hp->ht_flags & OP_ALLDIRS))) {
			DEBUG(2, "expdir_search: %s host %s", dirpath,
				(chkalldirs ? "alldirs" : "match"));
			*options = hp->ht_flags;
			bcopy(&hp->ht_sec, secflavs, sizeof(struct nfs_sec));
		} else if ((xd->xd_flags & OP_DEFEXP) &&
		           (!chkalldirs || (xd->xd_flags & OP_ALLDIRS))) {
			DEBUG(2, "expdir_search: %s defexp %s",
				dirpath, (chkalldirs ? "alldirs" : "match"));
			*options = xd->xd_flags;
			bcopy(&xd->xd_sec, secflavs, sizeof(struct nfs_sec));
		} else {
			/* not exported to this host */
			*options = 0;
			DEBUG(2, "expdir_search: %s NO match", dirpath);
			return (0);
		}
		return (1);
	}

	/* search for a matching mountdir */
	TAILQ_FOREACH(mxd, &xd->xd_mountdirs, xd_next) {
		cmp = subdir_check(mxd->xd_dir, dirpath);
		if (cmp < 0)
			continue;
		DEBUG(1, "expdir_search: %s subdir path match %s",
			dirpath, mxd->xd_dir);
		chkalldirs = (cmp != 0);
		/* found a match on a mountdir */
		hp = find_host(&mxd->xd_hosts, saddr);
		if (hp && (!chkalldirs || (hp->ht_flags & OP_ALLDIRS))) {
			DEBUG(2, "expdir_search: %s -> %s subdir host %s",
				dirpath, mxd->xd_dir, (chkalldirs ? "alldirs" : "match"));
			*options = hp->ht_flags;
			bcopy(&hp->ht_sec, secflavs, sizeof(struct nfs_sec));
			return (1);
		} else if ((mxd->xd_flags & OP_DEFEXP) &&
			   (!chkalldirs || (mxd->xd_flags & OP_ALLDIRS))) {
			DEBUG(2, "expdir_search: %s -> %s subdir defexp %s",
				dirpath, mxd->xd_dir, (chkalldirs ? "alldirs" : "match"));
			*options = mxd->xd_flags;
			bcopy(&mxd->xd_sec, secflavs, sizeof(struct nfs_sec));
			return (1);
		}
		/* not exported to this host */
	}

	DEBUG(1, "expdir_search: %s NO match, check alldirs", dirpath);
	chkalldirs = 1;
	goto check_xd_hosts;
}

/*
 * search a host list for a match for the given address
 */
struct host *
find_host(struct hosttqh *head, uint32_t saddr)
{
	struct host *hp;
	struct grouplist *grp;
	uint32_t **addrp;

	TAILQ_FOREACH(hp, head, ht_next) {
		grp = hp->ht_grp;
		switch (grp->gr_type) {
		case GT_HOST:
			addrp = (uint32_t **) grp->gr_u.gt_hostent->h_addr_list;
			while (*addrp) {
				if (**addrp == saddr)
					return (hp);
				addrp++;
			}
			break;
		case GT_NET:
			if ((saddr & grp->gr_u.gt_net.nt_mask) ==
			    grp->gr_u.gt_net.nt_net)
				return (hp);
			break;
		}
	}

	return (NULL);
}

/*
 * Parse the option string and update fields.
 * Option arguments may either be -<option>=<value> or
 * -<option> <value>
 */
int
do_opt( char **cpp,
	char **endcpp,
	struct grouplist *ngrp,
	int *hostcountp,
	int *opt_flagsp,
	int *exflagsp,
	struct xucred *cr,
	struct nfs_sec *sec_flavs,
	char *fspath,
	u_char *fsuuid)
{
	char *cpoptarg = NULL, *cpoptend = NULL;
	char *cp, *endcp, *cpopt, *cpu, savedc, savedc2 = '\0', savedc3 = '\0';
	int mapallflag, usedarg;
	int i, rv = 0;
	size_t len;

	cpopt = *cpp;
	cpopt++;
	cp = *endcpp;
	savedc = *cp;
	*cp = '\0';
	while (cpopt && *cpopt) {
		mapallflag = 1;
		usedarg = -2;
		if (NULL != (cpoptend = strchr(cpopt, ','))) {
			*cpoptend++ = '\0';
			if (NULL != (cpoptarg = strchr(cpopt, '='))) {
				savedc3 = *cpoptarg;
				*cpoptarg++ = '\0';
			}
		} else {
			if (NULL != (cpoptarg = strchr(cpopt, '='))) {
				savedc3 = *cpoptarg;
				*cpoptarg++ = '\0';
			} else {
				*cp = savedc;
				nextfield(&cp, &endcp);
				**endcpp = '\0';
				if (endcp > cp && *cp != '-') {
					cpoptarg = cp;
					savedc2 = *endcp;
					*endcp = '\0';
					usedarg = 0;
				}
			}
		}
		if (!strcmp(cpopt, "ro") || !strcmp(cpopt, "o")) {
			*exflagsp |= NX_READONLY;
			*opt_flagsp |= OP_READONLY;
		} else if (cpoptarg && (!strcmp(cpopt, "maproot") ||
			!(mapallflag = strcmp(cpopt, "mapall")) ||
			!strcmp(cpopt, "root") || !strcmp(cpopt, "r"))) {
			usedarg++;
			rv = parsecred(cpoptarg, cr);
			if (rv) {
				log(LOG_ERR, "map credential error");
				goto out;
			} else if (mapallflag == 0) {
				*exflagsp |= NX_MAPALL;
				*opt_flagsp |= OP_MAPALL;
			} else {
				*exflagsp |= NX_MAPROOT;
				*opt_flagsp |= OP_MAPROOT;
			}
		} else if (cpoptarg && (!strcmp(cpopt, "mask") ||
			!strcmp(cpopt, "m"))) {
			if (*opt_flagsp & OP_MASK) {
				log(LOG_ERR, "Network option conflict");
				rv = 1;
				goto out;
			}
			if (get_net(cpoptarg, &ngrp->gr_u.gt_net, 1)) {
				log(LOG_ERR, "Bad mask: %s", cpoptarg);
				rv = 1;
				goto out;
			}
			usedarg++;
			*opt_flagsp |= OP_MASK;
		} else if (cpoptarg && (!strcmp(cpopt, "network") ||
			!strcmp(cpopt, "n"))) {
			if (*opt_flagsp & OP_NET) {
				log(LOG_ERR, "Network option conflict");
				rv = 1;
				goto out;
			}
			if (get_net(cpoptarg, &ngrp->gr_u.gt_net, 0)) {
				log(LOG_ERR, "Bad net: %s", cpoptarg);
				rv = 1;
				goto out;
			}
			ngrp->gr_type = GT_NET;
			*hostcountp = *hostcountp + 1;
			usedarg++;
			*opt_flagsp |= OP_NET;
		} else if (cpoptarg && (!strcmp(cpopt, "sec"))) {
			if (*opt_flagsp & OP_SECFLAV)
				log(LOG_WARNING, "A security option was already specified and will be replaced.");
			if (get_sec_flavors(cpoptarg, sec_flavs)) {
				log(LOG_ERR, "Bad security option: %s", cpoptarg);
				rv = 1;
				goto out;
			}
			usedarg++;
			*opt_flagsp |= OP_SECFLAV;
		} else if (!strcmp(cpopt, "alldirs")) {
			*opt_flagsp |= OP_ALLDIRS;
		} else if (!strcmp(cpopt, "32bitclients")) {
			*exflagsp |= NX_32BITCLIENTS;
			*opt_flagsp |= OP_32BITCLIENTS;
		} else if (!strcmp(cpopt, "fspath")) {
			if (!cpoptarg) {
				log(LOG_WARNING, "export option '%s' missing a value.", cpopt);
			} else if (cpoptarg[0] != '/') {
				log(LOG_ERR, "invalid fspath: %s", cpoptarg);
				rv = 1;
				goto out;
			} else {
				len = strlcpy(fspath, cpoptarg, MAXPATHLEN);
				if (len >= MAXPATHLEN) {
					log(LOG_ERR, "%s option path too long: %s", cpopt, cpoptarg);
					rv = 1;
					goto out;
				}
				*opt_flagsp |= OP_FSPATH;
			}
		} else if (!strcmp(cpopt, "fsuuid")) {
			if (!cpoptarg) {
				log(LOG_WARNING, "export option '%s' missing a value.", cpopt);
			} else {
				cpu = cpoptarg;
				for (i=0; i < 16; i++, cpu+=2) {
					if (*cpu == '-')
						cpu++;
					if (!isxdigit(*cpu) || !isxdigit(*(cpu+1))) {
						log(LOG_ERR, "invalid fsuuid: %s", cpoptarg);
						rv = 1;
						goto out;
					}
					fsuuid[i] = HEXSTRTOI(cpu);
				}
				*opt_flagsp |= OP_FSUUID;
			}
		} else if (!strcmp(cpopt, "offline")) {
			*exflagsp |= NX_OFFLINE;
			*opt_flagsp |= OP_OFFLINE;
		} else {
			log(LOG_WARNING, "unrecognized export option: %s", cpopt);
			goto out;
		}
		if (usedarg >= 0) {
			*endcp = savedc2;
			**endcpp = savedc;
			if (usedarg > 0) {
				*cpp = cp;
				*endcpp = endcp;
			}
			return (0);
		}
		if (cpoptend)
			*(cpoptend-1) = ',';
		if (savedc3) {
			*(cpoptarg-1) = savedc3;
			savedc3 = '\0';
		}
		cpopt = cpoptend;
	}
out:
	if (savedc2)
		*endcp = savedc2;
	if (cpoptend)
		*(cpoptend-1) = ',';
	if (savedc3)
		*(cpoptarg-1) = savedc3;
	**endcpp = savedc;
	return (rv);
}

/*
 * Translate a character string to the corresponding list of network
 * addresses for a hostname.
 */
int
get_host_addresses(char *cp, struct grouplist *grp)
{
	struct hostent *hp, *nhp;
	char **addrp, **naddrp;
	struct hostent t_host;
	int i;
	uint32_t saddr;
	char *aptr[2];

	if (grp->gr_type != GT_NULL)
		return (1);
	if ((hp = gethostbyname(cp)) == NULL) {
		if (!isdigit(*cp)) {
			log(LOG_ERR, "Gethostbyname failed for %s", cp);
			hostnamecount++;
			return (1);
		}
		saddr = inet_addr(cp);
		if (saddr == INADDR_NONE) {
			log(LOG_ERR, "Inet_addr failed for %s", cp);
			hostnamecount++;
			return (1);
		}
		if ((hp = gethostbyaddr((caddr_t)&saddr, sizeof (saddr),
					AF_INET)) == NULL) {
			hp = &t_host;
			hp->h_name = cp;
			hp->h_addrtype = AF_INET;
			hp->h_length = sizeof (uint32_t);
			hp->h_addr_list = aptr;
			aptr[0] = (char *)&saddr;
			aptr[1] = (char *)NULL;
		}
	} else {
		hostnamecount++;
		hostnamegoodcount++;
	}
	nhp = malloc(sizeof(struct hostent));
	if (nhp == NULL)
		goto nomem;
	memmove(nhp, hp, sizeof(struct hostent));
	i = strlen(hp->h_name)+1;
	nhp->h_name = malloc(i);
	if (nhp->h_name == NULL)
		goto nomem;
	memmove(nhp->h_name, hp->h_name, i);
	addrp = hp->h_addr_list;
	i = 1;
	while (*addrp++)
		i++;
	naddrp = nhp->h_addr_list = malloc(i*sizeof(char *));
	if (naddrp == NULL)
		goto nomem;
	bzero(naddrp, i*sizeof(char *));
	addrp = hp->h_addr_list;
	while (*addrp) {
		*naddrp = malloc(hp->h_length);
		if (*naddrp == NULL)
			goto nomem;
		memmove(*naddrp, *addrp, hp->h_length);
		addrp++;
		naddrp++;
	}
	*naddrp = NULL;
	grp->gr_type = GT_HOST;
	grp->gr_u.gt_hostent = nhp;
	return (0);
nomem:
	if (nhp) {
		if (nhp->h_name) {
			naddrp = nhp->h_addr_list;
			while (naddrp && *naddrp)
				free(*naddrp++);
			if (nhp->h_addr_list)
				free(nhp->h_addr_list);
			free(nhp->h_name);
		}
		free(nhp);
	}
	log(LOG_ERR, "can't allocate memory for host address(es) for %s", cp);
	return (1);
}

/*
 * Free up an exported directory structure
 */
void
free_expdir(struct expdir *xd)
{
	struct expdir *xd2;

	free_hosts(&xd->xd_hosts);
	while ((xd2 = TAILQ_FIRST(&xd->xd_mountdirs))) {
		TAILQ_REMOVE(&xd->xd_mountdirs, xd2, xd_next);
		free_expdir(xd2);
	}
	if (xd->xd_dir)
		free(xd->xd_dir);
	free(xd);
}

/*
 * Free up an exportfs structure
 */
void
free_expfs(struct expfs *xf)
{
	struct expdir *xd;

	while ((xd = TAILQ_FIRST(&xf->xf_dirl))) {
		TAILQ_REMOVE(&xf->xf_dirl, xd, xd_next);
		free_expdir(xd);
	}
	if (xf->xf_fsdir)
		free(xf->xf_fsdir);
	free(xf);
}

/*
 * Free up a host.
 */
void
free_host(struct host *hp)
{
	if (hp->ht_grp)
		free_grp(hp->ht_grp);
	free(hp);
}

/*
 * Free up a list of hosts.
 */
void
free_hosts(struct hosttqh *head)
{
	struct host *hp, *hp2;

	TAILQ_FOREACH_SAFE(hp, head, ht_next, hp2) {
		TAILQ_REMOVE(head, hp, ht_next);
		free_host(hp);
	}
}

/*
 * Allocate a host structure
 */
struct host *
get_host(void)
{
	struct host *hp;

	hp = malloc(sizeof(struct host));
	if (hp == NULL)
		return (NULL);
	hp->ht_flags = 0;
	return (hp);
}

/*
 * Out of memory, fatal
 */
void
out_of_mem(const char *msg)
{
	log(LOG_ERR, "%s: Out of memory", msg);
	exit(2);
}

/*
 * Do the NFSSVC_EXPORT syscall to push the export info into the kernel.
 */
int
do_export(
	int expcmd,
	struct expfs *xf,
	struct expdir *xd,
	struct grouplist *grplist,
	int exflags,
	struct xucred *cr,
	struct nfs_sec *secflavs)
{
	struct nfs_export_args nxa;
	struct nfs_export_net_args *netargs, *na;
	struct grouplist *grp;
	uint32_t **addrp;
	struct sockaddr_in *sin, *imask;
	uint32_t net;

	nxa.nxa_flags = expcmd;
	if ((exflags & NX_OFFLINE) && (expcmd != NXA_CHECK))
		nxa.nxa_flags |= NXA_OFFLINE;
	nxa.nxa_fsid = xf->xf_fsid;
	nxa.nxa_fspath = xf->xf_fsdir;
	nxa.nxa_exppath = xd->xd_xid->xid_path;
	nxa.nxa_expid = xd->xd_xid->xid_id;

	/* first, count the number of hosts/nets we're pushing in for this export */
	/* !grplist => default export */
	nxa.nxa_netcount = (!grplist ? 1 : 0);
	grp = grplist;
	while (grp) {
		if (grp->gr_type == GT_HOST) {
			addrp = (uint32_t **)grp->gr_u.gt_hostent->h_addr_list;
			/* count # addresses given for this host */
			while (addrp && *addrp) {
				nxa.nxa_netcount++;
				addrp++;
			}
		} else if (grp->gr_type == GT_NET) {
			nxa.nxa_netcount++;
		}
		grp = grp->gr_next;
	}

	netargs = malloc(nxa.nxa_netcount * sizeof(struct nfs_export_net_args));
	if (!netargs) {
		/* XXX we could possibly fall back to pushing them in, one-by-one */
		log(LOG_ERR, "do_export(): malloc failed for %d net args", nxa.nxa_netcount);
		return (1);
	}
	nxa.nxa_nets = netargs;

#define INIT_NETARG(N) \
	do { \
		(N)->nxna_flags = exflags; \
		(N)->nxna_cred = cr ? *cr : def_anon; \
		memset(&(N)->nxna_sec, 0, sizeof(struct nfs_sec)); \
		if (secflavs != NULL) \
			bcopy(secflavs, &(N)->nxna_sec, sizeof(struct nfs_sec)); \
		sin = (struct sockaddr_in*)&(N)->nxna_addr; \
		imask = (struct sockaddr_in*)&(N)->nxna_mask; \
		memset(sin, 0, sizeof(*sin)); \
		memset(imask, 0, sizeof(*imask)); \
		sin->sin_family = AF_INET; \
		sin->sin_len = sizeof(*sin); \
		imask->sin_family = AF_INET; \
		imask->sin_len = sizeof(*imask); \
	} while (0)

	na = netargs;
	if (!grplist) {
		/* default export, no address */
		INIT_NETARG(na);
		sin->sin_len = 0;
		imask->sin_len = 0;
		na++;
	}
	grp = grplist;
	while (grp) {
		switch (grp->gr_type) {
		case GT_HOST:
			addrp = (uint32_t **)grp->gr_u.gt_hostent->h_addr_list;
			/* handle each host address in h_addr_list */
			while (*addrp) {
				INIT_NETARG(na);
				sin->sin_addr.s_addr = **addrp;
				imask->sin_len = 0;
				addrp++;
				na++;
			}
			break;
		case GT_NET:
			INIT_NETARG(na);
			if (grp->gr_u.gt_net.nt_mask)
			    imask->sin_addr.s_addr = grp->gr_u.gt_net.nt_mask;
			else {
			    net = ntohl(grp->gr_u.gt_net.nt_net);
			    if (IN_CLASSA(net))
				imask->sin_addr.s_addr = inet_addr("255.0.0.0");
			    else if (IN_CLASSB(net))
				imask->sin_addr.s_addr =
				    inet_addr("255.255.0.0");
			    else
				imask->sin_addr.s_addr =
				    inet_addr("255.255.255.0");
			    grp->gr_u.gt_net.nt_mask = imask->sin_addr.s_addr;
			}
			sin->sin_addr.s_addr = grp->gr_u.gt_net.nt_net;
			na++;
			break;
		case GT_NETGROUP:
			break;
		default:
			log(LOG_ERR, "Bad grouptype");
			free(netargs);
			return (1);
		}

		grp = grp->gr_next;
	}

	if (nfssvc(NFSSVC_EXPORT, &nxa)) {
		if ((expcmd != NXA_CHECK) && (expcmd != NXA_DELETE) && (errno == EPERM)) {
			log(LOG_ERR, "Can't change attributes for %s.  See 'exports' man page.",
				xd->xd_dir);
			free(netargs);
			return (1);
		}
		log(LOG_ERR, "Can't %sexport %s: %s (%d)",
			(expcmd == NXA_DELETE) ? "un" : "",
			xd->xd_dir, strerror(errno), errno);
		free(netargs);
		return (1);
	}

	free(netargs);
	return (0);
}

/*
 * Translate a net address.
 */
int
get_net(char *cp, struct netmsk *net, int maskflg)
{
	struct netent *np;
	uint32_t netaddr;
	struct in_addr inetaddr, inetaddr2;
	char *name;

	if (NULL != (np = getnetbyname(cp)))
		inetaddr = inet_makeaddr(np->n_net, 0);
	else if (isdigit(*cp)) {
		if ((netaddr = inet_network(cp)) == INADDR_NONE)
			return (1);
		inetaddr = inet_makeaddr(netaddr, 0);
		/*
		 * Due to arbritrary subnet masks, you don't know how many
		 * bits to shift the address to make it into a network,
		 * however you do know how to make a network address into
		 * a host with host == 0 and then compare them.
		 * (What a pest)
		 */
		if (!maskflg) {
			setnetent(0);
			while (NULL != (np = getnetent())) {
				inetaddr2 = inet_makeaddr(np->n_net, 0);
				if (inetaddr2.s_addr == inetaddr.s_addr)
					break;
			}
			endnetent();
		}
	} else
		return (1);
	if (maskflg)
		net->nt_mask = inetaddr.s_addr;
	else {
		if (np)
			name = np->n_name;
		else
			name = inet_ntoa(inetaddr);
		net->nt_name = malloc(strlen(name) + 1);
		if (net->nt_name == NULL) {
			log(LOG_ERR, "can't allocate memory for net: %s", cp);
			return (1);
		}
		strlcpy(net->nt_name, name, strlen(name)+1);
		net->nt_net = inetaddr.s_addr;
		DEBUG(3, "got net: %s", net->nt_name);
	}
	return (0);
}

/*
 * Parse security flavors
 */
int
get_sec_flavors(char *flavorlist, struct nfs_sec *sec_flavs)
{
	char *flavorlistcopy;
	char *flavor;
	u_int32_t flav_bits;

#define SYS_BIT   0x00000001
#define KRB5_BIT  0x00000002
#define KRB5I_BIT 0x00000004
#define KRB5P_BIT 0x00000008

	/* try to make a copy of the string so we don't butcher the original */
	flavorlistcopy = strdup(flavorlist);
	if (flavorlistcopy)
		flavorlist = flavorlistcopy;

	bzero(sec_flavs, sizeof(struct nfs_sec));
	flav_bits = 0;
	while ( ((flavor = strsep(&flavorlist, ":")) != NULL) && (sec_flavs->count < NX_MAX_SEC_FLAVORS)) {
		if (flavor[0] == '\0')
			continue;
		if (!strcmp("krb5p", flavor)) {
			if (flav_bits & KRB5P_BIT) {
				log(LOG_WARNING, "krb5p appears more than once: %s", flavorlist);
				continue;
			}
			flav_bits |= KRB5P_BIT;
			sec_flavs->flavors[sec_flavs->count++] = RPCAUTH_KRB5P;
		} else if (!strcmp("krb5i", flavor)) {
			if (flav_bits & KRB5I_BIT) {
				log(LOG_WARNING, "krb5i appears more than once: %s", flavorlist);
				continue;
			}
			flav_bits |= KRB5I_BIT;
			sec_flavs->flavors[sec_flavs->count++] = RPCAUTH_KRB5I;
		} else if (!strcmp("krb5", flavor)) {
			if (flav_bits & KRB5_BIT) {
				log(LOG_WARNING, "krb5 appears more than once: %s", flavorlist);
				continue;
			}
			flav_bits |= KRB5_BIT;
			sec_flavs->flavors[sec_flavs->count++] = RPCAUTH_KRB5;
		} else if (!strcmp("sys", flavor)) {
			if (flav_bits & SYS_BIT) {
				log(LOG_WARNING, "Security mechanism 'sys' appears more than once: %s", flavorlist);
				continue;
			}
			flav_bits |= SYS_BIT;
			sec_flavs->flavors[sec_flavs->count++] = RPCAUTH_SYS;
		} else {
			log(LOG_ERR, "Unknown security mechanism '%s'.  See the exports(5) man page.", flavor);
			bzero(sec_flavs, sizeof(struct nfs_sec));
			break;
		}
	}

	if (flavorlistcopy)
		free(flavorlistcopy);

	if (sec_flavs->count)
		return 0;
	else
		return 1;
}

/*
 * Compare two security flavor structs
 */
int
cmp_secflavs(struct nfs_sec *sf1, struct nfs_sec *sf2)
{
	int32_t i;

	if (sf1->count != sf2->count)
		return 1;
	for (i = 0; i < sf1->count; i++)
		if (sf1->flavors[i] != sf2->flavors[i])
			return 1;
	return 0;
}

/*
 * merge new security flavors into a current set
 */
void
merge_secflavs(struct nfs_sec *cur, struct nfs_sec *new)
{
	int32_t i, j;

	for (i = 0; i < new->count; i++) {
		for (j = 0; j < cur->count; j++)
			if (new->flavors[i] == cur->flavors[j])
				break;
		if (j < cur->count)
			continue;
		if (cur->count < NX_MAX_SEC_FLAVORS) {
			cur->flavors[j] = new->flavors[i];
			cur->count++;
		}
	}
}

/*
 * Find the next field in a line.
 * Fields are separated by white space.
 * Space, tab, and quote characters may be escaped.
 * Quoted strings are not broken at white space.
 */
void
nextfield(char **line_start, char **line_end)
{
	char *a, q;
	u_int32_t esc;

	if (line_start == NULL)
		return;
	a = *line_start;

	/* Skip white space */
	while (*a == ' ' || *a == '\t')
		a++;
	*line_start = a;

	/* Stop at end of line */
	if (*a == '\n' || *a == '\0') {
		*line_end = a;
		return;
	}

	/* Check for single or double quote */
	if (*a == '\'' || *a == '"') {
		q = *a;
		a++;
		for (esc = 0; *a != '\0'; a++) {
			if (esc)
				esc = 0;
			else if (*a == '\\')
				esc = 1;
			else if (*a == q || *a == '\n')
				break;
		}
		if (*a == q)
			a++;
		*line_end = a;
		return;
	}

	/* Skip to next non-escaped white space or end of line */
	for (;; a++) {
		if (*a == '\0' || *a == '\n')
			break;
		else if (*a == '\\') {
			a++;
			if (*a == '\n' || *a == '\0')
				break;
		} else if (*a == ' ' || *a == '\t')
			break;
	}

	*line_end = a;
}

/*
 * Get an exports file line. Skip over blank lines and handle line continuations.
 * (char *line is a global)
 */
int
get_export_entry(void)
{
	char *p, *cp, *newline;
	size_t len, totlen;
	int cont_line;

	if (linenum == 0)
		linenum = 1;
	else
		linenum += entrylines;
	entrylines = 1;

	/*
	 * Loop around ignoring blank lines and getting all continuation lines.
	 */
	totlen = 0;
	do {
		if ((p = fgetln(exp_file, &len)) == NULL)
			return (0);
		cp = p + len - 1;
		cont_line = 0;
		while (cp >= p &&
		       (*cp == ' ' || *cp == '\t' || *cp == '\n' ||
			*cp == '\\')) {
			if (*cp == '\\')
				cont_line = 1;
			cp--;
			len--;
		}
		if (linesize < (totlen + len + 1)) {
			newline = realloc(line, (totlen + len + 1));
			if (!newline) {
				log(LOG_ERR, "Exports line too long, can't allocate memory");
				return (0);
			}
			line = newline;
			linesize = (totlen + len + 1);
		}
		memcpy(line + totlen, p, len);
		totlen += len;
		line[totlen] = '\0';
		if (cont_line) {
			entrylines++;
		} else if (totlen == 0) {
			linenum += entrylines;
			entrylines = 1;
		}
	} while (totlen == 0 || cont_line);
	return (1);
}

/*
 * Parse a description of a credential.
 */
int
parsecred(char *namelist, struct xucred *cr)
{
	char *namelistcopy;
	char *name;
	int cnt;
	char *names;
	struct passwd *pw;
	struct group *gr;
	int ngroups, groups[NGROUPS + 1];

	/* try to make a copy of the string so we don't butcher the original */
	namelistcopy = strdup(namelist);
	if (namelistcopy)
		namelist = namelistcopy;

	/*
	 * Set up the unpriviledged user.
	 */
	cr->cr_version = XUCRED_VERSION;
	cr->cr_uid = -2;
	cr->cr_groups[0] = -2;
	cr->cr_ngroups = 1;
	/*
	 * Get the user's password table entry.
	 */
	names = strsep(&namelist, " \t\n");
	name = strsep(&names, ":");
	if (isdigit(*name) || *name == '-')
		pw = getpwuid(atoi(name));
	else
		pw = getpwnam(name);
	/*
	 * Credentials specified as those of a user.
	 */
	if (names == NULL) {
		if (pw == NULL) {
			log(LOG_ERR, "Unknown user: %s", name);
			if (namelistcopy)
				free(namelistcopy);
			return (ENOENT);
		}
		cr->cr_uid = pw->pw_uid;
		ngroups = NGROUPS + 1;
		if (getgrouplist(pw->pw_name, pw->pw_gid, groups, &ngroups))
			log(LOG_NOTICE, "Too many groups for %s", pw->pw_name);
		/* Convert from int's to gid_t's */
		cr->cr_ngroups = ngroups;
		for (cnt = 0; cnt < ngroups; cnt++)
			cr->cr_groups[cnt] = groups[cnt];
		if (namelistcopy)
			free(namelistcopy);
		goto out;
	}
	/*
	 * Explicit credential specified as a colon separated list:
	 *	uid:gid:gid:...
	 */
	if (pw != NULL)
		cr->cr_uid = pw->pw_uid;
	else if (isdigit(*name) || *name == '-')
		cr->cr_uid = atoi(name);
	else {
		log(LOG_ERR, "Unknown user: %s", name);
		if (namelistcopy)
			free(namelistcopy);
		return (ENOENT);
	}
	cr->cr_ngroups = 0;
	while (names != NULL && *names != '\0' && cr->cr_ngroups < NGROUPS) {
		name = strsep(&names, ":");
		if (isdigit(*name) || *name == '-') {
			cr->cr_groups[cr->cr_ngroups++] = atoi(name);
		} else {
			if ((gr = getgrnam(name)) == NULL) {
				log(LOG_ERR, "Unknown group: %s", name);
				continue;
			}
			cr->cr_groups[cr->cr_ngroups++] = gr->gr_gid;
		}
	}
	if (names != NULL && *names != '\0' && cr->cr_ngroups == NGROUPS)
		log(LOG_ERR, "Too many groups in %s", namelist);
	if (namelistcopy)
		free(namelistcopy);
out:
	if (config.verbose >= 5) {
		char buf[256];
		snprintf_cred(buf, sizeof(buf), cr);
		DEBUG(3, "got cred: %s", buf);
	}
	if (cr->cr_ngroups < 1) {
		log(LOG_ERR, "no groups found: %s", namelist);
		return (EINVAL);
	}
	return (0);
}

#define	STRSIZ	(RPCMNT_NAMELEN+RPCMNT_PATHLEN+50)
/*
 * Routines that maintain the remote mounttab
 */
void
get_mountlist(void)
{
	struct mountlist *mlp, *lastmlp;
	char *host, *dir, *cp;
	char str[STRSIZ];
	FILE *mlfile;
	int hlen, dlen;

	if ((mlfile = fopen(_PATH_RMOUNTLIST, "r")) == NULL) {
		if (errno != ENOENT)
			log(LOG_ERR, "Can't open %s: %s (%d)",
			    _PATH_RMOUNTLIST, strerror(errno), errno);
		else
			DEBUG(1, "Can't open %s: %s (%d)",
			    _PATH_RMOUNTLIST, strerror(errno), errno);
		return;
	}
	lastmlp = NULL;
	while (fgets(str, STRSIZ, mlfile) != NULL) {
		cp = str;
		host = strsep(&cp, " \t\n");
		dir = strsep(&cp, " \t\n");
		if ((host == NULL) || (dir == NULL))
			continue;
		hlen = strlen(host);
		if (hlen > RPCMNT_NAMELEN)
			hlen = RPCMNT_NAMELEN;
		dlen = strlen(dir);
		if (dlen > RPCMNT_PATHLEN)
			dlen = RPCMNT_PATHLEN;
		mlp = malloc(sizeof(*mlp));
		if (mlp) {
			mlp->ml_host = malloc(hlen+1);
			mlp->ml_dir = malloc(dlen+1);
		}
		if (!mlp || !mlp->ml_host || !mlp->ml_dir) {
			log(LOG_ERR, "can't allocate memory while reading in mount list: %s %s",
				host, dir);
			if (mlp) {
				if (mlp->ml_host)
					free(mlp->ml_host);
				if (mlp->ml_dir)
					free(mlp->ml_dir);
				free(mlp);
			}
			break;
		}
		strncpy(mlp->ml_host, host, hlen);
		mlp->ml_host[hlen] = '\0';
		strncpy(mlp->ml_dir, dir, dlen);
		mlp->ml_dir[dlen] = '\0';
		mlp->ml_next = NULL;
		if (lastmlp)
			lastmlp->ml_next = mlp;
		else
			mlhead = mlp;
		lastmlp = mlp;
	}
	fclose(mlfile);
}

void
del_mlist(char *host, char *dir)
{
	struct mountlist *mlp, **mlpp;
	struct mountlist *mlp2;
	FILE *mlfile;
	int fnd = 0;

	mlpp = &mlhead;
	mlp = mlhead;
	while (mlp) {
		if (!strcmp(mlp->ml_host, host) &&
		    (!dir || !strcmp(mlp->ml_dir, dir))) {
			fnd = 1;
			mlp2 = mlp;
			*mlpp = mlp = mlp->ml_next;
			free(mlp2->ml_host);
			free(mlp2->ml_dir);
			free(mlp2);
		} else {
			mlpp = &mlp->ml_next;
			mlp = mlp->ml_next;
		}
	}
	if (fnd) {
		if ((mlfile = fopen(_PATH_RMOUNTLIST, "w")) == NULL) {
			log(LOG_ERR, "Can't write %s: %s (%d)",
			    _PATH_RMOUNTLIST, strerror(errno), errno);
			return;
		}
		mlp = mlhead;
		while (mlp) {
			fprintf(mlfile, "%s %s\n", mlp->ml_host, mlp->ml_dir);
			mlp = mlp->ml_next;
		}
		fclose(mlfile);
	}
}

void
add_mlist(char *host, char *dir)
{
	struct mountlist *mlp, **mlpp;
	FILE *mlfile;
	int hlen, dlen;

	mlpp = &mlhead;
	mlp = mlhead;
	while (mlp) {
		if (!strcmp(mlp->ml_host, host) && !strcmp(mlp->ml_dir, dir))
			return;
		mlpp = &mlp->ml_next;
		mlp = mlp->ml_next;
	}

	hlen = strlen(host);
	if (hlen > RPCMNT_NAMELEN)
		hlen = RPCMNT_NAMELEN;
	dlen = strlen(dir);
	if (dlen > RPCMNT_PATHLEN)
		dlen = RPCMNT_PATHLEN;

	mlp = malloc(sizeof(*mlp));
	if (mlp) {
		mlp->ml_host = malloc(hlen+1);
		mlp->ml_dir = malloc(dlen+1);
	}
	if (!mlp || !mlp->ml_host || !mlp->ml_dir) {
		if (mlp) {
			if (mlp->ml_host)
				free(mlp->ml_host);
			if (mlp->ml_dir)
				free(mlp->ml_dir);
			free(mlp);
		}
		log(LOG_ERR, "can't allocate memory to add to mount list: %s %s", host, dir);
		return;
	}
	strncpy(mlp->ml_host, host, hlen);
	mlp->ml_host[hlen] = '\0';
	strncpy(mlp->ml_dir, dir, dlen);
	mlp->ml_dir[dlen] = '\0';
	mlp->ml_next = NULL;
	*mlpp = mlp;
	if ((mlfile = fopen(_PATH_RMOUNTLIST, "a")) == NULL) {
		log(LOG_ERR, "Can't append %s: %s (%d)",
		    _PATH_RMOUNTLIST, strerror(errno), errno);
		return;
	}
	fprintf(mlfile, "%s %s\n", mlp->ml_host, mlp->ml_dir);
	fclose(mlfile);
}

/*
 * Check options for consistency.
 */
int
check_options(int opt_flags)
{
	if ((opt_flags & (OP_MAPROOT|OP_MAPALL)) == (OP_MAPROOT|OP_MAPALL)) {
		log(LOG_ERR, "-mapall and -maproot mutually exclusive");
		return (1);
	}
	if ((opt_flags & OP_MASK) && (opt_flags & OP_NET) == 0) {
		log(LOG_ERR, "-mask requires -net");
		return (1);
	}
	return (0);
}

/*
 * Check an absolute directory path for any symbolic links. Return true
 * if no symbolic links are found.
 */
int
check_dirpath(char *dir)
{
	char *cp;
	int ret = 1;
	struct stat sb;

	for (cp = dir + 1; *cp && ret; cp++)
		if (*cp == '/') {
			*cp = '\0';
			if ((lstat(dir, &sb) < 0) || !S_ISDIR(sb.st_mode))
				ret = 0;
			*cp = '/';
		}
	if (ret && ((lstat(dir, &sb) < 0) || !S_ISDIR(sb.st_mode)))
		ret = 0;
	return (ret);
}

void
snprintf_cred(char *buf, int buflen, struct xucred *cr)
{
	char crbuf2[32];
	int i;

	buf[0] = '\0';
	if (!cr)
		return;
	snprintf(crbuf2, sizeof(crbuf2), "%d", cr->cr_uid);
	strlcat(buf, crbuf2, buflen);
	for (i=0; i < cr->cr_ngroups; i++) {
		snprintf(crbuf2, sizeof(crbuf2), ":%d", cr->cr_groups[i]);
		strlcat(buf, crbuf2, buflen);
	}
}

void
snprintf_flags(char *buf, int buflen, int flags, struct xucred *cr)
{
	char crbuf[256];

	if (flags & (OP_MAPALL|OP_MAPROOT))
		snprintf_cred(crbuf, sizeof(crbuf), cr);
	else
		crbuf[0] = '\0';

	snprintf(buf, buflen, "FLAGS: 0x%08x %s%s%s%s%s%s%s%s%s%s%s %s",
		flags,
		(flags & OP_DEL) ? " DEL" : "",
		(flags & OP_ADD) ? " ADD" : "",
		(flags & OP_MISSING) ? " MISSING" : "",
		(flags & OP_OFFLINE) ? " OFFLINE" : "",
		(flags & OP_DEFEXP) ? " DEFEXP" : "",
		(flags & OP_READONLY) ? " READONLY" : "",
		(flags & OP_ALLDIRS) ? " ALLDIRS" : "",
		(flags & OP_NET) ? " NET" : "",
		(flags & OP_MASK) ? " MASK" : "",
		(flags & OP_MAPALL) ? " MAPALL" : "",
		(flags & OP_MAPROOT) ? " MAPROOT" : "",
		crbuf);
}

void
dump_expdir(struct expfs *xf, struct expdir *xd, int mdir)
{
	struct expdir *mxd;
	struct host *ht;
	struct grouplist *gr;
	char buf[2048];
	uint32_t **a;

	snprintf_flags(buf, sizeof(buf), xd->xd_iflags, NULL);
	DEBUG(1, "   %s  %s", xd->xd_dir, buf);
	if (!mdir) {
		DEBUG(1, "   %s/%s", xf->xf_fsdir, xd->xd_xid->xid_path);
		DEBUG(1, "   XID: 0x%08x", xd->xd_xid->xid_id);
	}
	snprintf_flags(buf, sizeof(buf), xd->xd_flags, mdir ? NULL : &xd->xd_cred);
	DEBUG(1, "   %s", buf);
	DEBUG(1, "   HOSTS:");

	TAILQ_FOREACH(ht, &xd->xd_hosts, ht_next) {
		snprintf_flags(buf, sizeof(buf), ht->ht_flags, mdir ? NULL : &ht->ht_cred);
		DEBUG(1, "      * %s", buf);
		gr = ht->ht_grp;
		if (gr) {
			switch(gr->gr_type) {
			case GT_NET:
				snprintf(buf, sizeof(buf), "%s %s/", grp_name(gr),
					inet_ntoa(*(struct in_addr*)&gr->gr_u.gt_net.nt_net));
				strlcat(buf, inet_ntoa(*(struct in_addr *)&gr->gr_u.gt_net.nt_mask), sizeof(buf));
				break;
			case GT_HOST:
				a = (uint32_t **)gr->gr_u.gt_hostent->h_addr_list;
				snprintf(buf, sizeof(buf), "%s %s", grp_name(gr), inet_ntoa(*(struct in_addr *)*a));
				a++;
				while (*a) {
					strlcat(buf, " ", sizeof(buf));
					if (strlcat(buf, inet_ntoa(*(struct in_addr *)*a), sizeof(buf)) > 2000) {
						strlcat(buf, " ...", sizeof(buf));
						break;
					}
					a++;
				}
				break;
			default:
				snprintf(buf, sizeof(buf), "%s", grp_name(gr));
				break;
			}
			DEBUG(1, "        %s", buf);
		} /* ht_grp */
	} /* for xd_hosts list */

	if (mdir)
		return;

	DEBUG(1, "   MOUNTDIRS:");
	TAILQ_FOREACH(mxd, &xd->xd_mountdirs, xd_next) {
		dump_expdir(xf, mxd, 1);
	}
}

void
dump_exports(void)
{
	struct expfs *xf;
	struct expdir *xd;
	char buf[64];

	if (!config.verbose)
		return;

	TAILQ_FOREACH(xf, &xfshead, xf_next) {
		DEBUG(1, "** %s %s (0x%08x)", xf->xf_fsdir,
			uuidstring(xf->xf_uuid, buf), UUID2FSID(xf->xf_uuid));
		TAILQ_FOREACH(xd, &xf->xf_dirl, xd_next) {
			dump_expdir(xf, xd, 0);
		}
	}
}

/*
 * code to monitor list of mounts
 */
static struct statfs *sfs[2];
static int size[2], cnt[2], cur, lastfscnt;
#define PREV	((cur + 1) & 1)

static int
sfscmp(const void *arg1, const void *arg2)
{
	const struct statfs *sfs1 = arg1;
	const struct statfs *sfs2 = arg2;
	return strcmp(sfs1->f_mntonname, sfs2->f_mntonname);
}

static void
get_mounts(void)
{
	cur = (cur + 1) % 2;
	while (size[cur] < (lastfscnt = getfsstat(sfs[cur], size[cur] * sizeof(struct statfs), MNT_NOWAIT))) {
		free(sfs[cur]);
		size[cur] = lastfscnt + 32;
		sfs[cur] = malloc(size[cur] * sizeof(struct statfs));
		if (!sfs[cur])
			err(1, "no memory");
	}
	cnt[cur] = lastfscnt;
	qsort(sfs[cur], cnt[cur], sizeof(struct statfs), sfscmp);
}

static int
check_xpaths(char *path)
{
	struct dirlist *dirl = xpaths;

	while (dirl) {
		if (subdir_check(path, dirl->dl_dir) >= 0) {
			DEBUG(1, "check_for_mount_changes: %s %s\n", path, dirl->dl_dir);
			return (1);
		}
		dirl = dirl->dl_next;
	}

	return (0);
}

int
check_for_mount_changes(void)
{
	int i, j, cmp, gotmount = 0;

#define RETURN_IF_DONE	do { if (gotmount && (config.verbose < 3)) return (gotmount); } while (0)

	get_mounts();

	if (!xpaths_complete) {
		DEBUG(1, "check_for_mount_changes: xpaths not complete\n");
		return (1);
	}

	for (i=j=0; (i < cnt[PREV]) && (j < cnt[cur]); ) {
		cmp = sfscmp(&sfs[PREV][i], &sfs[cur][j]);
		if (!cmp) {
			i++;
			j++;
			continue;
		}
		if (cmp < 0) {
			DEBUG(1, "- %s\n", sfs[PREV][i].f_mntonname);
			gotmount |= check_xpaths(sfs[PREV][i].f_mntonname);
			RETURN_IF_DONE;
			i++;
		}
		if (cmp > 0) {
			DEBUG(1, "+ %s\n", sfs[cur][j].f_mntonname);
			gotmount |= check_xpaths(sfs[cur][j].f_mntonname);
			RETURN_IF_DONE;
			j++;
		}
	}
	while (i < cnt[PREV]) {
		DEBUG(1, "- %s\n", sfs[PREV][i].f_mntonname);
		gotmount |= check_xpaths(sfs[PREV][i].f_mntonname);
		RETURN_IF_DONE;
		i++;
	}
	while (j < cnt[cur]) {
		DEBUG(1, "+ %s\n", sfs[cur][j].f_mntonname);
		gotmount |= check_xpaths(sfs[cur][j].f_mntonname);
		RETURN_IF_DONE;
		j++;
	}

	return (gotmount);
}


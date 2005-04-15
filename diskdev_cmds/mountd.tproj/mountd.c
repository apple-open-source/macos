/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 *
 *	@(#)pathnames.h	8.1 (Berkeley) 6/5/93
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

#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>
#ifdef ISO
#include <netiso/iso.h>
#endif
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <ufs/ufs/ufsmount.h>
#include <isofs/cd9660/cd9660_mount.h>	/* XXX need isofs in include */

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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
#include "pathnames.h"

#include <stdarg.h>

#define EXPORT_FROM_NETINFO 0	/* export list from NetInfo */
#define EXPORT_FROM_FILE 1	/* export list from file only */
#define EXPORT_FROM_FILEFIRST 2	/* export list from file first, then NetInfo */
static int source = EXPORT_FROM_FILEFIRST;

#include <netinfo/ni.h>

/*
 * Structures for keeping the mount list and export list
 */
struct mountlist {
	struct mountlist *ml_next;
	char	ml_host[RPCMNT_NAMELEN+1];
	char	ml_dirp[RPCMNT_PATHLEN+1];
};

struct dirlist {
	struct dirlist	*dp_left;
	struct dirlist	*dp_right;
	int		dp_flag;
	struct hostlist	*dp_hosts;	/* List of hosts this dir exported to */
	char		dp_dirp[1];	/* Actually malloc'd to size of dir */
};
/* dp_flag bits */
#define	DP_DEFSET	0x1
#define DP_HOSTSET	0x2
#define DP_KERB		0x4

struct reglist {
	struct reglist	*rl_next;
	char	*rl_dirp;
};
struct reglist *reghead;

struct exportlist {
	struct exportlist *ex_next;
	struct dirlist	*ex_dirl;
	struct dirlist	*ex_defdir;
	int		ex_flag;
	fsid_t		ex_fs;
	char		*ex_fsdir;
};
/* ex_flag bits */
#define	EX_LINKED	0x1

struct netmsk {
	u_long	nt_net;
	u_long	nt_mask;
	char *nt_name;
};

union grouptypes {
	struct hostent *gt_hostent;
	struct netmsk	gt_net;
#ifdef ISO
	struct sockaddr_iso *gt_isoaddr;
#endif
};

struct grouplist {
	int gr_type;
	union grouptypes gr_ptr;
	struct grouplist *gr_next;
};
/* Group types */
#define	GT_NULL		0x0
#define	GT_HOST		0x1
#define	GT_NET		0x2
#define	GT_ISO		0x4

struct hostlist {
	int		 ht_flag;	/* Uses DP_xx bits */
	struct grouplist *ht_grp;
	struct hostlist	 *ht_next;
};

#define DEFAULTHOSTNAME "localhost"

struct fhreturn {
	int	fhr_flag;
	int	fhr_vers;
	nfsfh_t	fhr_fh;
};

/* Global defs */
char	*add_expdir(struct dirlist **, char *, int);
void	add_dlist __P((struct dirlist **, struct dirlist *,
				struct grouplist *, int));
void	add_mlist(char *, char *);
int	check_dirpath(char *);
int	check_options(struct dirlist *);
int	chk_host(struct dirlist *, u_long, int *, int *);
void	del_mlist(char *, char *);
struct dirlist *dirp_search(struct dirlist *, char *);
int	do_mount __P((struct exportlist *, struct grouplist *, int,
		struct ucred *, char *, int, struct statfs *));
int	do_opt __P((char **, char **, struct exportlist *, struct grouplist *,
				int *, int *, struct ucred *));
struct	exportlist *ex_search(fsid_t *);
struct	exportlist *get_exp(void);
void	free_dir(struct dirlist *);
void	free_exp(struct exportlist *);
void	free_grp(struct grouplist *);
void	free_host(struct hostlist *);
void	get_hostnames(char **hostnamearray);
int	register_export(const char *path, const char **hostnamearray, int addurl);
char *	get_ifinfo(int family, int index, int *buflen);
void	get_exportlist(void);
int	get_host(char *, struct grouplist *);
int	get_num(char *);
struct hostlist *get_ht(void);
int get_line(int);
int ni_get_line(void);
int file_get_line(void);
void	get_mountlist(void);
int	get_net(char *, struct netmsk *, int);
void	getexp_err(struct exportlist *, struct grouplist *);
struct grouplist *get_grp(void);
void	hang_dirp __P((struct dirlist *, struct grouplist *,
				struct exportlist *, int));
void	mntsrv(struct svc_req *, SVCXPRT *);
void	my_svc_run(void);
void	nextfield(char **, char **);
void	out_of_mem(void);
void	parsecred(char *, struct ucred *);
int	put_exlist(struct dirlist *, XDR *, struct dirlist *, int *);
int	scan_tree(struct dirlist *, u_long);
void	send_umntall(void);
void	sigmux(int);
int	xdr_dir(XDR *, char *);
int	xdr_explist(XDR *, caddr_t);
int	xdr_fhs(XDR *, caddr_t);
int	xdr_mlist(XDR *, caddr_t);

/* C library */
int	getnetgrent(char **host, char **user, char **domain);
void	endnetgrent(void);
void	setnetgrent(const char *netgroup);

#ifdef ISO
struct iso_addr *iso_addr(void);
#endif

void ni_exports_open(void);
void ni_exports_close(void);

struct exportlist *exphead;
struct mountlist *mlhead;
struct grouplist *grphead;
char exname[MAXPATHLEN];
struct ucred def_anon = {
	1,
	(uid_t) -2,
	1,
	{ (gid_t) -2 }
};
int resvport_only = 1;
int dir_only = 1;
int opt_flags;
/* Bits for above */
#define	OP_MAPROOT	0x01
#define	OP_MAPALL	0x02
#define	OP_KERB		0x04
#define	OP_MASK		0x08
#define	OP_NET		0x10
#define	OP_ISO		0x20
#define	OP_ALLDIRS	0x40

#define MAXHOSTNAMES 10
char *our_hostnames[MAXHOSTNAMES];	/* for system we are running on */

static char urlprefix[] = "nfs://";
static char slpprefix[] = "service:x-file-service:nfs://";
static int maxprefix = (sizeof(slpprefix) > sizeof(urlprefix) ?
			sizeof(slpprefix) : sizeof(urlprefix));

static char URLRegistrar[] = _PATH_SLP_REG;

#define ADD_URL	1
#define DELETE_URL	0

int debug = 0;
void	SYSLOG(int, const char *, ...);
#define log (debug ? SYSLOG : syslog)

/*
 * Mountd server for NFS mount protocol as described in:
 * NFS: Network File System Protocol Specification, RFC1094, Appendix A
 * The optional arguments are the exports file name
 * default: _PATH_EXPORTS
 * and "-n" to allow nonroot mount.
 */
int
main(int argc, char *argv[])
{
	SVCXPRT *udptransp, *tcptransp;
	int c, i;

	while ((c = getopt(argc, argv, "dfnr")) != EOF) {
		switch (c) {
			case 'd':
				debug = 1;
				fprintf(stderr, "Debug Enabled.\n");
				break;
			case 'f':
				source = EXPORT_FROM_FILE;
				break;
			case 'n':
				resvport_only = 0;
				break;
			case 'r':
				dir_only = 0;
				break;
			default:
				fprintf(stderr, "Usage: mountd [-d] [-r] [-n] [-f] [export_file]\n");
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;
	grphead = (struct grouplist *)NULL;
	exphead = (struct exportlist *)NULL;
	mlhead = (struct mountlist *)NULL;

	for (i = 0; i < MAXHOSTNAMES; ++i)
		our_hostnames[i] = NULL;
	if (argc == 1) {
		source = EXPORT_FROM_FILE;
		strncpy(exname, *argv, MAXPATHLEN-1);
		exname[MAXPATHLEN-1] = '\0';
	} else
		strcpy(exname, _PATH_EXPORTS);
	openlog("mountd", LOG_PID, LOG_DAEMON);
	if (debug)
		fprintf(stderr,"Getting export list.\n");
	get_exportlist();
	if (debug)
		fprintf(stderr,"Getting mount list.\n");
	get_mountlist();
	if (debug)
		fprintf(stderr,"Here we go.\n");
	if (debug == 0) {
		daemon(0, 0);
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
	}
	signal(SIGHUP, sigmux);
	signal(SIGTERM, sigmux);
	{ FILE *pidfile = fopen(_PATH_MOUNTDPID, "w");
	  if (pidfile != NULL) {
		fprintf(pidfile, "%d\n", getpid());
		fclose(pidfile);
	  }
	}
	if ((udptransp = svcudp_create(RPC_ANYSOCK)) == NULL ||
	    (tcptransp = svctcp_create(RPC_ANYSOCK, 0, 0)) == NULL) {
		log(LOG_ERR, "Can't create socket");
		exit(1);
	}
	pmap_unset(RPCPROG_MNT, 1);
	pmap_unset(RPCPROG_MNT, 3);
	if (!svc_register(udptransp, RPCPROG_MNT, 1, mntsrv, IPPROTO_UDP) ||
	    !svc_register(udptransp, RPCPROG_MNT, 3, mntsrv, IPPROTO_UDP) ||
	    !svc_register(tcptransp, RPCPROG_MNT, 1, mntsrv, IPPROTO_TCP) ||
	    !svc_register(tcptransp, RPCPROG_MNT, 3, mntsrv, IPPROTO_TCP)) {
		log(LOG_ERR, "Can't register mount");
		exit(1);
	}
	my_svc_run();
	log(LOG_ERR, "Mountd died");
	exit(1);
}

volatile static int gothup;
volatile static int gotterm;

void
sigmux(int sig)
{

	switch (sig) {
	case SIGHUP:
		gothup = 1;
		break;
	case SIGTERM:
		gotterm = 1;
		break;
	}
}

void
my_svc_run(void)
{
	fd_set	readfdset;
	static int tsize = 0;
	int x;

	if (tsize == 0)
		tsize = getdtablesize();

	for ( ; ; ) {
		bcopy(&svc_fdset, &readfdset, sizeof(svc_fdset));
		x = select(tsize, &readfdset, NULL, NULL, NULL);
		if (x > 0) {
			svc_getreqset(&readfdset);
		} else if (x == -1) {
			switch (errno) {
			case EINTR:
				if (gotterm) {
					gotterm = 0;
					send_umntall();
				}
				if (gothup) {
					gothup = 0;
					get_exportlist();
				}
			}
		}
	}
}

/*
 * The mount rpc service
 */
void
mntsrv(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	struct exportlist *ep;
	struct dirlist *dp;
	struct fhreturn fhr;
	struct stat stb;
	struct statfs fsb;
	struct hostent *hp;
	u_long saddr;
	u_short sport;
	char rpcpath[RPCMNT_PATHLEN + 1], dirpath[MAXPATHLEN];
	int bad = ENOENT, defset, hostset;
	sigset_t sighup_mask;

	sigemptyset(&sighup_mask);
	sigaddset(&sighup_mask, SIGHUP);
	saddr = transp->xp_raddr.sin_addr.s_addr;
	sport = ntohs(transp->xp_raddr.sin_port);
	hp = (struct hostent *)NULL;
	switch (rqstp->rq_proc) {
	case NULLPROC:
		if (!svc_sendreply(transp, xdr_void, (caddr_t)NULL))
			log(LOG_ERR, "Can't send reply");
		return;
	case RPCMNT_MOUNT:
		if (sport >= IPPORT_RESERVED && resvport_only) {
			svcerr_weakauth(transp);
			return;
		}
		if (!svc_getargs(transp, xdr_dir, rpcpath)) {
			svcerr_decode(transp);
			return;
		}

		/*
		 * Get the real pathname and make sure it is a directory
		 * or a regular file if the -r option was specified
		 * and it exists.
		 */
		if (realpath(rpcpath, dirpath) == 0 ||
		    stat(dirpath, &stb) < 0 ||
		    (!S_ISDIR(stb.st_mode) &&
		     (dir_only || !S_ISREG(stb.st_mode))) ||
		    statfs(dirpath, &fsb) < 0) {
			chdir("/");	/* Just in case realpath doesn't */
			if (debug)
				fprintf(stderr, "stat failed on %s\n", dirpath);
			if (!svc_sendreply(transp, xdr_long, (caddr_t)&bad))
				log(LOG_ERR, "Can't send reply");
			return;
		}

		/* Check in the exports list */
		sigprocmask(SIG_BLOCK, &sighup_mask, NULL);
		ep = ex_search(&fsb.f_fsid);
		hostset = defset = 0;
		if (ep && (chk_host(ep->ex_defdir, saddr, &defset, &hostset) ||
		    ((dp = dirp_search(ep->ex_dirl, dirpath)) &&
		     chk_host(dp, saddr, &defset, &hostset)) ||
		     (defset && scan_tree(ep->ex_defdir, saddr) == 0 &&
		      scan_tree(ep->ex_dirl, saddr) == 0))) {
			if (hostset & DP_HOSTSET)
				fhr.fhr_flag = hostset;
			else
				fhr.fhr_flag = defset;
			fhr.fhr_vers = rqstp->rq_vers;
			/* Get the file handle */
			memset(&fhr.fhr_fh, 0, sizeof(nfsfh_t));
			if (getfh(dirpath, (fhandle_t *)&fhr.fhr_fh) < 0) {
				bad = errno;
				log(LOG_ERR, "Can't get fh for %s", dirpath);
				if (!svc_sendreply(transp, xdr_long,
				    (caddr_t)&bad))
					log(LOG_ERR, "Can't send reply");
				sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
				return;
			}
			if (!svc_sendreply(transp, xdr_fhs, (caddr_t)&fhr))
				log(LOG_ERR, "Can't send reply");
			if (hp == NULL)
				hp = gethostbyaddr((caddr_t)&saddr,
				    sizeof(saddr), AF_INET);
			if (hp)
				add_mlist(hp->h_name, dirpath);
			else
				add_mlist(inet_ntoa(transp->xp_raddr.sin_addr),
					dirpath);
			if (debug)
				fprintf(stderr,"Mount successfull.\n");
		} else {
			bad = EACCES;
			if (!svc_sendreply(transp, xdr_long, (caddr_t)&bad))
				log(LOG_ERR, "Can't send reply");
		}
		sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
		return;
	case RPCMNT_DUMP:
		if (!svc_sendreply(transp, xdr_mlist, (caddr_t)NULL))
			log(LOG_ERR, "Can't send reply");
		return;
	case RPCMNT_UMOUNT:
		if (sport >= IPPORT_RESERVED && resvport_only) {
			svcerr_weakauth(transp);
			return;
		}
		if (!svc_getargs(transp, xdr_dir, dirpath)) {
			svcerr_decode(transp);
			return;
		}
		if (!svc_sendreply(transp, xdr_void, (caddr_t)NULL))
			log(LOG_ERR, "Can't send reply");
		hp = gethostbyaddr((caddr_t)&saddr, sizeof(saddr), AF_INET);
		if (hp)
			del_mlist(hp->h_name, dirpath);
		del_mlist(inet_ntoa(transp->xp_raddr.sin_addr), dirpath);
		return;
	case RPCMNT_UMNTALL:
		if (sport >= IPPORT_RESERVED && resvport_only) {
			svcerr_weakauth(transp);
			return;
		}
		if (!svc_sendreply(transp, xdr_void, (caddr_t)NULL))
			log(LOG_ERR, "Can't send reply");
		hp = gethostbyaddr((caddr_t)&saddr, sizeof(saddr), AF_INET);
		if (hp)
			del_mlist(hp->h_name, (char *)NULL);
		del_mlist(inet_ntoa(transp->xp_raddr.sin_addr), (char *)NULL);
		return;
	case RPCMNT_EXPORT:
		if (!svc_sendreply(transp, xdr_explist, (caddr_t)NULL))
			log(LOG_ERR, "Can't send reply");
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
xdr_dir(xdrsp, dirp)
	XDR *xdrsp;
	char *dirp;
{
	return (xdr_string(xdrsp, &dirp, RPCMNT_PATHLEN));
}

/*
 * Xdr routine to generate file handle reply
 */
int
xdr_fhs(xdrsp, cp)
	XDR *xdrsp;
	caddr_t cp;
{
	register struct fhreturn *fhrp = (struct fhreturn *)cp;
	long ok = 0, len, auth;

	if (!xdr_long(xdrsp, &ok))
		return (0);
	switch (fhrp->fhr_vers) {
	case 1:
		return (xdr_opaque(xdrsp, (caddr_t)&fhrp->fhr_fh, NFSX_V2FH));
	case 3:
		len = NFSX_V3FH;
		if (!xdr_long(xdrsp, &len))
			return (0);
		if (!xdr_opaque(xdrsp, (caddr_t)&fhrp->fhr_fh, len))
			return (0);
		if (fhrp->fhr_flag & DP_KERB)
			auth = RPCAUTH_KERB4;
		else
			auth = RPCAUTH_UNIX;
		len = 1;
		if (!xdr_long(xdrsp, &len))
			return (0);
		return (xdr_long(xdrsp, &auth));
	};
	return (0);
}

int
xdr_mlist(xdrsp, cp)
	XDR *xdrsp;
	caddr_t cp;
{
	struct mountlist *mlp;
	int true = 1;
	int false = 0;
	char *strp;

	mlp = mlhead;
	while (mlp) {
		if (!xdr_bool(xdrsp, &true))
			return (0);
		strp = &mlp->ml_host[0];
		if (!xdr_string(xdrsp, &strp, RPCMNT_NAMELEN))
			return (0);
		strp = &mlp->ml_dirp[0];
		if (!xdr_string(xdrsp, &strp, RPCMNT_PATHLEN))
			return (0);
		mlp = mlp->ml_next;
	}
	if (!xdr_bool(xdrsp, &false))
		return (0);
	return (1);
}

/*
 * Xdr conversion for export list
 */
int
xdr_explist(xdrsp, cp)
	XDR *xdrsp;
	caddr_t cp;
{
	struct exportlist *ep;
	int false = 0;
	int putdef;
	sigset_t sighup_mask;

	sigemptyset(&sighup_mask);
	sigaddset(&sighup_mask, SIGHUP);
	sigprocmask(SIG_BLOCK, &sighup_mask, NULL);
	ep = exphead;
	while (ep) {
		putdef = 0;
		if (put_exlist(ep->ex_dirl, xdrsp, ep->ex_defdir, &putdef))
			goto errout;
		if (ep->ex_defdir && putdef == 0 &&
			put_exlist(ep->ex_defdir, xdrsp, (struct dirlist *)NULL,
			&putdef))
			goto errout;
		ep = ep->ex_next;
	}
	sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
	if (!xdr_bool(xdrsp, &false))
		return (0);
	return (1);
errout:
	sigprocmask(SIG_UNBLOCK, &sighup_mask, NULL);
	return (0);
}

/*
 * Called from xdr_explist() to traverse the tree and export the
 * directory paths.
 */
int
put_exlist(dp, xdrsp, adp, putdefp)
	struct dirlist *dp;
	XDR *xdrsp;
	struct dirlist *adp;
	int *putdefp;
{
	struct grouplist *grp;
	struct hostlist *hp;
	int true = 1;
	int false = 0;
	int gotalldir = 0;
	char *strp;

	if (dp) {
		if (put_exlist(dp->dp_left, xdrsp, adp, putdefp))
			return (1);
		if (!xdr_bool(xdrsp, &true))
			return (1);
		strp = dp->dp_dirp;
		if (!xdr_string(xdrsp, &strp, RPCMNT_PATHLEN))
			return (1);
		if (adp && !strcmp(dp->dp_dirp, adp->dp_dirp)) {
			gotalldir = 1;
			*putdefp = 1;
		}
		if ((dp->dp_flag & DP_DEFSET) == 0 &&
		    (gotalldir == 0 || (adp->dp_flag & DP_DEFSET) == 0)) {
			hp = dp->dp_hosts;
			while (hp) {
				grp = hp->ht_grp;
				if (grp->gr_type == GT_HOST) {
					if (!xdr_bool(xdrsp, &true))
						return (1);
					strp = grp->gr_ptr.gt_hostent->h_name;
					if (!xdr_string(xdrsp, &strp, 
					    RPCMNT_NAMELEN))
						return (1);
				} else if (grp->gr_type == GT_NET) {
					if (!xdr_bool(xdrsp, &true))
						return (1);
					strp = grp->gr_ptr.gt_net.nt_name;
					if (!xdr_string(xdrsp, &strp, 
					    RPCMNT_NAMELEN))
						return (1);
				}
				hp = hp->ht_next;
				if (gotalldir && hp == (struct hostlist *)NULL) {
					hp = adp->dp_hosts;
					gotalldir = 0;
				}
			}
		}
		if (!xdr_bool(xdrsp, &false))
			return (1);
		if (put_exlist(dp->dp_right, xdrsp, adp, putdefp))
			return (1);
	}
	return (0);
}



/*
 * Snitched from unpv12e (R. W. Stevens)
 * Fill in an array of sockaddr structs,
 *  based on a bit array indicating presence.
 */
void
get_rtinfo(int addrs,
	   struct sockaddr *sa,
	   struct sockaddr **rti_info) {
	register int i;

/*
 * Round up 'a' to next multiple of 'size'
 */
#define ROUNDUP(a, size) (((a) & ((size)-1)) ? (1 + ((a) | ((size)-1))) : (a))

/*
 * Step to next socket address structure;
 * if sa_len is 0, assume it is sizeof(u_long).
 */
#define NEXT_SA(ap)	ap = (struct sockaddr *) \
		((caddr_t) ap + (ap->sa_len ? \
				 ROUNDUP(ap->sa_len, sizeof (u_long)) : \
				 sizeof(u_long)))

	for (i = 0; i < RTAX_MAX; i++)
		if (addrs & (1 << i)) {
			rti_info[i] = sa;
			NEXT_SA(sa);
		} else
			rti_info[i] = NULL;
}


/*
 * Use sysctl() to get the info on interface-related
 *  information.  We can look for a specific device, or
 *  a specific protocol family.
 */
char *
get_ifinfo(int family, int index, int *buflen)
{
	int mib[6];
	char *buf;

	mib[0] = CTL_NET;
	mib[1] = AF_ROUTE;	/* PF_ maybe */
	mib[2] = 0;		/* XXX */
	mib[3] = family;	/* Target family */
	mib[4] = NET_RT_IFLIST;
	mib[5] = index;		/* either a specific link or 0 */

	/* Determine how much space is needed */
	if (sysctl(mib, 6, NULL, (size_t *)buflen, NULL, 0) == -1) {
		log(LOG_ERR,
		    "Error requesting network interface info size: errno = %d",
		    errno);
		return (NULL);
	}

	/* ... allocate space, */
	buf = (char *)malloc(*buflen);
	if (buf == NULL) {
		log(LOG_ERR,
		    "Error allocating %d bytes for network interface into",
		    buflen);
		out_of_mem();
	}

	/* ... and then get a buffer full */
	if (sysctl(mib, 6, buf, (size_t *)buflen, NULL, 0) == -1) {
		free(buf);
		log(LOG_ERR,
		    "Error requesting network interface info: errno = %d",
		    errno);
		return (NULL);
	}

	return (buf);
}



/*
 * Add a host name to the global table if it's not already in the table:
 */

void
record_hostname(const char *hostname, char **hostnamearray)
{
	int i, hostnamelen;

	hostnamelen = strlen(hostname);
	if (hostnamelen > MAXHOSTNAMELEN)
		return;
	for (i = 0; i < MAXHOSTNAMES && hostnamearray[i]; i++)
		if (!strcmp(hostnamearray[i], hostname))
			return;
	if (i >= MAXHOSTNAMES)
		return;
	hostnamearray[i] = malloc(hostnamelen + 1);
	if (hostnamearray[i] == NULL)
		out_of_mem();
	strcpy(hostnamearray[i], hostname);
	if (debug)
		fprintf(stderr, "record_hostname: hostnamearray[%d] = '%s'.\n",
			i, hostnamearray[i]);
}



/*
 * Get the list of host names:
 */
void
get_hostnames(char **hostnamearray)
{
	char *ifinfo;
	int ifinfo_len;
	int i;
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam;
	struct sockaddr *sa, *rti_info[RTAX_MAX];
	int if_flags = 0;
	struct hostent *hostinfo;
	struct sockaddr_in *sain;

	/* Release any currently allocated host name strings: */
	for (i = 0; (i < MAXHOSTNAMES) && (hostnamearray[i] != NULL); ++i) {
		free(hostnamearray[i]);
		hostnamearray[i] = NULL;
	}
	
	ifinfo = get_ifinfo(AF_INET, 0, &ifinfo_len);
	if (ifinfo == NULL) {
		log(LOG_ERR, "get_hostnames: no ifinfo");
		return;
	}
	
	/*
	 * Process info from the retrieved interface information
	 * We're looking for 'if' info and associated socketaddrs
	 * that describe assigned protocol addresses
	 * From Stevens' Unix Network Programming, V1, 2nd Ed.
	 * Ch. 17 (esp. 17.4).
	 * We borrowed a bit of code, as indicated.
	 */
	/* 'ifinfo' contains a batch of 'ifm' and 'ifam' structs */
	for (ifm = (struct if_msghdr *)ifinfo; (char *)ifm < ifinfo+ifinfo_len;
	     ifm = (struct if_msghdr *)((char *)ifm + ifm->ifm_msglen)) {
		/*
		 * We're interested in:
		 *  RTM_IFINFO (ifm structs describing an interface)
		 *  RTM_NEWADDR (ifam structs describing 'protocol' addresses
		 *   associated with the preceding ifm)
		 * Note that each IFINFO struct is followed by all of its
		 *  NEWADDR structs.
		 */
		if (ifm->ifm_type == RTM_IFINFO) {
			if_flags = ifm->ifm_flags; /* To check IFF_LOOPBACK */
			continue;
		}
		if (ifm->ifm_type != RTM_NEWADDR || (if_flags & IFF_LOOPBACK))
			continue;
		ifam = (struct ifa_msghdr *)ifm;
		sa = (struct sockaddr *)(ifam+1);
		get_rtinfo(ifam->ifam_addrs, sa, rti_info);
		sa = rti_info[RTAX_IFA];
		if (!sa)
			continue;
		if (sa->sa_family != AF_INET) {
			if (debug)
				fprintf(stderr,
					"unknown socket address family %d.\n",
					sa->sa_family);
			continue;
		}
		sain = (struct sockaddr_in *)sa;
		hostinfo = gethostbyaddr((caddr_t)&sain->sin_addr,
					 sizeof(sain->sin_addr), AF_INET);
		if (hostinfo) {
			record_hostname(hostinfo->h_name, hostnamearray);
		} else {
			record_hostname(inet_ntoa(sain->sin_addr), hostnamearray);
		};
	}
	free(ifinfo);	/* allocated by get_ifinfo */
}

/*
 * Strip escaped spaces and remove qoutes around qutoed strings.
 */
char *
clean_white_space(char *line)
{
	int len, esc;
	char c, *p, *s;

	if (line == NULL)
		return NULL;
	len = strlen(line);
	s = malloc(len + 1);

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

	s[len] = '\0';

	return (s);
}


int
safe_exec(char *pgm, char *arg1, char *arg2)
{
	int pid;
	union wait status;

	pid = fork();
	if (pid == 0) {
		(void) execl(pgm, pgm, arg1, arg2, NULL);
		log(LOG_ERR, "Exec of %s failed, errno=%d", pgm, errno);
		exit(2);
	}
	if (pid == -1) {
		log(LOG_ERR, "Fork for %s failed, errno=%d", pgm, errno);
		exit(2);
	}
	if (wait4(pid, (int *)&status, 0, NULL) != pid) {
		log(LOG_ERR, "BUG executing %s", pgm);
		exit(2);
	}
	if (!WIFEXITED(status)) {
		log(LOG_ERR, "%s aborted by signal %d", pgm, WTERMSIG(status));
		exit(2);
	}
	return (WEXITSTATUS(status));
}


/*
 * slp_reg returns this if you try to delete an unregistered URL
 * Better is to use "slp_reg -l" to enumerate actual registration, but that
 * flag is not yet implemented
 */
#define URLNOTFOUND	253

int
register_export(const char *path, const char **hostnamearray, int addurl)
{
	int i;
	int urlstringlength = maxprefix + MAXHOSTNAMELEN + strlen(path);
	char *urlstring;
	int rv, result = 0;

	/* note any errors we return are to be nonfatal and already logged */

	urlstring = malloc(urlstringlength);
	if (urlstring == NULL)
		out_of_mem();
	
	for (i = 0; (i < MAXHOSTNAMES) && (hostnamearray[i] != NULL); ++i) {
		if (urlstringlength < maxprefix + strlen(hostnamearray[i]) +
				      strlen(path)) {
			log(LOG_ERR, "huge hostname (ignored): %s",
			    hostnamearray[i]);
			result = ENAMETOOLONG;
			continue;
		}

		/* Register traditional URL: */
		strcpy(urlstring, urlprefix);
		strcat(urlstring, hostnamearray[i]);
		strcat(urlstring, path);
		if (debug)
			fprintf(stderr, "%sregistering URL %s\n",
				(addurl ? "" : "un"), urlstring);
		rv = safe_exec(URLRegistrar, (addurl ? "-r" : "-d"), urlstring);
		if (debug || rv && (addurl || rv != URLNOTFOUND))
			log(LOG_ERR, "%s exit status %d", URLRegistrar, rv);
		if (rv && (addurl || rv != URLNOTFOUND))
			result = rv; /* arbitrarily retaining last failure */

		/* Register new-style URL: */
		strcpy(urlstring, slpprefix);
		strcat(urlstring, hostnamearray[i]);
		strcat(urlstring, path);
		if (debug)
			fprintf(stderr, "%sregistering URL %s\n",
				(addurl ? "" : "un"), urlstring);
		rv = safe_exec(URLRegistrar, (addurl ? "-r" : "-d"), urlstring);
		if (debug || rv && (addurl || rv != URLNOTFOUND))
			log(LOG_ERR, "%s exit status %d", URLRegistrar, rv);
		if (rv && (addurl || rv != URLNOTFOUND))
			result = rv; /* arbitrarily retaining last failure */
	}
	free(urlstring);
	return (result);
}


int
exportable(struct statfs *fsp)
{
	/*
	 * XXX We should not hard code exportable file systems types.
	 * (getattrlist() or statfs() or the .util program should return
	 * whether filesystem supports NFS export)
	 */
	return (!strcmp(fsp->f_fstypename, "ufs") ||
		!strcmp(fsp->f_fstypename, "hfs") ||
		!strcmp(fsp->f_fstypename, "acfs") ||
		!strcmp(fsp->f_fstypename, "cd9660"));
}



#define LINESIZ	10240
char line[LINESIZ];
FILE *exp_file;

/*
 * Get the export list
 */
void
get_exportlist(void)
{
	struct exportlist *ep, *ep2;
	struct grouplist *grp, *tgrp;
	struct exportlist **epp;
	struct dirlist *dirhead;
	struct statfs fsb, *fsp, *firstfsp;
	struct hostent *hpe;
	struct ucred anon;
	char *cp, *endcp, *word, *dirp = "", *hst, *usr, *dom, savedc;
	int len, has_host, exflags, got_nondir, dirplen = 0, num, i, netgrp;
	union {
		struct ufs_args ua;
		struct iso_args ia;
	} targs;
	struct reglist *rl;

	/*
	 * First, get rid of the old list
	 */
	if (debug) fprintf(stderr, "get_exportlist: freeing old exports...\n");
	exphead = (struct exportlist *)NULL;

	if (debug) fprintf(stderr, "get_exportlist: freeing old groups...\n");
	grp = grphead;
	while (grp) {
		tgrp = grp;
		grp = grp->gr_next;
		free_grp(tgrp);
	}
	grphead = (struct grouplist *)NULL;

	get_hostnames(our_hostnames);

	num = getmntinfo(&firstfsp, MNT_NOWAIT);
	/*
	 * Unregistration is slow due to running slp_reg.  We
	 * do it *before* removing exports from kernel.  The point is to
	 * minimize the amount of time a legit export vanished from kernel
	 * so active clients don't receive fatal errors.
	 */
	for (i = 0, fsp = firstfsp; i < num; i++, fsp++)
		if (exportable(fsp))
			(void)register_export(fsp->f_mntonname, our_hostnames,
					      DELETE_URL);
	/*
	 * Delete exports that are in the kernel for all local file systems.
	 */
	for (i = 0, fsp = firstfsp; i < num; i++, fsp++)
		if (exportable(fsp)) {
			targs.ua.fspec = NULL;
			targs.ua.export.ex_flags = MNT_DELEXPORT;
			if (mount(fsp->f_fstypename, fsp->f_mntonname,
				  fsp->f_flags | MNT_UPDATE,
				  (caddr_t)&targs) < 0)
				log(LOG_ERR, "Can't delete exports for %s",
				    fsp->f_mntonname);
		}

	if ((exp_file = fopen(exname, "r")) != NULL) {
		source = EXPORT_FROM_FILE;
	} else {
		if (source == EXPORT_FROM_FILE) {
			log(LOG_ERR, "Can't open %s", exname);
			exit(2);
		}
		ni_exports_open();
		source = EXPORT_FROM_NETINFO;
	}
		
	/*
	 * Read in the exports and build the list, calling mount()
	 * as we go along to push the export rules into the kernel.
	 */

	dirhead = (struct dirlist *)NULL;
	reghead = (struct reglist *)NULL;
	while (get_line(source)) {
		if (debug)
			fprintf(stderr,"Got line %s\n",line);
		cp = line;
		nextfield(&cp, &endcp);
		if (*cp == '#')
			goto nextline;
		word = clean_white_space(cp);
		if (word == NULL)
			goto nextline;

		/*
		 * Set defaults.
		 */
		has_host = FALSE;
		anon = def_anon;
		exflags = MNT_EXPORTED;
		got_nondir = 0;
		opt_flags = 0;
		ep = (struct exportlist *)NULL;

		/*
		 * Create new exports list entry
		 */
		len = strlen(word);
		tgrp = grp = get_grp();
		while (len > 0) {
			if (len > RPCMNT_NAMELEN) {
				free(word);
				getexp_err(ep, tgrp);
				goto nextline;
			}
			if (*cp == '-') {
			    if (ep == (struct exportlist *)NULL) {
				free(word);
				getexp_err(ep, tgrp);
				goto nextline;
			    }
			    if (debug)
				fprintf(stderr, "doing opt %s\n", cp);
			    got_nondir = 1;
			    if (do_opt(&cp, &endcp, ep, grp, &has_host,
				&exflags, &anon)) {
				free(word);
				getexp_err(ep, tgrp);
				goto nextline;
			    }
			} else if (*word == '/') {
			    savedc = *endcp;
			    *endcp = '\0';
			    if (check_dirpath(word) &&
				statfs(word, &fsb) >= 0) {
				if (got_nondir) {
					free(word);
					log(LOG_ERR, "Dirs must be first");
					getexp_err(ep, tgrp);
					goto nextline;
				}
				if (ep) {
				    if (ep->ex_fs.val[0] != fsb.f_fsid.val[0] ||
					ep->ex_fs.val[1] != fsb.f_fsid.val[1]) {
					free(word);
					getexp_err(ep, tgrp);
					goto nextline;
				    }
				} else {
				    /*
				     * See if this directory is already
				     * in the list.
				     */
				    ep = ex_search(&fsb.f_fsid);
				    if (ep == (struct exportlist *)NULL) {
					ep = get_exp();
					ep->ex_fs = fsb.f_fsid;
					ep->ex_fsdir = (char *)
					    malloc(strlen(fsb.f_mntonname) + 1);
					if (ep->ex_fsdir)
						strcpy(ep->ex_fsdir,
						       fsb.f_mntonname);
					else
						out_of_mem();
					if (debug)
						fprintf(stderr,
							"New ep fs=0x%x,0x%x\n",
							fsb.f_fsid.val[0],
							fsb.f_fsid.val[1]);
				    } else if (debug)
					fprintf(stderr,
						"Found ep fs=0x%x,0x%x\n",
						fsb.f_fsid.val[0],
						fsb.f_fsid.val[1]);
				}

				/*
				 * Add dirpath to export mount point.
				 */
				dirp = add_expdir(&dirhead, word, len);
				dirplen = len;
			    } else {
				free(word);
				getexp_err(ep, tgrp);
				goto nextline;
			    }
			    *endcp = savedc;
			} else {
			    savedc = *endcp;
			    *endcp = '\0';
			    got_nondir = 1;
			    if (ep == (struct exportlist *)NULL) {
				free(word);
				getexp_err(ep, tgrp);
				goto nextline;
			    }

			    /*
			     * Get the host or netgroup.
			     */
			    setnetgrent(cp);
			    netgrp = getnetgrent(&hst, &usr, &dom);
			    do {
				if (has_host) {
					grp->gr_next = get_grp();
					grp = grp->gr_next;
				}
				if (netgrp) {
				    if (get_host(hst, grp)) {
					free(word);
					log(LOG_ERR, "Bad netgroup %s", cp);
					getexp_err(ep, tgrp);
					endnetgrent();
					goto nextline;
				    }
				} else if (get_host(cp, grp)) {
					free(word);
					getexp_err(ep, tgrp);
					goto nextline;
				}
				has_host = TRUE;
			    } while (netgrp && getnetgrent(&hst, &usr, &dom));
			    endnetgrent();
			    *endcp = savedc;
			}
			free(word);
			cp = endcp;
			nextfield(&cp, &endcp);
			word = clean_white_space(cp);
			if (word == NULL) len = 0;
			else len = strlen(word);
		}
		if (word != NULL)
			free(word);
		if (check_options(dirhead)) {
			getexp_err(ep, tgrp);
			goto nextline;
		}
		if (!has_host) {
			grp->gr_type = GT_HOST;
			if (debug)
				fprintf(stderr,"Adding a default entry\n");
			/* add a default group and make the grp list NULL */
			hpe = (struct hostent *)malloc(sizeof(struct hostent));
			if (hpe == (struct hostent *)NULL)
				out_of_mem();
			hpe->h_name = (char *)malloc(sizeof(DEFAULTHOSTNAME));
			if (hpe->h_name == NULL)
				out_of_mem();
			strcpy(hpe->h_name, DEFAULTHOSTNAME);
			hpe->h_addrtype = AF_INET;
			hpe->h_length = sizeof (u_long);
			hpe->h_addr_list = (char **)NULL;
			grp->gr_ptr.gt_hostent = hpe;

		/*
		 * Don't allow a network export coincide with a list of
		 * host(s) on the same line.
		 */
		} else if ((opt_flags & OP_NET) && tgrp->gr_next) {
			getexp_err(ep, tgrp);
			goto nextline;
		}

		/*
		 * Loop through hosts, pushing the exports into the kernel.
		 * After loop, tgrp points to the start of the list and
		 * grp points to the last entry in the list.
		 */
		grp = tgrp;
		do {
			if (do_mount(ep, grp, exflags, &anon, dirp, dirplen,
				     &fsb)) {
				getexp_err(ep, tgrp);
				goto nextline;
			}
		} while (grp->gr_next && (grp = grp->gr_next));

		/*
		 * Success. Update the data structures.
		 */
		if (has_host) {
			hang_dirp(dirhead, tgrp, ep, opt_flags);
			grp->gr_next = grphead;
			grphead = tgrp;
		} else {
			hang_dirp(dirhead, (struct grouplist *)NULL, ep,
				opt_flags);
			free_grp(grp);
		}
		dirhead = (struct dirlist *)NULL;
		if ((ep->ex_flag & EX_LINKED) == 0) {
			ep2 = exphead;
			epp = &exphead;

			/*
			 * Insert in the list in alphabetical order.
			 */
			while (ep2 && strcmp(ep2->ex_fsdir, ep->ex_fsdir) < 0) {
				epp = &ep2->ex_next;
				ep2 = ep2->ex_next;
			}
			if (ep2)
				ep->ex_next = ep2;
			*epp = ep;
			ep->ex_flag |= EX_LINKED;
		}
nextline:
		if (dirhead) {
			free_dir(dirhead);
			dirhead = (struct dirlist *)NULL;
		}
		
		/*
		 * Add exported directory path to reghead, the list of paths
		 * we will register.  Loop is to avoid adding duplicates.
		 */
		for (rl = reghead; rl; rl = rl->rl_next)
			if (!strcmp(dirp, rl->rl_dirp))
				break;
		if (rl == (struct reglist *)NULL) {
			rl = (struct reglist *)malloc(sizeof (struct reglist));
			if (rl == (struct reglist *)NULL)
				out_of_mem();
			rl->rl_dirp = (char *)malloc(1 + strlen(dirp));
			if (rl->rl_dirp == (char *)NULL)
				out_of_mem();
			strcpy(rl->rl_dirp, dirp);
			rl->rl_next = reghead;
			reghead = rl;
		}

	}
	/*
	 * Registration is slow due to running slp_reg.  We
	 * do it *after* adding exports from kernel.  The point is to
	 * minimize the amount of time a legit export vanished from kernel
	 * so active clients won't receive fatal errors.
	 */
 	while (reghead) {
		rl = reghead;
		reghead = rl->rl_next;
		(void)register_export(rl->rl_dirp, our_hostnames, ADD_URL);
		free(rl);
	}

	if (source == EXPORT_FROM_NETINFO) ni_exports_close();
	else fclose(exp_file);
}

/*
 * Allocate an export list element
 */
struct exportlist *
get_exp(void)
{
	struct exportlist *ep;

	ep = (struct exportlist *)malloc(sizeof (struct exportlist));
	if (ep == (struct exportlist *)NULL)
		out_of_mem();
	memset(ep, 0, sizeof(struct exportlist));
	return (ep);
}

/*
 * Allocate a group list element
 */
struct grouplist *
get_grp(void)
{
	struct grouplist *gp;

	gp = (struct grouplist *)malloc(sizeof (struct grouplist));
	if (gp == (struct grouplist *)NULL)
		out_of_mem();
	memset(gp, 0, sizeof(struct grouplist));
	return (gp);
}

/*
 * Clean up upon an error in get_exportlist().
 */
void
getexp_err(ep, grp)
	struct exportlist *ep;
	struct grouplist *grp;
{
	struct grouplist *tgrp;

	log(LOG_ERR, "Bad exports list line %s", line);
	if (ep && (ep->ex_flag & EX_LINKED) == 0)
		free_exp(ep);
	while (grp) {
		tgrp = grp;
		grp = grp->gr_next;
		free_grp(tgrp);
	}
}

/*
 * Search the export list for a matching fs.
 */
struct exportlist *
ex_search(fsid)
	fsid_t *fsid;
{
	struct exportlist *ep;

	ep = exphead;
	while (ep) {
		if (ep->ex_fs.val[0] == fsid->val[0] &&
		    ep->ex_fs.val[1] == fsid->val[1])
			return (ep);
		ep = ep->ex_next;
	}
	return (ep);
}

/*
 * Add a directory path to the list.
 */
char *
add_expdir(dpp, cp, len)
	struct dirlist **dpp;
	char *cp;
	int len;
{
	struct dirlist *dp;

	dp = (struct dirlist *)malloc(sizeof (struct dirlist) + len);
	if (dp == (struct dirlist *)NULL)
		out_of_mem();
	dp->dp_left = *dpp;
	dp->dp_right = (struct dirlist *)NULL;
	dp->dp_flag = 0;
	dp->dp_hosts = (struct hostlist *)NULL;
	strcpy(dp->dp_dirp, cp);
	*dpp = dp;
	return (dp->dp_dirp);
}

/*
 * Hang the dir list element off the dirpath binary tree as required
 * and update the entry for host.
 */
void
hang_dirp(dp, grp, ep, flags)
	struct dirlist *dp;
	struct grouplist *grp;
	struct exportlist *ep;
	int flags;
{
	struct hostlist *hp;
	struct dirlist *dp2;

	if (flags & OP_ALLDIRS) {
		if (ep->ex_defdir)
			free((caddr_t)dp);
		else
			ep->ex_defdir = dp;
		if (grp == (struct grouplist *)NULL) {
			ep->ex_defdir->dp_flag |= DP_DEFSET;
			if (flags & OP_KERB)
				ep->ex_defdir->dp_flag |= DP_KERB;
		} else while (grp) {
			hp = get_ht();
			if (flags & OP_KERB)
				hp->ht_flag |= DP_KERB;
			hp->ht_grp = grp;
			hp->ht_next = ep->ex_defdir->dp_hosts;
			ep->ex_defdir->dp_hosts = hp;
			grp = grp->gr_next;
		}
	} else {

		/*
		 * Loop throught the directories adding them to the tree.
		 */
		while (dp) {
			dp2 = dp->dp_left;
			add_dlist(&ep->ex_dirl, dp, grp, flags);
			dp = dp2;
		}
	}
}

/*
 * Traverse the binary tree either updating a node that is already there
 * for the new directory or adding the new node.
 */
void
add_dlist(dpp, newdp, grp, flags)
	struct dirlist **dpp;
	struct dirlist *newdp;
	struct grouplist *grp;
	int flags;
{
	struct dirlist *dp;
	struct hostlist *hp;
	int cmp;

	dp = *dpp;
	if (dp) {
		cmp = strcmp(dp->dp_dirp, newdp->dp_dirp);
		if (cmp > 0) {
			add_dlist(&dp->dp_left, newdp, grp, flags);
			return;
		} else if (cmp < 0) {
			add_dlist(&dp->dp_right, newdp, grp, flags);
			return;
		} else
			free((caddr_t)newdp);
	} else {
		dp = newdp;
		dp->dp_left = (struct dirlist *)NULL;
		*dpp = dp;
	}
	if (grp) {

		/*
		 * Hang all of the host(s) off of the directory point.
		 */
		do {
			hp = get_ht();
			if (flags & OP_KERB)
				hp->ht_flag |= DP_KERB;
			hp->ht_grp = grp;
			hp->ht_next = dp->dp_hosts;
			dp->dp_hosts = hp;
			grp = grp->gr_next;
		} while (grp);
	} else {
		dp->dp_flag |= DP_DEFSET;
		if (flags & OP_KERB)
			dp->dp_flag |= DP_KERB;
	}
}

/*
 * Search for a dirpath on the export point.
 */
struct dirlist *
dirp_search(dp, dirpath)
	struct dirlist *dp;
	char *dirpath;
{
	int cmp;

	if (dp) {
		cmp = strcmp(dp->dp_dirp, dirpath);
		if (cmp > 0)
			return (dirp_search(dp->dp_left, dirpath));
		else if (cmp < 0)
			return (dirp_search(dp->dp_right, dirpath));
		else
			return (dp);
	}
	return (dp);
}

/*
 * Scan for a host match in a directory tree.
 */
int
chk_host(dp, saddr, defsetp, hostsetp)
	struct dirlist *dp;
	u_long saddr;
	int *defsetp;
	int *hostsetp;
{
	struct hostlist *hp;
	struct grouplist *grp;
	u_long **addrp;

	if (!dp)
		return (0);
	if (dp->dp_flag & DP_DEFSET)
		*defsetp = dp->dp_flag;
	for (hp = dp->dp_hosts; hp; hp = hp->ht_next) {
		grp = hp->ht_grp;
		switch (grp->gr_type) {
		case GT_HOST:
			addrp = (u_long **) grp->gr_ptr.gt_hostent->h_addr_list;
			while (*addrp) {
				if (**addrp == saddr) {
					*hostsetp = (hp->ht_flag | DP_HOSTSET);
					return (1);
				}
				addrp++;
			}
			break;
		case GT_NET:
			if ((saddr & grp->gr_ptr.gt_net.nt_mask) ==
			    grp->gr_ptr.gt_net.nt_net) {
				*hostsetp = (hp->ht_flag | DP_HOSTSET);
				return (1);
			}
			break;
		};
	}
	return (0);
}

/*
 * Scan tree for a host that matches the address.
 */
int
scan_tree(dp, saddr)
	struct dirlist *dp;
	u_long saddr;
{
	int defset, hostset;

	if (dp) {
		if (scan_tree(dp->dp_left, saddr))
			return (1);
		if (chk_host(dp, saddr, &defset, &hostset))
			return (1);
		if (scan_tree(dp->dp_right, saddr))
			return (1);
	}
	return (0);
}

/*
 * Traverse the dirlist tree and free it up.
 */
void
free_dir(dp)
	struct dirlist *dp;
{

	if (dp) {
		free_dir(dp->dp_left);
		free_dir(dp->dp_right);
		free_host(dp->dp_hosts);
		free((caddr_t)dp);
	}
}

/*
 * Parse the option string and update fields.
 * Option arguments may either be -<option>=<value> or
 * -<option> <value>
 */
int
do_opt(cpp, endcpp, ep, grp, has_hostp, exflagsp, cr)
	char **cpp, **endcpp;
	struct exportlist *ep;
	struct grouplist *grp;
	int *has_hostp;
	int *exflagsp;
	struct ucred *cr;
{
	char *cpoptarg, *cpoptend;
	char *cp, *endcp, *cpopt, savedc, savedc2 = '\0';
	int allflag, usedarg;

	cpopt = *cpp;
	cpopt++;
	cp = *endcpp;
	savedc = *cp;
	*cp = '\0';
	while (cpopt && *cpopt) {
		allflag = 1;
		usedarg = -2;
		if (NULL != (cpoptend = strchr(cpopt, ','))) {
			*cpoptend++ = '\0';
			if (NULL != (cpoptarg = strchr(cpopt, '=')))
				*cpoptarg++ = '\0';
		} else {
			if (NULL != (cpoptarg = strchr(cpopt, '=')))
				*cpoptarg++ = '\0';
			else {
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
			*exflagsp |= MNT_EXRDONLY;
		} else if (cpoptarg && (!strcmp(cpopt, "maproot") ||
			!(allflag = strcmp(cpopt, "mapall")) ||
			!strcmp(cpopt, "root") || !strcmp(cpopt, "r"))) {
			usedarg++;
			parsecred(cpoptarg, cr);
			if (allflag == 0) {
				*exflagsp |= MNT_EXPORTANON;
				opt_flags |= OP_MAPALL;
			} else
				opt_flags |= OP_MAPROOT;
		} else if (!strcmp(cpopt, "kerb") || !strcmp(cpopt, "k")) {
			*exflagsp |= MNT_EXKERB;
			opt_flags |= OP_KERB;
		} else if (cpoptarg && (!strcmp(cpopt, "mask") ||
			!strcmp(cpopt, "m"))) {
			if (get_net(cpoptarg, &grp->gr_ptr.gt_net, 1)) {
				log(LOG_ERR, "Bad mask: %s", cpoptarg);
				return (1);
			}
			usedarg++;
			opt_flags |= OP_MASK;
		} else if (cpoptarg && (!strcmp(cpopt, "network") ||
			!strcmp(cpopt, "n"))) {
			if (grp->gr_type != GT_NULL) {
				log(LOG_ERR, "Network/host conflict");
				return (1);
			} else if (get_net(cpoptarg, &grp->gr_ptr.gt_net, 0)) {
				log(LOG_ERR, "Bad net: %s", cpoptarg);
				return (1);
			}
			grp->gr_type = GT_NET;
			*has_hostp = 1;
			usedarg++;
			opt_flags |= OP_NET;
		} else if (!strcmp(cpopt, "alldirs")) {
			opt_flags |= OP_ALLDIRS;
#ifdef ISO
		} else if (cpoptarg && !strcmp(cpopt, "iso")) {
			if (get_isoaddr(cpoptarg, grp)) {
				log(LOG_ERR, "Bad iso addr: %s", cpoptarg);
				return (1);
			}
			*has_hostp = 1;
			usedarg++;
			opt_flags |= OP_ISO;
#endif /* ISO */
		} else {
			log(LOG_ERR, "Bad opt %s", cpopt);
			return (1);
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
		cpopt = cpoptend;
	}
	**endcpp = savedc;
	return (0);
}

/*
 * Translate a character string to the corresponding list of network
 * addresses for a hostname.
 */
int
get_host(cp, grp)
	char *cp;
	struct grouplist *grp;
{
	struct hostent *hp, *nhp;
	char **addrp, **naddrp;
	struct hostent t_host;
	int i;
	u_long saddr;
	char *aptr[2];

	if (grp->gr_type != GT_NULL)
		return (1);
	if ((hp = gethostbyname(cp)) == NULL) {
		if (!isdigit(*cp)) {
			log(LOG_ERR, "Gethostbyname failed for %s", cp);
			return (1);
		}
		saddr = inet_addr(cp);
		if (saddr == -1) {
			log(LOG_ERR, "Inet_addr failed for %s", cp);
			return (1);
		}
		if ((hp = gethostbyaddr((caddr_t)&saddr, sizeof (saddr),
					AF_INET)) == NULL) {
			hp = &t_host;
			hp->h_name = cp;
			hp->h_addrtype = AF_INET;
			hp->h_length = sizeof (u_long);
			hp->h_addr_list = aptr;
			aptr[0] = (char *)&saddr;
			aptr[1] = (char *)NULL;
		}
	}
	grp->gr_type = GT_HOST;
	nhp = grp->gr_ptr.gt_hostent = (struct hostent *)
		malloc(sizeof(struct hostent));
	if (nhp == (struct hostent *)NULL)
		out_of_mem();
	memmove(nhp, hp, sizeof(struct hostent));
	i = strlen(hp->h_name)+1;
	nhp->h_name = (char *)malloc(i);
	if (nhp->h_name == (char *)NULL)
		out_of_mem();
	memmove(nhp->h_name, hp->h_name, i);
	addrp = hp->h_addr_list;
	i = 1;
	while (*addrp++)
		i++;
	naddrp = nhp->h_addr_list = (char **)
		malloc(i*sizeof(char *));
	if (naddrp == (char **)NULL)
		out_of_mem();
	addrp = hp->h_addr_list;
	while (*addrp) {
		*naddrp = (char *) malloc(hp->h_length);
		if (*naddrp == (char *)NULL)
			out_of_mem();
		memmove(*naddrp, *addrp, hp->h_length);
		addrp++;
		naddrp++;
	}
	*naddrp = (char *)NULL;
	if (debug)
		fprintf(stderr, "got host %s\n", hp->h_name);
	return (0);
}

/*
 * Free up an exports list component
 */
void
free_exp(ep)
	struct exportlist *ep;
{

	if (ep->ex_defdir) {
		free_host(ep->ex_defdir->dp_hosts);
		free((caddr_t)ep->ex_defdir);
	}
	if (ep->ex_fsdir)
		free(ep->ex_fsdir);
	free_dir(ep->ex_dirl);
	free((caddr_t)ep);
}

/*
 * Free hosts.
 */
void
free_host(hp)
	struct hostlist *hp;
{
	struct hostlist *hp2;

	while (hp) {
		hp2 = hp;
		hp = hp->ht_next;
		free((caddr_t)hp2);
	}
}

struct hostlist *
get_ht(void)
{
	struct hostlist *hp;

	hp = (struct hostlist *)malloc(sizeof (struct hostlist));
	if (hp == (struct hostlist *)NULL)
		out_of_mem();
	hp->ht_next = (struct hostlist *)NULL;
	hp->ht_flag = 0;
	return (hp);
}

#ifdef ISO
/*
 * Translate an iso address.
 */
get_isoaddr(cp, grp)
	char *cp;
	struct grouplist *grp;
{
	struct iso_addr *isop;
	struct sockaddr_iso *isoaddr;

	if (grp->gr_type != GT_NULL)
		return (1);
	if ((isop = iso_addr(cp)) == NULL) {
		log(LOG_ERR, "iso_addr failed, ignored");
		return (1);
	}
	isoaddr = (struct sockaddr_iso *) malloc(sizeof (struct sockaddr_iso));
	if (isoaddr == (struct sockaddr_iso *)NULL)
		out_of_mem();
	memset(isoaddr, 0, sizeof(struct sockaddr_iso));
	memmove(&isoaddr->siso_addr, isop, sizeof(struct iso_addr));
	isoaddr->siso_len = sizeof(struct sockaddr_iso);
	isoaddr->siso_family = AF_ISO;
	grp->gr_type = GT_ISO;
	grp->gr_ptr.gt_isoaddr = isoaddr;
	return (0);
}
#endif	/* ISO */

/*
 * Out of memory, fatal
 */
void
out_of_mem(void)
{
	log(LOG_ERR, "Out of memory");
	exit(2);
}

/*
 * Do the mount syscall with the update flag to push the export info into
 * the kernel.
 */
int
do_mount(ep, grp, exflags, anoncrp, dirp, dirplen, fsb)
	struct exportlist *ep;
	struct grouplist *grp;
	int exflags;
	struct ucred *anoncrp;
	char *dirp;
	int dirplen;
	struct statfs *fsb;
{
	char *cp = (char *)NULL;
	u_long **addrp;
	int done;
	char savedc = '\0';
	struct sockaddr_in sin, imask;
	union {
		struct ufs_args ua;
		struct iso_args ia;
	} args;
	u_long net;

	args.ua.fspec = 0;
	args.ua.export.ex_flags = exflags;
	args.ua.export.ex_anon = *anoncrp;
	memset(&sin, 0, sizeof(sin));
	memset(&imask, 0, sizeof(imask));
	sin.sin_family = AF_INET;
	sin.sin_len = sizeof(sin);
	imask.sin_family = AF_INET;
	imask.sin_len = sizeof(sin);
	if (grp->gr_type == GT_HOST)
		addrp = (u_long **)grp->gr_ptr.gt_hostent->h_addr_list;
	else
		addrp = (u_long **)NULL;
	done = FALSE;
	while (!done) {
		switch (grp->gr_type) {
		case GT_HOST:
			if (addrp) {
				sin.sin_addr.s_addr = **addrp;
				args.ua.export.ex_addrlen = sizeof(sin);
			} else
				args.ua.export.ex_addrlen = 0;
			args.ua.export.ex_addr = (struct sockaddr *)&sin;
			args.ua.export.ex_masklen = 0;
			break;
		case GT_NET:
			if (grp->gr_ptr.gt_net.nt_mask)
			    imask.sin_addr.s_addr = grp->gr_ptr.gt_net.nt_mask;
			else {
			    net = ntohl(grp->gr_ptr.gt_net.nt_net);
			    if (IN_CLASSA(net))
				imask.sin_addr.s_addr = inet_addr("255.0.0.0");
			    else if (IN_CLASSB(net))
				imask.sin_addr.s_addr =
				    inet_addr("255.255.0.0");
			    else
				imask.sin_addr.s_addr =
				    inet_addr("255.255.255.0");
			    grp->gr_ptr.gt_net.nt_mask = imask.sin_addr.s_addr;
			}
			sin.sin_addr.s_addr = grp->gr_ptr.gt_net.nt_net;
			args.ua.export.ex_addr = (struct sockaddr *)&sin;
			args.ua.export.ex_addrlen = sizeof (sin);
			args.ua.export.ex_mask = (struct sockaddr *)&imask;
			args.ua.export.ex_masklen = sizeof (imask);
			break;
#ifdef ISO
		case GT_ISO:
			args.ua.export.ex_addr =
				(struct sockaddr *)grp->gr_ptr.gt_isoaddr;
			args.ua.export.ex_addrlen =
				sizeof(struct sockaddr_iso);
			args.ua.export.ex_masklen = 0;
			break;
#endif	/* ISO */
		default:
			log(LOG_ERR, "Bad grouptype");
			if (cp)
				*cp = savedc;
			return (1);
		};

		/*
		 * XXX:
		 * Maybe I should just use the fsb->f_mntonname path instead
		 * of looping back up the dirp to the mount point??
		 * Also, needs to know how to export all types of local
		 * exportable file systems and not just "ufs".
		 */
		while (mount(fsb->f_fstypename, dirp,
		       fsb->f_flags | MNT_UPDATE, (caddr_t)&args) < 0) {
			if (cp)
				*cp-- = savedc;
			else
				cp = dirp + dirplen - 1;
			if (errno == EPERM) {
				log(LOG_ERR,
				   "Can't change attributes for %s.  See 'exports' man page.", dirp);
				return (1);
			}
			if (opt_flags & OP_ALLDIRS) {
				log(LOG_ERR, "Could not remount %s: %m",
					dirp);
				return (1);
			}
			/* back up over the last component */
			while (*cp == '/' && cp > dirp)
				cp--;
			while (*(cp - 1) != '/' && cp > dirp)
				cp--;
			if (cp == dirp) {
				if (debug)
					fprintf(stderr,"mnt unsucc\n");
				log(LOG_ERR, "Can't export %s", dirp);
				return (1);
			}
			savedc = *cp;
			*cp = '\0';
		}

		if (addrp) {
			++addrp;
			if (*addrp == (u_long *)NULL)
				done = TRUE;
		} else
			done = TRUE;
		if (cp) {
			*cp = savedc;
			cp = (char *)NULL;
		}
	}
	return (0);
}

/*
 * Translate a net address.
 */
int
get_net(cp, net, maskflg)
	char *cp;
	struct netmsk *net;
	int maskflg;
{
	struct netent *np;
	long netaddr;
	struct in_addr inetaddr, inetaddr2;
	char *name;

	if (NULL != (np = getnetbyname(cp)))
		inetaddr = inet_makeaddr(np->n_net, 0);
	else if (isdigit(*cp)) {
		if ((netaddr = inet_network(cp)) == -1)
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
		net->nt_name = (char *)malloc(strlen(name) + 1);
		if (net->nt_name == (char *)NULL)
			out_of_mem();
		strcpy(net->nt_name, name);
		net->nt_net = inetaddr.s_addr;
	}
	return (0);
}

/*
 * Find the next field in a line.
 * Fields are separated by white space.
 * Space and tab characters may be escasped.
 * Quoted strings are not broken at white space.
 */
void
nextfield(char **line_start, char **line_end)
{
	char *a, q;

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
		*line_start = a;
		a++;

		while (*a != q && *a != '\0' && *a != '\n')
			a++;
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

/* char *line is a global */
int
get_line(int source)
{
	if (source == EXPORT_FROM_NETINFO)
		return ni_get_line();
	return file_get_line();
}

/*
 * Get export entries from NetInfo and give it back to mountd a line
 * at a time.  Since mountd does all kinds of hacking on exports
 * at the same time that it parses the entries, it's a lot easier
 * replacing this get_line() than get_exportlist().
 */
static ni_idlist ni_exports_list;
static int ni_exports_index;
static void *ni;

void
ni_exports_open(void)
{
	int status;
	ni_id dir;

	status = ni_open(NULL, ".", &ni);
	if (status != NI_OK)
		log(LOG_ERR, "NetInfo open failed: %s", ni_error(status));

	NI_INIT(&ni_exports_list);
	ni_exports_index = 0;

	status = ni_pathsearch(ni, &dir, "/exports");
	if (status == NI_NODIR)
		return;
	if (status != NI_OK) {
		log(LOG_ERR, "NetInfo error searching for /exports: %s",
		    ni_error(status));
		return;
	}

	status = ni_children(ni, &dir, &ni_exports_list);
	if (status != NI_OK) {
		log(LOG_ERR, "NetInfo error reading /exports: %s",
		    ni_error(status));
		return;
	}
}

void
ni_exports_close(void)
{
	ni_free(ni);
	ni_idlist_free(&ni_exports_list);
	ni_exports_index = 0;
}

int
ni_get_line(void)
{
	ni_id dir;
	ni_proplist pl;
	ni_namelist *nl;
	ni_index where;
	int status, i;

	while (1) {
		line[0] = '\0';
		if (ni_exports_index >= ni_exports_list.ni_idlist_len)
			return (0);

		dir.nii_object = ni_exports_list.ni_idlist_val[ni_exports_index++];
		status = ni_read(ni, &dir, &pl);
		if (status != NI_OK) {
			log(LOG_ERR,
			    "NetInfo error reading /exports/dir:%lu: %s",
			    dir.nii_object, ni_error(status));
			continue;
		}
		where = ni_proplist_match(pl, "name", NULL);
		if (where == NI_INDEX_NULL) {
			log(LOG_ERR,
			    "NetInfo directory /exports/dir:%lu has no name",
			    dir.nii_object);
			continue;
		}
		if (pl.ni_proplist_val[where].nip_val.ni_namelist_len == 0) {
			log(LOG_ERR,
			    "NetInfo directory /exports/dir:%lu has no name",
			    dir.nii_object);
			continue;
		}
		nl = &pl.ni_proplist_val[where].nip_val;
		for (i = 0; i < nl->ni_namelist_len; i++) {
			strcat(line, nl->ni_namelist_val[i]);
			if (i + 1 < nl->ni_namelist_len)
				strcat(line, " ");
		}
		where = ni_proplist_match(pl, "opts", NULL);
		if (where != NI_INDEX_NULL &&
		    pl.ni_proplist_val[where].nip_val.ni_namelist_len > 0) {
			nl = &pl.ni_proplist_val[where].nip_val;
			for (i = 0; i < nl->ni_namelist_len; i++) {
				strcat(line, " -");
				strcat(line, nl->ni_namelist_val[i]);
			}
		}
		where = ni_proplist_match(pl, "clients", NULL);
		if (where != NI_INDEX_NULL &&
		    pl.ni_proplist_val[where].nip_val.ni_namelist_len > 0) {
			nl = &pl.ni_proplist_val[where].nip_val;
			for (i = 0; i < nl->ni_namelist_len; i++) {
				strcat(line, " ");
				strcat(line, nl->ni_namelist_val[i]);
			}
		}
		break;
	}
	return (1);
}

/*
 * Get an exports file line. Skip over blank lines and handle line
 * continuations.
 */
int
file_get_line(void)
{
	char *p, *cp;
	int len;
	int totlen, cont_line;

	/*
	 * Loop around ignoring blank lines and getting all continuation lines.
	 */
	p = line;
	totlen = 0;
	do {
		if (fgets(p, LINESIZ - totlen, exp_file) == NULL)
			return (0);
		len = strlen(p);
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
		*++cp = '\0';
		if (len > 0) {
			totlen += len;
			if (totlen >= LINESIZ) {
				log(LOG_ERR, "Exports line too long");
				exit(2);
			}
			p = cp;
		}
	} while (totlen == 0 || cont_line);
	return (1);
}

/*
 * Parse a description of a credential.
 */
void
parsecred(namelist, cr)
	char *namelist;
	struct ucred *cr;
{
	char *name;
	int cnt;
	char *names;
	struct passwd *pw;
	struct group *gr;
	int ngroups, groups[NGROUPS + 1];

	/*
	 * Set up the unpriviledged user.
	 */
	cr->cr_ref = 1;
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
			return;
		}
		cr->cr_uid = pw->pw_uid;
		ngroups = NGROUPS + 1;
		if (getgrouplist(pw->pw_name, pw->pw_gid, groups, &ngroups))
			log(LOG_ERR, "Too many groups");
		/*
		 * Convert from int's to gid_t's and compress out duplicate
		 */
		cr->cr_ngroups = ngroups - 1;
		cr->cr_groups[0] = groups[0];
		for (cnt = 2; cnt < ngroups; cnt++)
			cr->cr_groups[cnt - 1] = groups[cnt];
		return;
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
		return;
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
		log(LOG_ERR, "Too many groups");
}

#define	STRSIZ	(RPCMNT_NAMELEN+RPCMNT_PATHLEN+50)
/*
 * Routines that maintain the remote mounttab
 */
void
get_mountlist(void)
{
	struct mountlist *mlp, **mlpp;
	char *host, *dirp, *cp;
	char str[STRSIZ];
	FILE *mlfile;

	if ((mlfile = fopen(_PATH_RMOUNTLIST, "r")) == NULL) {
		if (errno != ENOENT || debug)
			log(LOG_ERR, "Can't open %s, errno=%d",
			    _PATH_RMOUNTLIST, errno);
		return;
	}
	mlpp = &mlhead;
	while (fgets(str, STRSIZ, mlfile) != NULL) {
		cp = str;
		host = strsep(&cp, " \t\n");
		dirp = strsep(&cp, " \t\n");
		if (host == NULL || dirp == NULL)
			continue;
		mlp = (struct mountlist *)malloc(sizeof (*mlp));
		if (mlp == (struct mountlist *)NULL)
			out_of_mem();
		strncpy(mlp->ml_host, host, RPCMNT_NAMELEN);
		mlp->ml_host[RPCMNT_NAMELEN] = '\0';
		strncpy(mlp->ml_dirp, dirp, RPCMNT_PATHLEN);
		mlp->ml_dirp[RPCMNT_PATHLEN] = '\0';
		mlp->ml_next = (struct mountlist *)NULL;
		*mlpp = mlp;
		mlpp = &mlp->ml_next;
	}
	fclose(mlfile);
}

void
del_mlist(hostp, dirp)
	char *hostp, *dirp;
{
	struct mountlist *mlp, **mlpp;
	struct mountlist *mlp2;
	FILE *mlfile;
	int fnd = 0;

	mlpp = &mlhead;
	mlp = mlhead;
	while (mlp) {
		if (!strcmp(mlp->ml_host, hostp) &&
		    (!dirp || !strcmp(mlp->ml_dirp, dirp))) {
			fnd = 1;
			mlp2 = mlp;
			*mlpp = mlp = mlp->ml_next;
			free((caddr_t)mlp2);
		} else {
			mlpp = &mlp->ml_next;
			mlp = mlp->ml_next;
		}
	}
	if (fnd) {
		if ((mlfile = fopen(_PATH_RMOUNTLIST, "w")) == NULL) {
			log(LOG_ERR, "Can't write %s, errno=%d",
			    _PATH_RMOUNTLIST, errno);
			return;
		}
		mlp = mlhead;
		while (mlp) {
			fprintf(mlfile, "%s %s\n", mlp->ml_host, mlp->ml_dirp);
			mlp = mlp->ml_next;
		}
		fclose(mlfile);
	}
}

void
add_mlist(hostp, dirp)
	char *hostp, *dirp;
{
	struct mountlist *mlp, **mlpp;
	FILE *mlfile;

	mlpp = &mlhead;
	mlp = mlhead;
	while (mlp) {
		if (!strcmp(mlp->ml_host, hostp) && !strcmp(mlp->ml_dirp, dirp))
			return;
		mlpp = &mlp->ml_next;
		mlp = mlp->ml_next;
	}
	mlp = (struct mountlist *)malloc(sizeof (*mlp));
	if (mlp == (struct mountlist *)NULL)
		out_of_mem();
	strncpy(mlp->ml_host, hostp, RPCMNT_NAMELEN);
	mlp->ml_host[RPCMNT_NAMELEN] = '\0';
	strncpy(mlp->ml_dirp, dirp, RPCMNT_PATHLEN);
	mlp->ml_dirp[RPCMNT_PATHLEN] = '\0';
	mlp->ml_next = (struct mountlist *)NULL;
	*mlpp = mlp;
	if ((mlfile = fopen(_PATH_RMOUNTLIST, "a")) == NULL) {
		log(LOG_ERR, "Can't append %s, errno=%d",
		    _PATH_RMOUNTLIST, errno);
		return;
	}
	fprintf(mlfile, "%s %s\n", mlp->ml_host, mlp->ml_dirp);
	fclose(mlfile);
}

/*
 * This function is called via. SIGTERM when the system is going down.
 * It sends a broadcast RPCMNT_UMNTALL.
 */
void
send_umntall(void)
{
	/* NULL callback tells it not to wait. */
	(void) clnt_broadcast(RPCPROG_MNT, RPCMNT_VER1, RPCMNT_UMNTALL,
			      xdr_void, (caddr_t)0, xdr_void, (caddr_t)0,
			      NULL);
	exit(0);
}

/*
 * Free up a group list.
 */
void
free_grp(grp)
	struct grouplist *grp;
{
	char **addrp;

	if (grp->gr_type == GT_HOST) {
		if (grp->gr_ptr.gt_hostent->h_name) {
			addrp = grp->gr_ptr.gt_hostent->h_addr_list;
			while (addrp && *addrp)
				free(*addrp++);
			if (grp->gr_ptr.gt_hostent->h_addr_list)
				free((caddr_t)grp->gr_ptr.gt_hostent->h_addr_list);
			free(grp->gr_ptr.gt_hostent->h_name);
		}
		free((caddr_t)grp->gr_ptr.gt_hostent);
	} else if (grp->gr_type == GT_NET) {
		if (grp->gr_ptr.gt_net.nt_name)
			free(grp->gr_ptr.gt_net.nt_name);
	}
#ifdef ISO
	else if (grp->gr_type == GT_ISO)
		free((caddr_t)grp->gr_ptr.gt_isoaddr);
#endif
	free((caddr_t)grp);
}

void
SYSLOG(int pri, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	va_end(ap);
}

/*
 * Check options for consistency.
 */
int
check_options(dp)
	struct dirlist *dp;
{

	if (dp == (struct dirlist *)NULL)
		return (1);
	if ((opt_flags & (OP_MAPROOT | OP_MAPALL)) == (OP_MAPROOT|OP_MAPALL) ||
	    (opt_flags & (OP_MAPROOT | OP_KERB)) == (OP_MAPROOT | OP_KERB) ||
	    (opt_flags & (OP_MAPALL | OP_KERB)) == (OP_MAPALL | OP_KERB)) {
		log(LOG_ERR, "-mapall, -maproot and -kerb mutually exclusive");
		return (1);
	}
	if ((opt_flags & OP_MASK) && (opt_flags & OP_NET) == 0) {
		log(LOG_ERR, "-mask requires -net");
		return (1);
	}
	if ((opt_flags & (OP_NET | OP_ISO)) == (OP_NET | OP_ISO)) {
		log(LOG_ERR, "-net and -iso mutually exclusive");
		return (1);
	}
	if ((opt_flags & OP_ALLDIRS) && dp->dp_left) {
		log(LOG_ERR, "-alldir has multiple directories");
		return (1);
	}
	return (0);
}

/*
 * Check an absolute directory path for any symbolic links. Return true
 * if no symbolic links are found.
 */
int
check_dirpath(dirp)
	char *dirp;
{
	char *cp;
	int ret = 1;
	struct stat sb;

	for (cp = dirp + 1; *cp && ret; cp++)
		if (*cp == '/') {
			*cp = '\0';
			if (lstat(dirp, &sb) < 0 || !S_ISDIR(sb.st_mode))
				ret = 0;
			*cp = '/';
		}
	if (ret && (lstat(dirp, &sb) < 0 || !S_ISDIR(sb.st_mode)))
		ret = 0;
	return (ret);
}

/*
 * Just translate an ascii string to an integer.
 */
int
get_num(cp)
	register char *cp;
{
	register int res = 0;

	while (*cp) {
		if (*cp < '0' || *cp > '9')
			return (-1);
		res = res * 10 + (*cp++ - '0');
	}
	return (res);
}

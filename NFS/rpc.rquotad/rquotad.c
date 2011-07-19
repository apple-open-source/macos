/*
 * Copyright (c) 2007-2010 Apple Inc.  All rights reserved.
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

/*	$NetBSD: rquotad.c,v 1.23 2006/05/09 20:18:07 mrg Exp $	*/

/*
 * by Manuel Bouyer (bouyer@ensta.fr). Public domain.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: rquotad.c,v 1.23 2006/05/09 20:18:07 mrg Exp $");
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/event.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/queue.h>
#include <sys/quota.h>

#include <stdio.h>
#include <fstab.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <unistd.h>
#include <err.h>
#include <pthread.h>
#include <syslog.h>
#include <libutil.h>

#include <oncrpc/rpc.h>
#include <oncrpc/rpcb.h>
#include "rquota.h"

int bindresvport_sa(int sd, struct sockaddr *sa);

void rquota_service(struct svc_req *request, SVCXPRT *transp);
void ext_rquota_service(struct svc_req *request, SVCXPRT *transp);
void sendquota(struct svc_req *request, int vers, SVCXPRT *transp);
int getfsquota(int type, int id, char *path, struct dqblk *dqblk);
int hasquota(struct statfs *fst, char **uqfnamep, char **gqfnamep);
void lock_fsq(void);
void unlock_fsq(void);
void check_mounts(void);
void sigmux(int);
void *rquotad_thread(void *arg);

#if 0
#define DEBUG(args...)	printf(args)
#else
#define DEBUG(args...)
#endif

#define _PATH_NFS_CONF		"/etc/nfs.conf"
#define _PATH_RQUOTAD_PID	"/var/run/rquotad.pid"

/*
 * structure holding NFS server config values
 */
struct nfs_conf_server {
	int rquota_port;
	int tcp;
	int udp;
	int verbose;
};
const struct nfs_conf_server config_defaults =
{
	0,		/* rquota_port */
	1,		/* tcp */
	1,		/* udp */
	0		/* verbose */
};
int config_read(struct nfs_conf_server *conf);

/*
 * structure containing informations about file systems with quota files
 */
struct fsq_stat {
	TAILQ_ENTRY(fsq_stat) chain;	/* list of file systems */
	char   *mountdir;		/* mount point of the filesystem */
	char   *uqfpathname;		/* pathname of the user quota file */
	char   *gqfpathname;		/* pathname of the group quota file */
	fsid_t	fsid;			/* fsid for the file system */
	dev_t   st_dev;			/* device of the filesystem */
};
TAILQ_HEAD(fsqhead,fsq_stat) fsqhead;
pthread_mutex_t fsq_mutex;		/* mutex for file system quota list */
int gotterm = 0;
struct nfs_conf_server config;

const char *qfextension[] = INITQFNAMES;

void 
sigmux(__unused int dummy)
{
	gotterm = 1;
}

int
main(__unused int argc, __unused char *argv[])
{
	SVCXPRT *transp;
	struct sockaddr_storage saddr;
	struct sockaddr_in *sin = (struct sockaddr_in*)&saddr;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)&saddr;
	int error, on = 1, svcregcnt;
	pthread_attr_t pattr;
	pthread_t thd;
	int kq, rv;
	struct kevent ke;
	struct pidfh *pfh;
	pid_t pid;
	int rqudpsock, rqtcpsock;
	int rqudp6sock, rqtcp6sock;

	/* If we are serving UDP, set up the RQUOTA/UDP sockets. */

	/* set defaults then do config_read() to get config values */
	config = config_defaults;
	config_read(&config);

	openlog("rpc.rquotad", LOG_CONS|LOG_PID, LOG_DAEMON);

	/* claim PID file */
	pfh = pidfile_open(_PATH_RQUOTAD_PID, 0644, &pid);
	if (pfh == NULL) {
		syslog(LOG_ERR, "can't open rquotad pidfile: %s (%d)", strerror(errno), errno);
		if ((errno == EACCES) && getuid())
			syslog(LOG_ERR, "rquotad is expected to be run as root, not as uid %d.", getuid());
		else if (errno == EEXIST)
			syslog(LOG_ERR, "rquotad already running, pid: %d", pid);
		exit(2);
	}
	if (pidfile_write(pfh) == -1)
		syslog(LOG_WARNING, "can't write to rquotad pidfile: %s (%d)", strerror(errno), errno);

	rpcb_unset(NULL, RQUOTAPROG, RQUOTAVERS);
	rpcb_unset(NULL, RQUOTAPROG, EXT_RQUOTAVERS);

	signal(SIGINT, sigmux);
	signal(SIGTERM, sigmux);
	signal(SIGHUP, sigmux);

	/* create and register the service */
	rqudpsock = rqtcpsock = -1;
	rqudp6sock = rqtcp6sock = -1;

	/* If we are serving UDP, set up the RQUOTA/UDP sockets. */
	if (config.udp) {

		/* IPv4 */
		if ((rqudpsock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
			syslog(LOG_WARNING, "can't create UDP IPv4 socket: %s (%d)", strerror(errno), errno);
		if (rqudpsock >= 0) {
			sin->sin_family = AF_INET;
			sin->sin_addr.s_addr = INADDR_ANY;
			sin->sin_port = htons(config.rquota_port);
			sin->sin_len = sizeof(*sin);
			if (bindresvport_sa(rqudpsock, (struct sockaddr *)sin) < 0) {
				syslog(LOG_WARNING, "can't bind UDP IPv4 addr: %s (%d)", strerror(errno), errno);
				close(rqudpsock);
				rqudpsock = -1;
			}
		}
		if ((rqudpsock >= 0) && ((transp = svcudp_create(rqudpsock)) == NULL)) {
			syslog(LOG_WARNING, "cannot create UDP IPv4 service");
			close(rqudpsock);
			rqudpsock = -1;
		}
		if (rqudpsock >= 0) {
			svcregcnt = 0;
			if (!svc_register(transp, RQUOTAPROG, RQUOTAVERS, rquota_service, IPPROTO_UDP))
				syslog(LOG_WARNING, "unable to register (RQUOTAPROG, RQUOTAVERS, UDP/IPv4)");
			else
				svcregcnt++;
			if (!svc_register(transp, RQUOTAPROG, EXT_RQUOTAVERS, ext_rquota_service, IPPROTO_UDP))
				syslog(LOG_WARNING, "unable to register (RQUOTAPROG, EXT_RQUOTAVERS, UDP/IPv4)");
			else
				svcregcnt++;
			if (!svcregcnt) {
				svc_destroy(transp);
				close(rqudpsock);
				rqudpsock = -1;
			}
		}

		/* IPv6 */
		if ((rqudp6sock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
			syslog(LOG_WARNING, "can't create UDP IPv6 socket: %s (%d)", strerror(errno), errno);
		if (rqudp6sock >= 0) {
			setsockopt(rqudp6sock, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on));
			sin6->sin6_family = AF_INET6;
			sin6->sin6_addr = in6addr_any;
			sin6->sin6_port = htons(config.rquota_port);
			sin6->sin6_len = sizeof(*sin6);
			if (bindresvport_sa(rqudp6sock, (struct sockaddr *)sin6) < 0) {
				syslog(LOG_WARNING, "can't bind UDP IPv6 addr: %s (%d)", strerror(errno), errno);
				close(rqudp6sock);
				rqudp6sock = -1;
			}
		}
		if ((rqudp6sock >= 0) && ((transp = svcudp_create(rqudp6sock)) == NULL)) {
			syslog(LOG_WARNING, "cannot create UDP IPv6 service");
			close(rqudp6sock);
			rqudp6sock = -1;
		}
		if (rqudp6sock >= 0) {
			svcregcnt = 0;
			if (!svc_register(transp, RQUOTAPROG, RQUOTAVERS, rquota_service, IPPROTO_UDP))
				syslog(LOG_WARNING, "unable to register (RQUOTAPROG, RQUOTAVERS, UDP/IPv6)");
			else
				svcregcnt++;
			if (!svc_register(transp, RQUOTAPROG, EXT_RQUOTAVERS, ext_rquota_service, IPPROTO_UDP))
				syslog(LOG_WARNING, "unable to register (RQUOTAPROG, EXT_RQUOTAVERS, UDP/IPv6)");
			else
				svcregcnt++;
			if (!svcregcnt) {
				svc_destroy(transp);
				close(rqudp6sock);
				rqudp6sock = -1;
			}
		}
	}

	/* If we are serving TCP, set up the RQUOTA/TCP sockets. */
	if (config.tcp) {

		/* IPv4 */
		if ((rqtcpsock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			syslog(LOG_WARNING, "can't create TCP IPv4 socket: %s (%d)", strerror(errno), errno);
		if (rqtcpsock >= 0) {
			if (setsockopt(rqtcpsock, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
				syslog(LOG_WARNING, "setsockopt TCP IPv4 SO_REUSEADDR: %s (%d)", strerror(errno), errno);
			sin->sin_family = AF_INET;
			sin->sin_addr.s_addr = INADDR_ANY;
			sin->sin_port = htons(config.rquota_port);
			sin->sin_len = sizeof(*sin);
			if (bindresvport_sa(rqtcpsock, (struct sockaddr *)sin) < 0) {
				syslog(LOG_WARNING, "can't bind TCP IPv4 addr: %s (%d)", strerror(errno), errno);
				close(rqtcpsock);
				rqtcpsock = -1;
			}
		}
		if ((rqtcpsock >= 0) && ((transp = svctcp_create(rqtcpsock, 0, 0)) == NULL)) {
			syslog(LOG_WARNING, "cannot create TCP IPv4 service");
			close(rqtcpsock);
			rqtcpsock = -1;
		}
		if (rqtcpsock >= 0) {
			svcregcnt = 0;
			if (!svc_register(transp, RQUOTAPROG, RQUOTAVERS, rquota_service, IPPROTO_TCP))
				syslog(LOG_WARNING, "unable to register (RQUOTAPROG, RQUOTAVERS, TCP/IPv4)");
			else
				svcregcnt++;
			if (!svc_register(transp, RQUOTAPROG, EXT_RQUOTAVERS, ext_rquota_service, IPPROTO_TCP))
				syslog(LOG_WARNING, "unable to register (RQUOTAPROG, EXT_RQUOTAVERS, TCP/IPv4)");
			else
				svcregcnt++;
			if (!svcregcnt) {
				svc_destroy(transp);
				close(rqtcpsock);
				rqtcpsock = -1;
			}
		}

		/* IPv6 */
		if ((rqtcp6sock = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
			syslog(LOG_WARNING, "can't create TCP IPv6 socket: %s (%d)", strerror(errno), errno);
		if (rqtcp6sock >= 0) {
			if (setsockopt(rqtcp6sock, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
				syslog(LOG_WARNING, "setsockopt TCP IPv4 SO_REUSEADDR: %s (%d)", strerror(errno), errno);
			setsockopt(rqtcp6sock, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on));
			sin6->sin6_family = AF_INET6;
			sin6->sin6_addr = in6addr_any;
			sin6->sin6_port = htons(config.rquota_port);
			sin6->sin6_len = sizeof(*sin6);
			if (bindresvport_sa(rqtcp6sock, (struct sockaddr *)sin6) < 0) {
				syslog(LOG_WARNING, "can't bind TCP IPv6 addr: %s (%d)", strerror(errno), errno);
				close(rqtcp6sock);
				rqtcp6sock = -1;
			}
		}
		if ((rqtcp6sock >= 0) && ((transp = svctcp_create(rqtcp6sock, 0, 0)) == NULL)) {
			syslog(LOG_WARNING, "cannot create TCP IPv6 service");
			close(rqtcp6sock);
			rqtcp6sock = -1;
		}
		if (rqtcp6sock >= 0) {
			svcregcnt = 0;
			if (!svc_register(transp, RQUOTAPROG, RQUOTAVERS, rquota_service, IPPROTO_TCP))
				syslog(LOG_WARNING, "unable to register (RQUOTAPROG, RQUOTAVERS, TCP/IPv6)");
			else
				svcregcnt++;
			if (!svc_register(transp, RQUOTAPROG, EXT_RQUOTAVERS, ext_rquota_service, IPPROTO_TCP))
				syslog(LOG_WARNING, "unable to register (RQUOTAPROG, EXT_RQUOTAVERS, TCP/IPv6)");
			else
				svcregcnt++;
			if (!svcregcnt) {
				svc_destroy(transp);
				close(rqtcp6sock);
				rqtcp6sock = -1;
			}
		}
	}

	if ((rqudp6sock < 0) && (rqtcp6sock < 0))
		syslog(LOG_WARNING, "Can't create RQUOTA IPv6 sockets");
	if ((rqudpsock < 0) && (rqtcpsock < 0))
		syslog(LOG_WARNING, "Can't create RQUOTA IPv4 sockets");
	if ((rqudp6sock < 0) && (rqtcp6sock < 0) &&
	    (rqudpsock < 0) && (rqtcpsock < 0)) {
		syslog(LOG_ERR, "Can't create any RQUOTA sockets!");
		exit(1);
	}

	/* init file system quotas list */
	error = pthread_mutex_init(&fsq_mutex, NULL);
	if (error) {
		syslog(LOG_ERR, "file system quota mutex init failed: %s (%d)", strerror(error), error);
		exit(1);
	}
	TAILQ_INIT(&fsqhead);
	check_mounts();

	/* launch rquotad pthread */
	pthread_attr_init(&pattr);
	pthread_attr_setdetachstate(&pattr, PTHREAD_CREATE_DETACHED);
	error = pthread_create(&thd, &pattr, rquotad_thread, NULL);
	if (error) {
		syslog(LOG_ERR, "rquotad pthread_create: %s (%d)", strerror(error), error);
		exit(1);
	}

	/* sit around waiting for mount/unmount events and/or a signal */
	if ((kq = kqueue()) < 0) {
		syslog(LOG_ERR, "kqueue: %s (%d)", strerror(errno), errno);
		exit(1);
	}
	EV_SET(&ke, 0, EVFILT_FS, EV_ADD, 0, 0, 0);
	rv = kevent(kq, &ke, 1, NULL, 0, NULL);
	if (rv < 0) {
		syslog(LOG_ERR, "kevent(EVFILT_FS): %s (%d)", strerror(errno), errno);
		exit(1);
	}

	while (!gotterm) {
		rv = kevent(kq, NULL, 0, &ke, 1, NULL);
		if ((rv > 0) && !(ke.flags & EV_ERROR) && (ke.fflags & (VQ_MOUNT|VQ_UNMOUNT))) {
			/* syslog(LOG_DEBUG, "mount list changed: 0x%x", ke.fflags); */
			check_mounts();
		}
	}

	alarm(1); /* XXX 5028243 in case rpcb_unset() gets hung up during shutdown */
	rpcb_unset(NULL, RQUOTAPROG, RQUOTAVERS);
	rpcb_unset(NULL, RQUOTAPROG, EXT_RQUOTAVERS);
	pidfile_remove(pfh);
	exit(0);
}

/*
 * The incredibly complex rquotad thread function
 */
void *
rquotad_thread(__unused void *arg)
{
	sigset_t sigset;

	sigemptyset(&sigset);
	sigaddset(&sigset, SIGINT);
	sigaddset(&sigset, SIGQUIT);
	sigaddset(&sigset, SIGSYS);
	sigaddset(&sigset, SIGPIPE);
	sigaddset(&sigset, SIGTERM);
	sigaddset(&sigset, SIGHUP);
	sigaddset(&sigset, SIGUSR1);
	sigaddset(&sigset, SIGUSR2);
	sigaddset(&sigset, SIGABRT);
	pthread_sigmask(SIG_BLOCK, &sigset, NULL);

	svc_run();
	syslog(LOG_ERR, "rquotad died");
	exit(1);
}

void 
rquota_service(struct svc_req *request, SVCXPRT *transp)
{
	switch (request->rq_proc) {
	case NULLPROC:
		svc_sendreply(transp, (xdrproc_t)xdr_void, (char *)NULL);
		break;

	case RQUOTAPROC_GETQUOTA:
	case RQUOTAPROC_GETACTIVEQUOTA:
		sendquota(request, RQUOTAVERS, transp);
		break;

	default:
		svcerr_noproc(transp);
		break;
	}
}

void 
ext_rquota_service(struct svc_req *request, SVCXPRT *transp)
{
	switch (request->rq_proc) {
	case NULLPROC:
		svc_sendreply(transp, (xdrproc_t)xdr_void, (char *)NULL);
		break;

	case RQUOTAPROC_GETQUOTA:
	case RQUOTAPROC_GETACTIVEQUOTA:
		sendquota(request, EXT_RQUOTAVERS, transp);
		break;

	default:
		svcerr_noproc(transp);
		break;
	}
}

int
ismember(struct authunix_parms *aup, int gid)
{
	uint g;

	if (aup->aup_gid == (uint32_t)gid)
		return (1);
	for (g=0; g < aup->aup_len; g++)
		if (aup->aup_gids[g] == (uint32_t)gid)
			return (1);
	return (0);
}

/* read quota for the specified id, and send it */
void 
sendquota(struct svc_req *request, int vers, SVCXPRT *transp)
{
	struct getquota_args getq_args;
	struct ext_getquota_args ext_getq_args;
	struct getquota_rslt getq_rslt;
	struct dqblk dqblk;
	struct timeval timev;
	struct authunix_parms *aup;

	memset((char *)&getq_args, 0, sizeof(getq_args));
	memset((char *)&ext_getq_args, 0, sizeof(ext_getq_args));
	switch (vers) {
	case RQUOTAVERS:
		if (!svc_getargs(transp, (xdrproc_t)xdr_getquota_args,
		    (caddr_t)&getq_args)) {
			svcerr_decode(transp);
			return;
		}
		ext_getq_args.gqa_pathp = getq_args.gqa_pathp;
		ext_getq_args.gqa_id = getq_args.gqa_uid;
		ext_getq_args.gqa_type = RQUOTA_USRQUOTA;
		break;
	case EXT_RQUOTAVERS:
		if (!svc_getargs(transp, (xdrproc_t)xdr_ext_getquota_args,
		    (caddr_t)&ext_getq_args)) {
			svcerr_decode(transp);
			return;
		}
		break;
	}
	aup = (struct authunix_parms *)request->rq_clntcred;
	if (request->rq_cred.oa_flavor != AUTH_UNIX) {
		/* bad auth */
		getq_rslt.status = Q_EPERM;
	} else if ((ext_getq_args.gqa_type == RQUOTA_USRQUOTA) && aup->aup_uid &&
	    (aup->aup_uid != (uint32_t)ext_getq_args.gqa_id)) {
		/* only allow user or root to get a user quota */
		getq_rslt.status = Q_EPERM;
	} else if ((ext_getq_args.gqa_type == RQUOTA_GRPQUOTA) && aup->aup_uid &&
	    !ismember(aup, ext_getq_args.gqa_id)) {
		/* only allow root or group members to get a group quota */
		getq_rslt.status = Q_EPERM;
	} else if (!getfsquota(ext_getq_args.gqa_type, ext_getq_args.gqa_id,
	    ext_getq_args.gqa_pathp, &dqblk)) {
		/* failed, return noquota */
		getq_rslt.status = Q_NOQUOTA;
	} else {
		uint32_t bsize = DEV_BSIZE;
#define CLAMP_MAX_32(V)	(((V) > UINT32_MAX) ? UINT32_MAX : (V))

		/* scale the block size up to fit values into 32 bits */
		while ((bsize < INT32_MAX) &&
			(((dqblk.dqb_bhardlimit / bsize) > UINT32_MAX) ||
			 ((dqblk.dqb_bsoftlimit / bsize) > UINT32_MAX) ||
			 ((dqblk.dqb_curbytes / bsize) > UINT32_MAX)))
			bsize <<= 1;

		gettimeofday(&timev, NULL);
		getq_rslt.status = Q_OK;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_active = TRUE;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_bsize = bsize;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_bhardlimit =
			CLAMP_MAX_32(dqblk.dqb_bhardlimit / bsize);
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_bsoftlimit =
			CLAMP_MAX_32(dqblk.dqb_bsoftlimit / bsize);
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_curblocks =
			CLAMP_MAX_32(dqblk.dqb_curbytes / bsize);
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_fhardlimit =
			CLAMP_MAX_32(dqblk.dqb_ihardlimit);
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_fsoftlimit =
			CLAMP_MAX_32(dqblk.dqb_isoftlimit);
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_curfiles =
			CLAMP_MAX_32(dqblk.dqb_curinodes);
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_btimeleft =
		    dqblk.dqb_btime - timev.tv_sec;
		getq_rslt.getquota_rslt_u.gqr_rquota.rq_ftimeleft =
		    dqblk.dqb_itime - timev.tv_sec;
	}
	if (!svc_sendreply(transp, (xdrproc_t)xdr_getquota_rslt, &getq_rslt))
		svcerr_systemerr(transp);
	if (!svc_freeargs(transp, (xdrproc_t)xdr_getquota_args, &getq_args)) {
		syslog(LOG_ERR, "unable to free arguments");
		exit(1);
	}
}

/*
 * gets the quotas for id, filesystem path.
 * Return 0 if fail, 1 otherwise
 */
int
getfsquota(int type, int id, char *path, struct dqblk *dqblk)
{
	struct stat st_path;
	struct fsq_stat *fs;
	int	qcmd, fd, ret = 0;
	char *filename;

	qcmd = QCMD(Q_GETQUOTA, type == RQUOTA_USRQUOTA ? USRQUOTA : GRPQUOTA);

	lock_fsq();

	/* first, ask for quota directly */
	if (quotactl(path, qcmd, id, (char*)dqblk) == 0) {
		ret = 1;
		goto out;
	}

	/* otherwise, search/check manually */
	if (stat(path, &st_path) < 0) {
		ret = 0;
		goto out;
	}

	TAILQ_FOREACH(fs, &fsqhead, chain) {
		/* where the device is the same as path */
		if (st_path.st_dev < fs->st_dev) {
			ret = 0;
			goto out;
		}
		if (fs->st_dev != st_path.st_dev)
			continue;

		filename = (type == RQUOTA_USRQUOTA) ?
		    fs->uqfpathname : fs->gqfpathname;
		if (filename == NULL) {
			ret = 0;
			goto out;
		}
		if ((fd = open(filename, O_RDONLY)) < 0) {
			syslog(LOG_WARNING, "open error: %s: %m", filename);
			ret = 0;
			goto out;
		}
		if (lseek(fd, (off_t)(id * sizeof(struct dqblk)), SEEK_SET)
		    == (off_t)-1) {
			close(fd);
			ret = 0;
			goto out;
		}
		switch (read(fd, dqblk, sizeof(struct dqblk))) {
		case 0:
			/*
                         * Convert implicit 0 quota (EOF)
                         * into an explicit one (zero'ed dqblk)
                         */
			memset((caddr_t) dqblk, 0, sizeof(struct dqblk));
			ret = 1;
			break;
		case sizeof(struct dqblk):	/* OK */
			ret = 1;
			break;
		default:	/* ERROR */
			syslog(LOG_WARNING, "read error: %s: %m", filename);
			close(fd);
			ret = 0;
			goto out;
		}
		close(fd);
	}
out:
	unlock_fsq();
	return (ret);
}

/*
 * Check to see if a particular quota is to be enabled.
 * Comes from quota.c, NetBSD 0.9
 */
int
hasquota(struct statfs *fst, char **uqfnamep, char **gqfnamep)
{
	static char buf[MAXPATHLEN], ubuf[MAXPATHLEN], gbuf[MAXPATHLEN];
	struct stat sb;
	int qcnt = 0;

	/*
	  From quota.c:
	  We only support the default path to the
	  on disk quota files.
	*/

	snprintf(buf, sizeof(buf), "%s/%s.%s", fst->f_mntonname, QUOTAOPSNAME, qfextension[USRQUOTA] );
	if (stat(buf, &sb) == 0) {
		snprintf(ubuf, sizeof(ubuf), "%s/%s.%s", fst->f_mntonname, QUOTAFILENAME, qfextension[USRQUOTA]);
		*uqfnamep = ubuf;
		qcnt++;
	}
	snprintf(buf, sizeof(buf), "%s/%s.%s", fst->f_mntonname, QUOTAOPSNAME, qfextension[GRPQUOTA] );
	if (stat(buf, &sb) == 0) {
		snprintf(gbuf, sizeof(gbuf), "%s/%s.%s", fst->f_mntonname, QUOTAFILENAME, qfextension[GRPQUOTA]);
		*gqfnamep = gbuf;
		qcnt++;
	}
	return (qcnt);
}

/* functions for locking/unlocking the file system quota list */
void
lock_fsq(void)
{
	int error = pthread_mutex_lock(&fsq_mutex);
	if (error)
		syslog(LOG_ERR, "mutex lock failed: %s (%d)", strerror(error), error);
}
void
unlock_fsq(void)
{
	int error = pthread_mutex_unlock(&fsq_mutex);
	if (error)
		syslog(LOG_ERR, "mutex unlock failed: %s (%d)", strerror(error), error);
}

/* functions for adding/deleting entries from the file system quota list */
static void
fsadd(struct statfs *fst)
{
	struct fsq_stat *fs = NULL, *fs2;
	char *uqfpathname = NULL, *gqfpathname = NULL;
	struct stat st;

	if (strcmp(fst->f_fstypename, "hfs") &&
	    strcmp(fst->f_fstypename, "ufs"))
		return;
	if (!hasquota(fst, &uqfpathname, &gqfpathname))
		return;

	fs = (struct fsq_stat *) malloc(sizeof(*fs));
	if (fs == NULL) {
		syslog(LOG_ERR, "can't malloc: %m");
		return;
	}
	bzero(fs, sizeof(*fs));

	fs->mountdir = strdup(fst->f_mntonname);
	if (fs->mountdir == NULL) {
		syslog(LOG_ERR, "can't strdup: %m");
		goto failed;
	}

	if (uqfpathname) {
		fs->uqfpathname = strdup(uqfpathname);
		if (fs->uqfpathname == NULL) {
			syslog(LOG_ERR, "can't strdup: %m");
			goto failed;
		}
	}
	if (gqfpathname) {
		fs->gqfpathname = strdup(gqfpathname);
		if (fs->gqfpathname == NULL) {
			syslog(LOG_ERR, "can't strdup: %m");
			goto failed;
		}
	}
	if (stat(fst->f_mntonname, &st))
		goto failed;
	fs->st_dev = st.st_dev;
	fs->fsid.val[0] = fst->f_fsid.val[0];
	fs->fsid.val[1] = fst->f_fsid.val[1];

	/* insert it into the list by st_dev order */
	TAILQ_FOREACH(fs2, &fsqhead, chain) {
		if (fs->st_dev < fs2->st_dev)
			break;
	}
	if (fs2)
		TAILQ_INSERT_BEFORE(fs2, fs, chain);
	else
		TAILQ_INSERT_TAIL(&fsqhead, fs, chain);

	return;
failed:
	if (fs->gqfpathname)
		free(fs->gqfpathname);
	if (fs->uqfpathname)
		free(fs->uqfpathname);
	if (fs->mountdir)
		free(fs->mountdir);
	free(fs);
	return;
}
static void
fsdel(struct statfs *fst)
{
	struct fsq_stat *fs;

	TAILQ_FOREACH(fs, &fsqhead, chain) {
		if ((fs->fsid.val[0] != fst->f_fsid.val[0]) ||
		    (fs->fsid.val[1] != fst->f_fsid.val[1]))
		    	continue;
		if (strcmp(fs->mountdir, fst->f_mntonname))
			continue;
		break;
	}
	if (!fs)
		return;
	TAILQ_REMOVE(&fsqhead, fs, chain);
	if (fs->gqfpathname)
		free(fs->gqfpathname);
	if (fs->uqfpathname)
		free(fs->uqfpathname);
	if (fs->mountdir)
		free(fs->mountdir);
	free(fs);
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

void
check_mounts(void)
{
	int i, j, cmp;

	lock_fsq();
	get_mounts();

	for (i=j=0; (i < cnt[PREV]) && (j < cnt[cur]); ) {
		cmp = sfscmp(&sfs[PREV][i], &sfs[cur][j]);
		if (!cmp) {
			i++;
			j++;
			continue;
		}
		if (cmp < 0) {
			/* file system no longer mounted */
			DEBUG("- %s\n", sfs[PREV][i].f_mntonname);
			fsdel(&sfs[PREV][i]);
			i++;
		}
		if (cmp > 0) {
			/* file system mounted */
			DEBUG("+ %s\n", sfs[cur][j].f_mntonname);
			fsadd(&sfs[cur][j]);
			j++;
		}
	}
	while (i < cnt[PREV]) {
		/* file system no longer mounted */
		DEBUG("- %s\n", sfs[PREV][i].f_mntonname);
		fsdel(&sfs[PREV][i]);
		i++;
	}
	while (j < cnt[cur]) {
		/* file system mounted */
		DEBUG("+ %s\n", sfs[cur][j].f_mntonname);
		fsadd(&sfs[cur][j]);
		j++;
	}

	unlock_fsq();
}

/*
 * read the NFS server values from nfs.conf
 */
int
config_read(struct nfs_conf_server *conf)
{
	FILE *f;
	size_t len, linenum = 0;
	char *line, *p, *key, *value;
	long val;

	if (!(f = fopen(_PATH_NFS_CONF, "r"))) {
		if (errno != ENOENT)
			syslog(LOG_WARNING, "%s", _PATH_NFS_CONF);
		return (1);
	}

	for (;(line = fparseln(f, &len, &linenum, NULL, 0)); free(line)) {
		if (len <= 0)
			continue;
		/* trim trailing whitespace */
		p = line + len - 1;
		while ((p > line) && isspace(*p))
			*p-- = '\0';
		/* find key start */
		key = line;
		while (isspace(*key))
			key++;
		/* find equals/value */
		value = p = strchr(line, '=');
		if (p) /* trim trailing whitespace on key */
			do { *p-- = '\0'; } while ((p > line) && isspace(*p));
		/* find value start */
		if (value)
			do { value++; } while (isspace(*value));

		/* all server keys start with "nfs.server." */
		if (strncmp(key, "nfs.server.", 11)) {
			DEBUG("%4ld %s=%s\n", linenum, key, value ? value : "");
			continue;
		}

		val = !value ? 1 : strtol(value, NULL, 0);
		DEBUG("%4ld %s=%s (%d)\n", linenum, key, value ? value : "", val);

		if (!strcmp(key, "nfs.server.rquota.port")) {
			if (value && val)
				conf->rquota_port = val;
		} else if (!strcmp(key, "nfs.server.rquota.tcp")) {
			conf->tcp = val;
		} else if (!strcmp(key, "nfs.server.rquota.udp")) {
			conf->udp = val;
		} else if (!strcmp(key, "nfs.server.verbose")) {
			conf->verbose = val;
		} else {
			DEBUG("ignoring unknown config value: %4ld %s=%s\n", linenum, key, value ? value : "");
		}

	}

	fclose(f);
	return (0);
}


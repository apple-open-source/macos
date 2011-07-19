/*
 * Copyright (c) 2002-2010 Apple Inc.  All rights reserved.
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
/*-
 * Copyright (c) 1997 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Berkeley Software Design Inc's name may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN INC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      from BSDI kern.c,v 1.2 1998/11/25 22:38:27 don Exp
 * $FreeBSD: src/usr.sbin/rpc.lockd/kern.c,v 1.11 2002/08/15 21:52:21 alfred Exp $
 */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>

#include "nlm_prot.h"
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs_lock.h>
#include <nfs/nfs.h>

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>

#include "lockd_mach.h"
#include "lockd_machServer.h"

#include "lockd.h"
#include "lockd_lock.h"

#define nfslockdans(_v, _ansp)	\
	((_ansp)->la_version = (_v), \
	nfsclnt(NFSCLNT_LOCKDANS, (_ansp)))

static mach_port_t lockd_receive_right;

union MaxMsgSize {
	union __RequestUnion__lockd_mach_subsystem req;
	union __ReplyUnion__lockd_mach_subsystem rep;
};

#define	MAX_LOCKD_MSG_SIZE	(sizeof (union MaxMsgSize) + MAX_TRAILER_SIZE)

#define BOOTSTRAP_NAME "com.apple.lockd"

/* Lock request owner. */
typedef struct __owner {
	pid_t	 pid;				/* Process ID. */
	time_t	 tod;				/* Time-of-day. */
} OWNER;
static OWNER owner;

static char hostname[MAXHOSTNAMELEN + 1];	/* Hostname. */

static time_t shutdown_time_client = 0;
static time_t shutdown_time_server = 0;

static void	set_auth(CLIENT *cl, struct xucred *ucred);
int	lock_request(LOCKD_MSG *);
int	cancel_request(LOCKD_MSG *);
int	test_request(LOCKD_MSG *);
void	show(LOCKD_MSG *);
int	unlock_request(LOCKD_MSG *);

#define d_calls (config.verbose > 1)
#define d_args (config.verbose > 2)

static const char *
from_addr(saddr)
	struct sockaddr *saddr;
{
	static char inet_buf[INET6_ADDRSTRLEN];

	if (getnameinfo(saddr, saddr->sa_len, inet_buf, sizeof(inet_buf),
			NULL, 0, NI_NUMERICHOST) == 0)
		return inet_buf;
	return "???";
}

/*
 * Use sysctl to get NFS client and NFS server state from kernel.
 */
static int
get_client_and_server_state(int *mounts, int *servers, int *maxservers)
{
	size_t size = sizeof(int);
	int rv = 0;

	*mounts = *servers = *maxservers = 0;
	if (sysctlbyname("vfs.generic.nfs.client.lockd_mounts", mounts, &size, NULL, 0))
		rv++;
	if (sysctlbyname("vfs.generic.nfs.server.nfsd_thread_count", servers, &size, NULL, 0))
		rv++;
	if (sysctlbyname("vfs.generic.nfs.server.nfsd_thread_max", maxservers, &size, NULL, 0))
		rv++;

	return (rv);
}

/*
 * shutdown timeout handler
 *
 * Double check and shut down.
 */
static void
shutdown_timer(void)
{
	if (!shutdown_time_client || !shutdown_time_server) {
		/* don't shut down yet, something's still running */
		if (config.verbose) {
			int mounts, servers, maxservers;
			get_client_and_server_state(&mounts, &servers, &maxservers);
			syslog(LOG_DEBUG, "shutdown_timer: %d %d, mounts %d servers %d %d\n",
				shutdown_time_client, shutdown_time_server,
				mounts, servers, maxservers);
		}
		return;
	}

	/* shut down statd too */
	statd_stop();

	handle_sig_cleanup(0);
	/*NOTREACHED*/
}

kern_return_t
svc_lockd_request(
	mach_port_t mp __attribute__((unused)),
	uint32_t vers,
	uint32_t flags,
	uint64_t xid,
	int64_t  flk_start,
	int64_t  flk_len,
	int32_t  flk_pid,
	int32_t  flk_type,
	int32_t  flk_whence,
	uint32_t *sock_address,
	uint32_t *cred,
	uint32_t fh_len,
	uint8_t  *fh)
{
	LOCKD_MSG msg;
	int ret;

	/* make sure shutdown timer is disabled */
	if (shutdown_time_client) {
		shutdown_time_client = 0;
		alarm(0);
	}

	/*
	 * Hold off getting hostname until first
	 * lock request. Otherwise we risk getting
	 * an initial ".local" name.
	 */
	if (hostname[0] == '\0')
		(void)gethostname(hostname, sizeof(hostname) - 1);

	/* Marshal mach arguments back into a LOCKD_MSG */
	msg.lm_version = vers;
	msg.lm_flags = flags;
	msg.lm_xid = xid;
	msg.lm_fl.l_start = flk_start;
	msg.lm_fl.l_len = flk_len;
	msg.lm_fl.l_pid = flk_pid;
	msg.lm_fl.l_type = flk_type;
	msg.lm_fl.l_whence = flk_whence;
	msg.lm_fh_len = fh_len;
	bcopy(sock_address, &msg.lm_addr, sizeof(msg.lm_addr));
	bcopy(cred, &msg.lm_cred, sizeof(struct xucred));
	bcopy(fh, msg.lm_fh, NFSV3_MAX_FH_SIZE);

	if (d_args)
		show(&msg);

	if (msg.lm_version != LOCKD_MSG_VERSION) {
		syslog(LOG_ERR,	"unknown msg type: %d", msg.lm_version);
	}
	/*
	 * Send it to the NLM server and don't grant the lock
	 * if we fail for any reason.
	 */
	 switch (msg.lm_fl.l_type) {
	 case F_RDLCK:
	 case F_WRLCK:
		 if (msg.lm_flags & LOCKD_MSG_TEST)
			 ret = test_request(&msg);
		 else if (msg.lm_flags & LOCKD_MSG_CANCEL)
			 ret = cancel_request(&msg);
		 else
			 ret = lock_request(&msg);
		 break;
	 case F_UNLCK:
		 ret = unlock_request(&msg);
		 break;
	 default:
		 ret = 1;
		 syslog(LOG_ERR,	 "unknown lock type: %d", msg.lm_fl.l_type);
		 break;
	 }
	if (ret) {
		struct lockd_ans ans;

		bzero(&ans, sizeof(ans));
		ans.la_xid = msg.lm_xid;
		ans.la_errno = ENOTSUP;

		if (nfslockdans(LOCKD_ANS_VERSION, &ans)) {
			syslog(LOG_DEBUG, "process %d: %m", msg.lm_fl.l_pid);
		}
	}
	return (KERN_SUCCESS);
}

/*
 * nfsd is pinging us to let us know that it's running.
 */
kern_return_t
svc_lockd_ping(mach_port_t mp __attribute__((unused)))
{
	/* make sure shutdown timer is disabled */
	if (shutdown_time_server) {
		shutdown_time_server = 0;
		alarm(0);
	}

	return (KERN_SUCCESS);
}

/*
 * This request is called whenever either nfsd shuts down or the
 * last (lockd-needing) NFS mount is unmounted.
 *
 * We take note of our current state and prepare to shutdown.
 */
kern_return_t
svc_lockd_shutdown(mach_port_t mp __attribute__((unused)))
{
	int mounts, servers, maxservers;
	struct timeval now;
	time_t shutdown_time;
	unsigned int delay;

	if (get_client_and_server_state(&mounts, &servers, &maxservers)) {
		syslog(LOG_ERR, "lockd_shutdown: sysctl failed");
		return (KERN_FAILURE);
	}

	gettimeofday(&now, NULL);

	if ((!servers || !maxservers) && !shutdown_time_server) {
		/* nfsd is no longer running, set server shutdown time */
		syslog(LOG_DEBUG, "lockd_shutdown: server, delay %d", config.shutdown_delay_server);
		shutdown_time_server = now.tv_sec + config.shutdown_delay_server;
	}
	if (!mounts && !shutdown_time_client) {
		/* must have just unmounted last mount, set client shutdown time */
		syslog(LOG_DEBUG, "lockd_shutdown: client, delay %d", config.shutdown_delay_client);
		shutdown_time_client = now.tv_sec + config.shutdown_delay_client;
	}

	if (!shutdown_time_client || !shutdown_time_server) {
		syslog(LOG_DEBUG, "lockd_shutdown: hold on, client %d server %d", shutdown_time_client, shutdown_time_server);
		/*
		 * Either the client or server is still
		 * running, so we don't want to shut down yet.
		 */
		alarm(0);
		return (KERN_SUCCESS);
	}

	/* figure out when the timer should go off */
	shutdown_time = MAX(shutdown_time_client, shutdown_time_server);
	delay = shutdown_time - now.tv_sec;
	syslog(LOG_DEBUG, "lockd_shutdown: arm timer, delay %d", delay);

	/* arm the timer */
	alarm(delay);

	return (KERN_SUCCESS);
}

static void *
client_request_thread(void *arg __attribute__((unused)))
{
	mach_msg_server(lockd_mach_server,  MAX_LOCKD_MSG_SIZE,
			lockd_receive_right,
			MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT) |
			MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0));

	/* Should never return */
	return (NULL);
}

void
client_mach_request(void)
{
#if 0 /* Currently rpc library is MT hostile, so we can't do this :( */
	pthread_t request_thr;
	pthread_attr_t attr[1];
#endif
	struct sigaction sigalarm;
	kern_return_t kr;
	int mounts, servers, maxservers;
	struct timeval now;
	time_t shutdown_time;
	unsigned int delay;

	/*
	 * Check in with launchd to get the receive right.
	 * N.B. Since we're using a host special port, if launchd
	 * does not have the receive right we can't get it.
	 * And since we should always be started by launchd
	 * this should always succeed.
	 */
	kr = bootstrap_check_in(bootstrap_port,
			BOOTSTRAP_NAME, &lockd_receive_right);
	if (kr != BOOTSTRAP_SUCCESS) {
		syslog(LOG_ERR, "Could not checkin for receive right %s\n",
			bootstrap_strerror(kr));
		return;
	}

	/* Setup. */
	(void)time(&owner.tod);
	owner.pid = getpid();

	/* set up shutdown timer handler */
	sigalarm.sa_handler = (sig_t) shutdown_timer;
	sigemptyset(&sigalarm.sa_mask);
	sigalarm.sa_flags = SA_RESTART;
	if (sigaction(SIGALRM, &sigalarm, NULL) != 0) {
		syslog(LOG_WARNING, "sigaction(SIGALRM) failed: %s",
		    strerror(errno));
	}

	/* Check the current status of the NFS server and NFS mounts */
	gettimeofday(&now, NULL);
	mounts = servers = maxservers = 0;
	if (get_client_and_server_state(&mounts, &servers, &maxservers))
		syslog(LOG_ERR, "lockd setup: sysctl failed");

	if (!servers || !maxservers) {
		/* nfsd is not running, set server shutdown time */
		shutdown_time_server = now.tv_sec + config.shutdown_delay_server;
	} else {
		shutdown_time_server = 0;
	}
	if (!mounts) {
		/* no NFS mounts, set client shutdown time */
		shutdown_time_client = now.tv_sec + config.shutdown_delay_client;
	} else {
		shutdown_time_client = 0;
	}

	if (shutdown_time_client && shutdown_time_server) {
		/* No server and no mounts, so plan for shutdown. */
		/* Figure out when the timer should go off. */
		shutdown_time = MAX(shutdown_time_client, shutdown_time_server);
		delay = shutdown_time - now.tv_sec;
		syslog(LOG_DEBUG, "lockd setup: no client or server, arm timer, delay %d", delay);
		/* arm the timer */
		alarm(delay);
	}

#if 0
	pthread_attr_init(attr);
	(void) pthread_attr_setdetachstate(attr, PTHREAD_CREATE_DETACHED);

	error = pthread_create(&request_thr, attr, client_request_thread, NULL);
	if (error) {
		syslog(LOG_ERR, "unable to create request thread: %s",
			strerror(error));
		return;
	}
#endif

	(void) client_request_thread(NULL);
}

void
set_auth(cl, xucred)
	CLIENT *cl;
	struct xucred *xucred;
{
        if (cl->cl_auth != NULL)
                cl->cl_auth->ah_ops->ah_destroy(cl->cl_auth);
        cl->cl_auth = authunix_create(hostname,
                        xucred->cr_uid,
                        xucred->cr_groups[0],
                        xucred->cr_ngroups - 1,
                        (int *)&xucred->cr_groups[1]);
}


/*
 * test_request --
 *	Convert a lock LOCKD_MSG into an NLM request, and send it off.
 */
int
test_request(LOCKD_MSG *msg)
{
	CLIENT *cli;
	struct timeval timeout = {0, 0};	/* No timeout, no response. */
	char dummy;

	if (d_calls)
		syslog(LOG_DEBUG, "test request: %s: %s to %s",
		    (msg->lm_flags & LOCKD_MSG_NFSV3) ? "V4" : "V1/3",
		    msg->lm_fl.l_type == F_WRLCK ? "write" : "read",
		    from_addr((struct sockaddr *)&msg->lm_addr));

	if (msg->lm_flags & LOCKD_MSG_NFSV3) {
		struct nlm4_testargs arg4;

		arg4.cookie.n_bytes = (char *)&msg->lm_xid;
		arg4.cookie.n_len = sizeof(msg->lm_xid);
		arg4.exclusive = msg->lm_fl.l_type == F_WRLCK ? 1 : 0;
		arg4.alock.caller_name = hostname;
		arg4.alock.fh.n_bytes = (char *)&msg->lm_fh;
		arg4.alock.fh.n_len = msg->lm_fh_len;
		arg4.alock.oh.n_bytes = (char *)&owner;
		arg4.alock.oh.n_len = sizeof(owner);
		arg4.alock.svid = msg->lm_fl.l_pid;
		arg4.alock.l_offset = msg->lm_fl.l_start;
		arg4.alock.l_len = msg->lm_fl.l_len;

		if ((cli = get_client((struct sockaddr *)&msg->lm_addr, NLM_VERS4, 1, (msg->lm_flags & LOCKD_MSG_TCP))) == NULL)
			return (1);

		set_auth(cli, &msg->lm_cred);
		(void)clnt_call(cli, NLM4_TEST_MSG,
		    (xdrproc_t)xdr_nlm4_testargs, &arg4, (xdrproc_t)xdr_void, &dummy, timeout);
	} else {
		struct nlm_testargs arg;

		arg.cookie.n_bytes = (char *)&msg->lm_xid;
		arg.cookie.n_len = sizeof(msg->lm_xid);
		arg.exclusive = msg->lm_fl.l_type == F_WRLCK ? 1 : 0;
		arg.alock.caller_name = hostname;
		arg.alock.fh.n_bytes = (char *)&msg->lm_fh;
		arg.alock.fh.n_len = msg->lm_fh_len;
		arg.alock.oh.n_bytes = (char *)&owner;
		arg.alock.oh.n_len = sizeof(owner);
		arg.alock.svid = msg->lm_fl.l_pid;
		arg.alock.l_offset = msg->lm_fl.l_start;
		arg.alock.l_len = msg->lm_fl.l_len;

		if ((cli = get_client((struct sockaddr *)&msg->lm_addr, NLM_VERS, 1, (msg->lm_flags & LOCKD_MSG_TCP))) == NULL)
			return (1);

		set_auth(cli, &msg->lm_cred);
		(void)clnt_call(cli, NLM_TEST_MSG,
		    (xdrproc_t)xdr_nlm_testargs, &arg, (xdrproc_t)xdr_void, &dummy, timeout);
	}
	return (0);
}

/*
 * lock_request --
 *	Convert a lock LOCKD_MSG into an NLM request, and send it off.
 */
int
lock_request(LOCKD_MSG *msg)
{
	CLIENT *cli;
	struct nlm4_lockargs arg4;
	struct nlm_lockargs arg;
	struct timeval timeout = {0, 0};	/* No timeout, no response. */
	char dummy;

	if (d_calls)
		syslog(LOG_DEBUG, "lock request: %s %s: %s to %s",
		    (msg->lm_flags & LOCKD_MSG_RECLAIM) ? "RECLAIM" : "",
		    (msg->lm_flags & LOCKD_MSG_NFSV3) ? "V4" : "V1/3",
		    msg->lm_fl.l_type == F_WRLCK ? "write" : "read",
		    from_addr((struct sockaddr *)&msg->lm_addr));

	monitor_lock_host_by_addr((struct sockaddr *)&msg->lm_addr);

	if (msg->lm_flags & LOCKD_MSG_NFSV3) {
		arg4.cookie.n_bytes = (char *)&msg->lm_xid;
		arg4.cookie.n_len = sizeof(msg->lm_xid);
		arg4.block = (msg->lm_flags & LOCKD_MSG_BLOCK) ? 1 : 0;
		arg4.exclusive = msg->lm_fl.l_type == F_WRLCK ? 1 : 0;
		arg4.alock.caller_name = hostname;
		arg4.alock.fh.n_bytes = (char *)&msg->lm_fh;
		arg4.alock.fh.n_len = msg->lm_fh_len;
		arg4.alock.oh.n_bytes = (char *)&owner;
		arg4.alock.oh.n_len = sizeof(owner);
		arg4.alock.svid = msg->lm_fl.l_pid;
		arg4.alock.l_offset = msg->lm_fl.l_start;
		arg4.alock.l_len = msg->lm_fl.l_len;
		arg4.reclaim = (msg->lm_flags & LOCKD_MSG_RECLAIM) ? 1 : 0;
		arg4.state = nsm_state;

		if ((cli = get_client((struct sockaddr *)&msg->lm_addr, NLM_VERS4, 1+arg4.reclaim, (msg->lm_flags & LOCKD_MSG_TCP))) == NULL)
			return (1);

		set_auth(cli, &msg->lm_cred);
		(void)clnt_call(cli, NLM4_LOCK_MSG,
		    (xdrproc_t)xdr_nlm4_lockargs, &arg4, (xdrproc_t)xdr_void, &dummy, timeout);
	} else {
		arg.cookie.n_bytes = (char *)&msg->lm_xid;
		arg.cookie.n_len = sizeof(msg->lm_xid);
		arg.block = (msg->lm_flags & LOCKD_MSG_BLOCK) ? 1 : 0;
		arg.exclusive = msg->lm_fl.l_type == F_WRLCK ? 1 : 0;
		arg.alock.caller_name = hostname;
		arg.alock.fh.n_bytes = (char *)&msg->lm_fh;
		arg.alock.fh.n_len = msg->lm_fh_len;
		arg.alock.oh.n_bytes = (char *)&owner;
		arg.alock.oh.n_len = sizeof(owner);
		arg.alock.svid = msg->lm_fl.l_pid;
		arg.alock.l_offset = msg->lm_fl.l_start;
		arg.alock.l_len = msg->lm_fl.l_len;
		arg.reclaim = (msg->lm_flags & LOCKD_MSG_RECLAIM) ? 1 : 0;
		arg.state = nsm_state;

		if ((cli = get_client((struct sockaddr *)&msg->lm_addr, NLM_VERS, 1+arg.reclaim, (msg->lm_flags & LOCKD_MSG_TCP))) == NULL)
			return (1);

		set_auth(cli, &msg->lm_cred);
		(void)clnt_call(cli, NLM_LOCK_MSG,
		    (xdrproc_t)xdr_nlm_lockargs, &arg, (xdrproc_t)xdr_void, &dummy, timeout);
	}
	return (0);
}

/*
 * cancel_request --
 *	Convert a lock LOCKD_MSG into an NLM request, and send it off.
 */
int
cancel_request(LOCKD_MSG *msg)
{
	CLIENT *cli;
	struct nlm4_cancargs arg4;
	struct nlm_cancargs arg;
	struct timeval timeout = {0, 0};	/* No timeout, no response. */
	char dummy;

	if (d_calls)
		syslog(LOG_DEBUG, "cancel request: %s: %s to %s",
		    (msg->lm_flags & LOCKD_MSG_NFSV3) ? "V4" : "V1/3",
		    msg->lm_fl.l_type == F_WRLCK ? "write" : "read",
		    from_addr((struct sockaddr *)&msg->lm_addr));

	if (msg->lm_flags & LOCKD_MSG_NFSV3) {
		arg4.cookie.n_bytes = (char *)&msg->lm_xid;
		arg4.cookie.n_len = sizeof(msg->lm_xid);
		arg4.block = (msg->lm_flags & LOCKD_MSG_BLOCK) ? 1 : 0;
		arg4.exclusive = msg->lm_fl.l_type == F_WRLCK ? 1 : 0;
		arg4.alock.caller_name = hostname;
		arg4.alock.fh.n_bytes = (char *)&msg->lm_fh;
		arg4.alock.fh.n_len = msg->lm_fh_len;
		arg4.alock.oh.n_bytes = (char *)&owner;
		arg4.alock.oh.n_len = sizeof(owner);
		arg4.alock.svid = msg->lm_fl.l_pid;
		arg4.alock.l_offset = msg->lm_fl.l_start;
		arg4.alock.l_len = msg->lm_fl.l_len;

		if ((cli = get_client((struct sockaddr *)&msg->lm_addr, NLM_VERS4, 1, (msg->lm_flags & LOCKD_MSG_TCP))) == NULL)
			return (1);

		set_auth(cli, &msg->lm_cred);
		(void)clnt_call(cli, NLM4_CANCEL_MSG,
		    (xdrproc_t)xdr_nlm4_cancargs, &arg4, (xdrproc_t)xdr_void, &dummy, timeout);
	} else {
		arg.cookie.n_bytes = (char *)&msg->lm_xid;
		arg.cookie.n_len = sizeof(msg->lm_xid);
		arg.block = (msg->lm_flags & LOCKD_MSG_BLOCK) ? 1 : 0;
		arg.exclusive = msg->lm_fl.l_type == F_WRLCK ? 1 : 0;
		arg.alock.caller_name = hostname;
		arg.alock.fh.n_bytes = (char *)&msg->lm_fh;
		arg.alock.fh.n_len = msg->lm_fh_len;
		arg.alock.oh.n_bytes = (char *)&owner;
		arg.alock.oh.n_len = sizeof(owner);
		arg.alock.svid = msg->lm_fl.l_pid;
		arg.alock.l_offset = msg->lm_fl.l_start;
		arg.alock.l_len = msg->lm_fl.l_len;

		if ((cli = get_client((struct sockaddr *)&msg->lm_addr, NLM_VERS, 1, (msg->lm_flags & LOCKD_MSG_TCP))) == NULL)
			return (1);

		set_auth(cli, &msg->lm_cred);
		(void)clnt_call(cli, NLM_CANCEL_MSG,
		    (xdrproc_t)xdr_nlm_cancargs, &arg, (xdrproc_t)xdr_void, &dummy, timeout);
	}
	return (0);
}

/*
 * unlock_request --
 *	Convert an unlock LOCKD_MSG into an NLM request, and send it off.
 */
int
unlock_request(LOCKD_MSG *msg)
{
	CLIENT *cli;
	struct nlm4_unlockargs arg4;
	struct nlm_unlockargs arg;
	struct timeval timeout = {0, 0};	/* No timeout, no response. */
	char dummy;

	if (d_calls)
		syslog(LOG_DEBUG, "unlock request: %s: to %s",
		    (msg->lm_flags & LOCKD_MSG_NFSV3) ? "V4" : "V1/3",
		    from_addr((struct sockaddr *)&msg->lm_addr));

	if (msg->lm_flags & LOCKD_MSG_NFSV3) {
		arg4.cookie.n_bytes = (char *)&msg->lm_xid;
		arg4.cookie.n_len = sizeof(msg->lm_xid);
		arg4.alock.caller_name = hostname;
		arg4.alock.fh.n_bytes = (char *)&msg->lm_fh;
		arg4.alock.fh.n_len = msg->lm_fh_len;
		arg4.alock.oh.n_bytes = (char *)&owner;
		arg4.alock.oh.n_len = sizeof(owner);
		arg4.alock.svid = msg->lm_fl.l_pid;
		arg4.alock.l_offset = msg->lm_fl.l_start;
		arg4.alock.l_len = msg->lm_fl.l_len;

		if ((cli = get_client((struct sockaddr *)&msg->lm_addr, NLM_VERS4, 1, (msg->lm_flags & LOCKD_MSG_TCP))) == NULL)
			return (1);

		set_auth(cli, &msg->lm_cred);
		(void)clnt_call(cli, NLM4_UNLOCK_MSG,
		    (xdrproc_t)xdr_nlm4_unlockargs, &arg4, (xdrproc_t)xdr_void, &dummy, timeout);
	} else {
		arg.cookie.n_bytes = (char *)&msg->lm_xid;
		arg.cookie.n_len = sizeof(msg->lm_xid);
		arg.alock.caller_name = hostname;
		arg.alock.fh.n_bytes = (char *)&msg->lm_fh;
		arg.alock.fh.n_len = msg->lm_fh_len;
		arg.alock.oh.n_bytes = (char *)&owner;
		arg.alock.oh.n_len = sizeof(owner);
		arg.alock.svid = msg->lm_fl.l_pid;
		arg.alock.l_offset = msg->lm_fl.l_start;
		arg.alock.l_len = msg->lm_fl.l_len;

		if ((cli = get_client((struct sockaddr *)&msg->lm_addr, NLM_VERS, 1, (msg->lm_flags & LOCKD_MSG_TCP))) == NULL)
			return (1);

		set_auth(cli, &msg->lm_cred);
		(void)clnt_call(cli, NLM_UNLOCK_MSG,
		    (xdrproc_t)xdr_nlm_unlockargs, &arg, (xdrproc_t)xdr_void, &dummy, timeout);
	}

	return (0);
}

int
lock_answer(int version, netobj *netcookie, nlm4_lock *lock, int flags, int result)
{
	struct lockd_ans ans;

	ans.la_flags = 0;
	ans.la_pid = 0;

	if (flags & LOCK_ANSWER_GRANTED)
		ans.la_flags |= LOCKD_ANS_GRANTED;

	if (netcookie->n_len != sizeof(ans.la_xid)) {
		if (lock == NULL) {	/* we're screwed */
			syslog(LOG_ERR, "inedible nlm cookie");
			return -1;
		}
		/* no/bad cookie - need to copy lock info to identify request */
		ans.la_xid = 0;
		/* copy lock info */
		ans.la_fh_len = lock->fh.n_len;
		if (!lock->fh.n_len || (lock->fh.n_len > NFS_SMALLFH)) {
			syslog(LOG_ERR, "bogus filehandle size %d in answer", lock->fh.n_len);
			return -1;
		}
		memcpy(ans.la_fh, lock->fh.n_bytes, ans.la_fh_len);
		ans.la_pid = lock->svid;
		ans.la_start = lock->l_offset;
		ans.la_len = lock->l_len;
		ans.la_flags |= LOCKD_ANS_LOCK_INFO;
		if (flags & LOCK_ANSWER_LOCK_EXCL)
			ans.la_flags |= LOCKD_ANS_LOCK_EXCL;
	} else {
		memcpy(&ans.la_xid, netcookie->n_bytes, sizeof(ans.la_xid));
		ans.la_fh_len = 0;
	}

	if (d_calls)
		syslog(LOG_DEBUG, "lock answer: pid %d: %s %d", ans.la_pid,
		    version == NLM_VERS4 ? "nlmv4" : "nlmv3", result);

	if (version == NLM_VERS4)
		switch (result) {
		case nlm4_granted:
			ans.la_errno = 0;
			if ((flags & LOCK_ANSWER_GRANTED) && lock &&
			    !(ans.la_flags & LOCKD_ANS_LOCK_INFO)) {
				/* copy lock info */
				ans.la_fh_len = lock->fh.n_len;
				if (!lock->fh.n_len || (lock->fh.n_len > NFS_SMALLFH)) {
					syslog(LOG_ERR, "bogus filehandle size %d in answer", lock->fh.n_len);
					return -1;
				}
				memcpy(ans.la_fh, lock->fh.n_bytes, ans.la_fh_len);
				ans.la_pid = lock->svid;
				ans.la_start = lock->l_offset;
				ans.la_len = lock->l_len;
				ans.la_flags |= LOCKD_ANS_LOCK_INFO;
				if (flags & LOCK_ANSWER_LOCK_EXCL)
					ans.la_flags |= LOCKD_ANS_LOCK_EXCL;
			}
			break;
		default:
			ans.la_errno = EACCES;
			break;
		case nlm4_denied:
			if (lock == NULL)
				ans.la_errno = EAGAIN;
			else {
				/* this is an answer to a nlm_test msg */
				ans.la_pid = lock->svid;
				ans.la_start = lock->l_offset;
				ans.la_len = lock->l_len;
				ans.la_flags |= LOCKD_ANS_LOCK_INFO;
				if (flags & LOCK_ANSWER_LOCK_EXCL)
					ans.la_flags |= LOCKD_ANS_LOCK_EXCL;
				ans.la_errno = 0;
			}
			break;
		case nlm4_denied_nolocks:
			ans.la_errno = ENOLCK;
			break;
		case nlm4_blocked:
			ans.la_errno = EINPROGRESS;
			break;
		case nlm4_denied_grace_period:
			ans.la_errno = EAGAIN;
			ans.la_flags |= LOCKD_ANS_DENIED_GRACE;
			break;
		case nlm4_deadlck:
			ans.la_errno = EDEADLK;
			break;
		case nlm4_rofs:
			ans.la_errno = EROFS;
			break;
		case nlm4_stale_fh:
			ans.la_errno = ESTALE;
			break;
		case nlm4_fbig:
			ans.la_errno = EFBIG;
			break;
		case nlm4_failed:
			ans.la_errno = EACCES;
			break;
		}
	else
		switch (result) {
		case nlm_granted:
			ans.la_errno = 0;
			if ((flags & LOCK_ANSWER_GRANTED) && lock &&
			    !(ans.la_flags & LOCKD_ANS_LOCK_INFO)) {
				/* copy lock info */
				ans.la_fh_len = lock->fh.n_len;
				if (!lock->fh.n_len || (lock->fh.n_len > NFS_SMALLFH)) {
					syslog(LOG_ERR, "bogus filehandle size %d in answer", lock->fh.n_len);
					return -1;
				}
				memcpy(ans.la_fh, lock->fh.n_bytes, ans.la_fh_len);
				ans.la_pid = lock->svid;
				ans.la_start = lock->l_offset;
				ans.la_len = lock->l_len;
				ans.la_flags |= LOCKD_ANS_LOCK_INFO;
				if (flags & LOCK_ANSWER_LOCK_EXCL)
					ans.la_flags |= LOCKD_ANS_LOCK_EXCL;
			}
			break;
		default:
			ans.la_errno = EACCES;
			break;
		case nlm_denied:
			if (lock == NULL)
				ans.la_errno = EAGAIN;
			else {
				/* this is an answer to a nlm_test msg */
				ans.la_pid = lock->svid;
				ans.la_start = lock->l_offset;
				ans.la_len = lock->l_len;
				ans.la_flags |= LOCKD_ANS_LOCK_INFO;
				if (flags & LOCK_ANSWER_LOCK_EXCL)
					ans.la_flags |= LOCKD_ANS_LOCK_EXCL;
				ans.la_errno = 0;
			}
			break;
		case nlm_denied_nolocks:
			ans.la_errno = ENOLCK;
			break;
		case nlm_blocked:
			ans.la_errno = EINPROGRESS;
			break;
		case nlm_denied_grace_period:
			ans.la_errno = EAGAIN;
			ans.la_flags |= LOCKD_ANS_DENIED_GRACE;
			break;
		case nlm_deadlck:
			ans.la_errno = EDEADLK;
			break;
		}

	if (nfslockdans(LOCKD_ANS_VERSION, &ans)) {
		syslog(LOG_DEBUG, "lock_answer(%d): process %d: %m",
			result, ans.la_pid);
		return -1;
	}
	return 0;
}

/*
 * show --
 *	Display the contents of a kernel LOCKD_MSG structure.
 */
void
show(LOCKD_MSG *mp)
{
	static char hex[] = "0123456789abcdef";
	size_t len;
	u_int8_t *p, *t, buf[NFS_SMALLFH*3+1];

	syslog(LOG_DEBUG, "process ID: %d\n", mp->lm_fl.l_pid);

	for (t = buf, p = (u_int8_t *)mp->lm_fh,
	    len = mp->lm_fh_len;
	    len > 0; ++p, --len) {
		*t++ = '\\';
		*t++ = hex[(*p & 0xf0) >> 4];
		*t++ = hex[*p & 0x0f];
	}
	*t = '\0';

	syslog(LOG_DEBUG, "fh_len %d, fh %s\n", mp->lm_fh_len, buf);

	/* Show flock structure. */
	syslog(LOG_DEBUG, "start %qu; len %qu; pid %d; type %d; whence %d\n",
	    mp->lm_fl.l_start, mp->lm_fl.l_len, mp->lm_fl.l_pid,
	    mp->lm_fl.l_type, mp->lm_fl.l_whence);

	/* Show wait flag. */
	syslog(LOG_DEBUG, "wait was %s\n", (mp->lm_flags & LOCKD_MSG_BLOCK) ? "set" : "not set");
}

/*
 * Copyright (c) 2002-2008 Apple Inc.  All rights reserved.
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
 * Copyright (c) 1995
 *	A.R. Gordon (andrew.gordon@net-tel.co.uk).  All rights reserved.
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
 *	This product includes software developed for the FreeBSD project
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ANDREW GORDON AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef lint
static const char rcsid[] = "$FreeBSD$";
#endif				/* not lint */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <search.h>
#include <rpc/rpc.h>
#include <syslog.h>
#include <vis.h>
#include <netdb.h>		/* for getaddrinfo()		 */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "statd.h"

/* sm_check_hostname -------------------------------------------------------- */
/*
 * Purpose: Check `mon_name' member of sm_name struct to ensure that the array
 * consists only of printable characters.
 *
 * Returns: TRUE if hostname is good. FALSE if hostname contains binary or
 * otherwise non-printable characters.
 *
 * Notes: Will syslog(3) to warn of corrupt hostname.
 */

int 
sm_check_hostname(struct svc_req * req, char *arg)
{
	int len, dstlen, ret;
	struct sockaddr_in *claddr;
	char *dst;

	len = strlen(arg);
	dstlen = (4 * len) + 1;
	dst = malloc(dstlen);
	claddr = svc_getcaller(req->rq_xprt);
	ret = 1;

	if (claddr == NULL || dst == NULL) {
		ret = 0;
	} else if (strvis(dst, arg, VIS_WHITE) != len) {
		log(LOG_ERR,
		    "sm_stat: client %s hostname %s contained invalid characters.",
		    inet_ntoa(claddr->sin_addr), dst);
		ret = 0;
	}
	free(dst);
	return (ret);
}

/* sm_stat_1 --------------------------------------------------------------- */
/*
   Purpose:	RPC call to enquire if a host can be monitored
   Returns:	TRUE for any hostname that can be looked up to give
		an address.
*/

struct sm_stat_res *
sm_stat_1_svc(sm_name * arg, struct svc_req * req)
{
	static sm_stat_res res;
	struct addrinfo *ai;
	struct sockaddr_in *claddr;
	static int err;

	err = 1;
	if ((err = sm_check_hostname(req, arg->mon_name)) == 0) {
		res.res_stat = stat_fail;
	}
	if (err != 0) {
		DEBUG(1, "stat called for host %s", arg->mon_name);
		if (getaddrinfo(arg->mon_name, NULL, NULL, &ai) == 0) {
			res.res_stat = stat_succ;
			freeaddrinfo(ai);
		} else {
			claddr = svc_getcaller(req->rq_xprt);
			log(LOG_ERR, "invalid hostname to sm_stat from %s: %s",
			    inet_ntoa(claddr->sin_addr), arg->mon_name);
			res.res_stat = stat_fail;
		}
	}
	res.state = ntohl(status_info->fh_state);
	return (&res);
}

/* sm_mon_1 ---------------------------------------------------------------- */
/*
   Purpose:	RPC procedure to establish a monitor request
   Returns:	Success, unless lack of resources prevents
		the necessary structures from being set up
		to record the request, or if the hostname is not
		valid (as judged by getaddrinfo())
*/

struct sm_stat_res *
sm_mon_1_svc(mon * arg, struct svc_req * req)
{
	static sm_stat_res res;
	MonitoredHost *mhp;
	Notify *np;
	struct addrinfo *ai;
	int namelen;

	res.res_stat = stat_fail;	/* Assume fail until set otherwise      */

	if (sm_check_hostname(req, arg->mon_id.mon_name) == 0)
		return (&res);

	DEBUG(1, "monitor request for host %s", arg->mon_id.mon_name);
	DEBUG(1, "recall host: %s prog: %d ver: %d proc: %d",
	      arg->mon_id.my_id.my_name, arg->mon_id.my_id.my_prog,
	      arg->mon_id.my_id.my_vers, arg->mon_id.my_id.my_proc);
	res.state = ntohl(status_info->fh_state);

	/* Find existing host entry, or create one if not found            */
	/* If find_host() fails, it will have logged the error already.    */
	if (getaddrinfo(arg->mon_id.mon_name, NULL, NULL, &ai) != 0) {
		log(LOG_ERR, "Invalid hostname to sm_mon: %s", arg->mon_id.mon_name);
		return (&res);
	}
	freeaddrinfo(ai);

	namelen = strlen(arg->mon_id.my_id.my_name);
	np = malloc(sizeof(Notify) + namelen);
	if (!np) {
		log(LOG_ERR, "Out of memory");
		return (&res);
	}
	mhp = find_host(arg->mon_id.mon_name, TRUE);
	if (!mhp) {
		free(np);
		return (&res);
	}
	strncpy(np->n_host, arg->mon_id.my_id.my_name, namelen+1);
	np->n_prog = arg->mon_id.my_id.my_prog;
	np->n_vers = arg->mon_id.my_id.my_vers;
	np->n_proc = arg->mon_id.my_id.my_proc;
	memcpy(np->n_data, arg->priv, sizeof(np->n_data));

	np->n_next = mhp->mh_notify_list;
	mhp->mh_notify_list = np;

	res.res_stat = stat_succ;	/* Report success                       */
	return (&res);
}

/* do_unmon ---------------------------------------------------------------- */
/*
   Purpose:	Remove a monitor request from a host
   Returns:	TRUE if found, FALSE if not found.
   Notes:	Common code from sm_unmon_1_svc and sm_unmon_all_1_svc
		In the unlikely event of more than one identical monitor
		request, all are removed.
*/

static int 
do_unmon(MonitoredHost * mhp, my_id * idp)
{
	Notify *np, *next, *last = NULL;
	int result = FALSE;

	np = mhp->mh_notify_list;
	if (!np)
		return (result);

	while (np) {
		if (!strncasecmp(idp->my_name, np->n_host, SM_MAXSTRLEN)
		    && (idp->my_prog == np->n_prog) && (idp->my_proc == np->n_proc)
		    && (idp->my_vers == np->n_vers)) {
			/* found one.  Unhook from chain and free.		 */
			next = np->n_next;
			if (last)
				last->n_next = next;
			else
				mhp->mh_notify_list = next;
			free(np);
			np = next;
			result = TRUE;
		} else {
			last = np;
			np = np->n_next;
		}
	}
	if (result && !mhp->mh_notify_list) {
		HostInfo *hip = HOSTINFO(mhp->mh_hostinfo_offset);
		hip->hi_monitored = 0;
		sync_file();
		tdelete(mhp, &mhroot, mhcmp);
		free(mhp);
	}
	return (result);
}

/* sm_unmon_1 -------------------------------------------------------------- */
/*
   Purpose:	RPC procedure to release a monitor request.
   Returns:	Local machine's status number
   Notes:	The supplied mon_id should match the value passed in an
		earlier call to sm_mon_1
*/

struct sm_stat *
sm_unmon_1_svc(mon_id * arg, struct svc_req * req __unused)
{
	static sm_stat res;
	MonitoredHost *mhp;

	DEBUG(1, "un-monitor request for host %s", arg->mon_name);
	DEBUG(1, "recall host: %s prog: %d ver: %d proc: %d", arg->my_id.my_name,
	      arg->my_id.my_prog, arg->my_id.my_vers, arg->my_id.my_proc);

	if ((mhp = find_host(arg->mon_name, FALSE))) {
		if (!do_unmon(mhp, &arg->my_id))
			log(LOG_ERR, "unmon request from %s, no matching monitor",
			    arg->my_id.my_name);
	} else {
		log(LOG_ERR, "unmon request from %s for unknown host %s",
		    arg->my_id.my_name, arg->mon_name);
	}

	res.state = ntohl(status_info->fh_state);

	return (&res);
}

/* sm_unmon_all_1 ---------------------------------------------------------- */
/*
   Purpose:	RPC procedure to release monitor requests.
   Returns:	Local machine's status number
   Notes:	Releases all monitor requests (if any) from the specified
		host and program number.

		Ideally we would do a twalk()/tdelete(), but paranoia
		about the (current and future) safety of performing
		tdelete() during a twalk() has driven this alternative
		implementation.
*/

struct sm_stat *
sm_unmon_all_1_svc(my_id * arg, struct svc_req * req __unused)
{
	static sm_stat res;
	HostInfo *hip;
	MonitoredHost mhtmp, **mhpp;
	off_t off;
	uint i;

	DEBUG(1, "unmon_all for host: %s prog: %d ver: %d proc: %d",
	      arg->my_name, arg->my_prog, arg->my_vers, arg->my_proc);

	bzero(&mhtmp, sizeof(mhtmp));

	off = sizeof(FileHeader);
	for (i = 0; i < ntohl(status_info->fh_reccnt); i++) {
		hip = HOSTINFO(off);
		off += ntohs(hip->hi_len);
		if (hip->hi_monitored) {
			/* find the MonitoredHost and try to do the unmon */
			mhtmp.mh_name = hip->hi_name;
			mhpp = tfind(&mhtmp, &mhroot, mhcmp);
			if (mhpp)
				do_unmon(*mhpp, arg);
		}
	}

	res.state = ntohl(status_info->fh_state);

	return (&res);
}

/* sm_simu_crash_1 --------------------------------------------------------- */
/*
   Purpose:	RPC procedure to simulate a crash
   Returns:	Nothing
   Notes:	Standardised mechanism for debug purposes
		The specification says that we should drop all of our
		status information (apart from the list of monitored hosts
		on disc).  However, this would confuse the rpc.lockd
		which would be unaware that all of its monitor requests
		had been silently junked.  Hence we in fact retain all
		current requests and simply increment the status counter
		and inform all hosts on the monitor list.
*/

void *
sm_simu_crash_1_svc(void *v __unused, struct svc_req * req)
{
	static char dummy;
	int need_notify = FALSE;
	HostInfo *hip;
	off_t off;
	uint i;

	if (!config.simu_crash_allowed) {
		struct sockaddr_in *claddr;
		struct hostent *he;
		char *hname = NULL;

		if ((claddr = svc_getcaller(req->rq_xprt))) {
			he = gethostbyaddr((char *) &claddr->sin_addr, sizeof(claddr->sin_addr), AF_INET);
			if (he)
				hname = he->h_name;
			else
				hname = inet_ntoa(claddr->sin_addr);
		}
		log(LOG_WARNING, "simu_crash call from %s denied!", hname);
		return (&dummy);
	}

	log(LOG_WARNING, "simu_crash called!!");

	/* Simulate crash by setting notify-required flag on all monitored	 */
	/* hosts, and incrementing our status number.  fork a process and	 */
	/* call notify_hosts() to do the notifications.			 */

	off = sizeof(FileHeader);
	for (i = 0; i < ntohl(status_info->fh_reccnt); i++) {
		hip = HOSTINFO(off);
		if (hip->hi_monitored)
			hip->hi_notify = htons(1);
		if (hip->hi_notify)
			need_notify = TRUE;
		off += ntohs(hip->hi_len);
	}
	/* always odd numbers if not crashed	 */
	status_info->fh_state = htonl(ntohl(status_info->fh_state) + 2);

	if (need_notify && !get_statd_notify_pid()) {
		/*
	         * It looks like there are notifications that need to be made, but that the
	         * statd.notify service isn't running.  Let's try to start it up.
	         */
		log(LOG_INFO, "need to start statd notify");
		if (statd_notify_is_loaded())
			statd_notify_start();
		else
			statd_notify_load();
	}
	return (&dummy);
}

/* sm_notify_1 ------------------------------------------------------------- */
/*
   Purpose:	RPC procedure notifying local statd of the crash of another
   Returns:	Nothing
   Notes:	There is danger of deadlock, since it is quite likely that
		the client procedure that we call will in turn call us
		to remove or adjust the monitor request.
		We therefore fork() a process to do the notifications.
		Note that the main HostInfo structure is in a mmap()
		region and so will be shared with the child, but the
		monList pointed to by the HostInfo is in normal memory.
		Hence if we read the monList before forking, we are
		protected from the parent servicing other requests
		that modify the list.
*/

void *
sm_notify_1_svc(stat_chge * arg, struct svc_req * req)
{
	struct timeval timeout = {20, 0};	/* 20 secs timeout		 */
	CLIENT *cli;
	static char dummy;
	char proto[] = "udp", empty[] = "";
	sm_status tx_arg;	/* arg sent to callback procedure	 */
	MonitoredHost *mhp;
	Notify *np;
	HostInfo *hip;
	pid_t pid;

	DEBUG(1, "notify from host %s, new state %d", arg->mon_name, arg->state);

	mhp = find_host(arg->mon_name, FALSE);
	if (!mhp) {
		/*
	         * Hmmm... We've never heard of this host.
	         * It's possible the host just didn't give us the right hostname.
	         * Let's try the IP address the request came from and any hostnames it has.
	         */
		struct sockaddr_in *claddr;
		if ((claddr = svc_getcaller(req->rq_xprt))) {
			struct hostent *he;
			he = gethostbyaddr((char *) &claddr->sin_addr, sizeof(claddr->sin_addr), AF_INET);
			if (he) {
				char **npp = he->h_aliases;
				/* make sure host name isn't > SM_MAXSTRLEN */
				if (strlen(he->h_name) <= SM_MAXSTRLEN)
					mhp = find_host(he->h_name, FALSE);
				while (!mhp && *npp) {
					if (strlen(*npp) <= SM_MAXSTRLEN)
						mhp = find_host(*npp, FALSE);
					if (!mhp)
						npp++;
				}
			}
			if (mhp)
				DEBUG(1, "Notification from host %s found as %s",
				      arg->mon_name, HOSTINFO(mhp->mh_hostinfo_offset)->hi_name);
		}
		if (!mhp) {
			/* Never heard of this host - why is it notifying us?		 */
			DEBUG(1, "Unsolicited notification from host %s", arg->mon_name);
			return (&dummy);
		}
	}
	hip = HOSTINFO(mhp->mh_hostinfo_offset);
	np = mhp->mh_notify_list;
	if (!np) /* We know this host, but have no outstanding requests. */
		return (&dummy);
	pid = fork();
	if (pid == -1) {
		log(LOG_ERR, "Unable to fork notify process - %s", strerror(errno));
		return (NULL);	/* no answer, the client will retry */
	}
	if (pid)
		return (&dummy); /* Parent returns */

	while (np) {
		tx_arg.mon_name = hip->hi_name;
		tx_arg.state = arg->state;
		memcpy(tx_arg.priv, np->n_data, sizeof(tx_arg.priv));
		cli = clnt_create(np->n_host, np->n_prog, np->n_vers, proto);
		if (!cli) {
			log(LOG_ERR, "Failed to contact host %s%s", np->n_host, clnt_spcreateerror(empty));
		} else {
			if (clnt_call(cli, np->n_proc, (xdrproc_t) xdr_sm_status, &tx_arg, (xdrproc_t) xdr_void,
				      &dummy, timeout) != RPC_SUCCESS) {
				log(LOG_ERR, "Failed to call rpc.statd client at host %s", np->n_host);
			}
			clnt_destroy(cli);
		}
		np = np->n_next;
	}

	exit(0);		/* Child quits	 */
}

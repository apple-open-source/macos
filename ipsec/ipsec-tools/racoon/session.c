/*	$NetBSD: session.c,v 1.7.6.2 2007/08/01 11:52:22 vanhu Exp $	*/

/*	$KAME: session.c,v 1.32 2003/09/24 02:01:17 jinmei Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#if HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
# define WEXITSTATUS(s)	((unsigned)(s) >> 8)
#endif
#ifndef WIFEXITED
# define WIFEXITED(s)	(((s) & 255) == 0)
#endif

#ifndef HAVE_NETINET6_IPSEC
#include <netinet/ipsec.h>
#else
#include <netinet6/ipsec.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <signal.h>
#include <sys/stat.h>
#include <paths.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#include <resolv.h>
#include <TargetConditionals.h>
#include <vproc_priv.h>

#include "libpfkey.h"

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "debug.h"

#include "schedule.h"
#include "session.h"
#include "grabmyaddr.h"
#include "evt.h"
#include "cfparse_proto.h"
#include "isakmp_var.h"
#include "isakmp_xauth.h"
#include "isakmp_cfg.h"
#include "admin_var.h"
#include "admin.h"
#include "privsep.h"
#include "oakley.h"
#include "pfkey.h"
#include "handler.h"
#include "localconf.h"
#include "remoteconf.h"
#include "backupsa.h"
#ifdef ENABLE_NATT
#include "nattraversal.h"
#endif
#include "vpn_control_var.h"
#include "policy.h"
#include "algorithm.h" /* XXX ??? */

#include "sainfo.h"
#include "power_mgmt.h"



extern pid_t racoon_pid;
extern char	logFileStr[];
extern int launchdlaunched;
static void close_session __P((void));
static void check_rtsock __P((void *));
static void initfds __P((void));
static void init_signal __P((void));
static int set_signal __P((int sig, RETSIGTYPE (*func) __P((int, siginfo_t *, void *))));
static void check_sigreq __P((void));
static void check_flushsa_stub __P((void *));
static void check_flushsa __P((void));
static void auto_exit_do __P((void *));
static int close_sockets __P((void));

static fd_set mask0;
static fd_set maskdying;
static int nfds = 0;
static volatile sig_atomic_t sigreq[NSIG + 1];
static int dying = 0;
static struct sched *check_rtsock_sched = NULL;
int terminated = 0;

#define HANDLE_TENTATIVE_INTF_FAILURES() do {																			  \
											if (tentative_failures) {													  \
												plog(LLV_ERROR, LOCATION, NULL,											  \
													 "detected tentative interface/address issues: will retry later.\n"); \
												if (check_rtsock_sched == NULL) {										  \
													/* only schedule if not already done */								  \
													check_rtsock_sched = sched_new(5, check_rtsock, NULL);				  \
												}																		  \
											}																			  \
										} while(0)

static void
reinit_socks (void)
{
	int tentative_failures;

	isakmp_close(); 
	close(lcconf->rtsock);
	initmyaddr();
	if (isakmp_open(&tentative_failures) < 0) {
		plog(LLV_ERROR2, LOCATION, NULL,
			 "failed to reopen isakmp sockets\n");
	}
	initfds();	
	HANDLE_TENTATIVE_INTF_FAILURES();
}

static int64_t racoon_keepalive = -1;

/*
 * This is used to (manually) update racoon's launchd keepalive, which is needed because racoon is (mostly) 
 * launched on demand and for <rdar://problem/8768510> requires a keepalive on dirty/failure exits.
 * The launchd plist can't be used for this because RunOnLoad is required to have keepalive on a failure exit.
 */
int64_t
launchd_update_racoon_keepalive (Boolean enabled)
{
	if (launchdlaunched) {
		vproc_t vp = vprocmgr_lookup_vproc("com.apple.racoon");
		if (vp) {
			int64_t     val = (__typeof__(val))enabled;
			if (vproc_swap_integer(vp,
								   VPROC_GSK_BASIC_KEEPALIVE,
								   &val,
								   &racoon_keepalive)) {
				plog(LLV_ERROR2, LOCATION, NULL,
					 "failed to swap launchd keepalive integer %d\n", enabled);
			}
			vproc_release(vp);
		}
	}
	return racoon_keepalive;
}

int
session(void)
{
	fd_set rfds;
	struct timeval *timeout;
	int error;
	struct myaddrs *p;
	char pid_file[MAXPATHLEN];
	FILE *fp;
	int i, update_fds;
	int tentative_failures;

	/* initialize schedular */
	sched_init();

	/* needs to be called after schedular */
	if (init_power_mgmt() < 0) {
		errx(1, "failed to initialize power-mgmt.");
	}

	initmyaddr();

	if (isakmp_init(false, &tentative_failures) < 0) {
		plog(LLV_ERROR2, LOCATION, NULL,
				"failed to initialize isakmp");
		exit(1);
	}
	HANDLE_TENTATIVE_INTF_FAILURES();
		
#ifdef ENABLE_ADMINPORT
	if (admin_init() < 0) {
		plog(LLV_ERROR2, LOCATION, NULL,
				"failed to initialize admin port");
		exit(1);
	}
#endif
#ifdef ENABLE_VPNCONTROL_PORT
	if (vpncontrol_init() < 0) {
		plog(LLV_ERROR2, LOCATION, NULL,
			"failed to initialize vpn control port");
		exit(1);
	}
	
#endif

	init_signal();
	initfds();

#ifdef HAVE_OPENSSL
	if (privsep_init() != 0) {
		plog(LLV_ERROR2, LOCATION, NULL,
			"failed to initialize privsep");
		exit(1);
	}
#endif
	
	for (i = 0; i <= NSIG; i++)
		sigreq[i] = 0;

	/* write .pid file */
	if (!f_foreground) {
		racoon_pid = getpid();
		if (lcconf->pathinfo[LC_PATHTYPE_PIDFILE] == NULL) 
			strlcpy(pid_file, _PATH_VARRUN "racoon.pid", sizeof(pid_file));
		else if (lcconf->pathinfo[LC_PATHTYPE_PIDFILE][0] == '/') 
			strlcpy(pid_file, lcconf->pathinfo[LC_PATHTYPE_PIDFILE], sizeof(pid_file));
		else {
			strlcat(pid_file, _PATH_VARRUN, sizeof(pid_file));
			strlcat(pid_file, lcconf->pathinfo[LC_PATHTYPE_PIDFILE], sizeof(pid_file));
		} 
		fp = fopen(pid_file, "w");
		if (fp) {
			if (fchmod(fileno(fp),
				S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH) == -1) {
				syslog(LOG_ERR, "%s", strerror(errno));
				fclose(fp);
				exit(1);
			}
			fprintf(fp, "%ld\n", (long)racoon_pid);
			fclose(fp);
		} else {
			plog(LLV_ERROR, LOCATION, NULL,
				"cannot open %s", pid_file);
		}
	}

#if !TARGET_OS_EMBEDDED
	// enable keepalive for recovery (from crashes and bad exits... after init)
	(void)launchd_update_racoon_keepalive(true);
#endif // !TARGET_OS_EMBEDDED
		
	while (1) {
		if (!TAILQ_EMPTY(&lcconf->saved_msg_queue))
			pfkey_post_handler();
		update_fds = 0;
		/*
		 * asynchronous requests via signal.
		 * make sure to reset sigreq to 0.
		 */
		check_sigreq();

		check_power_mgmt();

		/* scheduling */
		timeout = schedular();
		// <rdar://problem/7650111> Workaround: make sure timeout is playing nice
		if (timeout) {
			if (timeout->tv_usec < 0 || timeout->tv_usec > SELECT_USEC_MAX ) {
				timeout->tv_sec += ((__typeof__(timeout->tv_sec))timeout->tv_usec)/SELECT_USEC_MAX;
				timeout->tv_usec %= SELECT_USEC_MAX;
			}
			if (timeout->tv_sec > SELECT_SEC_MAX /* tv_sec is unsigned */) {
				timeout->tv_sec = SELECT_SEC_MAX;
			}
			if (!timeout->tv_sec && !timeout->tv_usec) {
				timeout->tv_sec = 1;
			}
		}

		if (dying)
			rfds = maskdying;
		else
			rfds = mask0;
		error = select(nfds, &rfds, (fd_set *)0, (fd_set *)0, timeout);
		if (error < 0) {
			switch (errno) {
			case EINTR:
				continue;
			default:
				plog(LLV_ERROR2, LOCATION, NULL,
					"failed select (%s) nfds %d\n",
					strerror(errno), nfds);
				reinit_socks();
				update_fds = 0;
				continue;
			}
			/*NOTREACHED*/
		}

#ifdef ENABLE_ADMINPORT
		if ((lcconf->sock_admin != -1) &&
		    (FD_ISSET(lcconf->sock_admin, &rfds)))
			admin_handler();
#endif
#ifdef ENABLE_VPNCONTROL_PORT
		{
			struct vpnctl_socket_elem *elem;
			struct vpnctl_socket_elem *t_elem;
			
			if ((lcconf->sock_vpncontrol != -1) &&
				(FD_ISSET(lcconf->sock_vpncontrol, &rfds))) {
				vpncontrol_handler();
				update_fds = 1;			//  in case new socket created - update mask
			}
			/* The handler may close and remove the list element
			 * so we can't rely on it being valid after calling
			 * the handler.
			 */
			LIST_FOREACH_SAFE(elem, &lcconf->vpnctl_comm_socks, chain, t_elem) {
				if ((elem->sock != -1) &&
					(FD_ISSET(elem->sock, &rfds)))
					if (vpncontrol_comm_handler(elem))
						update_fds = 1;		// socket closed by peer - update mask
			}
		}
#endif			

		for (p = lcconf->myaddrs; p; p = p->next) {
			if (!p->addr)
				continue;
			if ((p->sock != -1) &&
				(FD_ISSET(p->sock, &rfds)))
				if ((error = isakmp_handler(p->sock)) == -2)
					break;
		}
		if (error == -2) {
			plog(LLV_ERROR2, LOCATION, NULL,
				 "failed to process isakmp port\n");
			reinit_socks();
			update_fds = 0;
			continue;
		}

		if (FD_ISSET(lcconf->sock_pfkey, &rfds))
			pfkey_handler();

		if (lcconf->rtsock >= 0 && FD_ISSET(lcconf->rtsock, &rfds)) {
			if (update_myaddrs() && lcconf->autograbaddr)
				if (check_rtsock_sched == NULL)	/* only schedule if not already done */
					check_rtsock_sched = sched_new(1, check_rtsock, NULL);
				else {
					// force reinit if schedule is too far off (3 seconds or more)
					time_t too_far = current_time() + 3;
					if (check_rtsock_sched->dead ||
						check_rtsock_sched->xtime >= too_far) {
						plog(LLV_DEBUG, LOCATION, NULL,
							 "forced reinit of addrs\n");
						update_fds = 0;
						check_rtsock(NULL);
					}
				}
			// initfds();	//%%% BUG FIX - not needed here
		}
		if (update_fds) {
			initfds();
			update_fds = 0;
		}
	}
}


/* clear all status and exit program. */
static void
close_session()
{
	if ( terminated )
		flushph2(false);
	flushph1(false);
	close_sockets();
	backupsa_clean();

#if !TARGET_OS_EMBEDDED
	// a clean exit, so disable launchd keepalive
	(void)launchd_update_racoon_keepalive(false);
#endif // !TARGET_OS_EMBEDDED

	plog(LLV_INFO, LOCATION, NULL, "racoon shutdown\n");
	exit(0);
}

static void
check_rtsock(p)
	void *p;
{	
	int tentative_failures;

	check_rtsock_sched = NULL;
	grab_myaddrs();
	isakmp_close_unused();

	autoconf_myaddrsport();
	isakmp_open(&tentative_failures);

	/* initialize socket list again */
	initfds();
	HANDLE_TENTATIVE_INTF_FAILURES();
}

static void
initfds()
{
	struct myaddrs *p;

	nfds = 0;

	FD_ZERO(&mask0);
	FD_ZERO(&maskdying);

#ifdef ENABLE_ADMINPORT
	if (lcconf->sock_admin != -1) {
		if (lcconf->sock_admin >= FD_SETSIZE) {
			plog(LLV_ERROR2, LOCATION, NULL, "fd_set overrun - admin socket\n");
			exit(1);
		}
		FD_SET(lcconf->sock_admin, &mask0);
		/* XXX should we listen on admin socket when dying ?
		 */
#if 0
		FD_SET(lcconf->sock_admin, &maskdying);
#endif
		nfds = (nfds > lcconf->sock_admin ? nfds : lcconf->sock_admin);
	}
#endif
#ifdef ENABLE_VPNCONTROL_PORT
	{
		struct vpnctl_socket_elem *elem;
		
		if (lcconf->sock_vpncontrol != -1) {
			if (lcconf->sock_vpncontrol >= FD_SETSIZE) {
				plog(LLV_ERROR2, LOCATION, NULL, "fd_set overrun - vpncontrol socket\n");
				exit(1);
			}
			FD_SET(lcconf->sock_vpncontrol, &mask0);
			nfds = (nfds > lcconf->sock_vpncontrol ? nfds : lcconf->sock_vpncontrol);
		}
		
		LIST_FOREACH(elem, &lcconf->vpnctl_comm_socks, chain) {
			if (elem->sock != -1) {
				if (elem->sock >= FD_SETSIZE) {
					plog(LLV_ERROR2, LOCATION, NULL, "fd_set overrun vpnctl_comm socket\n");
					exit(1);
				}
				FD_SET(elem->sock, &mask0);
				nfds = (nfds > elem->sock ? nfds : elem->sock);
			}
		}
	}
#endif

	if (lcconf->sock_pfkey >= FD_SETSIZE) {
		plog(LLV_ERROR2, LOCATION, NULL, "fd_set overrun - pfkey socket\n");
		exit(1);
	}
	FD_SET(lcconf->sock_pfkey, &mask0);
	FD_SET(lcconf->sock_pfkey, &maskdying);
	nfds = (nfds > lcconf->sock_pfkey ? nfds : lcconf->sock_pfkey);
	if (lcconf->rtsock >= 0) {
		if (lcconf->rtsock >= FD_SETSIZE) {
			plog(LLV_ERROR2, LOCATION, NULL, "fd_set overrun - rt socket\n");
			exit(1);
		}
		FD_SET(lcconf->rtsock, &mask0);
		nfds = (nfds > lcconf->rtsock ? nfds : lcconf->rtsock);
	}
	
	for (p = lcconf->myaddrs; p; p = p->next) {
		if (!p->addr)
			continue;
		if (p->sock < 0)
			continue;
		if (p->sock >= FD_SETSIZE) {
			plog(LLV_ERROR2, LOCATION, NULL, "fd_set overrun - isakmp socket\n");
			exit(1);
		}
		FD_SET(p->sock, &mask0);
		nfds = (nfds > p->sock ? nfds : p->sock);
	}
	nfds++;
}


static int signals[] = {
	SIGHUP,
	SIGINT,
	SIGTERM,
	SIGUSR1,
	SIGUSR2,
	SIGCHLD,
	SIGPIPE,
	0
};

/*
 * asynchronous requests will actually dispatched in the
 * main loop in session().
 */
RETSIGTYPE
signal_handler(sig, sigi, ctx)
	int sig;
	siginfo_t *sigi;
	void *ctx;
{
#if 0
	plog(LLV_DEBUG, LOCATION, NULL, 
		 "%s received signal %d from pid %d uid %d\n\n",
		 __FUNCTION__, sig, sigi->si_pid, sigi->si_uid);
#endif

	/* Do not just set it to 1, because we may miss some signals by just setting
	 * values to 0/1
	 */
	sigreq[sig]++;
	if ( sig == SIGTERM ){
		terminated = 1;
	}
}

static void
check_sigreq()
{
	int sig;
	int tentative_failures;

	/* 
	 * XXX We are not able to tell if we got 
	 * several time the same signal. This is
	 * not a problem for the current code, 
	 * but we shall remember this limitation.
	 */
	for (sig = 0; sig <= NSIG; sig++) {
		if (sigreq[sig] == 0)
			continue;

		sigreq[sig]--;
		switch(sig) {
		case 0:
			return;
			
			/* Catch up childs, mainly scripts.
			 */
		case SIGCHLD:
	    {
			pid_t pid;
			int s;
			
			pid = wait(&s);
	    }
		break;

#ifdef DEBUG_RECORD_MALLOCATION
		/* 
		 * XXX This operation is signal handler unsafe and may lead to 
		 * crashes and security breaches: See Henning Brauer talk at
		 * EuroBSDCon 2005. Do not run in production with this option
		 * enabled.
		 */
		case SIGUSR2:
			DRM_dump();
			break;
#endif

		case SIGUSR1:
		case SIGHUP:
#ifdef ENABLE_HYBRID
			if ((isakmp_cfg_init(ISAKMP_CFG_INIT_WARM)) != 0) {
				plog(LLV_ERROR, LOCATION, NULL, 
		  	 	 "ISAKMP mode config structure reset failed, "
		  	 	 "not reloading\n");
				return;
			}
#endif
			if ( terminated )
				break;
				
			/*
			 * if we got a HUP... try graceful teardown of sessions before we close and reopen sockets...
			 * so that info-deletes notifications can make it to the peer.
			 */
			if (sig == SIGHUP) {
				flushph2(true);
				flushph1(true);
			}		
			/* Save old configuration, load new one...  */
			isakmp_close();
			close(lcconf->rtsock);
			if (cfreparse(sig)) {
				plog(LLV_ERROR2, LOCATION, NULL,
					 "configuration read failed\n");
				exit(1);
			}
			if (lcconf->logfile_param == NULL && logFileStr[0] == 0)
				plogreset(lcconf->pathinfo[LC_PATHTYPE_LOGFILE]);
				
			initmyaddr();
			isakmp_cleanup();
			isakmp_init(true, &tentative_failures);
			HANDLE_TENTATIVE_INTF_FAILURES();
			initfds();
#if TARGET_OS_EMBEDDED
			if (no_remote_configs(TRUE)) {
				EVT_PUSH(NULL, NULL, EVTT_RACOON_QUIT, NULL);
				pfkey_send_flush(lcconf->sock_pfkey, SADB_SATYPE_UNSPEC);
#ifdef ENABLE_FASTQUIT
				close_session();
#else
				sched_new(1, check_flushsa_stub, NULL);
#endif
				dying = 1;
			}
#endif
			break;

		case SIGINT:
		case SIGTERM:			
			plog(LLV_INFO, LOCATION, NULL, 
			    "caught signal %d\n", sig);
			EVT_PUSH(NULL, NULL, EVTT_RACOON_QUIT, NULL);
			pfkey_send_flush(lcconf->sock_pfkey, 
			    SADB_SATYPE_UNSPEC);
			if ( sig == SIGTERM ){
				terminated = 1;			/* in case if it hasn't been set yet */
				close_session();
			}
			else
				sched_new(1, check_flushsa_stub, NULL);

				dying = 1;
			break;

		default:
			plog(LLV_INFO, LOCATION, NULL, 
			    "caught signal %d\n", sig);
			break;
		}
	}
}

/*
 * waiting the termination of processing until sending DELETE message
 * for all inbound SA will complete.
 */
static void
check_flushsa_stub(p)
	void *p;
{

	check_flushsa();
}

static void
check_flushsa()
{
	vchar_t *buf;
	struct sadb_msg *msg, *end, *next;
	struct sadb_sa *sa;
	caddr_t mhp[SADB_EXT_MAX + 1];
	int n;

	buf = pfkey_dump_sadb(SADB_SATYPE_UNSPEC);
	if (buf == NULL) {
		plog(LLV_DEBUG, LOCATION, NULL,
		    "pfkey_dump_sadb: returned nothing.\n");
		return;
	}

	msg = ALIGNED_CAST(struct sadb_msg *)buf->v; 
	end = ALIGNED_CAST(struct sadb_msg *)(buf->v + buf->l);

	/* counting SA except of dead one. */
	n = 0;
	while (msg < end) {
		if (PFKEY_UNUNIT64(msg->sadb_msg_len) < sizeof(*msg))
			break;
		next = ALIGNED_CAST(struct sadb_msg *)((caddr_t)msg + PFKEY_UNUNIT64(msg->sadb_msg_len));    // Wcast-align fix (void*) - aligned buffer + multiple of 64
		if (msg->sadb_msg_type != SADB_DUMP) {
			msg = next;
			continue;
		}

		if (pfkey_align(msg, mhp) || pfkey_check(mhp)) {
			plog(LLV_ERROR, LOCATION, NULL,
				"pfkey_check (%s)\n", ipsec_strerror());
			msg = next;
			continue;
		}

		sa = ALIGNED_CAST(struct sadb_sa *)(mhp[SADB_EXT_SA]);       // Wcast-align fix (void*) - mhp contains pointers to aligned structs
		if (!sa) {
			msg = next;
			continue;
		}

		if (sa->sadb_sa_state != SADB_SASTATE_DEAD) {
			n++;
			msg = next;
			continue;
		}

		msg = next;
	}

	if (buf != NULL)
		vfree(buf);

	if (n) {
		sched_new(1, check_flushsa_stub, NULL);
		return;
	}

	close_session();
#if !TARGET_OS_EMBEDDED
	if (lcconf->vt)
		vproc_transaction_end(NULL, lcconf->vt);
#endif
}

void
auto_exit_do(void *p)
{
	EVT_PUSH(NULL, NULL, EVTT_RACOON_QUIT, NULL);
	plog(LLV_DEBUG, LOCATION, NULL,
				"performing auto exit\n");
	pfkey_send_flush(lcconf->sock_pfkey, SADB_SATYPE_UNSPEC);
	sched_new(1, check_flushsa_stub, NULL);
	dying = 1;
}

void
check_auto_exit(void)
{
	if (lcconf->auto_exit_sched != NULL) {	/* exit scheduled? */
		if (lcconf->auto_exit_state != LC_AUTOEXITSTATE_ENABLED
			|| vpn_control_connected()				/* vpn control connected */
			|| policies_installed()			/* policies installed in kernel */
			|| !no_remote_configs(FALSE))			/* remote or anonymous configs */
			SCHED_KILL(lcconf->auto_exit_sched);
	} else {								/* exit not scheduled */
		if (lcconf->auto_exit_state == LC_AUTOEXITSTATE_ENABLED
			&& !vpn_control_connected()	
			&& !policies_installed()
			&& no_remote_configs(FALSE))
				if (lcconf->auto_exit_delay == 0)
					auto_exit_do(NULL);		/* immediate exit */
				else
					lcconf->auto_exit_sched = sched_new(lcconf->auto_exit_delay, auto_exit_do, NULL);
	}
}


static void
init_signal()
{
	int i;

	for (i = 0; signals[i] != 0; i++)
		if (set_signal(signals[i], signal_handler) < 0) {
			plog(LLV_ERROR2, LOCATION, NULL,
				"failed to set_signal (%s)\n",
				strerror(errno));
			exit(1);
		}
}

static int
set_signal(sig, func)
	int sig;
	RETSIGTYPE (*func) __P((int, siginfo_t *, void *));
{
	struct sigaction sa;

	memset((caddr_t)&sa, 0, sizeof(sa));
	sa.sa_handler = func;
	sa.sa_flags = SA_RESTART | SA_SIGINFO;

	if (sigemptyset(&sa.sa_mask) < 0)
		return -1;

	if (sigaction(sig, &sa, (struct sigaction *)0) < 0)
		return(-1);

	return 0;
}

static int
close_sockets()
{
	isakmp_close();
	pfkey_close(lcconf->sock_pfkey);
#ifdef ENABLE_ADMINPORT
	(void)admin_close();
#endif
#ifdef ENABLE_VPNCONTROL_PORT
	vpncontrol_close();
#endif
	return 0;
}



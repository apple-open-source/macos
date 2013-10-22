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
#include <dispatch/dispatch.h>

#include "libpfkey.h"

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "debug.h"
#include "plog.h"

#include "schedule.h"
#include "session.h"
#include "grabmyaddr.h"
#include "cfparse_proto.h"
#include "isakmp_var.h"
#include "isakmp_xauth.h"
#include "isakmp_cfg.h"
#include "oakley.h"
#include "pfkey.h"
#include "handler.h"
#include "localconf.h"
#include "remoteconf.h"
#ifdef ENABLE_NATT
#include "nattraversal.h"
#endif
#include "vpn_control_var.h"
#include "policy.h"
#include "algorithm.h" /* XXX ??? */

#include "sainfo.h"
#include "power_mgmt.h"



extern pid_t racoon_pid;
extern int launchdlaunched;
static void close_session (int);
static int init_signal (void);
static int set_signal (int sig, RETSIGTYPE (*func) (int, siginfo_t *, void *));
static void check_sigreq (void);
static void check_flushsa_stub (void *);
static void check_flushsa (void);
static void auto_exit_do (void *);
static int close_sockets (void);

static volatile sig_atomic_t sigreq[NSIG + 1];
int terminated = 0;

static int64_t racoon_keepalive = -1;

dispatch_queue_t main_queue;

/*
 * This is used to (manually) update racoon's launchd keepalive, which is needed because racoon is (mostly) 
 * launched on demand and for <rdar://problem/8768510> requires a keepalive on dirty/failure exits.
 * The launchd plist can't be used for this because RunOnLoad is required to have keepalive on a failure exit.
 */
int64_t
launchd_update_racoon_keepalive (Boolean enabled)
{
	if (launchdlaunched) {		
		int64_t     val = (__typeof__(val))enabled;
		/* Set our own KEEPALIVE value */
		if (vproc_swap_integer(NULL,
							   VPROC_GSK_BASIC_KEEPALIVE,
							   &val,
							   &racoon_keepalive)) {
			plog(ASL_LEVEL_ERR, 
				 "failed to swap launchd keepalive integer %d\n", enabled);
		}
	}
	return racoon_keepalive;
}

//
// Session
// 
// Initialize listening sockets, timers, vpn control etc.,
// write the PID file and call dispatch_main.
//
void
session(void)
{
	char pid_file[MAXPATHLEN];
	FILE *fp;
	int i;
    
    main_queue = dispatch_get_main_queue();

	/* initialize schedular */
	sched_init();

	/* needs to be called after schedular */
	if (init_power_mgmt() < 0) {
        plog(ASL_LEVEL_ERR, 
             "failed to initialize power-mgmt.");
		exit(1);
	}

    if (lcconf->autograbaddr == 1)
        if (pfroute_init()) {
            plog(ASL_LEVEL_ERR, "failed to initialize route socket.\n");
            exit(1);
        }
    if (initmyaddr()) {
        plog(ASL_LEVEL_ERR, "failed to initialize listening addresses.\n");
        exit(1);
    }
	if (isakmp_init()) {
		plog(ASL_LEVEL_ERR, "failed to initialize isakmp");
		exit(1);
	}    
#ifdef ENABLE_VPNCONTROL_PORT
	if (vpncontrol_init()) {
		plog(ASL_LEVEL_ERR, "failed to initialize vpn control port");
		//exit(1);
	}
#endif

	if (init_signal()) {
        plog(ASL_LEVEL_ERR, "failed to initialize signals.\n");
		exit(1); 
    }
    
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
				plog(ASL_LEVEL_ERR, "%s", strerror(errno));
				fclose(fp);
				exit(1);
			}
			fprintf(fp, "%ld\n", (long)racoon_pid);
			fclose(fp);
		} else {
			plog(ASL_LEVEL_ERR, 
				"cannot open %s", pid_file);
		}
	}
#if !TARGET_OS_EMBEDDED
	// enable keepalive for recovery (from crashes and bad exits... after init)
	(void)launchd_update_racoon_keepalive(true);
#endif // !TARGET_OS_EMBEDDED
		
    // Off to the races!
    if (!terminated) {
        dispatch_main();
    }
            
    exit(1);    // should not be reached!!!
}


/* clear all status and exit program. */
static void
close_session(int error)
{
    sched_killall();    
    cleanup_power_mgmt();
	if ( terminated )
		ike_session_flush_all_phase2(false);
	ike_session_flush_all_phase1(false);
	close_sockets();

#if !TARGET_OS_EMBEDDED
	// a clean exit, so disable launchd keepalive
	(void)launchd_update_racoon_keepalive(false);
#endif // !TARGET_OS_EMBEDDED

	plog(ASL_LEVEL_INFO, "racoon shutdown\n");
	exit(0);
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
		plog(ASL_LEVEL_DEBUG, 
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
			plog(ASL_LEVEL_ERR, 
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

#if !TARGET_OS_EMBEDDED
	if (lcconf->vt)
		vproc_transaction_end(NULL, lcconf->vt);
#endif
    close_session(0);
}

void
auto_exit_do(void *p)
{
	plog(ASL_LEVEL_DEBUG, 
				"performing auto exit\n");
	pfkey_send_flush(lcconf->sock_pfkey, SADB_SATYPE_UNSPEC);
	sched_new(1, check_flushsa_stub, NULL);
	dying();
}

void
check_auto_exit(void)
{
	if (lcconf->auto_exit_sched) {	/* exit scheduled? */
		if (lcconf->auto_exit_state != LC_AUTOEXITSTATE_ENABLED
			|| vpn_control_connected()          /* vpn control connected */
			|| policies_installed()             /* policies installed in kernel */
			|| !no_remote_configs(FALSE))	{	/* remote or anonymous configs */
			SCHED_KILL(lcconf->auto_exit_sched);
        }
	} else {								/* exit not scheduled */
		if (lcconf->auto_exit_state == LC_AUTOEXITSTATE_ENABLED
			&& !vpn_control_connected()
			&& !policies_installed()
			&& no_remote_configs(FALSE)) {
            if (lcconf->auto_exit_delay == 0) {
                auto_exit_do(NULL);		/* immediate exit */
            } else {
                lcconf->auto_exit_sched = sched_new(lcconf->auto_exit_delay, auto_exit_do, NULL);
            }
        }
	}
}

static int signals[] = {
	SIGHUP,
	SIGINT,
	SIGTERM,
	SIGUSR1,
	SIGUSR2,
	SIGPIPE,
	0
};


static void
check_sigreq()
{
	int sig;
    
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
                                
            case SIGUSR1:
            case SIGHUP:
#ifdef ENABLE_HYBRID
                if ((isakmp_cfg_init(ISAKMP_CFG_INIT_WARM)) != 0) {
                    plog(ASL_LEVEL_ERR, 
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
                    ike_session_flush_all_phase2(true);
                    ike_session_flush_all_phase1(true);
                }		

                /* Save old configuration, load new one...  */
                if (cfreparse(sig)) {
                    plog(ASL_LEVEL_ERR, 
                         "configuration read failed\n");
                    exit(1);
                }
                if (lcconf->logfile_param == NULL && logFileStr[0] == 0)
                    plogresetfile(lcconf->pathinfo[LC_PATHTYPE_LOGFILE]);
				            
#if TARGET_OS_EMBEDDED
                if (no_remote_configs(TRUE)) {
                    pfkey_send_flush(lcconf->sock_pfkey, SADB_SATYPE_UNSPEC);
#ifdef ENABLE_FASTQUIT
                    close_session(0);
#else
                    sched_new(1, check_flushsa_stub, NULL);
#endif
                    dying();
                }
#endif

                break;
                
            case SIGINT:
            case SIGTERM:			
                plog(ASL_LEVEL_INFO, 
                     "caught signal %d\n", sig);
                pfkey_send_flush(lcconf->sock_pfkey, 
                                 SADB_SATYPE_UNSPEC);
                if ( sig == SIGTERM ){
                    terminated = 1;			/* in case if it hasn't been set yet */
                    close_session(0);
                }
                else
                    sched_new(1, check_flushsa_stub, NULL);
                
				dying();
                break;
                
            default:
                plog(ASL_LEVEL_INFO, 
                     "caught signal %d\n", sig);
                break;
		}
	}
}

    
/*
 * asynchronous requests will actually dispatched in the
 * main loop in session().
 */
RETSIGTYPE
signal_handler(int sig, siginfo_t *sigi, void *ctx)
{
#if 0
    plog(ASL_LEVEL_DEBUG, 
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
    dispatch_async(main_queue, 
                   ^{
                        check_sigreq(); 
                   });
}


static int
init_signal()
{
	int i;

	for (i = 0; signals[i] != 0; i++) {
		if (set_signal(signals[i], signal_handler) < 0) {
			plog(ASL_LEVEL_ERR, 
				"failed to set_signal (%s)\n",
				strerror(errno));
			return (1);
		}
    }
    return 0;
}

static int
set_signal(int sig, RETSIGTYPE (*func) (int, siginfo_t *, void *))
{
	struct sigaction sa;

	memset((caddr_t)&sa, 0, sizeof(sa));
    sa.sa_sigaction = func;
	sa.sa_flags = SA_RESTART | SA_SIGINFO;

	if (sigemptyset(&sa.sa_mask) < 0)
		return -1;

	if (sigaction(sig, &sa, (struct sigaction *)0) < 0)
		return(-1);

	return 0;
}

void
fatal_error(int error)
{
    close_session(error == 0 ? -1 : error);
}

/* suspend all socket sources except pf_key */
void
dying(void)
{
    if (lcconf->rt_source)
        dispatch_suspend(lcconf->rt_source);
    if (lcconf->vpncontrol_source)
        dispatch_suspend(lcconf->vpncontrol_source);
    isakmp_suspend_sockets();
}
    
static int
close_sockets()
{
    pfroute_close();
	isakmp_close();
	pfkey_close();
#ifdef ENABLE_VPNCONTROL_PORT
	vpncontrol_close();
#endif
    
	return 0;
}



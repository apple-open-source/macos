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
 * NetInfo server main
 * Copyright (C) 1989 by NeXT, Inc.
 *
 * TODO: shutdown at time of signal if no write transaction is in progress
 */
#include <NetInfo/config.h>
#include <NetInfo/project_version.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <rpc/rpc.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <notify.h>
#include "ni_server.h"
#include <NetInfo/system_log.h>
#include <NetInfo/network.h>
#include "ni_globals.h"
#include "getstuff.h"
#include <NetInfo/mm.h>
#include "ni_notify.h"
#include "ni_dir.h"
#include "alert.h"
#include <NetInfo/syslock.h>
#include <NetInfo/socket_lock.h>
#include <NetInfo/rpc_extra.h>
#include "sanitycheck.h"
#include "proxy_pids.h"

#ifndef S_IRGRP
#define S_IRGRP 0000040
#endif

#ifndef S_IWGRP
#define S_IWGRP 0000020
#endif

#ifndef S_IROTH
#define S_IROTH 0000004
#endif

#ifndef S_IWOTH
#define S_IWOTH 0000002
#endif

#ifndef LOG_NETINFO
#define LOG_NETINFO LOG_DAEMON
#endif

#define NIBIND_TIMEOUT 60
#define NIBIND_RETRIES 9

#define LOCAL_PORT 1033

#ifdef _UNIX_BSD_43_
#define PID_FILE "/etc/netinfo_%s.pid"
#else
#define PID_FILE "/var/run/netinfo_%s.pid"
#endif

#define FD_SLOPSIZE 15 /* # of fds for things other than connections */

extern int svc_maxfd;
extern int _rpc_dtablesize();

extern void ni_prog_2();
extern void ni_svc_run(int);

static void sig_shutdown(void);
static ni_status start_service(char *);
static ni_status ni_register(ni_name, unsigned, unsigned);
static void usage(char *);
void setproctitle(char *, ...);
void writepid(ni_name tag);

extern void readall_catcher(void);
extern void dblock_catcher(void);

extern void waitforparent(void);

char **Argv;	/* used to set the information displayed with ps(1) */
int    Argc;

static void
closeall(void)
{
	int i;

	for (i = getdtablesize() - 1; i >= 0; i--) close(i);

	/*
	 * We keep 0, 1 & 2 open to avoid using them. If we didn't, a
	 * library routine might do a printf to our descriptor and screw
	 * us up.
	 */
	open("/dev/null", O_RDWR, 0);
	dup(0);
	dup(0);
}

void
catch_sighup(void)
{
	system_log(LOG_DEBUG, "Caught SIGHUP - unbinding");
	set_binding_status(NI_FAILED);
}

int
main(int argc, char *argv[])
{
	ni_status status;
	ni_name myname = argv[0];
	int create = 0;
	int log_pri = LOG_NOTICE;
	ni_name dbsource_name = NULL;
	ni_name dbsource_addr = NULL;
	ni_name dbsource_tag = NULL;
	struct rlimit rlim;
	char *str;
	unsigned db_checksum;
	FILE *logf;
	int nctoken;

	logf = NULL;
	forcedIsRoot = 0;

	Argv = argv;	/* Save program and argument information for setproctitle */
	Argc = argc;

	argc--;
	argv++;
	while (argc > 0 && **argv == '-')
	{
		if (strcmp(*argv, "-d") == 0)
		{
			debug = 1;
			log_pri = LOG_DEBUG;
			if (argc < 2) logf = stderr;
			else
			{
				debug = atoi(argv[1]);
				argc -= 1;
				argv += 1;
			}
		}
		else if (strcmp(*argv, "-l") == 0)
		{
			if (argc < 2) usage(myname);
			else
			{
				log_pri = atoi(argv[1]);
				argc -= 1;
				argv += 1;
			}
		}
		else if (strcmp(*argv, "-n") == 0) forcedIsRoot = 1;
		else if (strcmp(*argv, "-s") == 0) standalone = 1;
		else if (strcmp(*argv, "-m") == 0) create++;
		else if (strcmp(*argv, "-c") == 0)
		{
			if (argc < 4) usage(myname);

			create++;
			dbsource_name = argv[1];
			dbsource_addr = argv[2];
			dbsource_tag = argv[3];
			argc -= 3;
			argv += 3;
		}
		else usage(myname);

		argc--;
		argv++;
	}

	if (argc != 1) usage(myname);

	if (debug == 0)
	{
		closeall();
		if (standalone == 1) daemon(1, 1);
	}

	db_tag = malloc(strlen(argv[0]) + 1);
	strcpy(db_tag, argv[0]);

	str = malloc(strlen("netinfod ") + strlen(db_tag) + 1);
	sprintf(str, "netinfod %s", db_tag);
	system_log_open(str, (LOG_NDELAY | LOG_PID), LOG_NETINFO, logf);
	free(str);
	system_log_set_max_priority(log_pri);

	system_log(LOG_DEBUG, "version %s (pid %d) - starting",
		_PROJECT_VERSION_, getpid());

	rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_CORE, &rlim);

	rlim.rlim_cur = rlim.rlim_max = FD_SETSIZE;
	setrlimit(RLIMIT_NOFILE, &rlim);

	umask(S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

	srandom(gethostid() ^ time(NULL));

	readall_syslock = syslock_new(0);
	lockup_syslock= syslock_new(0);
	cleanupwait = CLEANUPWAIT;
	auth_count[GOOD] = 0;
	auth_count[BAD] = 0;
	auth_count[WGOOD] = 0;
	auth_count[WBAD] = 0;

	if (create)
	{
		if (dbsource_addr == NULL)
		{
			system_log(LOG_DEBUG, "creating master");
			status = dir_mastercreate(db_tag);
		}
		else
		{
			system_log(LOG_DEBUG, "creating clone");
			status = dir_clonecreate(db_tag, dbsource_name,
				dbsource_addr, dbsource_tag);
		}

		if (status != NI_OK)
		{
			system_log_close();
			exit(status);
		}
	}
	
	nctoken = -1;
	notify_register_signal(NETWORK_CHANGE_NOTIFICATION, SIGHUP, &nctoken);
	
	if (standalone == 0) signal(SIGTERM, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, (void *)catch_sighup);
	signal(SIGCHLD, (void *)readall_catcher);
	if (debug == 0)
	{
		signal(SIGINT, (void *)dblock_catcher);
		if (standalone == 0)
		{
			if (setsid() < 0) syslog(LOG_WARNING, "setsid failed: %m");
		}
	}

	writepid(db_tag);

	status = start_service(db_tag);
	if (status != NI_OK)
	{
		system_log(LOG_ERR, "start_service failed: %s - exiting", ni_error(status));
		system_log_close();
		exit(status);
	}

	setproctitle("netinfod %s (%s)", db_tag, i_am_clone ? "clone" : "master");

	if (i_am_clone)
	{
		system_log(LOG_DEBUG, "checking clone");
		cloneReadallResponseOK = get_clone_readall(db_ni);
		dir_clonecheck();
		if (get_sanitycheck(db_ni)) sanitycheck(db_tag);
		system_log(LOG_DEBUG, "finished clone check");
	}
	else
	{
		system_log(LOG_DEBUG, "setting up master server");
		promote_admins = get_promote_admins(db_ni);
		get_readall_info(db_ni, &max_readall_proxies, &strict_proxies);
		max_subthreads = get_max_subthreads(db_ni);
		update_latency_secs = get_update_latency(db_ni);

		/* Tracking readall proxy pids uses ObjC, so isolate it */
		initialize_readall_proxies(-1 == max_readall_proxies ?
			MAX_READALL_PROXIES : max_readall_proxies);

		system_log(LOG_DEBUG, "starting notify thread");
		(void) notify_start();
	}

	/* Shutdown gracefully after this point */
	if (standalone == 1) signal(SIGTERM, (void *)sig_shutdown);
	signal(SIGUSR1, (void *)sig_shutdown);

	system_log(LOG_DEBUG, "starting RPC service");

	ni_svc_run(_rpc_dtablesize() - (FD_SLOPSIZE + max_subthreads));

	system_log(LOG_DEBUG, "shutting down");

	/*
	 * Tell the readall proxies to shut down
	 */
	if (readall_proxies > 0)
	{
		system_log(LOG_INFO, "killing %d readall prox%s", readall_proxies,
			1 == readall_proxies ? "y" : "ies");
		if (!kill_proxies())
			system_log(LOG_WARNING, "some readall proxies still running");
	}

	db_checksum = ni_getchecksum(db_ni);
	ni_shutdown(db_ni, db_checksum);
	system_log(LOG_INFO, "exiting; checksum %u", db_checksum);
	system_log_close();
	exit(0);
}


static ni_status
register_it(ni_name tag)
{
	SVCXPRT *transp;
	ni_status status;
	unsigned udp_port;
	unsigned tcp_port;
	struct sockaddr_in addr;
	bool_t desktop;
	int ret;

	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = get_port(db_ni, NULL);
	if ((addr.sin_port == 0) && !strcmp(tag, "local")) addr.sin_port = htons(LOCAL_PORT);

	desktop = is_desktop(db_ni);

	if (desktop) addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	transp = svcudp_bind(RPC_ANYSOCK, addr);
	if (transp == NULL) return NI_SYSTEMERR;

	ret = svc_register(transp, NI_PROG, NI_VERS, ni_prog_2, 0);
	if (ret == 0)
	{
		system_log(LOG_DEBUG, "udp svc_register returned %d\n", ret);
		return NI_SYSTEMERR;
	}

	udp_port = transp->xp_port;
	udp_sock = transp->xp_sock;

	transp = svctcp_bind(RPC_ANYSOCK, addr, NI_SENDSIZE, NI_RECVSIZE);
	if (transp == NULL) return NI_SYSTEMERR;

	ret = svc_register(transp, NI_PROG, NI_VERS, ni_prog_2, 0);
	if (ret == 0)
	{
		system_log(LOG_DEBUG, "tcp svc_register returned %d\n", ret);
		return NI_SYSTEMERR;
	}

	tcp_port = transp->xp_port;
	tcp_sock = transp->xp_sock;

	if ((forcedIsRoot == 0) && (ni_name_match(tag, "local")))
		waitforparent();

	if (standalone == 0)
	{
		system_log(LOG_DEBUG, "registering %s tcp %u udp %u", tag, tcp_port, udp_port);

		status = ni_register(tag, udp_port, tcp_port);
		if (status != NI_OK)
		{
			system_log(LOG_DEBUG, "ni_register: %s", tag, ni_error(status));
			return (status);
		}
	}

	return (NI_OK);
}

static ni_status
start_service(ni_name tag)
{
	ni_name master;
	ni_name mastertag;
	ni_status status;
	ni_name dbname;
	struct in_addr inaddr;

	system_log(LOG_DEBUG, "directory cleanup");
	dir_cleanup(tag);
	dir_getnames(tag, &dbname, NULL, NULL);

	system_log(LOG_DEBUG, "initializing server");
	status = ni_init(dbname, &db_ni);
	ni_name_free(&dbname);
	if (status != NI_OK)
	{
		system_log(LOG_ERR, "ni_init failed: %s", ni_error(status));
		return (status);
	}

	system_log(LOG_DEBUG, "checksum = %u", ni_getchecksum(db_ni));

	/* "local" is never a clone */
	if (strcmp(tag, "local"))
	{
		if (getmaster(db_ni, &master, &mastertag))
		{
			inaddr.s_addr = getaddress(db_ni, master);
			if (!sys_is_my_address(&inaddr)) i_am_clone++;
			if (!ni_name_match(tag, mastertag)) i_am_clone++;
			ni_name_free(&master);
			ni_name_free(&mastertag);
		}
	}

	if (forcedIsRoot == 0)
		forcedIsRoot = get_forced_root(db_ni);

	system_log(LOG_DEBUG, "registering tag %s", tag);
	status = register_it(tag);
	return (status);
}

static void
sig_shutdown(void)
{
	shutdown_server++;
}

static ni_status
ni_register(ni_name tag, unsigned udp_port, unsigned tcp_port)
{
	nibind_registration reg;
	ni_status status;
	CLIENT *cl;
	int sock;
	struct sockaddr_in sin;
	struct timeval tv;

	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	sin.sin_port = 0;
	sin.sin_family = AF_INET;
	bzero(sin.sin_zero, sizeof(sin.sin_zero));
	sock = socket_connect(&sin, NIBIND_PROG, NIBIND_VERS);
	if (sock < 0) return (NI_SYSTEMERR);

	tv.tv_sec = NIBIND_TIMEOUT / (NIBIND_RETRIES + 1);
	tv.tv_usec = 0;
	cl = clnttcp_create(&sin, NIBIND_PROG, NIBIND_VERS, &sock, 0, 0);
	if (cl == NULL)
	{
		socket_close(sock);
		return (NI_SYSTEMERR);
	}

	reg.tag = tag;
	reg.addrs.udp_port = udp_port;
	reg.addrs.tcp_port = tcp_port;
	tv.tv_sec = NIBIND_TIMEOUT;
	if (clnt_call(cl, NIBIND_REGISTER, xdr_nibind_registration,
		  &reg, xdr_ni_status, &status, tv) != RPC_SUCCESS)
	{
		clnt_destroy(cl);
		socket_close(sock);
		return (NI_SYSTEMERR);
	}
	clnt_destroy(cl);
	socket_close(sock);
	return (status);
}

static void 
usage(char *myname)
{
	fprintf(stderr, "usage: netinfod [-m] [-l log_priority] [-c name addr tag] tag\n");
	exit(1);
}

#ifdef MALLOC_DEBUG
void  catch_malloc_problems(int problem)
{
	abort();
}
#endif

void writepid(ni_name tag)
{
	FILE *fp;
	char *fname;
	int pid;

	pid = getpid();

	fname = (char *)malloc(strlen(tag) + strlen(PID_FILE) + 1);
	sprintf(fname, PID_FILE, tag);

	fp = fopen(fname, "w");
	if (fp == NULL)
	{
		system_log(LOG_ERR, "Cannot open PID file %s", fname);
		free(fname);
		return;
	}

	fprintf(fp, "%d\n", pid);
	if (fclose(fp) != 0) system_log(LOG_ERR, "error closing PID file '%s': %m", fname);
	free(fname);
}

/*VARARGS1*/
void setproctitle(char *fmt, ...)
{
	va_list ap;
	char *last, *p;
	int i, len, arglen;
	char buf[NI_NAME_MAXLEN + BUFSIZ];	/* Message buffer */

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);

	last = Argv[Argc - 1] + strlen(Argv[Argc - 1]);
	p = Argv[0];
	arglen = last - p;

	len = strlen(buf);
	if (len > arglen) return;

	(void)strcpy(p, buf);
	p += len;
	for (i = len; i < arglen; i++) *p++ = ' ';
}

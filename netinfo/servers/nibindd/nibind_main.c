/*
 * nibindd - NetInfo binder
 * Copyright 1989-94, NeXT Computer Inc.
 */
#include <NetInfo/config.h>
#include <NetInfo/project_version.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <stdio.h>
#include <libc.h>
#include <stdlib.h>
#include <string.h>
#include <netinfo/ni.h>
#include <sys/dir.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <syslog.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <errno.h>
#include <NetInfo/mm.h>
#include <NetInfo/system_log.h>
#include <NetInfo/rpc_extra.h>

#ifndef LOG_NETINFO
#define LOG_NETINFO LOG_DAEMON
#endif

extern int debug;

/* getppid() should be in libc.h, but it isn't */
extern int getppid(void);

extern int svc_maxfd;

#ifdef _UNIX_BSD_43_
const char NETINFO_PROG[] = "/usr/etc/netinfod";
const char NIBINDD_PROG[] = "/usr/etc/nibindd";
const char PID_FILE[] = "/etc/nibindd.pid";
#else
const char NETINFO_PROG[] = "/usr/sbin/netinfod";
const char NIBINDD_PROG[] = "/usr/sbin/nibindd";
const char PID_FILE[] = "/var/run/nibindd.pid";
#endif

#ifdef _OS_VERSION_MACOS_X_
const char NETINFO_DIR[] = "/var/db/netinfo";
#else
#ifdef _OS_VERSION_DARWIN_
const char NETINFO_DIR[] = "/var/db/netinfo";
#else
const char NETINFO_DIR[] = "/etc/netinfo";
#endif
#endif

void nibind_svc_run(void);
extern void nibind_prog_1();

int isnidir(char *, ni_name *);

extern void catchchild(int);
extern void killchildren(int);
extern void respawn(void);
extern void storepid(int, ni_name);

static void parentexit(int);
static void closeall(void);
static void writepid(void);
static void killparent(void);
static void catchhup(int);

extern int waitreg;
static int restart;

int udp_sock = -1;

int
main(int argc, char *argv[])
{
	SVCXPRT *utransp, *ttransp;
	struct sockaddr_in addr;
	DIR *dp;
	struct direct *d;
	ni_name tag = NULL;
	ni_namelist nl;
	ni_index i;
	int pid, localonly, nctoken = -1;
	int log_pri = LOG_NOTICE;
	struct rlimit rlim;
	char *netinfod_argv[16]; /* XXX */
	int netinfod_argc, x;
	union wait wait_stat;
	pid_t child_pid;
	char *pri;
#ifdef _UNIX_BSD_43_
	int ttyfd;
#endif

	localonly = 1;

	netinfod_argc = 0;
	netinfod_argv[netinfod_argc++] = (char *)NETINFO_PROG;

	debug = 0;

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-n"))
		{
			netinfod_argv[netinfod_argc++] = argv[i];
		}

		if (!strcmp(argv[i], "-d"))
		{
			debug = 1;
			log_pri = LOG_DEBUG;
			if ((argc > (i+1)) && (argv[i+1][0] != '-'))
				debug = atoi(argv[++i]);
		}

		if (!strcmp(argv[i], "-l"))
		{
			if ((argc > (i+1)) && (argv[i+1][0] != '-'))
				log_pri = atoi(argv[++i]);
		}

		if (!strcmp(argv[i], "-D"))
		{
			netinfod_argv[netinfod_argc++] = "-d";

			if ((argc > (i+1)) && (argv[i+1][0] != '-'))
			{
				netinfod_argv[netinfod_argc++] = argv[i];
			}
		}

		if (!strcmp(argv[i], "-L"))
		{
			netinfod_argv[netinfod_argc++] = "-l";

			if ((argc > (i+1)) && (argv[i+1][0] != '-'))
			{
				netinfod_argv[netinfod_argc++] = argv[i];
			}
			else
			{
				pri = malloc(sizeof("999"));
				sprintf(pri, "%d", LOG_DEBUG);
				netinfod_argv[netinfod_argc++] = pri;
			}
		}
	}

	if (debug == 1)
	{
		system_log_open("nibindd", LOG_NDELAY | LOG_PID, LOG_NETINFO, stderr);
		system_log_set_max_priority(log_pri);
		system_log(LOG_DEBUG, "version %s - debug mode\n", _PROJECT_VERSION_);
	}
	else
	{
		closeall();
		system_log_open("nibindd", LOG_NDELAY | LOG_PID, LOG_NETINFO, NULL);
		system_log_set_max_priority(log_pri);
		system_log(LOG_DEBUG, "version %s - starting\n", _PROJECT_VERSION_);

		child_pid = fork();
		if (child_pid == -1)
		{
			system_log(LOG_ALERT, "fork() failed: %m, aborting");
			system_log_close();
			exit(1);
		}
		else if (child_pid > 0)
		{
			signal(SIGTERM, parentexit);
			system_log(LOG_DEBUG, "parent waiting for child to start");
			wait4(child_pid, (_WAIT_TYPE_ *)&wait_stat, 0, 0);

			if (WIFEXITED(wait_stat))
			{
				system_log(LOG_DEBUG,
					"unexpected child exit, status=%d",
					WEXITSTATUS(wait_stat));
			}
			else
			{
				system_log(LOG_DEBUG,
					"unexpected child exit, received signal=%d",
					WTERMSIG(wait_stat));
			}
			system_log_close();
			exit(1);
		}
	}

	restart = 0;

	rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
	setrlimit(RLIMIT_CORE, &rlim);
	signal(SIGCHLD, catchchild);
	signal(SIGTERM, killchildren);
	signal(SIGHUP, catchhup);
	signal(SIGINT, SIG_IGN);

	notify_register_signal(NETWORK_CHANGE_NOTIFICATION, SIGHUP, &nctoken);

	writepid();

	/*
	 * cd to netinfo directory, find out which databases should
	 * be served and lock the directory before registering service.
	 */
	if (chdir(NETINFO_DIR) < 0)
	{
		killparent();
		system_log(LOG_ALERT, "cannot chdir to netinfo directory");
		exit(1);
	}

	dp = opendir(NETINFO_DIR);
	if (dp == NULL)
	{
		killparent();
		system_log(LOG_ALERT, "cannot open netinfo directory");
		exit(1);
	}

	MM_ZERO(&nl);
	while ((d = readdir(dp)))
	{
		if (isnidir(d->d_name, &tag))
		{
			if (ni_namelist_match(nl, tag) == NI_INDEX_NULL)
			{
				system_log(LOG_DEBUG, "found database: %s", tag);
				ni_namelist_insert(&nl, tag, NI_INDEX_NULL);
				if (strcmp(tag, "local")) localonly = 0;
			} 
			ni_name_free(&tag);
		}
	}

#ifdef _NETINFO_FLOCK_
	/*
	 * Do not close the directory: keep it locked so another nibindd
	 * won't run.
	 */
	if (flock(dp->dd_fd, LOCK_EX|LOCK_NB) < 0)
	{
		killparent();
		system_log(LOG_ALERT, "nibindd already running");
		exit(1);
	}
	fcntl(dp->dd_fd, F_SETFD, 1);
#else
	closedir(dp);
#endif

	/*
	 * Register as a SUNRPC service
	 */
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	if (localonly == 1) addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	pmap_unset(NIBIND_PROG, NIBIND_VERS);
	utransp = svcudp_bind(RPC_ANYSOCK, addr);
	if (utransp == NULL)
	{
		killparent();
		system_log(LOG_ALERT, "cannot start udp service");
		exit(1);
	}

	if (!svc_register(utransp, NIBIND_PROG, NIBIND_VERS, nibind_prog_1, IPPROTO_UDP))
	{
		killparent();
		system_log(LOG_ALERT, "cannot register udp service");
		exit(1);
	}

	udp_sock = utransp->xp_sock;

	ttransp = svctcp_bind(RPC_ANYSOCK, addr, 0, 0);
	if (ttransp == NULL)
	{
		killparent();
		system_log(LOG_ALERT, "cannot start tcp service");
		exit(1);
	}

	if (!svc_register(ttransp, NIBIND_PROG, NIBIND_VERS, nibind_prog_1, IPPROTO_TCP))
	{
		killparent();
		system_log(LOG_ALERT, "cannot register tcp service");
		exit(1);
	}

	waitreg = 0;
	for (i = 0; i < nl.ninl_len; i++)
	{
		netinfod_argv[netinfod_argc] = nl.ninl_val[i];
		netinfod_argv[netinfod_argc + 1] = NULL;

		system_log(LOG_DEBUG, "starting netinfod %s", nl.ninl_val[i]);
		system_log(LOG_DEBUG, "execv debug 0: %s", NETINFO_PROG);
		for (x = 0; netinfod_argv[x] != NULL; x++)
		{
			system_log(LOG_DEBUG, "execv debug %d: %s", x, netinfod_argv[x]);
		}

		pid = fork();
		if (pid == 0)
		{
			/* child */
			execv(NETINFO_PROG, netinfod_argv);
			exit(-1);
		}

#ifdef DEBUG
		system_log(LOG_DEBUG, "netinfod %s pid = %d", nl.ninl_val[i], pid);
#endif

		if (pid > 0)
		{
			waitreg++;
			storepid(pid, nl.ninl_val[i]);
		}
		else
		{
			system_log(LOG_ERR, "server for tag %s failed to start", nl.ninl_val[i]);
		}
	}

	ni_namelist_free(&nl);
		
	/*
	 * Detach from controlling tty.
	 * Do this AFTER starting netinfod so "type c to continue..." works.
	 */
#ifdef _UNIX_BSD_43_
	ttyfd = open("/dev/tty", O_RDWR, 0);
	if (ttyfd > 0)
	{
		ioctl(ttyfd, TIOCNOTTY, NULL);
		close(ttyfd);
	}

	setpgrp(0, getpid());
#else
	if (setsid() < 0) syslog(LOG_ERR, "nibindd: setsid() failed: %m");
#endif

	system_log(LOG_DEBUG, "starting RPC service");

	nibind_svc_run();
	system_log(LOG_ALERT, "svc_run returned");
	system_log_close();
	exit(1);
}

static char *suffixes[] = {
	".nidb",
	".move",
	".temp",
	NULL
};

int
isnidir(char *dir, ni_name *tag)
{
	char *s;
	int i;
	
	s = rindex(dir, '.');
	if (s == NULL) {
		return (0);
	}
	for (i = 0; suffixes[i] != NULL; i++) {
		if (ni_name_match(s, suffixes[i])) {
			*tag = ni_name_dup(dir);
			s = rindex(*tag, '.');
			*s = 0;
			return (1);
		}
	}
	return (0);
}

static void
closeall(void)
{
	int i;

	for (i = getdtablesize() - 1; i >= 0; i--) close(i);

	open("/dev/null", O_RDWR, 0);
	dup(0);
	dup(0);
}

static void
killparent(void)
{
	kill(getppid(), SIGTERM);
}

void
nibind_svc_run(void)
{
	fd_set readfds;

	for (;;)
	{
		readfds = svc_fdset;
		switch (select(svc_maxfd+1, &readfds, NULL, NULL, NULL))
		{
			case -1:
				if (errno != EINTR)
				{
					system_log(LOG_ALERT,
						"unexpected errno: %m, aborting");
				}
				break;

			case 0:
				break;
			default:
				svc_getreqset(&readfds);
				break;
		}

		if (waitreg == 0)
		{
			waitreg = -1;
			if (debug == 1)
			{
				system_log(LOG_DEBUG, "all servers registered");
			}
			else 
			{
				system_log(LOG_DEBUG,
					"all servers registered - signalling parent");
				killparent();
			}
		}

		if (restart == 1)
		{
			svc_unregister(NIBIND_PROG, NIBIND_VERS);
			pmap_unset(NIBIND_PROG, NIBIND_VERS);
			respawn();
		}
	}
}
	 
static void
parentexit(int x)
{
	system_log(LOG_DEBUG, "parent exiting");
	system_log_close();
	exit(0);
}

static void
catchhup(int sig)
{
	restart = 1;
}

static void
writepid(void)
{
	FILE *fp;

	fp = fopen(PID_FILE, "w");
	if (fp != NULL)
	{
		fprintf(fp, "%d\n", getpid());
		fclose(fp);
	}
}

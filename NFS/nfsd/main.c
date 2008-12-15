/*
 * Copyright (c) 1999-2008 Apple Inc.  All rights reserved.
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
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <notify.h>
#include <errno.h>
#include <err.h>
#include <pthread.h>
#include <dns_sd.h>

#include <mach/mach.h>
#include <mach/host_special_ports.h>

#include <sys/syslog.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/mount.h>
#include <sys/sysctl.h>

#include <libutil.h>
#include <launch.h>

#include <netinet/in.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>

#include "lockd_mach.h"
#include "pathnames.h"
#include "common.h"

#define GETOPT			"F:Nn:P:p:Rrtuv"

#define MAX_NFSD_THREADS	128

const struct nfs_conf_server config_defaults =
{
	0,		/* async */
	1,		/* bonjour */
	1,		/* fsevents */
	0,		/* mount_port */
	0,		/* mount_regular_files */
	1,		/* mount_require_resv_port */
	8,		/* nfsd_threads */
	NFS_PORT,	/* port */
	64,		/* reqcache_size */
	128,		/* request_queue_length */
	0,		/* require_resv_port */
	1,		/* tcp */
	1,		/* udp */
	1,		/* user_stats */
	0,		/* verbose */
	1000,		/* wg_delay */
	0,		/* wg_delay_v3 */
};

/* globals */
pthread_attr_t pattr;
struct nfs_conf_server config;
char exportsfilepath[MAXPATHLEN];
volatile int gothup, gotterm;
int checkexports = 0, log_to_stderr = 0;
int mountudpport = 0, mounttcpport = 0;
time_t recheckexports = 0;

DNSServiceRef nfs_dns_service;

static int config_read(struct nfs_conf_server *);
static void config_sanity_check(struct nfs_conf_server *conf);
static void config_sysctl_changed(struct nfs_conf_server *, struct nfs_conf_server *);
static void config_loop(void);

static pid_t get_pid(const char *);
static pid_t get_nfsd_pid(void);
static void signal_nfsd(int);
static void sigmux(int);

static int service_is_enabled(char *);
static int nfsd_is_enabled(void);
static int nfsd_is_loaded(void);
static int nfsd_is_running(void);

static int nfsd_enable(void);
static int nfsd_disable(void);
static int nfsd_load(void);
static int nfsd_unload(void);
static int nfsd_start(void);
static int nfsd_stop(void);

static void register_services(void);
static int safe_exec(char **, int);
static void do_lockd_ping(void);
static void do_lockd_shutdown(void);
static int rquotad_start(void);
static int rquotad_stop(void);

static void
usage(void)
{
	fprintf(stderr, "usage: nfsd [-NRrtuv] [-F export_file] [-n num_servers] "
			"[-p nfsport] [-P mountport] [command]\n");
	fprintf(stderr, "commands: enable, disable, start, stop, restart, update, status, checkexports, verbose [up|down]\n");
	exit(1);
}

int
main(int argc, char *argv[], char *envp[])
{
	struct pidfh *nfsd_pfh, *mountd_pfh;
	pid_t pid;
	struct stat st;
	int ch, reregister, rv;
	int tcpflag, udpflag, protocnt;
	int nfsdcnt, nfsport, mountport;
	int mount_require_resv_port = 1;
	int mount_regular_files = 0;
	extern int optind;

	/* set defaults then do config_read() to get config values */
	config = config_defaults;
	config_read(&config);

	/* init command-line flags */
	reregister = 0;
	nfsdcnt = 0;
	protocnt = tcpflag = udpflag = 0;
	nfsport = mountport = 0;
	exportsfilepath[0] = '\0';

	while ((ch = getopt(argc, argv, GETOPT)) != EOF)
		switch (ch) {
		// nfsd
		case 'n':
			nfsdcnt = atoi(optarg);
			break;
		case 'p':
			nfsport = atoi(optarg);
			break;
		case 'r':
			reregister = 1;
			break;
		case 't':
			tcpflag = 1;
			protocnt++;
			break;
		case 'u':
			udpflag = 1;
			protocnt++;
			break;
		// mountd
		case 'F':
			strlcpy(exportsfilepath, optarg, MAXPATHLEN);
			break;
		case 'N':
			mount_require_resv_port = 0;
			break;
		case 'P':
			mountport = atoi(optarg);
			break;
		case 'R':
			mount_regular_files = 1;
			break;
		// miscellaneous
		case 'v':
			config.verbose++;
			break;
		default:
		case '?':
			usage();
		};
	argv += optind;
	argc -= optind;

	/* set config values for flags specified */
	if (nfsdcnt)
		config.nfsd_threads = nfsdcnt;
	if (protocnt) {
		config.tcp = tcpflag;
		config.udp = udpflag;
	}
	if (nfsport)
		config.port = nfsport;
	if (mountport)
		config.mount_port = mountport;
	if (!mount_require_resv_port)
		config.mount_require_resv_port = mount_require_resv_port;
	if (mount_regular_files)
		config.mount_regular_files = mount_regular_files;
	if (!exportsfilepath[0])
		strcpy(exportsfilepath, _PATH_EXPORTS);

	if (reregister || (argc > 0))
		log_to_stderr = 1;

	if (reregister) {
		signal_nfsd(SIGHUP);
		exit(0);
	}

	rv = 0;

	if (argc > 0) {
		/* process the given, unprivileged command */
		if (!strcmp(argv[0], "status")) {
			int enabled = nfsd_is_enabled();
			printf("nfsd service is %s\n", enabled ? "enabled" : "disabled");
			pid = get_nfsd_pid();
			if (pid <= 0) {
				printf("nfsd is not running\n");
			} else {
				int cur = 0;
				sysctl_get("vfs.generic.nfs.server.nfsd_thread_count", &cur);
				printf("nfsd is running (pid %d, %d threads)\n", pid, cur);
			}
			rv = enabled ? 0 : 1;
			if (config.verbose) {
				/* print info about related daemons too */
				/* lockd */
				enabled = service_is_enabled(_LOCKD_SERVICE_LABEL);
				printf("lockd service is %s\n", enabled ? "enabled" : "disabled");
				pid = get_pid(_PATH_LOCKD_PID);
				if (pid <= 0)
					printf("lockd is not running\n");
				else
					printf("lockd is running (pid %d)\n", pid);
				/* statd.notify */
				enabled = service_is_enabled(_STATD_NOTIFY_SERVICE_LABEL);
				printf("statd.notify service is %s\n", enabled ? "enabled" : "disabled");
				pid = get_pid(_PATH_STATD_NOTIFY_PID);
				if (pid <= 0)
					printf("statd.notify is not running\n");
				else
					printf("statd.notify is running (pid %d)\n", pid);
				/* statd */
				pid = get_pid(_PATH_STATD_PID);
				if (pid <= 0)
					printf("statd is not running\n");
				else
					printf("statd is running (pid %d)\n", pid);
				/* rquotad */
				pid = get_pid(_PATH_RQUOTAD_PID);
				if (pid <= 0)
					printf("rquotad is not running\n");
				else
					printf("rquotad is running (pid %d)\n", pid);
			}
			exit(rv);
		} else if (!strcmp(argv[0], "checkexports")) {
			checkexports = 1;
			mountd_init();
			rv = get_exportlist();
			exit(rv);
		}
	}

	if (getuid()) {
		printf("Sorry, nfsd must be run as root\n");
		printf("unprivileged usage: nfsd [ status | [-F file] checkexports]\n");
		/* try to make sure the nfsd service isn't loaded in the per-user launchd */
		if (nfsd_is_loaded())
			nfsd_unload();
		exit(2);
	}

	if (argc > 0) {
		/* process the given, privileged command */
		if (!strcmp(argv[0], "enable")) {
			if (!nfsd_is_enabled()) {
				rv = nfsd_enable();
			} else {
				printf("The nfsd service is already enabled.\n");
				/* make sure it's running */
				if (!nfsd_is_running())
					rv = nfsd_is_loaded() ? nfsd_start() : nfsd_load();
			}
		} else if (!strcmp(argv[0], "disable")) {
			if (nfsd_is_enabled()) {
				rv = nfsd_disable();
			} else {
				printf("The nfsd service is already disabled.\n");
				if (nfsd_is_loaded())
					rv = nfsd_unload();
			}
		} else if (!strcmp(argv[0], "start")) {
			if (nfsd_is_running()) {
				printf("The nfsd service is already running.\n");
			} else {
				printf("Starting the nfsd service%s\n",
					nfsd_is_enabled() ? "" : " (use 'enable' to make permanent)");
				rv = nfsd_is_loaded() ? nfsd_start() : nfsd_load();
			}
		} else if (!strcmp(argv[0], "stop")) {
			if (!nfsd_is_running()) {
				printf("The nfsd service is not running.\n");
			} else {
				printf("Stopping the nfsd service%s\n",
					!nfsd_is_enabled() ? "" : " (use 'disable' to make permanent)");
				rv = nfsd_unload();
			}
		} else if (!strcmp(argv[0], "restart")) {
			if (!nfsd_is_running() || !nfsd_is_loaded())
				printf("The nfsd service does not appear to be running.\n");
			if (nfsd_is_running()) {
				/* should be immediately restarted if /etc/exports exists */
				rv = nfsd_stop();
			} else {
				printf("Starting the nfsd service%s\n",
					nfsd_is_enabled() ? "" : " (use 'enable' to permanently enable)");
				rv = nfsd_is_loaded() ? nfsd_start() : nfsd_load();
			}
		} else if (!strcmp(argv[0], "update")) {
			signal_nfsd(SIGHUP);
		} else if (!strcmp(argv[0], "verbose")) {
			argc--;
			argv++;
			for (;argc;argc--,argv++) {
				if (!strcmp(argv[0], "up"))
					signal_nfsd(SIGUSR1);
				else if (!strcmp(argv[0], "down"))
					signal_nfsd(SIGUSR2);
				else
					errx(1, "unknown verbose command: %s", argv[0]);
				usleep(100000);
			}
		} else {
			warnx("unknown command: %s", argv[0]);
			usage();
		}
		exit(rv);
	}

	config_sanity_check(&config);

	pthread_attr_init(&pattr);
	pthread_attr_setdetachstate(&pattr, PTHREAD_CREATE_DETACHED);

	/* set up signal handling */
	signal(SIGQUIT, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGSYS, sigmux);
	signal(SIGTERM, sigmux);
	signal(SIGHUP, sigmux);
	signal(SIGUSR1, sigmux);
	signal(SIGUSR2, sigmux);

	/* set up logging */
	openlog(NULL, LOG_PID, LOG_DAEMON);
	setlogmask(LOG_UPTO(LOG_LEVEL));

	/* we really shouldn't be running if there's no exports file */
	if (stat(exportsfilepath, &st)) {
		/* exports file doesn't exist, so just unload ourselves */
		log(LOG_WARNING, "no exports file, unloading nfsd service");
		rv = nfsd_unload();
		exit(rv);
	}

	/* claim PID files */
	nfsd_pfh = pidfile_open(_PATH_NFSD_PID, 0644, &pid);
	if (nfsd_pfh == NULL) {
		log(LOG_ERR, "can't open nfsd pidfile: %s (%d)", strerror(errno), errno);
		if ((errno == EACCES) && getuid())
			log(LOG_ERR, "nfsd is expected to be run as root, not as uid %d.", getuid());
		else if (errno == EEXIST)
			log(LOG_ERR, "nfsd already running, pid: %d", pid);
		exit(2);
	}
	if (pidfile_write(nfsd_pfh) == -1)
		log(LOG_WARNING, "can't write to nfsd pidfile: %s (%d)", strerror(errno), errno);

	mountd_pfh = pidfile_open(_PATH_MOUNTD_PID, 0644, &pid);
	if (mountd_pfh == NULL) {
		log(LOG_ERR, "can't open mountd pidfile: %s (%d)", strerror(errno), errno);
		if (errno == EEXIST)
			log(LOG_ERR, "mountd already running, pid: %d", pid);
		exit(2);
	}
	if (pidfile_write(mountd_pfh) == -1)
		log(LOG_WARNING, "can't write to mountd pidfile: %s (%d)", strerror(errno), errno);

	log(LOG_NOTICE, "nfsd starting");
	if (config.verbose)
		log(LOG_NOTICE, "verbose level set to %d", config.verbose);

	/* set up sysctl config values */
	config_sysctl_changed(NULL, &config);

	/* initialize/start mountd */
	mountd();

	/* initialize/start nfsd */
	nfsd();

	/* make sure rpc.lockd is running */
	do_lockd_ping();

	/* make sure rpc.rquotad is running */
	rquotad_start();

	/* tell others about our services */
	register_services();

	/* main thread loops to handle config updates */
	config_loop();

	/* nfsd is exiting... */
	sysctl_set("vfs.generic.nfs.server.nfsd_thread_max", 0);

	/* tell lockd to prepare to shut down */
	do_lockd_shutdown();

	/* stop rpc.rquotad */
	rquotad_stop();

	/* clean up */
	alarm(1); /* XXX 5028243 in case pmap_unset() gets hung up during shutdown */
	pmap_unset(RPCPROG_NFS, 2);
	pmap_unset(RPCPROG_NFS, 3);
	pmap_unset(RPCPROG_MNT, 1);
	pmap_unset(RPCPROG_MNT, 3);
	if (nfs_dns_service)
		DNSServiceRefDeallocate(nfs_dns_service);

	/* and get out */
	pidfile_remove(mountd_pfh);
	pidfile_remove(nfsd_pfh);
	exit(0);
}

/*
 * read the NFS server values from nfs.conf
 */
static int
config_read(struct nfs_conf_server *conf)
{
	FILE *f;
	size_t len, linenum = 0;
	char *line, *p, *key, *value;
	long val;

	if (!(f = fopen(_PATH_NFS_CONF, "r"))) {
		if (errno != ENOENT)
			log(LOG_WARNING, "%s", _PATH_NFS_CONF);
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
			DEBUG(3, "%4ld %s=%s", linenum, key, value ? value : "");
			continue;
		}

		val = !value ? 1 : strtol(value, NULL, 0);
		DEBUG(2, "%4ld %s=%s (%d)", linenum, key, value ? value : "", val);

		if (!strcmp(key, "nfs.server.async")) {
			conf->async = val;
		} else if (!strcmp(key, "nfs.server.bonjour")) {
			conf->bonjour = val;
		} else if (!strcmp(key, "nfs.server.fsevents")) {
			conf->fsevents = val;
		} else if (!strcmp(key, "nfs.server.mount.port")) {
			conf->mount_port = val;
		} else if (!strcmp(key, "nfs.server.mount.regular_files")) {
			conf->mount_regular_files = val;
		} else if (!strcmp(key, "nfs.server.mount.require_resv_port")) {
			conf->mount_require_resv_port = val;
		} else if (!strcmp(key, "nfs.server.nfsd_threads")) {
			conf->nfsd_threads = val;
		} else if (!strcmp(key, "nfs.server.port")) {
			conf->port = val;
		} else if (!strcmp(key, "nfs.server.reqcache_size")) {
			conf->reqcache_size = val;
		} else if (!strcmp(key, "nfs.server.request_queue_length")) {
			conf->request_queue_length = val;
		} else if (!strcmp(key, "nfs.server.require_resv_port")) {
			conf->require_resv_port = val;
		} else if (!strcmp(key, "nfs.server.tcp")) {
			conf->tcp = val;
		} else if (!strcmp(key, "nfs.server.udp")) {
			conf->udp = val;
		} else if (!strcmp(key, "nfs.server.user_stats")) {
			conf->user_stats = val;
		} else if (!strcmp(key, "nfs.server.verbose")) {
			conf->verbose = val;
		} else if (!strcmp(key, "nfs.server.wg_delay")) {
			conf->wg_delay = val;
		} else if (!strcmp(key, "nfs.server.wg_delay_v3")) {
			conf->wg_delay_v3 = val;
		} else {
			DEBUG(1, "ignoring unknown config value: %4ld %s=%s", linenum, key, value ? value : "");
		}

	}

	fclose(f);
	return (0);
}

/*
 * sanity check config values
 */
static void
config_sanity_check(struct nfs_conf_server *conf)
{
	if (conf->nfsd_threads < 1) {
		warnx("nfsd thread count %d; reset to %d", conf->nfsd_threads, config_defaults.nfsd_threads);
		conf->nfsd_threads = config_defaults.nfsd_threads;
	} else if (conf->nfsd_threads > MAX_NFSD_THREADS) {
		warnx("nfsd thread count %d; limited to %d", conf->nfsd_threads, MAX_NFSD_THREADS);
		conf->nfsd_threads = MAX_NFSD_THREADS;
	}
}

/*
 * config_loop()
 *
 * Loop until terminated.
 * Just wait for a signal or mount notification.
 */
static void
config_loop(void)
{
	int kq, rv, gotmount = 0, exports_changed;
	struct kevent ke;
	struct nfs_conf_server newconf;
	struct stat st, stnew;
	struct timespec ts = { 10, 0 };

	/* set up mount/unmount kqueue */
	if ((kq = kqueue()) < 0) {
		log(LOG_ERR, "kqueue: %s (%d)", strerror(errno), errno);
		exit(1);
	}
	EV_SET(&ke, 0, EVFILT_FS, EV_ADD, 0, 0, 0);
	rv = kevent(kq, &ke, 1, NULL, 0, NULL);
	if (rv < 0) {
		log(LOG_ERR, "kevent(EVFILT_FS): %s (%d)", strerror(errno), errno);
		exit(1);
	}

	/* get baseline stat values for exports file */
	stat(exportsfilepath, &st);

	while (!gotterm) {

		DEBUG(1, "config_loop: waiting...");
		rv = kevent(kq, NULL, 0, &ke, 1, ((recheckexports > 0) ? &ts : NULL));
		if ((rv > 0) && !(ke.flags & EV_ERROR) && (ke.fflags & (VQ_MOUNT|VQ_UNMOUNT))) {
			log(LOG_INFO, "mount list changed: 0x%x", ke.fflags);
			gotmount = check_for_mount_changes();
		}
		if (recheckexports > 0)  {	/* make sure we check the exports again */
			if (!gotmount)
				log(LOG_INFO, "rechecking exports");
			gotmount = 1;
		}

		while (!gotterm && (gothup || gotmount)) {
			if (gothup) {
				DEBUG(1, "handling HUP");
				newconf = config_defaults;
				if (!config_read(&newconf)) {
					config_sanity_check(&newconf);
					/* if port/transport/reqcachesize change detected exit to initiate a restart */
					if ((newconf.port != config.port) || (newconf.mount_port != config.mount_port) ||
					    (newconf.tcp != config.tcp) || (newconf.udp != config.udp) ||
					    (newconf.reqcache_size != config.reqcache_size)) {
						/* port, transport, and reqcache size changes require a restart */
						if (newconf.reqcache_size != config.reqcache_size)
							log(LOG_NOTICE, "request cache size change (%d -> %d) requires restart",
								config.reqcache_size, newconf.reqcache_size);
						if ((newconf.port != config.port) || (newconf.mount_port != config.mount_port) ||
						    (newconf.tcp != config.tcp) || (newconf.udp != config.udp))
							log(LOG_NOTICE, "port/transport changes require restart");
						gotterm = 1;
						break;
					}
					config_sysctl_changed(&config, &newconf);
					/* update nfsd_thread_max in kernel */
					sysctl_set("vfs.generic.nfs.server.nfsd_thread_max", newconf.nfsd_threads);
					/* launch any new nfsd threads */
					if (newconf.nfsd_threads > config.nfsd_threads)
						nfsd_start_server_threads(newconf.nfsd_threads - config.nfsd_threads);
					/* make new config current */
					config = newconf;
				}
				/* ping lockd, in case it needs to be restarted */
				do_lockd_ping();
				/* make sure rquotad is running */
				rquotad_start();
				/* reregister services */
				register_services();
			}

			/* check if it looks like the exports file changed */
			if (stat(exportsfilepath, &stnew)) {
				exports_changed = 0;
			} else {
				exports_changed =
					(stnew.st_dev != st.st_dev) || (stnew.st_ino != st.st_ino) ||
					(stnew.st_ctimespec.tv_sec != st.st_ctimespec.tv_sec) ||
					(stnew.st_ctimespec.tv_nsec != st.st_ctimespec.tv_nsec);
				st = stnew;
			}

			/* clear export errors on HUP or exports file change */
			if (exports_changed && clear_export_errors(0))
				log(LOG_WARNING, "exports file changed: previous errors cleared");

			gotmount = gothup = 0;
			get_exportlist();
		}
	}
}

/*
 * set config's sysctl values
 */
static void
config_sysctl_changed(struct nfs_conf_server *old, struct nfs_conf_server *new)
{
	if (!old || (old->async != new->async))
		sysctl_set("vfs.generic.nfs.server.async", new->async);
	if (!old || (old->fsevents != new->fsevents))
		sysctl_set("vfs.generic.nfs.server.fsevents", new->fsevents);
	if (!old) /* should only be set at startup */
		sysctl_set("vfs.generic.nfs.server.reqcache_size", new->reqcache_size);
	if (!old || (old->request_queue_length != new->request_queue_length))
		sysctl_set("vfs.generic.nfs.server.request_queue_length", new->request_queue_length);
	if (!old || (old->require_resv_port != new->require_resv_port))
		sysctl_set("vfs.generic.nfs.server.require_resv_port", new->require_resv_port);
	if (!old || (old->user_stats != new->user_stats))
		sysctl_set("vfs.generic.nfs.server.user_stats", new->user_stats);
	if (!old || (old->wg_delay != new->wg_delay))
		sysctl_set("vfs.generic.nfs.server.wg_delay", new->wg_delay);
	if (!old || (old->wg_delay_v3 != new->wg_delay_v3))
		sysctl_set("vfs.generic.nfs.server.wg_delay_v3", new->wg_delay_v3);
}

/*
 * get a sysctl config value
 */
int
sysctl_get(const char *name, int *val)
{
	size_t size = sizeof(int);

	return sysctlbyname(name, val, &size, NULL, 0);
}

/*
 * set a sysctl config value
 */
int
sysctl_set(const char *name, int val)
{
	int rv = sysctlbyname(name, NULL, 0, &val, sizeof(val));
	DEBUG(1, "sysctl_set: %s = %d, rv %d", name, val, rv);
	return (rv);
}

/*
 * all threads besides the main thread should have signals blocked.
 */
void
set_thread_sigmask(void)
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
}

/*
 * generic, flag-setting signal handler
 */
static void
sigmux(int sig)
{
	switch (sig) {
	case SIGHUP:
		gothup = 1;
		log(LOG_NOTICE, "SIGHUP\n");
		break;
	case SIGSYS:
		log(LOG_ERR, "missing system call: NFS not available.");
		/*FALLTHRU*/
	case SIGTERM:
		gotterm = 1;
		break;
	case SIGUSR1:
		config.verbose++;
		setlogmask(LOG_UPTO(LOG_LEVEL));
		log(LOG_WARNING, "verbose level is now %d\n", config.verbose);
		break;
	case SIGUSR2:
		if (config.verbose > 0)
			config.verbose--;
		setlogmask(LOG_UPTO(LOG_LEVEL));
		log(LOG_WARNING, "verbose level is now %d\n", config.verbose);
		break;
	}
}

/*
 * XXX The following code should be removed when xnu is in sync with
 * the build machine's /usr/include/mach/host_special_ports.h
 */
#ifndef HOST_LOCKD_PORT
#define HOST_LOCKD_PORT                 (5 + HOST_MAX_SPECIAL_KERNEL_PORT)

#define host_get_lockd_port(host, port)	\
	(host_get_special_port((host), 			\
	HOST_LOCAL_NODE, HOST_LOCKD_PORT, (port)))
#endif

/*
 * Start up lockd, (which in turn should start up statd)
 * We use a mach host special port to send a null mach message. If lockd
 * is not running, launchd should have the receive right and will start
 * lockd which will then ack our ping.
 */
static void
do_lockd_ping(void)
{
	kern_return_t kr;
	mach_port_t mp;

	kr = host_get_lockd_port(mach_host_self(), &mp);
	if (kr != KERN_SUCCESS || !MACH_PORT_VALID(mp)) {
		log(LOG_ERR, "Can't get lockd mach port!");
		return;
	}
	kr = lockd_ping(mp);
	mach_port_destroy(mach_task_self(), mp);
	if (kr != KERN_SUCCESS) {
		log(LOG_ERR, "Lockd did not start!");
		return;
	}
	return;
}

/*
 * Inform lockd to prepare for shutting down.
 */
static void
do_lockd_shutdown(void)
{
	kern_return_t kr;
	mach_port_t mp;

	kr = host_get_lockd_port(mach_host_self(), &mp);
	if (kr != KERN_SUCCESS || !MACH_PORT_VALID(mp)) {
		log(LOG_ERR, "Can't get lockd mach port!");
		return;
	}
	kr = lockd_shutdown(mp);
	mach_port_destroy(mach_task_self(), mp);
	if (kr != KERN_SUCCESS) {
		log(LOG_ERR, "lockd shutdown failed!");
		return;
	}
	return;
}

/*
 * register NFS and MOUNT services with portmap
 */
static void
register_services(void)
{
	/* register NFS/MOUNT services with portmap */

	/* nfsd */
	pmap_unset(RPCPROG_NFS, 2);
	pmap_unset(RPCPROG_NFS, 3);
	if (config.udp &&
	    (!pmap_set(RPCPROG_NFS, 2, IPPROTO_UDP, config.port) ||
	     !pmap_set(RPCPROG_NFS, 3, IPPROTO_UDP, config.port)))
		log(LOG_ERR, "can't register NFS/UDP service.");
	if (config.tcp &&
	    (!pmap_set(RPCPROG_NFS, 2, IPPROTO_TCP, config.port) ||
	     !pmap_set(RPCPROG_NFS, 3, IPPROTO_TCP, config.port)))
		log(LOG_ERR, "can't register NFS/TCP service.");

	/* mountd */
	pmap_unset(RPCPROG_MNT, 1);
	pmap_unset(RPCPROG_MNT, 3);
	if (config.udp &&
	    (!pmap_set(RPCPROG_MNT, 1, IPPROTO_UDP, mountudpport) ||
	     !pmap_set(RPCPROG_MNT, 3, IPPROTO_UDP, mountudpport)))
		log(LOG_ERR, "can't register MOUNT/UDP service.");
	if (config.tcp &&
	    (!pmap_set(RPCPROG_MNT, 1, IPPROTO_TCP, mounttcpport) ||
	     !pmap_set(RPCPROG_MNT, 3, IPPROTO_TCP, mounttcpport)))
		log(LOG_ERR, "can't register MOUNT/TCP service.");

	/* Register NFS exports with service discovery mechanism(s). */

	/* bonjour */
	if (nfs_dns_service) {
		DNSServiceRefDeallocate(nfs_dns_service);
		nfs_dns_service = NULL;
	}
	if (config.bonjour && (config.tcp || config.udp)) {
		DNSServiceErrorType dserr;
		dserr = DNSServiceRegister(&nfs_dns_service, 0, 0, NULL,
				config.tcp ? "_nfs._tcp" : "_nfs._udp",
				NULL, NULL, htons(config.port), 0, NULL, NULL, NULL);
		if (dserr != kDNSServiceErr_NoError) {
			log(LOG_ERR, "DNSServiceRegister(_nfs._tcp) failed with %d\n", dserr);
			nfs_dns_service = NULL;
		}
	}
}

/*
 * our own little logging function...
 */
void
SYSLOG(int pri, const char *fmt, ...)
{
	va_list ap;

	if (pri > LOG_LEVEL)
		return;

	va_start(ap, fmt);
	if (log_to_stderr) {
		vfprintf(stderr, fmt, ap);
		fputc('\n', stderr);
		fflush(stderr);
	} else {
		vsyslog(pri, fmt, ap);
	}
	va_end(ap);
}

/*
 * get the PID from the given pidfile
 */
static pid_t
get_pid(const char *path)
{
	char pidbuf[128], *pidend;
	int fd, len, rv;
	pid_t pid;
	struct flock lock;

	if ((fd = open(path, O_RDONLY)) < 0) {
		DEBUG(3, "%s: %s (%d)", path, strerror(errno), errno);
		return (0);
	}
	len = sizeof(pidbuf) - 1;
	if ((len = read(fd, pidbuf, len)) < 0) {
		DEBUG(1, "%s: %s (%d)", path, strerror(errno), errno);
		return (0);
	}

	/* parse PID */
	pidbuf[len] = '\0';
	pid = strtol(pidbuf, &pidend, 10);
	if (!len || (pid < 1)) {
		DEBUG(1, "%s: bogus pid: %s", path, pidbuf);
		return (0);
	}

	/* check for lock on file by PID */
	lock.l_type = F_RDLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;
	rv = fcntl(fd, F_GETLK, &lock);
	close(fd);
	if (rv != 0) {
		DEBUG(1, "%s: fcntl: %s (%d)", path, strerror(errno), errno);
		return (0);
	} else if (lock.l_type == F_UNLCK) {
		DEBUG(1, "%s: not locked", path);
		return (0);
	}
	return (pid);
}

/*
 * get the PID of the running nfsd
 */
static pid_t
get_nfsd_pid(void)
{
	return (get_pid(_PATH_NFSD_PID));
}

/*
 * send the running nfsd a SIGHUP to get it to update its config
 */
static void
signal_nfsd(int signal)
{
	pid_t pid = get_nfsd_pid();
	if (pid <= 0)
		errx(1, "nfsd not running?");
	if (kill(pid, signal) < 0)
		err(1, "kill(%d, %d)", pid, signal);
}

/*
 * Check whether the given service appears to be enabled.
 */
static int
service_is_enabled(char *service)
{
	char *args[] = { _PATH_SERVICE, "--test-if-configured-on", service, NULL };
	return (safe_exec(args, 1) == 0);
}

/*
 * Check whether the nfsd service appears to be enabled.
 */
static int
nfsd_is_enabled(void)
{
	return service_is_enabled(_NFSD_SERVICE_LABEL);
}

/*
 * Check whether the nfsd service is loaded.
 */
static int
nfsd_is_loaded(void)
{
	char *args[] = { _PATH_LAUNCHCTL, "list", _NFSD_SERVICE_LABEL, NULL };
	return (safe_exec(args, 1) == 0);
}

/*
 * Use launchctl to enable the nfsd service.
 */
static int
nfsd_enable(void)
{
	char *args[] = { _PATH_LAUNCHCTL, "load", "-w", _PATH_NFSD_PLIST, NULL };
	return safe_exec(args, 0);
}

/*
 * Use launchctl to disable the nfsd service.
 */
static int
nfsd_disable(void)
{
	char *args[] = { _PATH_LAUNCHCTL, "unload", "-w", _PATH_NFSD_PLIST, NULL };
	return safe_exec(args, 0);
}

/*
 * Use launchctl to load the nfsd service.
 */
static int
nfsd_load(void)
{
	char *args[] = { _PATH_LAUNCHCTL, "load", "-F", _PATH_NFSD_PLIST, NULL };
	return safe_exec(args, 0);
}

/*
 * Use launchctl to unload the nfsd service.
 */
static int
nfsd_unload(void)
{
	char *args[] = { _PATH_LAUNCHCTL, "unload", _PATH_NFSD_PLIST, NULL };
	return safe_exec(args, 0);
}

/*
 * Use launchctl to start the nfsd service.
 */
static int
nfsd_start(void)
{
	char *args[] = { _PATH_LAUNCHCTL, "start", _NFSD_SERVICE_LABEL, NULL };
	return safe_exec(args, 0);
}

/*
 * Use launchctl to stop the nfsd service.
 */
static int
nfsd_stop(void)
{
	char *args[] = { _PATH_LAUNCHCTL, "stop", _NFSD_SERVICE_LABEL, NULL };
	return safe_exec(args, 0);
}

/*
 * Check whether the nfsd service appears to be running.
 */
static int
nfsd_is_running(void)
{
	return (get_nfsd_pid() > 0);
}

/*
 * run an external program
 */
static int
safe_exec(char *argv[], int silent)
{
	int pid, status;

	if ((pid = fork()) == 0) {
		if (silent) {
			close(0);
			close(1);
			close(2);
			dup(dup(open("/dev/null", O_RDWR)));
		}
		execv(argv[0], argv);
		log(LOG_ERR, "Exec of %s failed: %s (%d)", argv[0],
			strerror(errno), errno);
		exit(2);
	}
	if (pid == -1) {
		log(LOG_ERR, "Fork for %s failed: %s (%d)", argv[0],
			strerror(errno), errno);
		return (1);
	}
	while ((waitpid(pid, &status, 0) == -1) && (errno == EINTR))
		usleep(1000);
	if (WIFSIGNALED(status)) {
		log(LOG_ERR, "%s aborted by signal %d", argv[0], WTERMSIG(status));
		return (1);
	} else if (WIFSTOPPED(status)) {
		log(LOG_ERR, "%s stopped by signal %d ?", argv[0], WSTOPSIG(status));
		return (1);
	} else if (WEXITSTATUS(status) && !silent) {
		log(LOG_ERR, "%s exited with status %d", argv[0], WEXITSTATUS(status));
	}
	return (WEXITSTATUS(status));
}

/*
 * functions for managing rquotad
 */
static int
rquotad_is_loaded(void)
{
	launch_data_t msg, resp;
	int rv = 0;

	msg = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
	if (!msg)
		return (0);
	launch_data_dict_insert(msg, launch_data_new_string(_RQUOTAD_SERVICE_LABEL), LAUNCH_KEY_GETJOB);

	resp = launch_msg(msg);
	if (resp) {
		if (launch_data_get_type(resp) == LAUNCH_DATA_DICTIONARY)
			rv = 1;
		launch_data_free(resp);
	} else {
		syslog(LOG_ERR, "launch_msg(): %m");
	}

	launch_data_free(msg);
	return (rv);
}

static int
rquotad_load(void)
{
	launch_data_t msg, job, args, resp;
	int rv = 1;

	msg = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
	job = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
	args = launch_data_alloc(LAUNCH_DATA_ARRAY);
	if (!msg || !job || !args) {
		if (msg) launch_data_free(msg);
		if (job) launch_data_free(job);
		if (args) launch_data_free(args);
		return (1);
	}
	launch_data_array_set_index(args, launch_data_new_string(_PATH_RQUOTAD), 0);
	launch_data_dict_insert(job, launch_data_new_string(_RQUOTAD_SERVICE_LABEL), LAUNCH_JOBKEY_LABEL);
	launch_data_dict_insert(job, launch_data_new_bool(FALSE), LAUNCH_JOBKEY_ONDEMAND);
	launch_data_dict_insert(job, args, LAUNCH_JOBKEY_PROGRAMARGUMENTS);
	launch_data_dict_insert(msg, job, LAUNCH_KEY_SUBMITJOB);

	resp = launch_msg(msg);
	if (!resp)
		rv = errno;
	else if (launch_data_get_type(resp) == LAUNCH_DATA_ERRNO)
		rv = launch_data_get_errno(resp);

	launch_data_free(msg);
	return (rv);
}

static int
rquotad_service_start(void)
{
	launch_data_t msg, resp;
	int rv = 1;

	msg = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
	if (!msg)
		return (1);
	launch_data_dict_insert(msg, launch_data_new_string(_RQUOTAD_SERVICE_LABEL), LAUNCH_KEY_STARTJOB);

	resp = launch_msg(msg);
	if (!resp)
		rv = errno;
	else if (launch_data_get_type(resp) == LAUNCH_DATA_ERRNO)
		rv = launch_data_get_errno(resp);

	launch_data_free(msg);
	return (rv);
}

static int
rquotad_start(void)
{
	if (get_pid(_PATH_RQUOTAD_PID) > 0)
		return (0);
	else if (rquotad_is_loaded())
		return (rquotad_service_start());
	return (rquotad_load());
}

static int
rquotad_stop(void)
{
	launch_data_t msg, resp;
	int rv = 1;

	msg = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
	if (!msg)
		return (1);
	launch_data_dict_insert(msg, launch_data_new_string(_RQUOTAD_SERVICE_LABEL), LAUNCH_KEY_REMOVEJOB);

	resp = launch_msg(msg);
	if (!resp)
		rv = errno;
	else if (launch_data_get_type(resp) == LAUNCH_DATA_ERRNO)
		rv = launch_data_get_errno(resp);

	launch_data_free(msg);
	return (rv);
}


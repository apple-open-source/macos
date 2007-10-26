/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_time.h>
#include <servers/bootstrap.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <pthread.h>
#include <dirent.h>
#include <dlfcn.h>
#include <libgen.h>
#include <notify.h>
#include "daemon.h"

#define SERVICE_NAME "com.apple.system.logger"
#define SERVER_STATUS_ERROR -1
#define SERVER_STATUS_INACTIVE 0
#define SERVER_STATUS_ACTIVE 1
#define SERVER_STATUS_ON_DEMAND 2

#define DEFAULT_MARK_SEC 0
#define DEFAULT_SWEEP_SEC 3600
#define DEFAULT_FIRST_SWEEP_SEC 300
#define DEFAULT_UTMP_TTL_SEC 31622400
#define DEFAULT_FS_TTL_SEC 31622400
#define DEFAULT_TTL_SEC 86400
#define DEFAULT_BSD_MAX_DUP_SEC 30
#define BILLION 1000000000

#define NOTIFY_DELAY 1

#define NETWORK_CHANGE_NOTIFICATION "com.apple.system.config.network_change"

#define streq(A,B) (strcmp(A,B)==0)
#define forever for(;;)

static int reset = 0;
static double mach_time_factor = 1.0;

static TAILQ_HEAD(ml, module_list) Moduleq;

/* global */
int asl_log_filter = ASL_FILTER_MASK_UPTO(ASL_LEVEL_NOTICE);
int archive_enable = 0;
int restart = 0;
int debug = 0;
mach_port_t server_port = MACH_PORT_NULL;
launch_data_t launch_dict;
const char *debug_file = NULL;

extern int __notify_78945668_info__;
extern int _malloc_no_asl_log;

/* monitor the database */
uint64_t db_max_size = 25600000;
uint64_t db_curr_size = 0;
uint64_t db_shrink_size;
uint64_t db_curr_empty = 0;
uint64_t db_max_empty;

/* kernel log fd */
extern int kfd;

/* timers */
uint64_t time_last = 0;
uint64_t time_start = 0;
uint64_t mark_last = 0;
uint64_t mark_time = 0;
uint64_t sweep_last = 0;
uint64_t sweep_time = DEFAULT_SWEEP_SEC;
uint64_t first_sweep_delay = DEFAULT_FIRST_SWEEP_SEC;
uint64_t bsd_flush_time = 0;
uint64_t bsd_max_dup_time = DEFAULT_BSD_MAX_DUP_SEC;

time_t utmp_ttl = DEFAULT_UTMP_TTL_SEC;
time_t fs_ttl = DEFAULT_FS_TTL_SEC;
time_t ttl = DEFAULT_TTL_SEC;

/* Static Modules */
int asl_in_init();
int asl_in_reset();
int asl_in_close();
static int activate_asl_in = 1;

int asl_action_init();
int asl_action_reset();
int asl_action_close();
static int activate_asl_action = 1;

int klog_in_init();
int klog_in_reset();
int klog_in_close();
static int activate_klog_in = 1;

int bsd_in_init();
int bsd_in_reset();
int bsd_in_close();
static int activate_bsd_in = 1;

int bsd_out_init();
int bsd_out_reset();
int bsd_out_close();
static int activate_bsd_out = 1;

int udp_in_init();
int udp_in_reset();
int udp_in_close();
static int activate_udp_in = 1;

extern void database_server();
extern void db_worker();
extern void launchd_drain();
extern void bsd_flush_duplicates();

/*
 * Module approach: only one type of module.  This module may implement
 * the set of functions necessary for any of the functions (input, output,
 * etc.)  Prior to using the modules, dlsym() is consulted to see what it
 * implements.
 */

static int
static_modules()
{
	struct module_list *tmp;

	/*
	 * The order of these initializations is important.
	 * When messages are sent to output modules, they are
	 * sent in the same order as these initializations.
	 * asl_action may add modify messages, for example to
	 * add access controls, so it must come first.
	 */
	if (activate_asl_action != 0)
	{
		tmp = calloc(1, sizeof(struct module_list));
		if (tmp == NULL) return 1;

		tmp->name = strdup("asl_action");
		if (tmp->name == NULL)
		{
			free(tmp);
			return 1;
		}

		tmp->module = NULL;
		tmp->init = asl_action_init;
		tmp->reset = asl_action_reset;
		tmp->close = asl_action_close;
		TAILQ_INSERT_TAIL(&Moduleq, tmp, entries);
	}

	if (activate_asl_in != 0)
	{
		tmp = calloc(1, sizeof(struct module_list));
		if (tmp == NULL) return 1;

		tmp->name = strdup("asl_in");
		if (tmp->name == NULL)
		{
			free(tmp);
			return 1;
		}

		tmp->module = NULL;
		tmp->init = asl_in_init;
		tmp->reset = asl_in_reset;
		tmp->close = asl_in_close;
		TAILQ_INSERT_TAIL(&Moduleq, tmp, entries);
	}

	if (activate_klog_in != 0)
	{
		tmp = calloc(1, sizeof(struct module_list));
		if (tmp == NULL) return 1;

		tmp->name = strdup("klog_in");
		if (tmp->name == NULL)
		{
			free(tmp);
			return 1;
		}

		tmp->module = NULL;
		tmp->init = klog_in_init;
		tmp->reset = klog_in_reset;
		tmp->close = klog_in_close;
		TAILQ_INSERT_TAIL(&Moduleq, tmp, entries);
	}

	if (activate_bsd_in != 0)
	{
		tmp = calloc(1, sizeof(struct module_list));
		if (tmp == NULL) return 1;

		tmp->name = strdup("bsd_in");
		if (tmp->name == NULL)
		{
			free(tmp);
			return 1;
		}

		tmp->module = NULL;
		tmp->init = bsd_in_init;
		tmp->reset = bsd_in_reset;
		tmp->close = bsd_in_close;
		TAILQ_INSERT_TAIL(&Moduleq, tmp, entries);
	}

	if (activate_bsd_out != 0)
	{
		tmp = calloc(1, sizeof(struct module_list));
		if (tmp == NULL) return 1;

		tmp->name = strdup("bsd_out");
		if (tmp->name == NULL)
		{
			free(tmp);
			return 1;
		}

		tmp->module = NULL;
		tmp->init = bsd_out_init;
		tmp->reset = bsd_out_reset;
		tmp->close = bsd_out_close;
		TAILQ_INSERT_TAIL(&Moduleq, tmp, entries);
	}

	if (activate_udp_in != 0)
	{
		tmp = calloc(1, sizeof(struct module_list));
		if (tmp == NULL) return 1;

		tmp->name = strdup("udp_in");
		if (tmp->name == NULL)
		{
			free(tmp);
			return 1;
		}

		tmp->module = NULL;
		tmp->init = udp_in_init;
		tmp->reset = udp_in_reset;
		tmp->close = udp_in_close;
		TAILQ_INSERT_TAIL(&Moduleq, tmp, entries);
	}

	return 0;
}

/*
 * Loads all the modules.  This DOES NOT call the modules initializer
 * functions.  It simply scans the modules directory looking for modules
 * and loads them.  This does not mean the module will be used.  
 */
static int
load_modules(const char *mp)
{
	DIR *d;
	struct dirent *de;
	struct module_list *tmp;
	void *c, *bn;
	char *modulepath = NULL;

	d = opendir(mp);
	if (d == NULL) return -1;

	while (NULL != (de = readdir(d)))
	{
		if (de->d_name[0] == '.') continue;

		/* Must have ".so" in the name" */
		if (!strstr(de->d_name, ".so")) continue;

		asprintf(&modulepath, "%s/%s", mp, de->d_name);
		if (!modulepath) continue;

		c = dlopen(modulepath, RTLD_LOCAL);
		if (c == NULL)
		{
			free(modulepath);
			continue;
		}

		tmp = calloc(1, sizeof(struct module_list));
		if (tmp == NULL)
		{
			free(modulepath);
			dlclose(c);
			continue;
		}

		bn = basename(modulepath);
		tmp->name = strdup(bn);
		if (tmp->name == NULL)
		{
			free(tmp);
			return 1;
		}

		tmp->module = c;
		TAILQ_INSERT_TAIL(&Moduleq, tmp, entries);

		tmp->init = dlsym(tmp->module, "aslmod_init");
		tmp->reset = dlsym(tmp->module, "aslmod_reset");
		tmp->close = dlsym(tmp->module, "aslmod_close");

		free(modulepath);
	}

	closedir(d);

	return 0;
}

static void
writepid(void)
{
	FILE *fp;

	fp = fopen(_PATH_PIDFILE, "w");
	if (fp != NULL)
	{
		fprintf(fp, "%d\n", getpid());
		fclose(fp);
	}
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
detach(void)
{
	signal(SIGINT, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	setsid();
}

static void
catch_sighup(int x)
{
	reset = 1;
}

static void
send_reset(void)
{
	struct module_list *mod;

	for (mod = Moduleq.tqh_first; mod != NULL; mod = mod->entries.tqe_next)
	{
		if (mod->reset != NULL) mod->reset();
	}
}

/*
 * perform timed activities and set next run-loop timeout
 */
static void
timed_events(struct timeval **run)
{
	uint64_t nanonow, now, delta, t;
	static struct timeval next;
	double d;

	nanonow = mach_absolute_time();
	d = nanonow;
	d = d * mach_time_factor;
	nanonow = d;
	now = nanonow / 1000000000;

	*run = NULL;
	next.tv_sec = 0;
	next.tv_usec = 0;

	if (time_start == 0)
	{
		/* startup */
		time_start = now;
		time_last = now;
		mark_last = now;

		/* fake sweep_last so that we plan to run first_sweep_delay seconds after startup */
		sweep_last = (now - sweep_time) + first_sweep_delay;
	}

	/*
	 * At startup, we try sending a notification once a second.
	 * Once it succeeds, we set the Libc global __notify_78945668_info__ to 0
	 * which lets Libc's localtime calculations use notifyd to invalidate
	 * cached timezones.  This prevents a deadlock in localtime. 
	 */
	if (__notify_78945668_info__ < 0)
	{
		if (notify_post("com.apple.system.syslogd") == NOTIFY_STATUS_OK) __notify_78945668_info__ = 0;
		else next.tv_sec = 1;
	}

	if (time_last > now)
	{
		/*
		 * Despite Albert Einstein's assurance, time has gone backward.
		 * Reset "last" times to current time.
		 */
		time_last = now;
		sweep_last = now;
		mark_last = now;
	}

	/*
	 * Run database archiver.
	 */
	if (sweep_time > 0)
	{
		delta = now - sweep_last;
		if (delta >= sweep_time)
		{
			db_archive(time(NULL) - ttl, db_shrink_size);
			sweep_last = now;
			t = sweep_time;
		}
		else
		{
			t = sweep_time - delta;
		}

		if (next.tv_sec == 0) next.tv_sec = t;
		else if (t < next.tv_sec) next.tv_sec = t;
	}

	/*
	 * Shrink the database if it's too large.
	 */
	if (db_curr_size > db_max_size)
	{
		db_archive(time(NULL) - ttl, db_shrink_size);
	}

	/*
	 * Compact the database if there are too many empty slots.
	 */
	if (db_curr_empty > db_max_empty)
	{
		db_compact();
	}

	/*
	 * Tickle bsd_out module to flush duplicates.
	 */
	if (bsd_flush_time > 0)
	{
		bsd_flush_duplicates();
		if (bsd_flush_time > 0)
		{
			if (next.tv_sec == 0) next.tv_sec = bsd_flush_time;
			else if (bsd_flush_time < next.tv_sec) next.tv_sec = bsd_flush_time;
		}
	}

	/*
	 * Send MARK
	 */
	if (mark_time > 0)
	{
		delta = now - mark_last; 
		if (delta >= mark_time)
		{
			asl_mark();
			mark_last = now;
			t = mark_time;
		}
		else
		{
			t = mark_time - delta;
		}

		if (next.tv_sec == 0) next.tv_sec = t;
		else if (t < next.tv_sec) next.tv_sec = t;
	}

	/*
	 * set output timeout parameter if runloop needs to have a timer
	 */
	if (next.tv_sec > 0) *run = &next;

	time_last = now;
}

void
init_config()
{
	launch_data_t tmp, pdict;
	kern_return_t status;

	tmp = launch_data_new_string(LAUNCH_KEY_CHECKIN);
	launch_dict = launch_msg(tmp);
	launch_data_free(tmp);

	if (launch_dict == NULL)
	{
		fprintf(stderr, "%d launchd checkin failed\n", getpid());
		exit(1);
	}

	tmp = launch_data_dict_lookup(launch_dict, LAUNCH_JOBKEY_MACHSERVICES);
	if (tmp == NULL)
	{
		fprintf(stderr, "%d launchd lookup of LAUNCH_JOBKEY_MACHSERVICES failed\n", getpid());
		exit(1);
	}

	pdict = launch_data_dict_lookup(tmp, SERVICE_NAME);
	if (pdict == NULL)
	{
		fprintf(stderr, "%d launchd lookup of SERVICE_NAME failed\n", getpid());
		exit(1);
	}

	server_port = launch_data_get_machport(pdict);

	status = mach_port_insert_right(mach_task_self(), server_port, server_port, MACH_MSG_TYPE_MAKE_SEND);
	if (status != KERN_SUCCESS) fprintf(stderr, "Warning! Can't make send right for server_port: %x\n", status);
}

int
main(int argc, const char *argv[])
{
	struct module_list *mod;
	fd_set rd, wr, ex, kern;
	int32_t fd, i, max, status, daemonize;
	const char *mp;
	struct timeval *runloop_timer, zto;
	pthread_attr_t attr;
	pthread_t t;
	int nctoken;
	mach_timebase_info_data_t info;
	double mtn, mtd;
	char tstr[32];
	time_t now;

	mp = _PATH_MODULE_LIB;
	daemonize = 0;
	__notify_78945668_info__ = -1;
	_malloc_no_asl_log = 1; /* prevent malloc from calling ASL on error */
	kfd = -1;
	zto.tv_sec = 0;
	zto.tv_usec = 0;
	FD_ZERO(&kern);
	debug_file = NULL;

	memset(&info, 0, sizeof(mach_timebase_info_data_t));
	mach_timebase_info(&info);

	mtn = info.numer;
	mtd = info.denom;
	mach_time_factor = mtn / mtd;

	for (i = 1; i < argc; i++)
	{
		if (streq(argv[i], "-d"))
		{
			debug = 1;
			if (((i+1) < argc) && (argv[i+1][0] != '-')) debug_file = argv[++i];
			memset(tstr, 0, sizeof(tstr));
			now = time(NULL);
			ctime_r(&now, tstr);
			tstr[19] = '\0';
			asldebug("%s syslogd[%d]: Start\n", tstr, getpid());
		}
		else if (streq(argv[i], "-D"))
		{
			daemonize = 1;
		}
		else if (streq(argv[i], "-m"))
		{
			if ((i + 1) < argc) mark_time = 60 * atoll(argv[++i]);
		}
		else if (streq(argv[i], "-utmp_ttl"))
		{
			if ((i + 1) < argc) utmp_ttl = atol(argv[++i]);
		}
		else if (streq(argv[i], "-fs_ttl"))
		{
			if ((i + 1) < argc) fs_ttl = atol(argv[++i]);
		}
		else if (streq(argv[i], "-l"))
		{
			if ((i + 1) < argc) mp = argv[++i];
		}
		else if (streq(argv[i], "-c"))
		{
			if ((i + 1) < argc)
			{
				i++;
				if ((argv[i][0] >= '0') && (argv[i][0] <= '7') && (argv[i][1] == '\0')) asl_log_filter = ASL_FILTER_MASK_UPTO(atoi(argv[i]));
			}
		}
		else if (streq(argv[i], "-a"))
		{
			archive_enable = 1;
		}
		else if (streq(argv[i], "-sweep"))
		{
			if ((i + 1) < argc) sweep_time = atoll(argv[++i]);
		}
		else if (streq(argv[i], "-dup_delay"))
		{
			if ((i + 1) < argc) bsd_max_dup_time = atoll(argv[++i]);
		}
		else if (streq(argv[i], "-ttl"))
		{
			if ((i + 1) < argc) ttl = atol(argv[++i]);
		}
		else if (streq(argv[i], "-db_max"))
		{
			if ((i + 1) < argc) db_max_size = atoll(argv[++i]);
		}
		else if (streq(argv[i], "-asl_in"))
		{
			if ((i + 1) < argc) activate_asl_in = atoi(argv[++i]);
		}
		else if (streq(argv[i], "-asl_action"))
		{
			if ((i + 1) < argc) activate_asl_action = atoi(argv[++i]);
		}
		else if (streq(argv[i], "-klog_in"))
		{
			if ((i + 1) < argc) activate_klog_in = atoi(argv[++i]);
		}
		else if (streq(argv[i], "-bsd_in"))
		{
			if ((i + 1) < argc) activate_bsd_in = atoi(argv[++i]);
		}
		else if (streq(argv[i], "-bsd_out"))
		{
			if ((i + 1) < argc) activate_bsd_out = atoi(argv[++i]);
		}
		else if (streq(argv[i], "-udp_in"))
		{
			if ((i + 1) < argc) activate_udp_in = atoi(argv[++i]);
		}
	}

	db_max_empty = db_max_size / 8;
	db_shrink_size = db_max_size - (db_max_size / 8);

	TAILQ_INIT(&Moduleq);
	static_modules();
	load_modules(mp);
	aslevent_init();

	if (debug == 0)
	{
		if (daemonize != 0)
		{
			if (fork() != 0) exit(0);

			detach();
			closeall();
		}

		writepid();
	}

	init_config();

	signal(SIGHUP, catch_sighup);

	nctoken = -1;
	notify_register_signal(NETWORK_CHANGE_NOTIFICATION, SIGHUP, &nctoken);

	for (mod = Moduleq.tqh_first; mod != NULL; mod = mod->entries.tqe_next)
	{
		fd = mod->init();
		if (fd < 0) continue;
	}

	/*
	 * Start database server thread
	 */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&t, &attr, (void *(*)(void *))database_server, NULL);
	pthread_attr_destroy(&attr);

	/*
	 * Start database worker thread
	 */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&t, &attr, (void *(*)(void *))db_worker, NULL);
	pthread_attr_destroy(&attr);

	FD_ZERO(&rd);
	FD_ZERO(&wr);
	FD_ZERO(&ex);

	/*
	 * drain /dev/klog first
	 */
	if (kfd >= 0)
	{
		max = kfd + 1;
		while (select(max, &kern, NULL, NULL, &zto) > 0)
		{
			aslevent_handleevent(&kern, &wr, &ex);
		}
	}

	/*
	 * Start launchd drain thread
	 */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&t, &attr, (void *(*)(void *))launchd_drain, NULL);
	pthread_attr_destroy(&attr);

	runloop_timer = NULL;
	timed_events(&runloop_timer);

	forever
	{
		max = aslevent_fdsets(&rd, &wr, &ex) + 1;

		status = select(max, &rd, &wr, &ex, runloop_timer);
		if ((kfd >= 0) && FD_ISSET(kfd, &rd))
		{
			/*  drain /dev/klog */
			max = kfd + 1;

			while (select(max, &kern, NULL, NULL, &zto) > 0)
			{
				aslevent_handleevent(&kern, &wr, &ex);
			}
		}

		if (reset != 0)
		{
			send_reset();
			reset = 0;
		}

		if (status != 0) aslevent_handleevent(&rd, &wr, &ex);

		timed_events(&runloop_timer);
 	}
}

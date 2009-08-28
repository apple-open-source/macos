/*
 * Copyright (c) 2004-2009 Apple Inc. All rights reserved.
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
#define DEFAULT_UTMP_TTL_SEC 31622400
#define DEFAULT_FS_TTL_SEC 31622400
#define DEFAULT_BSD_MAX_DUP_SEC 30
#define DEFAULT_MPS_LIMIT 500
#define BILLION 1000000000

#define NOTIFY_DELAY 1

#define NETWORK_CHANGE_NOTIFICATION "com.apple.system.config.network_change"

#define streq(A,B) (strcmp(A,B)==0)
#define forever for(;;)

static uint64_t time_start = 0;
static uint64_t mark_last = 0;
static uint64_t ping_last = 0;
static uint64_t time_last = 0;

extern int __notify_78945668_info__;
extern int _malloc_no_asl_log;

static TAILQ_HEAD(ml, module_list) Moduleq;

/* global */
struct global_s global;

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

int remote_init();
int remote_reset();
int remote_close();
static int activate_remote = 0;

int udp_in_init();
int udp_in_reset();
int udp_in_close();
static int activate_udp_in = 1;

extern void database_server();
extern void output_worker();
extern void launchd_drain();
extern void bsd_flush_duplicates(time_t now);
extern void bsd_close_idle_files(time_t now);

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

	if (activate_remote != 0)
	{
		tmp = calloc(1, sizeof(struct module_list));
		if (tmp == NULL) return 1;

		tmp->name = strdup("remote");
		if (tmp->name == NULL)
		{
			free(tmp);
			return 1;
		}

		tmp->module = NULL;
		tmp->init = remote_init;
		tmp->reset = remote_reset;
		tmp->close = remote_close;
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
	global.reset = RESET_CONFIG;
}

static void
catch_siginfo(int x)
{
	global.reset = RESET_NETWORK;
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
	time_t now, delta, t;
	static struct timeval next;

	now = time(NULL);

	*run = NULL;
	next.tv_sec = 0;
	next.tv_usec = 0;

	if (time_start == 0)
	{
		/* startup */
		time_start = now;
		time_last = now;
		mark_last = now;
		ping_last = now;
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
		mark_last = now;
		ping_last = now;
	}

	/*
	 * Tickle bsd_out module to flush duplicates.
	 */
	if (global.bsd_flush_time > 0)
	{
		bsd_flush_duplicates(now);
		bsd_close_idle_files(now);
		if (global.bsd_flush_time > 0)
		{
			if (next.tv_sec == 0) next.tv_sec = global.bsd_flush_time;
			else if (global.bsd_flush_time < next.tv_sec) next.tv_sec = global.bsd_flush_time;
		}
	}

	/*
	 * Tickle asl_store to sweep file cache
	 */
	if (global.asl_store_ping_time > 0)
	{
		delta = now - ping_last; 
		if (delta >= global.asl_store_ping_time)
		{
			db_ping_store();
			bsd_close_idle_files(now);
			ping_last = now;
			t = global.asl_store_ping_time;
		}
		else
		{
			t = global.asl_store_ping_time - delta;
		}

		if (next.tv_sec == 0) next.tv_sec = t;
		else if (t < next.tv_sec) next.tv_sec = t;
	}

	/*
	 * Send MARK
	 */
	if (global.mark_time > 0)
	{
		delta = now - mark_last; 
		if (delta >= global.mark_time)
		{
			asl_mark();
			mark_last = now;
			t = global.mark_time;
		}
		else
		{
			t = global.mark_time - delta;
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
	global.launch_dict = launch_msg(tmp);
	launch_data_free(tmp);

	if (global.launch_dict == NULL)
	{
		fprintf(stderr, "%d launchd checkin failed\n", getpid());
		exit(1);
	}

	tmp = launch_data_dict_lookup(global.launch_dict, LAUNCH_JOBKEY_MACHSERVICES);
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

	global.server_port = launch_data_get_machport(pdict);

	/* port for receiving internal messages */
	status = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &(global.self_port));
	if (status != KERN_SUCCESS)
	{
		fprintf(stderr, "mach_port_allocate self_port failed: %d", status);
		exit(1);
	}

	status = mach_port_insert_right(mach_task_self(), global.self_port, global.self_port, MACH_MSG_TYPE_MAKE_SEND);
	if (status != KERN_SUCCESS)
	{
		fprintf(stderr, "Can't make send right for self_port: %d\n", status);
		exit(1);
	}

	/* port for receiving MACH_NOTIFY_DEAD_NAME notifications */
	status = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &(global.dead_session_port));
	if (status != KERN_SUCCESS)
	{
		fprintf(stderr, "mach_port_allocate dead_session_port failed: %d", status);
		exit(1);
	}

	status = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &(global.listen_set));
	if (status != KERN_SUCCESS)
	{
		fprintf(stderr, "mach_port_allocate listen_set failed: %d", status);
		exit(1);
	}

	status = mach_port_move_member(mach_task_self(), global.server_port, global.listen_set);
	if (status != KERN_SUCCESS)
	{
		fprintf(stderr, "mach_port_move_member server_port failed: %d", status);
		exit(1);
	}

	status = mach_port_move_member(mach_task_self(), global.self_port, global.listen_set);
	if (status != KERN_SUCCESS)
	{
		fprintf(stderr, "mach_port_move_member self_port failed: %d", status);
		exit(1);
	}

	status = mach_port_move_member(mach_task_self(), global.dead_session_port, global.listen_set);
	if (status != KERN_SUCCESS)
	{
		fprintf(stderr, "mach_port_move_member dead_session_port failed (%u)", status);
		exit(1);
	}
}

void
config_debug(int enable, const char *path)
{
	OSSpinLockLock(&global.lock);

	global.debug = enable;
	if (global.debug_file != NULL) free(global.debug_file);
	global.debug_file = strdup(path);

	OSSpinLockUnlock(&global.lock);
}

void
config_data_store(int type, uint32_t file_max, uint32_t memory_max, uint32_t mini_max)
{
	pthread_mutex_lock(global.db_lock);

	if (global.dbtype & DB_TYPE_FILE)
	{
		asl_store_close(global.file_db);
		global.file_db = NULL;
	}

	if (global.dbtype & DB_TYPE_MEMORY)
	{
		asl_memory_close(global.memory_db);
		global.memory_db = NULL;
	}

	if (global.dbtype & DB_TYPE_MINI)
	{
		asl_mini_memory_close(global.mini_db);
		global.mini_db = NULL;
	}

	global.dbtype = type;
	global.db_file_max = file_max;
	global.db_memory_max = memory_max;
	global.db_mini_max = mini_max;

	pthread_mutex_unlock(global.db_lock);
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
	int network_change_token;
	char tstr[32];
	time_t now;

	memset(&global, 0, sizeof(struct global_s));

	global.db_lock = (pthread_mutex_t *)calloc(1, sizeof(pthread_mutex_t));
	pthread_mutex_init(global.db_lock, NULL);

	global.work_queue_lock = (pthread_mutex_t *)calloc(1, sizeof(pthread_mutex_t));
	pthread_mutex_init(global.work_queue_lock, NULL);

	pthread_cond_init(&global.work_queue_cond, NULL);

	global.work_queue = (asl_search_result_t *)calloc(1, sizeof(asl_search_result_t));

	global.asl_log_filter = ASL_FILTER_MASK_UPTO(ASL_LEVEL_DEBUG);
	global.db_file_max = 16384000;
	global.db_memory_max = 8192;
	global.db_mini_max = 256;
	global.bsd_max_dup_time = DEFAULT_BSD_MAX_DUP_SEC;
	global.utmp_ttl = DEFAULT_UTMP_TTL_SEC;
	global.fs_ttl = DEFAULT_FS_TTL_SEC;
	global.mps_limit = DEFAULT_MPS_LIMIT;
	global.kfd = -1;

#ifdef CONFIG_MAC
	global.dbtype = DB_TYPE_FILE;
	global.db_file_max = 25600000;
	global.asl_store_ping_time = 150;
#endif

#ifdef CONFIG_APPLETV
	global.dbtype = DB_TYPE_FILE;
	global.db_file_max = 10240000;
	global.asl_store_ping_time = 150;
#endif

#ifdef CONFIG_IPHONE
	global.dbtype = DB_TYPE_MINI;
	activate_remote = 1;
	activate_bsd_out = 0;
#endif

	mp = _PATH_MODULE_LIB;
	daemonize = 0;
	__notify_78945668_info__ = 0xf0000000;
	zto.tv_sec = 0;
	zto.tv_usec = 0;

	/* prevent malloc from calling ASL on error */
	_malloc_no_asl_log = 1;

	/* first pass sets up default configurations */
	for (i = 1; i < argc; i++)
	{
		if (streq(argv[i], "-config"))
		{
			if (((i + 1) < argc) && (argv[i+1][0] != '-'))
			{
				i++;
				if (streq(argv[i], "mac"))
				{
					global.dbtype = DB_TYPE_FILE;
					global.db_file_max = 25600000;
				}
				else if (streq(argv[i], "appletv"))
				{
					global.dbtype = DB_TYPE_FILE;
					global.db_file_max = 10240000;
				}
				else if (streq(argv[i], "iphone"))
				{
					global.dbtype = DB_TYPE_MINI;
					activate_remote = 1;
				}
			}
		}
	}

	for (i = 1; i < argc; i++)
	{
		if (streq(argv[i], "-d"))
		{
			global.debug = 1;
			if (((i+1) < argc) && (argv[i+1][0] != '-')) global.debug_file = strdup(argv[++i]);
			memset(tstr, 0, sizeof(tstr));
			now = time(NULL);
			ctime_r(&now, tstr);
			tstr[19] = '\0';
			asldebug("%s syslogd[%d]: Start\n", tstr, getpid());
		}
		else if (streq(argv[i], "-db"))
		{
			if (((i + 1) < argc) && (argv[i+1][0] != '-'))
			{
				i++;
				if (streq(argv[i], "file"))
				{
					global.dbtype |= DB_TYPE_FILE;
					if (((i + 1) < argc) && (argv[i+1][0] != '-')) global.db_file_max = atol(argv[++i]);
				}
				else if (streq(argv[i], "memory"))
				{
					global.dbtype |= DB_TYPE_MEMORY;
					if (((i + 1) < argc) && (argv[i+1][0] != '-')) global.db_memory_max = atol(argv[++i]);
				}
				else if (streq(argv[i], "mini"))
				{
					global.dbtype |= DB_TYPE_MINI;
					if (((i + 1) < argc) && (argv[i+1][0] != '-')) global.db_mini_max = atol(argv[++i]);
				}
			}
		}
		else if (streq(argv[i], "-D"))
		{
			daemonize = 1;
		}
		else if (streq(argv[i], "-m"))
		{
			if ((i + 1) < argc) global.mark_time = 60 * atoll(argv[++i]);
		}
		else if (streq(argv[i], "-utmp_ttl"))
		{
			if ((i + 1) < argc) global.utmp_ttl = atol(argv[++i]);
		}
		else if (streq(argv[i], "-fs_ttl"))
		{
			if ((i + 1) < argc) global.fs_ttl = atol(argv[++i]);
		}
		else if (streq(argv[i], "-mps_limit"))
		{
			if ((i + 1) < argc) global.mps_limit = atol(argv[++i]);
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
				if ((argv[i][0] >= '0') && (argv[i][0] <= '7') && (argv[i][1] == '\0')) global.asl_log_filter = ASL_FILTER_MASK_UPTO(atoi(argv[i]));
			}
		}
		else if (streq(argv[i], "-dup_delay"))
		{
			if ((i + 1) < argc) global.bsd_max_dup_time = atoll(argv[++i]);
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
		else if (streq(argv[i], "-remote"))
		{
			if ((i + 1) < argc) activate_remote = atoi(argv[++i]);
		}
		else if (streq(argv[i], "-udp_in"))
		{
			if ((i + 1) < argc) activate_udp_in = atoi(argv[++i]);
		}
	}

	if (global.dbtype == 0)
	{
		global.dbtype = DB_TYPE_FILE;
		global.db_file_max = 25600000;
		global.asl_store_ping_time = 150;
	}

	TAILQ_INIT(&Moduleq);
	static_modules();
	load_modules(mp);
	aslevent_init();

	if (global.debug == 0)
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
	signal(SIGINFO, catch_siginfo);

	/* register for network change notifications if the udp_in module is active */
	network_change_token = -1;
	if (activate_udp_in != 0) notify_register_signal(NETWORK_CHANGE_NOTIFICATION, SIGINFO, &network_change_token);

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
	 * Start output worker thread
	 */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&t, &attr, (void *(*)(void *))output_worker, NULL);
	pthread_attr_destroy(&attr);

	FD_ZERO(&rd);
	FD_ZERO(&wr);
	FD_ZERO(&ex);

	/*
	 * drain /dev/klog first
	 */
	if (global.kfd >= 0)
	{
		FD_ZERO(&kern);
		FD_SET(global.kfd, &kern);
		max = global.kfd + 1;
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
		if ((global.kfd >= 0) && FD_ISSET(global.kfd, &rd))
		{
			/*  drain /dev/klog */
			FD_ZERO(&kern);
			FD_SET(global.kfd, &kern);
			max = global.kfd + 1;

			while (select(max, &kern, NULL, NULL, &zto) > 0)
			{
				aslevent_handleevent(&kern, &wr, &ex);
			}
		}

		if (global.reset != RESET_NONE)
		{
			send_reset();
			global.reset = RESET_NONE;
		}

		if (status != 0) aslevent_handleevent(&rd, &wr, &ex);

		timed_events(&runloop_timer);
 	}
}

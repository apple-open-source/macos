/*
 * Copyright (c) 2004-2011 Apple Inc. All rights reserved.
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
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/un.h>
#include <pthread.h>
#include <dirent.h>
#include <dlfcn.h>
#include <libgen.h>
#include <notify.h>
#include <notify_keys.h>
#include <utmpx.h>
#include <vproc_priv.h>
#include "daemon.h"

#define SERVICE_NAME "com.apple.system.logger"
#define SERVER_STATUS_ERROR -1
#define SERVER_STATUS_INACTIVE 0
#define SERVER_STATUS_ACTIVE 1
#define SERVER_STATUS_ON_DEMAND 2

#define BILLION 1000000000

#define NOTIFY_DELAY 1

#define streq(A,B) (strcmp(A,B)==0)
#define forever for(;;)

extern int __notify_78945668_info__;
extern int _malloc_no_asl_log;

/* global */
struct global_s global;

/* Input Modules */
int klog_in_init(void);
int klog_in_reset(void);
int klog_in_close(void);
static int activate_klog_in = 1;

int bsd_in_init(void);
int bsd_in_reset(void);
int bsd_in_close(void);
static int activate_bsd_in = 1;

int udp_in_init(void);
int udp_in_reset(void);
int udp_in_close(void);
static int activate_udp_in = 1;

/* Output Modules */
int bsd_out_init(void);
int bsd_out_reset(void);
int bsd_out_close(void);
static int activate_bsd_out = 1;

int asl_action_init(void);
int asl_action_reset(void);
int asl_action_close(void);
static int activate_asl_action = 1;

/* Interactive Module */
int remote_init(void);
int remote_reset(void);
int remote_close(void);
static int remote_enabled = 0;

extern void database_server();

static void
init_modules()
{
	module_t *m_klog_in, *m_bsd_in, *m_bsd_out, *m_udp_in;
	module_t *m_asl, *m_remote;

	/* ASL module (configured by /etc/asl.conf) */
	m_asl = (module_t *)calloc(1, sizeof(module_t));
	if (m_asl == NULL)
	{
		asldebug("alloc failed (init_modules asl_action)\n");
		exit(1);
	}

	m_asl->name = "asl_action";
	m_asl->enabled = activate_asl_action;
	m_asl->init = asl_action_init;
	m_asl->reset = asl_action_reset;
	m_asl->close = asl_action_close;

	if (m_asl->enabled) m_asl->init();

	/* BSD output module (configured by /etc/syslog.conf) */
	m_bsd_out = (module_t *)calloc(1, sizeof(module_t));
	if (m_bsd_out == NULL)
	{
		asldebug("alloc failed (init_modules bsd_out)\n");
		exit(1);
	}

	m_bsd_out->name = "bsd_out";
	m_bsd_out->enabled = activate_bsd_out;
	m_bsd_out->init = bsd_out_init;
	m_bsd_out->reset = bsd_out_reset;
	m_bsd_out->close = bsd_out_close;

	if (m_bsd_out->enabled)
	{
		m_bsd_out->init();
		global.bsd_out_enabled = 1;
	}

	/* kernel input module */
	m_klog_in = (module_t *)calloc(1, sizeof(module_t));
	if (m_klog_in == NULL)
	{
		asldebug("alloc failed (init_modules klog_in)\n");
		exit(1);
	}

	m_klog_in->name = "klog_in";
	m_klog_in->enabled = activate_klog_in;
	m_klog_in->init = klog_in_init;
	m_klog_in->reset = klog_in_reset;
	m_klog_in->close = klog_in_close;

	if (m_klog_in->enabled) m_klog_in->init();

	/* BSD (UNIX domain socket) input module */
	m_bsd_in = (module_t *)calloc(1, sizeof(module_t));
	if (m_bsd_in == NULL)
	{
		asldebug("alloc failed (init_modules bsd_in)\n");
		exit(1);
	}

	m_bsd_in->name = "bsd_in";
	m_bsd_in->enabled = activate_bsd_in;
	m_bsd_in->init = bsd_in_init;
	m_bsd_in->reset = bsd_in_reset;
	m_bsd_in->close = bsd_in_close;

	if (m_bsd_in->enabled) m_bsd_in->init();

	/* network (syslog protocol) input module */
	m_udp_in = (module_t *)calloc(1, sizeof(module_t));
	if (m_udp_in == NULL)
	{
		asldebug("alloc failed (init_modules udp_in)\n");
		exit(1);
	}

	m_udp_in->name = "udp_in";
	m_udp_in->enabled = activate_udp_in;
	m_udp_in->init = udp_in_init;
	m_udp_in->reset = udp_in_reset;
	m_udp_in->close = udp_in_close;

	if (m_udp_in->enabled) m_udp_in->init();

	/* remote (iOS support) module */
	m_remote = (module_t *)calloc(1, sizeof(module_t));
	if (m_remote == NULL)
	{
		asldebug("alloc failed (init_modules remote)\n");
		exit(1);
	}

	m_remote->name = "remote";
	m_remote->enabled = remote_enabled;
	m_remote->init = remote_init;
	m_remote->reset = remote_reset;
	m_remote->close = remote_close;

	if (m_remote->enabled) m_remote->init();

	/* save modules in global.module array */
	global.module_count = 6;
	global.module = (module_t **)calloc(global.module_count, sizeof(module_t *));
	if (global.module == NULL)
	{
		asldebug("alloc failed (init_modules)\n");
		exit(1);
	}

	global.module[0] = m_asl;
	global.module[1] = m_bsd_out;
	global.module[2] = m_klog_in;
	global.module[3] = m_bsd_in;
	global.module[4] = m_udp_in;
	global.module[5] = m_remote;
}

static void
writepid(int *first)
{
	struct stat sb;
	FILE *fp;
	pid_t pid = getpid();

	asldebug("\nsyslogd %d start\n", pid);

	if (first != NULL)
	{
		*first = 1;
		memset(&sb, 0, sizeof(struct stat));
		if (stat(_PATH_PIDFILE, &sb) == 0)
		{
			if (S_ISREG(sb.st_mode)) *first = 0;
		}
	}

	fp = fopen(_PATH_PIDFILE, "w");
	if (fp != NULL)
	{
		fprintf(fp, "%d\n", pid);
		fclose(fp);
	}
}

void
launch_config()
{
	launch_data_t tmp, pdict;
	kern_return_t status;

	tmp = launch_data_new_string(LAUNCH_KEY_CHECKIN);
	global.launch_dict = launch_msg(tmp);
	launch_data_free(tmp);

	if (global.launch_dict == NULL)
	{
		asldebug("%d launchd checkin failed\n", getpid());
		exit(1);
	}

	tmp = launch_data_dict_lookup(global.launch_dict, LAUNCH_JOBKEY_MACHSERVICES);
	if (tmp == NULL)
	{
		asldebug("%d launchd lookup of LAUNCH_JOBKEY_MACHSERVICES failed\n", getpid());
		exit(1);
	}

	pdict = launch_data_dict_lookup(tmp, SERVICE_NAME);
	if (pdict == NULL)
	{
		asldebug("%d launchd lookup of SERVICE_NAME failed\n", getpid());
		exit(1);
	}

	global.server_port = launch_data_get_machport(pdict);

	/* port for receiving MACH_NOTIFY_DEAD_NAME notifications */
	status = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &(global.dead_session_port));
	if (status != KERN_SUCCESS)
	{
		asldebug("mach_port_allocate dead_session_port failed: %d", status);
		exit(1);
	}

	status = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &(global.listen_set));
	if (status != KERN_SUCCESS)
	{
		asldebug("mach_port_allocate listen_set failed: %d", status);
		exit(1);
	}

	status = mach_port_move_member(mach_task_self(), global.server_port, global.listen_set);
	if (status != KERN_SUCCESS)
	{
		asldebug("mach_port_move_member server_port failed: %d", status);
		exit(1);
	}

	status = mach_port_move_member(mach_task_self(), global.dead_session_port, global.listen_set);
	if (status != KERN_SUCCESS)
	{
		asldebug("mach_port_move_member dead_session_port failed (%u)", status);
		exit(1);
	}
}

void
config_debug(int enable, const char *path)
{
	OSSpinLockLock(&global.lock);

	global.debug = enable;
	free(global.debug_file);
	global.debug_file = NULL;
	if (path != NULL) global.debug_file = strdup(path);

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

void
write_boot_log(int first)
{
    int mib[2] = {CTL_KERN, KERN_BOOTTIME};
	size_t len;
	aslmsg msg;
	char buf[256];
    struct utmpx utx;

	if (first == 0)
	{
		/* syslogd restart */
		msg = asl_new(ASL_TYPE_MSG);
		if (msg == NULL) return;

		asl_set(msg, ASL_KEY_SENDER, "syslogd");
		asl_set(msg, ASL_KEY_FACILITY, "daemon");
		asl_set(msg, ASL_KEY_LEVEL, "Notice");
		asl_set(msg, ASL_KEY_UID, "0");
		asl_set(msg, ASL_KEY_GID, "0");
		snprintf(buf, sizeof(buf), "%u", getpid());
		asl_set(msg, ASL_KEY_PID, buf);
		asl_set(msg, ASL_KEY_MSG, "--- syslogd restarted ---");
		dispatch_async(global.work_queue, ^{ process_message(msg, SOURCE_INTERNAL); });
		return;
	}

    bzero(&utx, sizeof(utx));
    utx.ut_type = BOOT_TIME;
    utx.ut_pid = 1;

	/* get the boot time */
    len = sizeof(struct timeval);
    if (sysctl(mib, 2, &utx.ut_tv, &len, NULL, 0) < 0)
	{
		gettimeofday(&utx.ut_tv, NULL);
	}

    pututxline(&utx);

	msg = asl_new(ASL_TYPE_MSG);
	if (msg == NULL) return;

	asl_set(msg, ASL_KEY_SENDER, "bootlog");
	asl_set(msg, ASL_KEY_FACILITY, "com.apple.system.utmpx");
	asl_set(msg, ASL_KEY_LEVEL, "Notice");
	asl_set(msg, ASL_KEY_UID, "0");
	asl_set(msg, ASL_KEY_GID, "0");
	asl_set(msg, ASL_KEY_PID, "0");
	snprintf(buf, sizeof(buf), "BOOT_TIME %lu %u", (unsigned long)utx.ut_tv.tv_sec, (unsigned int)utx.ut_tv.tv_usec);
	asl_set(msg, ASL_KEY_MSG, buf);
	asl_set(msg, "ut_id", "0x00 0x00 0x00 0x00");
	asl_set(msg, "ut_pid", "1");
	asl_set(msg, "ut_type", "2");
	snprintf(buf, sizeof(buf), "%lu", (unsigned long)utx.ut_tv.tv_sec);
	asl_set(msg, ASL_KEY_TIME, buf);
	asl_set(msg, "ut_tv.tv_sec", buf);
	snprintf(buf, sizeof(buf), "%u", (unsigned int)utx.ut_tv.tv_usec);
	asl_set(msg, "ut_tv.tv_usec", buf);
	snprintf(buf, sizeof(buf), "%u%s", (unsigned int)utx.ut_tv.tv_usec, (utx.ut_tv.tv_usec == 0) ? "" : "000");
	asl_set(msg, ASL_KEY_TIME_NSEC, buf);

	dispatch_async(global.work_queue, ^{ process_message(msg, SOURCE_INTERNAL); });
}

int
main(int argc, const char *argv[])
{
	int32_t i;
	int network_change_token, asl_db_token;
	char tstr[32];
	time_t now;
	int first_syslogd_start = 1;

	/* Set I/O policy */
	setiopolicy_np(IOPOL_TYPE_DISK, IOPOL_SCOPE_PROCESS, IOPOL_PASSIVE);

	memset(&global, 0, sizeof(struct global_s));

	global.db_lock = (pthread_mutex_t *)calloc(1, sizeof(pthread_mutex_t));
	pthread_mutex_init(global.db_lock, NULL);

	/*
	 * Create work queue, but suspend until output modules are initialized.
	 */
	global.work_queue = dispatch_queue_create("Work Queue", NULL);
	dispatch_suspend(global.work_queue);

	global.lockdown_session_fd = -1;

	init_globals();

#ifdef CONFIG_IPHONE
	remote_enabled = 1;
	activate_bsd_out = 0;
#endif

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
					remote_enabled = 1;
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
		else if (streq(argv[i], "-m"))
		{
			if ((i + 1) < argc) global.mark_time = 60 * atoll(argv[++i]);
		}
		else if (streq(argv[i], "-utmp_ttl"))
		{
			if ((i + 1) < argc) global.utmp_ttl = atol(argv[++i]);
		}
		else if (streq(argv[i], "-mps_limit"))
		{
			if ((i + 1) < argc) global.mps_limit = atol(argv[++i]);
		}
		else if (streq(argv[i], "-dup_delay"))
		{
			if ((i + 1) < argc) global.bsd_max_dup_time = atoll(argv[++i]);
		}
		else if (streq(argv[i], "-klog_in"))
		{
			if ((i + 1) < argc) activate_klog_in = atoi(argv[++i]);
		}
		else if (streq(argv[i], "-bsd_in"))
		{
			if ((i + 1) < argc) activate_bsd_in = atoi(argv[++i]);
		}
		else if (streq(argv[i], "-udp_in"))
		{
			if ((i + 1) < argc) activate_udp_in = atoi(argv[++i]);
		}
		else if (streq(argv[i], "-bsd_out"))
		{
			if ((i + 1) < argc) activate_bsd_out = atoi(argv[++i]);
		}
		else if (streq(argv[i], "-remote"))
		{
			if ((i + 1) < argc) remote_enabled = atoi(argv[++i]);
		}
	}

	if (global.dbtype == 0)
	{
		global.dbtype = DB_TYPE_FILE;
		global.db_file_max = 25600000;
	}

	signal(SIGHUP, SIG_IGN);

	writepid(&first_syslogd_start);

	/*
	 * Log UTMPX boot time record
	 */
	write_boot_log(first_syslogd_start);

	asldebug("reading launch plist\n");
	launch_config();

	asldebug("initializing modules\n");
	init_modules();
	dispatch_resume(global.work_queue);

	/* network change notification resets UDP and BSD modules */
    notify_register_dispatch(kNotifySCNetworkChange, &network_change_token, global.work_queue, ^(int x){
        if (activate_udp_in != 0) udp_in_reset();
        if (activate_bsd_out != 0) bsd_out_reset();
    });

	/* SIGHUP resets all modules */
	global.sig_hup_src = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, (uintptr_t)SIGHUP, 0, dispatch_get_main_queue());
	dispatch_source_set_event_handler(global.sig_hup_src, ^{
		dispatch_async(global.work_queue, ^{
			int i;

			asldebug("SIGHUP reset\n");
			for (i = 0; i < global.module_count; i++)
			{
				if (global.module[i]->enabled != 0) global.module[i]->reset();
			}
		});
	});

	dispatch_resume(global.sig_hup_src);

	/* register for DB notification (posted by dbserver) for performance */
	notify_register_plain(kNotifyASLDBUpdate, &asl_db_token);

	/* timer for MARK facility */
    if (global.mark_time > 0)
	{
		global.mark_timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_main_queue());
		dispatch_source_set_event_handler(global.mark_timer, ^{ 
			asl_mark();
		});
		dispatch_source_set_timer(global.mark_timer, dispatch_walltime(NULL, global.mark_time * NSEC_PER_SEC), global.mark_time * NSEC_PER_SEC, 0);
		dispatch_resume(global.mark_timer);
	}

	/*
	 * Start launchd service
	 * This pins a thread in _vprocmgr_log_drain.  Eventually we will either
	 * remove the whole stderr/stdout -> ASL mechanism entirely, or come up 
	 * with a communication channel that we can trigger with a dispatch source.
	 */
	dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
		forever _vprocmgr_log_drain(NULL, NULL, launchd_callback);
	});

	/*
	 * Start mach server
	 * Parks a thread in database_server.  In notifyd, we found that the overhead of
	 * a dispatch source for mach calls was too high, especially on iOS.
	 */
	dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
		database_server();
	});

	dispatch_main();

	/* NOTREACHED */
	return 0;
}

/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 2004 Apple Computer, Inc.  All Rights
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <dirent.h>
#include <dlfcn.h>
#include <libgen.h>
#include <notify.h>
#include "daemon.h"

#define DEFAULT_MARK_SEC 0
#define DEFAULT_PRUNE_DAYS 7
#define PRUNE_AFTER_START_DELAY 300

#define NOTIFY_DELAY 1

#define streq(A,B) (strcmp(A,B)==0)
#define forever for(;;)

static int debug = 0;
static int reset = 0;

static TAILQ_HEAD(ml, module_list) Moduleq;

/* global */
int asl_log_filter = ASL_FILTER_MASK_UPTO(ASL_LEVEL_NOTICE);
int prune = 0;

extern int __notify_78945668_info__;

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
static int activate_udp_in = 0;

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

	if (activate_asl_in != 0)
	{
		tmp = calloc(1, sizeof(struct module_list));
		if (tmp == NULL) return 1;
		tmp->name = strdup("asl_in");
		tmp->module = NULL;
		tmp->init = asl_in_init;
		tmp->reset = asl_in_reset;
		tmp->close = asl_in_close;
		TAILQ_INSERT_TAIL(&Moduleq, tmp, entries);
	}

	if (activate_asl_action != 0)
	{
		tmp = calloc(1, sizeof(struct module_list));
		if (tmp == NULL) return 1;
		tmp->name = strdup("asl_action");
		tmp->module = NULL;
		tmp->init = asl_action_init;
		tmp->reset = asl_action_reset;
		tmp->close = asl_action_close;
		TAILQ_INSERT_TAIL(&Moduleq, tmp, entries);
	}

	if (activate_klog_in != 0)
	{
		tmp = calloc(1, sizeof(struct module_list));
		if (tmp == NULL) return 1;
		tmp->name = strdup("klog_in");
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
load_modules(char *mp)
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
catch_sigwinch(int x)
{
	prune = 1;
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

int
main(int argc, char *argv[])
{
	struct module_list *mod;
	fd_set rd, wr, ex;
	int fd, i, max, status, pdays, daemonize;
	time_t lastmark, msec, ssec, tick, delta, ptime, notify_time;
	char *mp, *str;
	struct timeval timeout, *pto;
	asl_msg_t *pq;

	mp = _PATH_MODULE_LIB;
	msec = DEFAULT_MARK_SEC;
	pdays = DEFAULT_PRUNE_DAYS;
	daemonize = 0;
	__notify_78945668_info__ = -1;

	for (i = 1; i < argc; i++)
	{
		if (streq(argv[i], "-d"))
		{
			debug = 1;
		}
		if (streq(argv[i], "-D"))
		{
			daemonize = 1;
		}
		if (streq(argv[i], "-u"))
		{
			activate_udp_in = 1;
		}
		else if (streq(argv[i], "-m"))
		{
			if ((i + 1) < argc) msec = 60 * atoi(argv[++i]);
		}
		else if (streq(argv[i], "-p"))
		{
			if ((i + 1) < argc) pdays = atoi(argv[++i]);
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

	signal(SIGHUP, catch_sighup);
	signal(SIGWINCH, catch_sigwinch);

	FD_ZERO(&rd);
	FD_ZERO(&wr);
	FD_ZERO(&ex);

	for (mod = Moduleq.tqh_first; mod != NULL; mod = mod->entries.tqe_next)
	{
		fd = mod->init();
		if (fd < 0) continue;
	}

	lastmark = time(NULL);
	notify_time = lastmark;
	memset(&timeout, 0, sizeof(struct timeval));
	pto = NULL;

	ssec = msec;
	if (ssec > 0)
	{
		timeout.tv_sec = ssec;
		pto = &timeout;
	}

	ptime = 0;
	if (pdays > 0) ptime = lastmark + PRUNE_AFTER_START_DELAY;

	forever
	{
		if (pto != NULL) pto->tv_sec = ssec;
		max = aslevent_fdsets(&rd, &wr, &ex);

		status = select(max+1, &rd, &wr, &ex, pto);

		if (reset != 0)
		{
			send_reset();
			reset = 0;
		}

		if (pto != NULL)
		{
			tick = time(NULL);
			delta = tick - lastmark;
			if (delta >= msec)
			{
				lastmark = tick;
				aslmark();
			}
		}

		if (prune != 0)
		{
			asl_prune(NULL);
			prune = 0;
			ptime = 0;
		}
		else if (ptime != 0)
		{
			tick = time(NULL);
			if (tick >= ptime)
			{
				pq = asl_new(ASL_TYPE_QUERY);
				str = NULL;
				asprintf(&str, "-%dd", pdays);
				asl_set_query(pq, ASL_KEY_TIME, str, ASL_QUERY_OP_LESS);
				if (str != NULL) free(str);
			
				/* asl_prune frees the query */
				asl_prune(pq);
				ptime = 0;
			}
		}

		if (__notify_78945668_info__ < 0)
		{
			tick = time(NULL);
			if (tick >= notify_time)
			{
				if (notify_post("com.apple.system.syslogd") == NOTIFY_STATUS_OK) __notify_78945668_info__ = 0;
				else notify_time = tick + NOTIFY_DELAY;
			}
		}

		if (status != 0) aslevent_handleevent(rd, wr, ex, NULL);
 	}
}

int
asldebug(const char *str, ...)
{
	va_list v;

	if (debug == 0) return 0;

	va_start(v, str);
	return vprintf(str, v);
}

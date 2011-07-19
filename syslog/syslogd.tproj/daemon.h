/*
 * Copyright (c) 2004-2010 Apple Inc. All rights reserved.
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

#ifndef __DAEMON_H__
#define __DAEMON_H__

#include <stdio.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <time.h>
#include <asl.h>
#include <asl_msg.h>
#include <asl_private.h>
#include <asl_store.h>
#include "asl_memory.h"
#include "asl_mini_memory.h"
#include <notify.h>
#include <launch.h>
#include <libkern/OSAtomic.h>

#define ADDFD_FLAGS_LOCAL 0x00000001

#define ASL_DB_NOTIFICATION "com.apple.system.logger.message"
#define SELF_DB_NOTIFICATION "self.logger.message"

#define ASL_OPT_IGNORE "ignore"
#define ASL_OPT_STORE "store"

#define _PATH_PIDFILE		"/var/run/syslog.pid"
#define _PATH_ASL_IN		"/var/run/asl_input"
#define _PATH_SYSLOG_CONF   "/etc/syslog.conf"
#define _PATH_SYSLOG_IN		"/var/run/syslog"
#define _PATH_KLOG			"/dev/klog"
#define _PATH_MODULE_LIB	"/usr/lib/asl"

#define DB_TYPE_FILE	0x00000001
#define DB_TYPE_MEMORY	0x00000002
#define DB_TYPE_MINI	0x00000004

#define KERN_DISASTER_LEVEL 3

#define SOURCE_UNKNOWN      0
#define SOURCE_INTERNAL     1
#define SOURCE_ASL_SOCKET   2
#define SOURCE_BSD_SOCKET   3
#define SOURCE_UDP_SOCKET   4
#define SOURCE_KERN         5
#define SOURCE_ASL_MESSAGE  6
#define SOURCE_LAUNCHD      7

#define SOURCE_SESSION    100 /* does not generate messages */

#define STORE_FLAGS_FILE_CACHE_SWEEP_REQUESTED 0x00000001

#define RESET_NONE 0
#define RESET_CONFIG 1
#define RESET_NETWORK 2

struct global_s
{
	OSSpinLock lock;
	int client_count;
	int disaster_occurred;
	mach_port_t listen_set;
	mach_port_t server_port;
	mach_port_t self_port;
	mach_port_t dead_session_port;
	launch_data_t launch_dict;
	uint32_t store_flags;
	time_t start_time;
	int lockdown_session_fd;
	int watchers_active;
	int kfd;
	int reset;
	uint64_t bsd_flush_time;
	pthread_mutex_t *db_lock;
	pthread_mutex_t *work_queue_lock;
	pthread_cond_t work_queue_cond;
	asl_search_result_t *work_queue;
	asl_store_t *file_db;
	asl_memory_t *memory_db;
	asl_mini_memory_t *mini_db;
	asl_mini_memory_t *disaster_db;

	/* parameters below are configurable as command-line args or in /etc/asl.conf */
	int asl_log_filter;
	int debug;
	char *debug_file;
	int dbtype;
	uint32_t db_file_max;
	uint32_t db_memory_max;
	uint32_t db_mini_max;
	uint32_t mps_limit;
	uint64_t bsd_max_dup_time;
	uint64_t asl_store_ping_time;
	uint64_t mark_time;
	time_t utmp_ttl;
	time_t fs_ttl;
};

extern struct global_s global;

typedef aslmsg (*aslreadfn)(int);
typedef char *(*aslwritefn)(const char *, int);
typedef char *(*aslexceptfn)(int);
typedef int (*aslsendmsgfn)(aslmsg msg, const char *outid);

struct aslevent
{
	int source;
	int fd;
	unsigned char read:1; 
	unsigned char write:1; 
	unsigned char except:1;
	aslreadfn readfn;
	aslwritefn writefn;
	aslexceptfn exceptfn;
	char *sender;
	uid_t uid;
	gid_t gid;
	TAILQ_ENTRY(aslevent) entries;
};

struct module_list
{
	char *name;
	void *module;
	int (*init)(void);
	int (*reset)(void);
	int (*close)(void);
	TAILQ_ENTRY(module_list) entries;
};

void config_debug(int enable, const char *path);
void config_data_store(int type, uint32_t file_max, uint32_t memory_max, uint32_t mini_max);
void config_timers(uint64_t bsd_max_dup, uint64_t asl_store_ping, uint64_t utmp, uint64_t fs);

char **explode(const char *s, const char *delim);
void freeList(char **l);

int aslevent_init(void);
int aslevent_fdsets(fd_set *, fd_set *, fd_set *);
void aslevent_handleevent(fd_set *, fd_set *, fd_set *);
void asl_mark(void);
void asl_archive(void);
void aslevent_check(void);

void asl_client_count_increment();
void asl_client_count_decrement();

char *get_line_from_file(FILE *f);

int asldebug(const char *, ...);
int asl_log_string(const char *str);

asl_msg_t *asl_msg_from_string(const char *buf);
int asl_msg_cmp(asl_msg_t *a, asl_msg_t *b);
time_t asl_parse_time(const char *str);

int aslevent_addfd(int source, int fd, uint32_t flags, aslreadfn readfn, aslwritefn writefn, aslexceptfn exceptfn);
int aslevent_removefd(int fd);
int aslevent_addmatch(asl_msg_t *query, char *outid);

int asl_check_option(aslmsg msg, const char *opt);

int aslevent_addoutput(aslsendmsgfn, const char *outid);

void asl_enqueue_message(uint32_t source, struct aslevent *e, aslmsg msg);
aslmsg *work_dequeue(uint32_t *count);
void asl_message_match_and_log(aslmsg msg);
void send_to_direct_watchers(asl_msg_t *msg);

int asl_syslog_faciliy_name_to_num(const char *fac);
const char *asl_syslog_faciliy_num_to_name(int num);
aslmsg asl_input_parse(const char *in, int len, char *rhost, uint32_t source);

void db_ping_store(void);

/* notify SPI */
uint32_t notify_register_plain(const char *name, int *out_token);

#endif /* __DAEMON_H__ */

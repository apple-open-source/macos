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

#ifndef __DAEMON_H__
#define __DAEMON_H__

#include <stdio.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <time.h>
#include <asl.h>
#include <asl_private.h>
#include <notify.h>

#define _PATH_PIDFILE		"/var/run/syslog.pid"
#define _PATH_ASL_IN		"/var/run/asl_input"
#define _PATH_ASL_PRUNE		"/var/run/asl_prune"
#define _PATH_ASL_OUT		"/var/log/asl.log"
#define _PATH_SYSLOG_CONF   "/etc/syslog.conf"
#define _PATH_SYSLOG_IN		"/var/run/syslog"
#define _PATH_KLOG			"/dev/klog"
#define _PATH_MODULE_LIB	"/usr/lib/asl"

struct module_list
{
	char *name;
	void *module;
	int (*init)(void);
	int (*reset)(void);
	int (*close)(void);
	TAILQ_ENTRY(module_list) entries;
};

int aslevent_init(void);
int aslevent_fdsets(fd_set *, fd_set *, fd_set *);
void aslevent_handleevent(fd_set, fd_set, fd_set, char *);
void aslmark(void);

char *get_line_from_file(FILE *f);

int asldebug(const char *, ...);
int asl_log_string(const char *str);

char *asl_msg_to_string(asl_msg_t *msg, uint32_t *len);
asl_msg_t *asl_msg_from_string(const char *buf);
int asl_msg_cmp(asl_msg_t *a, asl_msg_t *b);
time_t asl_parse_time(const char *str);

typedef asl_msg_t *(*aslreadfn)(int);
typedef char *(*aslwritefn)(const char *, int);
typedef char *(*aslexceptfn)(int);
typedef int (*aslsendmsgfn)(asl_msg_t *msg, const char *outid);

int aslevent_addfd(int fd, aslreadfn, aslwritefn, aslexceptfn);
int aslevent_removefd(int fd);
int aslevent_addmatch(asl_msg_t *query, char *outid);

int aslevent_addoutput(aslsendmsgfn, const char *outid);

int asl_syslog_faciliy_name_to_num(const char *fac);
const char *asl_syslog_faciliy_num_to_name(int num);
asl_msg_t *asl_syslog_input_convert(const char *in, int len, char *rhost, int flag);
int asl_prune(asl_msg_t *pq);

/* notify SPI */
uint32_t notify_get_state(int token, int *state);
uint32_t notify_set_state(int token, int state);
uint32_t notify_register_plain(const char *name, int *out_token);

#endif /* __DAEMON_H__ */

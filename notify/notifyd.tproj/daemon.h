/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#ifndef _NOTIFY_DAEMON_H_
#define _NOTIFY_DAEMON_H_

#include <stdio.h>
#include <stdarg.h>
#include <asl.h>
#include "common.h"
#include "watcher.h"

extern mach_port_t dead_session_port;
extern int kq;
extern uint32_t debug;
extern uint32_t debug_log;
extern FILE *debug_log_file;
extern uint32_t shm_enabled;
extern uint32_t nslots;
extern int32_t shmid;
extern uint32_t *shm_base;
extern uint32_t *shm_refcount;
extern uint32_t slot_id;
extern notify_state_t *ns;
extern uint32_t watch_id;
extern list_t *watch_list;

/* Yes, it needs the leading dot.  See service.c */
#define NOTIFY_SERVICE_PREFIX ".service."
#define NOTIFY_SERVICE_PREFIX_LEN 9

#define NOTIFY_FILE_SERVICE "file:"
#define NOTIFY_FILE_SERVICE_LEN 5

#define SERVICE_TYPE_NONE 0
#define SERVICE_TYPE_FILE 1

extern void log_message(int priority, char *str, ...);

extern uint32_t daemon_post(const char *name, uint32_t u, uint32_t g);
extern void daemon_set_state(const char *name, uint64_t val);

#endif /* _NOTIFY_DAEMON_H_ */

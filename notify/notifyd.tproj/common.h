/*
 * Copyright (c) 2003-2007 Apple Inc. All rights reserved.
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

#ifndef _NOTIFY_COMMON_H_
#define _NOTIFY_COMMON_H_

#include <pthread.h>
#include <mach/mach.h>
#include "table.h"

#define SHM_ID "apple.shm.notification_center"

#define NOTIFY_SERVICE_NAME "com.apple.system.notification_center"
#define NOTIFY_SERVICE_NAME_LEN 36

/* Notification types */
#define NOTIFY_TYPE_NONE 0
#define NOTIFY_TYPE_PLAIN 1
#define NOTIFY_TYPE_MEMORY 2
#define NOTIFY_TYPE_PORT 3
#define NOTIFY_TYPE_FD 4
#define NOTIFY_TYPE_SIGNAL 5

/* Return values for notify_check() */
#define NOTIFY_CHECK_FALSE 0
#define NOTIFY_CHECK_TRUE 1
#define NOTIFY_CHECK_ERROR 2

/* Access control */
#define NOTIFY_ACCESS_READ   1
#define NOTIFY_ACCESS_WRITE  2

#define NOTIFY_ACCESS_OTHER_SHIFT 8
#define NOTIFY_ACCESS_GROUP_SHIFT 4
#define NOTIFY_ACCESS_USER_SHIFT  0

#define NOTIFY_ACCESS_DEFAULT 0x00000333

/* Filesystem Services */
#define NOTIFY_SERVICE_FILE_STATUS_QUO 0x00
#define NOTIFY_SERVICE_FILE_ADD        0x01
#define NOTIFY_SERVICE_FILE_DELETE     0x02
#define NOTIFY_SERVICE_FILE_MODIFY     0x04
#define NOTIFY_SERVICE_FILE_ATTR       0x08

#define NOTIFY_SERVICE_DIR_FILE_ADD    0x10
#define NOTIFY_SERVICE_DIR_FILE_DELETE 0x20

/* notify state flags */
#define NOTIFY_STATE_USE_LOCKS 0x00000001

typedef struct
{
	char *name;
	uint32_t uid;
	uint32_t gid;
	uint32_t access;
	uint32_t slot;
	uint32_t refcount;
	uint32_t val;
	uint64_t state;
	void *private;
	list_t *client_list;
} name_info_t;

typedef struct
{
	name_info_t *name_info;
	task_t session;
	uint32_t notify_type;
	uint32_t lastval;
	mach_msg_empty_send_t *msg;
	int fd;
	uint32_t pid;
	uint32_t sig;
	int token;
	void *private;
} client_info_t;

typedef struct client_s
{
	uint32_t client_id;
	client_info_t *info;
} client_t;

typedef struct
{
	uint32_t flags;
	table_t *name_table;
	table_t *client_table;
	list_t *free_client_list;
	name_info_t **controlled_name;
	uint32_t controlled_name_count;
	uint32_t client_id;
	pthread_mutex_t *lock;
	uint32_t session_count;
	task_t *session;
	int sock;
} notify_state_t;

notify_state_t *_notify_lib_notify_state_new(uint32_t flags);
void _notify_lib_notify_state_free(notify_state_t *ns);

uint32_t _notify_lib_post(notify_state_t *ns, const char *name, uint32_t uid, uint32_t gid);
uint32_t _notify_lib_check(notify_state_t *ns, uint32_t cid, int *check);
uint32_t _notify_lib_get_state(notify_state_t *ns, uint32_t cid, uint64_t *state);
uint32_t _notify_lib_set_state(notify_state_t *ns, uint32_t cid, uint64_t state, uint32_t uid, uint32_t gid);
uint32_t _notify_lib_get_val(notify_state_t *ns, uint32_t cid, int *val);
uint32_t _notify_lib_set_val(notify_state_t *ns, uint32_t cid, int val, uint32_t uid, uint32_t gid);

uint32_t _notify_lib_register_plain(notify_state_t *ns, const char *name, task_t session, uint32_t slot, uint32_t uid, uint32_t gid, uint32_t *out_token);
uint32_t _notify_lib_register_signal(notify_state_t *ns, const char *name, task_t session, uint32_t sig, uint32_t uid, uint32_t gid, uint32_t *out_token);
uint32_t _notify_lib_register_mach_port(notify_state_t *ns, const char *name, task_t session, mach_port_t port, uint32_t token, uint32_t uid, uint32_t gid, uint32_t *out_token);
uint32_t _notify_lib_register_file_descriptor(notify_state_t *ns, const char *name, task_t session, const char *path, uint32_t token, uint32_t uid, uint32_t gid, uint32_t *out_token);
void _notify_lib_cancel_session(notify_state_t *ns, task_t session);

uint32_t _notify_lib_get_owner(notify_state_t *ns, const char *name, uint32_t *uid, uint32_t *gid);
uint32_t _notify_lib_get_access(notify_state_t *ns, const char *name, uint32_t *access);

uint32_t _notify_lib_set_owner(notify_state_t *ns, const char *name, uint32_t uid, uint32_t gid);
uint32_t _notify_lib_set_access(notify_state_t *ns, const char *name, uint32_t access);

uint32_t _notify_lib_release_name(notify_state_t *ns, const char *name, uint32_t uid, uint32_t gid);

void _notify_lib_cancel(notify_state_t *ns, uint32_t cid);

#endif /* _NOTIFY_COMMON_H_ */

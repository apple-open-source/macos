/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/ipc.h>
#include <sys/signal.h>
#include <mach/mach.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <servers/bootstrap.h>
#include <errno.h>
#include <pthread.h>
#include "notify.h"
#include "notify_ipc.h"
#include "common.h"

#define SELF_PREFIX "self."
#define SELF_PREFIX_LEN 5

extern uint32_t _notify_lib_peek(notify_state_t *ns, uint32_t cid, int *val);

static notify_state_t *self_state = NULL;
static mach_port_t notify_server_port = MACH_PORT_NULL;

#define CLIENT_TOKEN_TABLE_SIZE 256

#define TOKEN_TYPE_SELF 0x00000001
#define TOKEN_TYPE_MEMORY 0x00000002

typedef struct
{
	uint32_t client_id;
	uint32_t slot;
	uint32_t val;
	uint32_t flags;
} token_table_node_t;

static table_t *token_table = NULL;
static uint32_t token_id = 0;
static pthread_mutex_t token_lock = PTHREAD_MUTEX_INITIALIZER;

static uint32_t *shm_base = NULL;

static int
shm_attach(uint32_t size)
{
	int32_t shmfd;

	shmfd = shm_open(SHM_ID, O_RDONLY, 0);
	if (shmfd == -1) return -1;

	shm_base = mmap(NULL, size, PROT_READ, MAP_SHARED, shmfd, 0);
	if (shm_base == (uint32_t *)-1)
	{
		shm_base = NULL;
		return -1;
	}
	return 0;
}

#ifdef NOTDEF
static void
shm_detach(void)
{
	if (shm_base)
	{
		shmdt(shm_base);
		shm_base = NULL;
	}
}
#endif

uint32_t
_notify_lib_init(char *sname)
{
	kern_return_t status;

	if (notify_server_port != MACH_PORT_NULL) return NOTIFY_STATUS_OK;
	status = bootstrap_look_up(bootstrap_port, sname, &notify_server_port);
	if (status != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return NOTIFY_STATUS_OK;
}

void _notify_fork_child()
{
	notify_server_port = MACH_PORT_NULL;
}

static uint32_t
token_table_add(uint32_t cid, uint32_t slot, uint32_t flags, uint32_t lock)
{
	uint32_t tid;
	token_table_node_t *t;

	if (lock != 0) pthread_mutex_lock(&token_lock);
	if (token_table == NULL) 
	{
		token_table = _nc_table_new(CLIENT_TOKEN_TABLE_SIZE);
		if (token_table == NULL)
		{
			if (lock != 0) pthread_mutex_unlock(&token_lock);
			return 0;
		}
	}

	t = (token_table_node_t *)malloc(sizeof(token_table_node_t));
	tid = token_id++;

	t->client_id = cid;
	t->slot = slot;
	t->val = 0;
	t->flags = flags;

	_nc_table_insert_n(token_table, tid, t);

	if (lock != 0) pthread_mutex_unlock(&token_lock);

	return tid;
}

static token_table_node_t *
token_table_find(uint32_t tid)
{
	token_table_node_t *t;
	
	pthread_mutex_lock(&token_lock);

	t = (token_table_node_t *)_nc_table_find_n(token_table, tid);

	pthread_mutex_unlock(&token_lock);

	return t;
}

static void
token_table_delete(uint32_t tid)
{	
	pthread_mutex_lock(&token_lock);

	_nc_table_delete_n(token_table, tid);

	pthread_mutex_unlock(&token_lock);
}

/*
 * PUBLIC API
 */
 
uint32_t
notify_post(const char *name)
{
	kern_return_t kstatus;
	uint32_t status;
	security_token_t sec;

	sec.val[0] = -1;
	sec.val[1] = -1;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		if (self_state == NULL) self_state = _notify_lib_notify_state_new(NOTIFY_STATE_USE_LOCKS);
		if (self_state == NULL) return NOTIFY_STATUS_FAILED;
		_notify_lib_post(self_state, name, 0, 0);
		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}
	
	kstatus = _notify_server_post(notify_server_port, (caddr_t)name, strlen(name), &status, &sec);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;

	return status;
}

uint32_t
notify_set_owner(const char *name, uint32_t uid, uint32_t gid)
{
	kern_return_t kstatus;
	uint32_t status;
	security_token_t sec;

	sec.val[0] = -1;
	sec.val[1] = -1;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		if (self_state == NULL) self_state = _notify_lib_notify_state_new(NOTIFY_STATE_USE_LOCKS);
		if (self_state == NULL) return NOTIFY_STATUS_FAILED;
		status = _notify_lib_set_owner(self_state, name, uid, gid);
		return status;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	kstatus = _notify_server_set_owner(notify_server_port, (caddr_t)name, strlen(name), uid, gid, &status, &sec);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_get_owner(const char *name, uint32_t *uid, uint32_t *gid)
{
	kern_return_t kstatus;
	uint32_t status;
	security_token_t sec;

	sec.val[0] = -1;
	sec.val[1] = -1;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		if (self_state == NULL) self_state = _notify_lib_notify_state_new(NOTIFY_STATE_USE_LOCKS);
		if (self_state == NULL) return NOTIFY_STATUS_FAILED;
		status = _notify_lib_get_owner(self_state, name, uid, gid);
		return status;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	kstatus = _notify_server_get_owner(notify_server_port, (caddr_t)name, strlen(name), uid, gid, &status, &sec);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_set_access(const char *name, uint32_t access)
{
	kern_return_t kstatus;
	uint32_t status;
	security_token_t sec;

	sec.val[0] = -1;
	sec.val[1] = -1;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		if (self_state == NULL) self_state = _notify_lib_notify_state_new(NOTIFY_STATE_USE_LOCKS);
		if (self_state == NULL) return NOTIFY_STATUS_FAILED;
		status = _notify_lib_set_access(self_state, name, access);
		return status;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	kstatus = _notify_server_set_access(notify_server_port, (caddr_t)name, strlen(name), access, &status, &sec);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_get_access(const char *name, uint32_t *access)
{
	kern_return_t kstatus;
	uint32_t status;
	security_token_t sec;

	sec.val[0] = -1;
	sec.val[1] = -1;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		if (self_state == NULL) self_state = _notify_lib_notify_state_new(NOTIFY_STATE_USE_LOCKS);
		if (self_state == NULL) return NOTIFY_STATUS_FAILED;
		status = _notify_lib_get_access(self_state, name, access);
		return status;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	kstatus = _notify_server_get_access(notify_server_port, (caddr_t)name, strlen(name), access, &status, &sec);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_release_name(const char *name)
{
	kern_return_t kstatus;
	uint32_t status;
	security_token_t sec;

	sec.val[0] = -1;
	sec.val[1] = -1;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		if (self_state == NULL) self_state = _notify_lib_notify_state_new(NOTIFY_STATE_USE_LOCKS);
		if (self_state == NULL) return NOTIFY_STATUS_FAILED;
		status = _notify_lib_release_name(self_state, name, 0, 0);
		return status;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	kstatus = _notify_server_release_name(notify_server_port, (caddr_t)name, strlen(name), &status, &sec);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_register_check(const char *name, int *out_token)
{
	kern_return_t kstatus;
	uint32_t status, cid;
	int32_t slot, shmsize;
	task_t task;
	security_token_t sec;

	sec.val[0] = -1;
	sec.val[1] = -1;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	task = mach_task_self();

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		if (self_state == NULL) self_state = _notify_lib_notify_state_new(NOTIFY_STATE_USE_LOCKS);
		if (self_state == NULL) return NOTIFY_STATUS_FAILED;
		status = _notify_lib_register_plain(self_state, name, task, -1, 0, 0, &cid);
		if (status != NOTIFY_STATUS_OK) return status;
		*out_token = token_table_add(cid, -1, TOKEN_TYPE_SELF, 1);
		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	kstatus = _notify_server_register_check(notify_server_port, (caddr_t)name, strlen(name), task, &shmsize, &slot, &cid, &status, &sec);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	if (status == NOTIFY_STATUS_OK)
	{
		if (shmsize == -1)
		{
			*out_token = token_table_add(cid, -1, 0, 1);
		}
		else
		{
			if (shm_base == NULL)
			{
				if (shm_attach(shmsize) != 0) return NOTIFY_STATUS_FAILED;
			}

			*out_token = token_table_add(cid, slot, TOKEN_TYPE_MEMORY, 1);
		}
	}
	return status;
}

uint32_t
notify_register_plain(const char *name, int *out_token)
{
	kern_return_t kstatus;
	uint32_t status, cid;
	task_t task;
	security_token_t sec;

	sec.val[0] = -1;
	sec.val[1] = -1;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	task = mach_task_self();

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		if (self_state == NULL) self_state = _notify_lib_notify_state_new(NOTIFY_STATE_USE_LOCKS);
		if (self_state == NULL) return NOTIFY_STATUS_FAILED;
		status = _notify_lib_register_plain(self_state, name, task, -1, 0, 0, &cid);
		if (status != NOTIFY_STATUS_OK) return status;
		*out_token = token_table_add(cid, -1, TOKEN_TYPE_SELF, 1);
		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	kstatus = _notify_server_register_plain(notify_server_port, (caddr_t)name, strlen(name), task, &cid, &status, &sec);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	if (status == NOTIFY_STATUS_OK) *out_token = token_table_add(cid, -1, 0, 1);
	return status;
}

uint32_t
notify_register_signal(const char *name, int sig, int *out_token)
{
	kern_return_t kstatus;
	uint32_t status, cid;
	task_t task;
	security_token_t sec;

	sec.val[0] = -1;
	sec.val[1] = -1;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	task = mach_task_self();

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		if (self_state == NULL) self_state = _notify_lib_notify_state_new(NOTIFY_STATE_USE_LOCKS);
		if (self_state == NULL) return NOTIFY_STATUS_FAILED;
		status = _notify_lib_register_signal(self_state, name, task, sig, 0, 0, &cid);
		if (status != NOTIFY_STATUS_OK) return status;
		*out_token = token_table_add(cid, -1, TOKEN_TYPE_SELF, 1);
		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	kstatus = _notify_server_register_signal(notify_server_port, (caddr_t)name, strlen(name), task, sig, &cid, &status, &sec);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	if (status == NOTIFY_STATUS_OK) *out_token = token_table_add(cid, -1, 0, 1);
	return status;
}

uint32_t
notify_register_mach_port(const char *name, mach_port_name_t *notify_port, int flags, int *out_token)
{
	kern_return_t kstatus;
	uint32_t status, cid;
	task_t task;
	security_token_t sec;

	sec.val[0] = -1;
	sec.val[1] = -1;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;
	if (notify_port == NULL) return NOTIFY_STATUS_INVALID_PORT;

	task = mach_task_self();

	if ((flags & NOTIFY_REUSE) == 0)
	{
		kstatus = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, notify_port);
		if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	}
	
	kstatus = mach_port_insert_right(mach_task_self(), *notify_port, *notify_port, MACH_MSG_TYPE_MAKE_SEND);
	if (kstatus != KERN_SUCCESS)
	{
		mach_port_destroy(mach_task_self(), *notify_port);
		return NOTIFY_STATUS_FAILED;
	}

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		if (self_state == NULL) self_state = _notify_lib_notify_state_new(NOTIFY_STATE_USE_LOCKS);
		if (self_state == NULL) return NOTIFY_STATUS_FAILED;

		/* lock so that we can reserve current token_id */
		pthread_mutex_lock(&token_lock);
		status = _notify_lib_register_mach_port(self_state, name, task, *notify_port, token_id, 0, 0, &cid);
		if (status != NOTIFY_STATUS_OK)
		{
			pthread_mutex_unlock(&token_lock);
			return status;
		}
		*out_token = token_table_add(cid, -1, TOKEN_TYPE_SELF, 0);
		pthread_mutex_unlock(&token_lock);

		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	/* lock so that we can reserve current token_id */
	pthread_mutex_lock(&token_lock);
	kstatus = _notify_server_register_mach_port(notify_server_port, (caddr_t)name, strlen(name), task, *notify_port, token_id, &cid, &status, &sec);
	if (kstatus != KERN_SUCCESS)
	{
		pthread_mutex_unlock(&token_lock);
		return NOTIFY_STATUS_FAILED;
	}

	if (status == NOTIFY_STATUS_OK) *out_token = token_table_add(cid, -1, 0, 0);
	pthread_mutex_unlock(&token_lock);
	return status;
}

uint32_t
notify_register_file_descriptor(const char *name, int *notify_fd, int flags, int *out_token)
{
	kern_return_t kstatus;
	uint32_t status, cid, port, len;
	struct sockaddr_in sin;
	task_t task;
	security_token_t sec;

	sec.val[0] = -1;
	sec.val[1] = -1;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;
	if (notify_fd == NULL) return NOTIFY_STATUS_INVALID_FILE;

	task = mach_task_self();
	port = 0;
	len = sizeof(struct sockaddr_in);
	if ((flags & NOTIFY_REUSE) == 0)
	{
		*notify_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (*notify_fd < 0) return NOTIFY_STATUS_FAILED;
		memset(&sin, 0, len);
		sin.sin_family = AF_INET;
		status = bind(*notify_fd, (struct sockaddr *)&sin, len);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	status = getsockname(*notify_fd, (struct sockaddr *)&sin, &len);
	if (status < 0) return NOTIFY_STATUS_INVALID_FILE;
	port = sin.sin_port;

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		if (self_state == NULL) self_state = _notify_lib_notify_state_new(NOTIFY_STATE_USE_LOCKS);
		if (self_state == NULL) return NOTIFY_STATUS_FAILED;

		/* lock so that we can reserve current token_id */
		pthread_mutex_lock(&token_lock);
		status = _notify_lib_register_file_descriptor(self_state, name, task, port, token_id, 0, 0, &cid);
		if (status != NOTIFY_STATUS_OK)
		{
			pthread_mutex_unlock(&token_lock);
			return status;
		}
		*out_token = token_table_add(cid, -1, TOKEN_TYPE_SELF, 0);
		pthread_mutex_unlock(&token_lock);

		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	/* lock so that we can reserve current token_id */
	pthread_mutex_lock(&token_lock);
	kstatus = _notify_server_register_file_descriptor(notify_server_port, (caddr_t)name, strlen(name), task, port, token_id, &cid, &status, &sec);
	if (kstatus != KERN_SUCCESS)
	{
		pthread_mutex_unlock(&token_lock);
		return NOTIFY_STATUS_FAILED;
	}

	if (status == NOTIFY_STATUS_OK) *out_token = token_table_add(cid, -1, 0, 0);
	pthread_mutex_unlock(&token_lock);
	return status;
}

uint32_t
notify_check(int token, int *check)
{
	kern_return_t kstatus;
	uint32_t status;
	token_table_node_t *t;
	security_token_t sec;

	sec.val[0] = -1;
	sec.val[1] = -1;

	t = token_table_find(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & TOKEN_TYPE_SELF)
	{
		return _notify_lib_check(self_state, t->client_id, check);
	}

	if (t->flags & TOKEN_TYPE_MEMORY)
	{
		*check = 0;
		if (t->val != shm_base[t->slot])
		{
			*check = 1;
			t->val = shm_base[t->slot];
		}
		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}
	
	kstatus = _notify_server_check(notify_server_port, t->client_id, check, &status, &sec);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_peek(int token, uint32_t *val)
{
	token_table_node_t *t;

	t = token_table_find(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & TOKEN_TYPE_SELF)
	{
		return _notify_lib_peek(self_state, t->client_id, val);
	}

	if (t->flags & TOKEN_TYPE_MEMORY)
	{
		*val = shm_base[t->slot];
		return NOTIFY_STATUS_OK;
	}
	
	return NOTIFY_STATUS_INVALID_REQUEST;
}

uint32_t
notify_monitor_file(int token, char *path, int flags)
{
	kern_return_t kstatus;
	uint32_t status, len;
	token_table_node_t *t;
	security_token_t sec;

	sec.val[0] = -1;
	sec.val[1] = -1;

	t = token_table_find(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & TOKEN_TYPE_SELF)
	{
		return NOTIFY_STATUS_INVALID_REQUEST;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	len = 0;
	if (path != NULL) len = strlen(path);

	kstatus = _notify_server_monitor_file(notify_server_port, t->client_id, path, len, flags, &status, &sec);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}
	
uint32_t
notify_get_event(int token, int *ev, char *buf, int *len)
{
	kern_return_t kstatus;
	uint32_t status;
	token_table_node_t *t;
	security_token_t sec;

	sec.val[0] = -1;
	sec.val[1] = -1;

	t = token_table_find(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & TOKEN_TYPE_SELF)
	{
		return NOTIFY_STATUS_INVALID_REQUEST;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}
	
	kstatus = _notify_server_get_event(notify_server_port, t->client_id, ev, buf, len, &status, &sec);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}


uint32_t
notify_get_state(int token, int *state)
{
	kern_return_t kstatus;
	uint32_t status;
	token_table_node_t *t;
	security_token_t sec;

	sec.val[0] = -1;
	sec.val[1] = -1;

	t = token_table_find(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & TOKEN_TYPE_SELF)
	{
		return _notify_lib_get_state(self_state, t->client_id, state);
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}
	
	kstatus = _notify_server_get_state(notify_server_port, t->client_id, state, &status, &sec);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_set_state(int token, int state)
{
	kern_return_t kstatus;
	uint32_t status;
	token_table_node_t *t;
	security_token_t sec;

	sec.val[0] = -1;
	sec.val[1] = -1;

	t = token_table_find(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & TOKEN_TYPE_SELF)
	{
		return _notify_lib_set_state(self_state, t->client_id, 0, 0, state);
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}
	
	kstatus = _notify_server_set_state(notify_server_port, t->client_id, state, &status, &sec);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_cancel(int token)
{
	token_table_node_t *t;
	uint32_t status;
	kern_return_t kstatus;
	security_token_t sec;

	sec.val[0] = -1;
	sec.val[1] = -1;

	t = token_table_find(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & TOKEN_TYPE_SELF)
	{
		_notify_lib_cancel(self_state, t->client_id);
		token_table_delete(token);
		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	kstatus = _notify_server_cancel(notify_server_port, t->client_id, &status, &sec);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;

	token_table_delete(token);
	return status;
}


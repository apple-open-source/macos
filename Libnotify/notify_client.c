/*
 * Copyright (c) 2003-2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 2003-2010 Apple Inc.  All Rights Reserved.
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
#include <sys/stat.h> 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/ipc.h>
#include <sys/signal.h>
#include <sys/syslimits.h>
#include <mach/mach.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <servers/bootstrap.h>
#include <errno.h>
#include <pthread.h>
#include <TargetConditionals.h>
#include "notify.h"
#include "notify_ipc.h"
#include "libnotify.h"
#ifdef __BLOCKS__
#include <Block.h>
#include <dispatch/dispatch.h>
#endif /* __BLOCKS__ */

#if TARGET_IPHONE_SIMULATOR
#define LOCAL_FD_DEMUX 1
#endif

#define SELF_PREFIX "self."
#define SELF_PREFIX_LEN 5

#define DISPATCH_COMMON_KEY "com.apple.system.notify.dispatch.common"

#define NOTIFYD_PROCESS_FLAG 0x00000001

#define DISPATCH_COMMON_KEY "com.apple.system.notify.dispatch.common"

extern uint32_t _notify_lib_peek(notify_state_t *ns, uint32_t cid, int *val);
extern int *_notify_lib_check_addr(notify_state_t *ns, uint32_t cid);

extern int __notify_78945668_info__;

static notify_state_t *self_state = NULL;
static mach_port_t notify_server_port = MACH_PORT_NULL;
static mach_port_t notify_dispatch_port = MACH_PORT_NULL;
static int notify_dispatch_token = -1;
static dispatch_source_t notify_dispatch_source = NULL;

#define CLIENT_TOKEN_TABLE_SIZE 256

#define TOKEN_TYPE_SELF 0x00000001
#define TOKEN_TYPE_MEMORY 0x00000002

typedef struct
{
	uint32_t refcount;
	uint32_t client_id;
	uint32_t slot;
	uint32_t val;
	uint32_t flags;
	int fd;
	mach_port_t mp;
#ifdef __BLOCKS__
	int token_id;
	dispatch_queue_t queue;
	notify_handler_t block;
#endif /* __BLOCKS__ */
} token_table_node_t;

static pthread_mutex_t notify_lock = PTHREAD_MUTEX_INITIALIZER;

static table_t *token_table = NULL;
static uint32_t token_id = 0;

static uint32_t fd_count = 0;
static int *fd_clnt = NULL;
static int *fd_srv = NULL;
static int *fd_refcount = NULL;

static uint32_t mp_count = 0;
static mach_port_t *mp_list = NULL;
static int *mp_refcount = NULL;
static int *mp_mine = NULL;

static uint32_t *shm_base = NULL;

static int
shm_attach(uint32_t size)
{
	int32_t shmfd;

	shmfd = shm_open(SHM_ID, O_RDONLY, 0);
	if (shmfd == -1) return -1;

	shm_base = mmap(NULL, size, PROT_READ, MAP_SHARED, shmfd, 0);
	close(shmfd);

	if (shm_base == (uint32_t *)-1) shm_base = NULL;
	if (shm_base == NULL) return -1;

	return 0;
}

#ifdef NOTDEF
static void
shm_detach(void)
{
	if (shm_base != NULL)
	{
		shmdt(shm_base);
		shm_base = NULL;
	}
}
#endif

uint32_t
_notify_lib_init(char *sname)
{
	__block kern_return_t status;
	static dispatch_once_t once;

	/*
	 * notifyd sets a bit (NOTIFYD_PROCESS_FLAG) in this global.  If some library routine
	 * calls into Libnotify from notifyd, we fail here.  This prevents deadlocks in notifyd.
	 */
	if (__notify_78945668_info__ & NOTIFYD_PROCESS_FLAG) return NOTIFY_STATUS_FAILED;

	status = NOTIFY_STATUS_OK;

	dispatch_once(&once, ^{
		status = bootstrap_look_up(bootstrap_port, sname, &notify_server_port);
	});

	if (status != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return NOTIFY_STATUS_OK;
}

/* Reset all internal state at fork */
void
_notify_fork_child()
{
	self_state = NULL;
	notify_server_port = MACH_PORT_NULL;

	token_table = NULL;
	token_id = 0;

	fd_count = 0;
	fd_clnt = NULL;
	fd_srv = NULL;
	fd_refcount = NULL;

	mp_count = 0;
	mp_list = NULL;
	mp_refcount = NULL;
	mp_mine = NULL;

	shm_base = NULL;
}

static uint32_t
token_table_add(uint32_t cid, uint32_t slot, uint32_t flags, uint32_t lock, int fd, mach_port_t mp)
{
	token_table_node_t *t;
	static dispatch_once_t once;

	dispatch_once(&once, ^{
		token_table = _nc_table_new(CLIENT_TOKEN_TABLE_SIZE);
	});

	if (token_table == NULL) return -1;

	if (lock != 0) pthread_mutex_lock(&notify_lock);

	t = (token_table_node_t *)calloc(1, sizeof(token_table_node_t));
	if (t == NULL)
	{
		if (lock != 0) pthread_mutex_unlock(&notify_lock);
		return -1;
	}

	t->token_id = token_id++;
	t->refcount = 1;
	t->client_id = cid;
	t->slot = slot;
	t->val = 0;
	t->flags = flags;
	t->fd = fd;
	t->mp = mp;

	_nc_table_insert_n(token_table, t->token_id, t);

	if (lock != 0) pthread_mutex_unlock(&notify_lock);

	return t->token_id;
}

static token_table_node_t *
token_table_find_retain(uint32_t tid)
{
	token_table_node_t *t;

	pthread_mutex_lock(&notify_lock);

	t = (token_table_node_t *)_nc_table_find_n(token_table, tid);
	if (t != NULL) t->refcount++;

	pthread_mutex_unlock(&notify_lock);

	return t;
}

/* notify_lock is NOT required in notify_retain_file_descriptor */
static void
notify_retain_file_descriptor(int clnt, int srv)
{
	int x, i;

	if (clnt < 0) return;
	if (srv < 0) return;

	x = -1;
	for (i = 0; (i < fd_count) && (x < 0); i++)
	{
		if (fd_clnt[i] == clnt) x = i;
	}

	if (x >= 0)
	{
		fd_refcount[x]++;
		return;
	}

	x = fd_count;
	fd_count++;

	if (x == 0)
	{
		fd_clnt = (int *)calloc(1, sizeof(int));
		fd_srv = (int *)calloc(1, sizeof(int));
		fd_refcount = (int *)calloc(1, sizeof(int));
	}
	else
	{
		fd_clnt = (int *)reallocf(fd_clnt, fd_count * sizeof(int));
		fd_srv = (int *)reallocf(fd_srv, fd_count * sizeof(int));
		fd_refcount = (int *)reallocf(fd_refcount, fd_count * sizeof(int));
	}

	if ((fd_clnt == NULL) || (fd_srv == NULL) || (fd_refcount == NULL))
	{
		free(fd_clnt);
		free(fd_srv);
		free(fd_refcount);
		fd_count = 0;
		return;
	}

	fd_clnt[x] = clnt;
	fd_srv[x] = srv;
	fd_refcount[x] = 1;
}

/* notify_lock is NOT required in notify_release_file_descriptor */
static void
notify_release_file_descriptor(int fd)
{
	int x, i, j;

	if (fd < 0) return;

	x = -1;
	for (i = 0; (i < fd_count) && (x < 0); i++)
	{
		if (fd_clnt[i] == fd) x = i;
	}

	if (x < 0) return;

	if (fd_refcount[x] > 0) fd_refcount[x]--;
	if (fd_refcount[x] > 0) return;

	close(fd_clnt[x]);
	close(fd_srv[x]);

	if (fd_count == 1)
	{
		free(fd_clnt);
		free(fd_srv);
		free(fd_refcount);
		fd_count = 0;
		return;
	}

	for (i = x + 1, j = x; i < fd_count; i++, j++)
	{
		fd_clnt[j] = fd_clnt[i];
		fd_srv[j] = fd_srv[i];
		fd_refcount[j] = fd_refcount[i];
	}

	fd_count--;

	fd_clnt = (int *)reallocf(fd_clnt, fd_count * sizeof(int));
	fd_srv = (int *)reallocf(fd_srv, fd_count * sizeof(int));
	fd_refcount = (int *)reallocf(fd_refcount, fd_count * sizeof(int));

	if ((fd_clnt == NULL) || (fd_srv == NULL) || (fd_refcount == NULL))
	{
		free(fd_clnt);
		free(fd_srv);
		free(fd_refcount);
		fd_count = 0;
	}
}

/* notify_lock is NOT required in notify_retain_mach_port */
static void
notify_retain_mach_port(mach_port_t mp, int mine)
{
	int x, i;

	if (mp == MACH_PORT_NULL) return;

	x = -1;
	for (i = 0; (i < mp_count) && (x < 0); i++)
	{
		if (mp_list[i] == mp) x = i;
	}

	if (x >= 0)
	{
		mp_refcount[x]++;
		return;
	}

	x = mp_count;
	mp_count++;

	if (x == 0)
	{
		mp_list = (mach_port_t *)calloc(1, sizeof(mach_port_t));
		mp_refcount = (int *)calloc(1, sizeof(int));
		mp_mine = (int *)calloc(1, sizeof(int));
	}
	else
	{
		mp_list = (mach_port_t *)reallocf(mp_list, mp_count * sizeof(mach_port_t));
		mp_refcount = (int *)reallocf(mp_refcount, mp_count * sizeof(int));
		mp_mine = (int *)reallocf(mp_mine, mp_count * sizeof(int));
	}

	if ((mp_list == NULL) || (mp_refcount == NULL) || (mp_mine == NULL))
	{
		if (mp_list != NULL) free(mp_list);
		if (mp_refcount != NULL) free(mp_refcount);
		if (mp_mine != NULL) free(mp_mine);
		mp_count = 0;
		return;
	}

	mp_list[x] = mp;
	mp_refcount[x] = 1;
	mp_mine[x] = mine;
}

/* notify_lock is NOT required in notify_release_mach_port */
static void
notify_release_mach_port(mach_port_t mp, uint32_t flags)
{
	int x, i;

	if (mp == MACH_PORT_NULL) return;

	x = -1;
	for (i = 0; (i < mp_count) && (x < 0); i++)
	{
		if (mp_list[i] == mp) x = i;
	}

	if (x < 0) return;

	if (mp_refcount[x] > 0) mp_refcount[x]--;
	if (mp_refcount[x] > 0) return;

	if (mp_mine[x] == 1)
	{
		mach_port_mod_refs(mach_task_self(), mp, MACH_PORT_RIGHT_RECEIVE, -1);

		/* release send right if this is a self notification */
		if (flags & TOKEN_TYPE_SELF) mach_port_deallocate(mach_task_self(), mp);
	}

	if (mp_count == 1)
	{
		if (mp_list != NULL) free(mp_list);
		if (mp_refcount != NULL) free(mp_refcount);
		if (mp_mine != NULL) free(mp_mine);
		mp_count = 0;
		return;
	}

	for (i = x + 1; i < mp_count; i++)
	{
		mp_list[i - 1] = mp_list[i];
		mp_refcount[i - 1] = mp_refcount[i];
		mp_mine[i - 1] = mp_mine[i];
	}

	mp_count--;

	mp_list = (mach_port_t *)reallocf(mp_list, mp_count * sizeof(mach_port_t));
	mp_refcount = (int *)reallocf(mp_refcount, mp_count * sizeof(int));
	mp_mine = (int *)reallocf(mp_mine, mp_count * sizeof(int));

	if ((mp_list == NULL) || (mp_refcount == NULL) || (mp_mine == NULL))
	{
		if (mp_list != NULL) free(mp_list);
		if (mp_refcount != NULL) free(mp_refcount);
		if (mp_mine != NULL) free(mp_mine);
		mp_count = 0;
	}
}

static void
token_table_release(token_table_node_t *t)
{
	if (t == NULL) return;

	pthread_mutex_lock(&notify_lock);

	if (t->refcount > 0) t->refcount--;
	if (t->refcount > 0)
	{
		pthread_mutex_unlock(&notify_lock);
		return;
	}

	notify_release_file_descriptor(t->fd);
	notify_release_mach_port(t->mp, t->flags);

	if (t->queue != NULL) dispatch_release(t->queue);
	t->queue = NULL;
	if (t->block != NULL) Block_release(t->block);
	t->block = NULL;

	_nc_table_delete_n(token_table, t->token_id);
	t->client_id = NOTIFY_INVALID_CLIENT_ID;

	pthread_mutex_unlock(&notify_lock);

	free(t);
}

static notify_state_t *
_notify_lib_self_state()
{
	static dispatch_once_t once;

	dispatch_once(&once, ^{
		self_state = _notify_lib_notify_state_new(NOTIFY_STATE_USE_LOCKS, 0);
	});

	return self_state;
}

/*
 * PUBLIC API
 */
 
uint32_t
notify_post(const char *name)
{
	notify_state_t *ns_self;
	kern_return_t kstatus;
	uint32_t status;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		ns_self = _notify_lib_self_state();
		if (ns_self == NULL) return NOTIFY_STATUS_FAILED;
		_notify_lib_post(ns_self, name, 0, 0);
		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	kstatus = _notify_server_post(notify_server_port, (caddr_t)name, strlen(name), (int32_t *)&status);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;

	return status;
}

uint32_t
notify_set_owner(const char *name, uint32_t uid, uint32_t gid)
{
	notify_state_t *ns_self;
	kern_return_t kstatus;
	uint32_t status;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		ns_self = _notify_lib_self_state();
		if (ns_self == NULL) return NOTIFY_STATUS_FAILED;
		status = _notify_lib_set_owner(ns_self, name, uid, gid);
		return status;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	kstatus = _notify_server_set_owner(notify_server_port, (caddr_t)name, strlen(name), uid, gid, (int32_t *)&status);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_get_owner(const char *name, uint32_t *uid, uint32_t *gid)
{
	notify_state_t *ns_self;
	kern_return_t kstatus;
	uint32_t status;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		ns_self = _notify_lib_self_state();
		if (ns_self == NULL) return NOTIFY_STATUS_FAILED;
		status = _notify_lib_get_owner(ns_self, name, uid, gid);
		return status;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	kstatus = _notify_server_get_owner(notify_server_port, (caddr_t)name, strlen(name), (int32_t *)uid, (int32_t *)gid, (int32_t *)&status);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_set_access(const char *name, uint32_t access)
{
	notify_state_t *ns_self;
	kern_return_t kstatus;
	uint32_t status;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		ns_self = _notify_lib_self_state();
		if (ns_self == NULL) return NOTIFY_STATUS_FAILED;
		status = _notify_lib_set_access(ns_self, name, access);
		return status;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	kstatus = _notify_server_set_access(notify_server_port, (caddr_t)name, strlen(name), access, (int32_t *)&status);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_get_access(const char *name, uint32_t *access)
{
	notify_state_t *ns_self;
	kern_return_t kstatus;
	uint32_t status;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		ns_self = _notify_lib_self_state();
		if (ns_self == NULL) return NOTIFY_STATUS_FAILED;
		status = _notify_lib_get_access(ns_self, name, access);
		return status;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	kstatus = _notify_server_get_access(notify_server_port, (caddr_t)name, strlen(name), (int32_t *)access, (int32_t *)&status);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_release_name(const char *name)
{
	notify_state_t *ns_self;
	kern_return_t kstatus;
	uint32_t status;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		ns_self = _notify_lib_self_state();
		if (ns_self == NULL) return NOTIFY_STATUS_FAILED;
		status = _notify_lib_release_name(ns_self, name, 0, 0);
		return status;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	kstatus = _notify_server_release_name(notify_server_port, (caddr_t)name, strlen(name), (int32_t *)&status);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}

static void
_notify_dispatch_handle(mach_port_t port)
{
	token_table_node_t *t;
	int token;
	mach_msg_empty_rcv_t msg;
	kern_return_t status;

	if (port == MACH_PORT_NULL) return;

	memset(&msg, 0, sizeof(msg));

	status = mach_msg(&msg.header, MACH_RCV_MSG, 0, sizeof(msg), port, 0, MACH_PORT_NULL);
	if (status != KERN_SUCCESS) return;

	token = msg.header.msgh_id;

	t = token_table_find_retain(token);

	if ((t != NULL) && (t->queue != NULL) && (t->block != NULL))
	{
		dispatch_async(t->queue, ^{
			t->block(token);
			token_table_release(t);
		});
	}
}

uint32_t
notify_register_dispatch(const char *name, int *out_token, dispatch_queue_t queue, notify_handler_t handler)
{
	__block uint32_t status;
	token_table_node_t *t;
	static dispatch_once_t once;

	if (queue == NULL) return NOTIFY_STATUS_FAILED;
	if (handler == NULL) return NOTIFY_STATUS_FAILED;

	status = NOTIFY_STATUS_OK;

	dispatch_once(&once, ^{
		status = notify_register_mach_port(DISPATCH_COMMON_KEY, &notify_dispatch_port, 0, &notify_dispatch_token);
		if (status == NOTIFY_STATUS_OK)
		{
			notify_dispatch_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, notify_dispatch_port, 0, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0));
			dispatch_source_set_event_handler(notify_dispatch_source, ^{ _notify_dispatch_handle(notify_dispatch_port); });
			dispatch_resume(notify_dispatch_source);
		}
	});

	if (status != NOTIFY_STATUS_OK) return status;
	if (notify_dispatch_port == MACH_PORT_NULL) return NOTIFY_STATUS_FAILED;

	status = notify_register_mach_port(name, &notify_dispatch_port, NOTIFY_REUSE, out_token);

	t = token_table_find_retain(*out_token);
	if (t == NULL) return NOTIFY_STATUS_FAILED;

	t->token_id = *out_token;
	t->queue = queue;
	dispatch_retain(t->queue);
	t->block = Block_copy(handler);
	token_table_release(t);

	return NOTIFY_STATUS_OK;
}

static uint32_t
notify_register_dispatch_fd_handler(const char *name, int *out_token, int rfd, int wfd)
{
	__block uint32_t status;
	token_table_node_t *t;
	static dispatch_once_t once;
	int val;

	status = NOTIFY_STATUS_OK;

	dispatch_once(&once, ^{
		status = notify_register_mach_port(DISPATCH_COMMON_KEY, &notify_dispatch_port, 0, &notify_dispatch_token);
		if (status == NOTIFY_STATUS_OK)
		{
			notify_dispatch_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, notify_dispatch_port, 0, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0));
			dispatch_source_set_event_handler(notify_dispatch_source, ^{ _notify_dispatch_handle(notify_dispatch_port); });
			dispatch_resume(notify_dispatch_source);
		}
	});

	if (status != NOTIFY_STATUS_OK) return status;
	if (notify_dispatch_port == MACH_PORT_NULL) return NOTIFY_STATUS_FAILED;

	status = notify_register_mach_port(name, &notify_dispatch_port, NOTIFY_REUSE, out_token);


	t = token_table_find_retain(*out_token);
	if (t == NULL) return NOTIFY_STATUS_FAILED;

	t->token_id = *out_token;
	t->fd = rfd;
	t->queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0);
	dispatch_retain(t->queue);
	val = htonl(t->token_id);
	t->block = (notify_handler_t)Block_copy(^(int unused){ write(wfd, &val, sizeof(val)); });

	token_table_release(t);

	return NOTIFY_STATUS_OK;
}

uint32_t
notify_register_check(const char *name, int *out_token)
{
	notify_state_t *ns_self;
	kern_return_t kstatus;
	uint32_t status, cid;
	int32_t slot, shmsize;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		ns_self = _notify_lib_self_state();
		if (ns_self == NULL) return NOTIFY_STATUS_FAILED;

		status = _notify_lib_register_plain(ns_self, name, -1, 0, 0, &cid);
		if (status != NOTIFY_STATUS_OK) return status;

		*out_token = token_table_add(cid, -1, TOKEN_TYPE_SELF, 1, -1, MACH_PORT_NULL);
		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	kstatus = _notify_server_register_check(notify_server_port, (caddr_t)name, strlen(name), &shmsize, &slot, (int32_t *)&cid, (int32_t *)&status);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	if (status == NOTIFY_STATUS_OK)
	{
		if (shmsize == -1)
		{
			*out_token = token_table_add(cid, -1, 0, 1, -1, MACH_PORT_NULL);
		}
		else
		{
			if (shm_base == NULL)
			{
				if (shm_attach(shmsize) != 0) return NOTIFY_STATUS_FAILED;
			}

			if (shm_base == NULL) return NOTIFY_STATUS_FAILED;

			*out_token = token_table_add(cid, slot, TOKEN_TYPE_MEMORY, 1, -1, MACH_PORT_NULL);
		}
	}

	return status;
}

uint32_t
notify_register_plain(const char *name, int *out_token)
{
	notify_state_t *ns_self;
	kern_return_t kstatus;
	uint32_t status, cid;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		ns_self = _notify_lib_self_state();
		if (ns_self == NULL) return NOTIFY_STATUS_FAILED;

		status = _notify_lib_register_plain(ns_self, name, -1, 0, 0, &cid);
		if (status != NOTIFY_STATUS_OK) return status;

		*out_token = token_table_add(cid, -1, TOKEN_TYPE_SELF, 1, -1, MACH_PORT_NULL);
		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	kstatus = _notify_server_register_plain(notify_server_port, (caddr_t)name, strlen(name), (int32_t *)&cid, (int32_t *)&status);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	if (status == NOTIFY_STATUS_OK) *out_token = token_table_add(cid, -1, 0, 1, -1, MACH_PORT_NULL);
	return status;
}

uint32_t
notify_register_signal(const char *name, int sig, int *out_token)
{
	notify_state_t *ns_self;
	kern_return_t kstatus;
	uint32_t status, cid;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		ns_self = _notify_lib_self_state();
		if (ns_self == NULL) return NOTIFY_STATUS_FAILED;

		status = _notify_lib_register_signal(ns_self, name, getpid(), sig, 0, 0, &cid);
		if (status != NOTIFY_STATUS_OK) return status;

		*out_token = token_table_add(cid, -1, TOKEN_TYPE_SELF, 1, -1, MACH_PORT_NULL);
		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	kstatus = _notify_server_register_signal(notify_server_port, (caddr_t)name, strlen(name), sig, (int32_t *)&cid, (int32_t *)&status);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	if (status == NOTIFY_STATUS_OK) *out_token = token_table_add(cid, -1, 0, 1, -1, MACH_PORT_NULL);
	return status;
}

uint32_t
notify_register_mach_port(const char *name, mach_port_name_t *notify_port, int flags, int *out_token)
{
	notify_state_t *ns_self;
	kern_return_t kstatus;
	uint32_t status, cid;
	task_t task;
	int mine;

	mine = 0;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;
	if (notify_port == NULL) return NOTIFY_STATUS_INVALID_PORT;

	task = mach_task_self();

	if ((flags & NOTIFY_REUSE) == 0)
	{
		mine = 1;
		kstatus = mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, notify_port);
		if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	}

	kstatus = mach_port_insert_right(task, *notify_port, *notify_port, MACH_MSG_TYPE_MAKE_SEND);
	if (kstatus != KERN_SUCCESS)
	{
		if (mine == 1) mach_port_mod_refs(task, *notify_port, MACH_PORT_RIGHT_RECEIVE, -1);
		return NOTIFY_STATUS_FAILED;
	}

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		ns_self = _notify_lib_self_state();
		if (ns_self == NULL)
		{
			if (mine == 1)
			{
				mach_port_mod_refs(task, *notify_port, MACH_PORT_RIGHT_RECEIVE, -1);
			}

			mach_port_deallocate(task, *notify_port);
			return NOTIFY_STATUS_FAILED;
		}

		/* lock so that we can reserve current token_id */
		pthread_mutex_lock(&notify_lock);
		status = _notify_lib_register_mach_port(ns_self, name, *notify_port, token_id, 0, 0, &cid);
		if (status != NOTIFY_STATUS_OK)
		{
			if (mine == 1)
			{
				mach_port_mod_refs(task, *notify_port, MACH_PORT_RIGHT_RECEIVE, -1);
			}

			mach_port_deallocate(task, *notify_port);
			pthread_mutex_unlock(&notify_lock);
			return status;
		}

		*out_token = token_table_add(cid, -1, TOKEN_TYPE_SELF, 0, -1, *notify_port);
		notify_retain_mach_port(*notify_port, mine);

		pthread_mutex_unlock(&notify_lock);
		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0)
		{
			if (mine == 1)
			{
				mach_port_mod_refs(task, *notify_port, MACH_PORT_RIGHT_RECEIVE, -1);
			}

			mach_port_deallocate(task, *notify_port);
			return NOTIFY_STATUS_FAILED;
		}
	}

	/* lock so that we can reserve current token_id */
	pthread_mutex_lock(&notify_lock);
	kstatus = _notify_server_register_mach_port(notify_server_port, (caddr_t)name, strlen(name), *notify_port, token_id, (int32_t *)&cid, (int32_t *)&status);
	if (kstatus != KERN_SUCCESS)
	{
		if (mine == 1)
		{
			mach_port_mod_refs(task, *notify_port, MACH_PORT_RIGHT_RECEIVE, -1);
		}

		mach_port_deallocate(task, *notify_port);
		pthread_mutex_unlock(&notify_lock);
		return NOTIFY_STATUS_FAILED;
	}

	if (status == NOTIFY_STATUS_OK)
	{
		*out_token = token_table_add(cid, -1, 0, 0, -1, *notify_port);
		notify_retain_mach_port(*notify_port, mine);
	}
	else if (mine == 1)
	{
		mach_port_mod_refs(task, *notify_port, MACH_PORT_RIGHT_RECEIVE, -1);
	}

	pthread_mutex_unlock(&notify_lock);
	return status;
}

static char *
_notify_mk_tmp_path(int tid)
{
#if TARGET_OS_EMBEDDED
	int freetmp = 0;
	char *path, *tmp = getenv("TMPDIR");

	if (tmp == NULL)
	{
		asprintf(&tmp, "/tmp/com.apple.notify.%d", geteuid());
		mkdir(tmp, 0755);
		freetmp = 1;
	}

	if (tmp == NULL) return NULL;

	asprintf(&path, "%s/com.apple.notify.%d.%d", tmp, getpid(), tid);
	if (freetmp) free(tmp);
	return path;
#else
    char tmp[PATH_MAX], *path;

	if (confstr(_CS_DARWIN_USER_TEMP_DIR, tmp, sizeof(tmp)) <= 0) return NULL;
#endif

	path = NULL;
	asprintf(&path, "%s/com.apple.notify.%d.%d", tmp, getpid(), tid);
	return path;
}
			 
uint32_t
notify_register_file_descriptor(const char *name, int *notify_fd, int flags, int *out_token)
{
	notify_state_t *ns_self;
	uint32_t i, status, cid;
	int mine, fdpair[2];
#ifndef LOCAL_FD_DEMUX
	fileport_t fileport;
	kern_return_t kstatus;
#endif

	mine = 0;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;
	if (notify_fd == NULL) return NOTIFY_STATUS_INVALID_FILE;

	if ((flags & NOTIFY_REUSE) == 0)
	{
		if (pipe(fdpair) < 0) return NOTIFY_STATUS_FAILED;

		mine = 1;
		*notify_fd = fdpair[0];
	}
	else
	{
		/* check the file descriptor - it must be one of "ours" */
		for (i = 0; i < fd_count; i++)
		{
			if (fd_clnt[i] == *notify_fd) break;
		}

		if (i >= fd_count) return NOTIFY_STATUS_INVALID_FILE;

		fdpair[0] = fd_clnt[i];
		fdpair[1] = fd_srv[i];
	}

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		ns_self = _notify_lib_self_state();
		if (ns_self == NULL)
		{
			if (mine == 1)
			{
				close(fdpair[0]);
				close(fdpair[1]);
			}

			return NOTIFY_STATUS_FAILED;
		}

		/* lock so that we can reserve current token_id */
		pthread_mutex_lock(&notify_lock);

		status = _notify_lib_register_file_descriptor(ns_self, name, fdpair[1], token_id, 0, 0, (uint32_t *)&cid);
		if (status != NOTIFY_STATUS_OK)
		{
			if (mine == 1)
			{
				close(fdpair[0]);
				close(fdpair[1]);
			}

			pthread_mutex_unlock(&notify_lock);
			return status;
		}

		*out_token = token_table_add(cid, -1, TOKEN_TYPE_SELF, 0, *notify_fd, MACH_PORT_NULL);

		notify_retain_file_descriptor(fdpair[0], fdpair[1]);

		pthread_mutex_unlock(&notify_lock);
		return NOTIFY_STATUS_OK;
	}

#ifdef LOCAL_FD_DEMUX
	/*
	 * Use dispatch to do a write() on fdpair[1] when notified.
	 */
	status = notify_register_dispatch_fd_handler(name, out_token, fdpair[0], fdpair[1]);
	if (status != NOTIFY_STATUS_OK)
	{
		if (mine == 1)
		{
			close(fdpair[0]);
			close(fdpair[1]);
		}

		return status;
	}

	notify_retain_file_descriptor(fdpair[0], fdpair[1]);
	return NOTIFY_STATUS_OK;

#else
	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0)
		{
			if (mine == 1)
			{
				close(fdpair[0]);
				close(fdpair[1]);
			}

			return NOTIFY_STATUS_FAILED;
		}
	}

	/* send fdpair[1] (the sender's fd) to notifyd using a fileport */
	fileport = MACH_PORT_NULL;
	if (fileport_makeport(fdpair[1], (fileport_t *)&fileport) < 0)
	{
		if (mine == 1)
		{
			close(fdpair[0]);
			close(fdpair[1]);
		}

		return NOTIFY_STATUS_FAILED;
	}

	/* lock so that we can reserve current token_id */
	pthread_mutex_lock(&notify_lock);

	kstatus = _notify_server_register_file_descriptor(notify_server_port, (caddr_t)name, strlen(name), (mach_port_t)fileport, token_id, (int32_t *)&cid, (int32_t *)&status);
	if (kstatus != KERN_SUCCESS)
	{
		if (mine == 1)
		{
			close(fdpair[0]);
			close(fdpair[1]);
		}

		pthread_mutex_unlock(&notify_lock);
		return NOTIFY_STATUS_FAILED;
	}

	if (status == NOTIFY_STATUS_OK)
	{
		*out_token = token_table_add(cid, -1, 0, 0, *notify_fd, MACH_PORT_NULL);
		notify_retain_file_descriptor(fdpair[0], fdpair[1]);
	}
	else if (mine == 1)
	{
		close(fdpair[0]);
		close(fdpair[1]);
	}

	pthread_mutex_unlock(&notify_lock);
	return status;
#endif
}

uint32_t
notify_check(int token, int *check)
{
	kern_return_t kstatus;
	uint32_t status, val;
	token_table_node_t *t;

	t = token_table_find_retain(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & TOKEN_TYPE_SELF)
	{
		/* _notify_lib_check returns NOTIFY_STATUS_FAILED if self_state is NULL */
		status = _notify_lib_check(self_state, t->client_id, check);
		token_table_release(t);
		return status;
	}

	if (t->flags & TOKEN_TYPE_MEMORY)
	{
		if (shm_base == NULL) return NOTIFY_STATUS_FAILED;

		*check = 0;
		val = ntohl(shm_base[t->slot]);
		if (t->val != val)
		{
			*check = 1;
			t->val = val;
		}

		token_table_release(t);
		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0)
		{
			token_table_release(t);
			return NOTIFY_STATUS_FAILED;
		}
	}

	kstatus = _notify_server_check(notify_server_port, t->client_id, check, (int32_t *)&status);
	token_table_release(t);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_peek(int token, uint32_t *val)
{
	token_table_node_t *t;
	uint32_t status;

	t = token_table_find_retain(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & TOKEN_TYPE_SELF)
	{
		/* _notify_lib_peek returns NOTIFY_STATUS_FAILED if self_state is NULL */
		status = _notify_lib_peek(self_state, t->client_id, (int *)val);
		token_table_release(t);
		return status;
	}

	if (t->flags & TOKEN_TYPE_MEMORY)
	{
		if (shm_base == NULL)
		{
			token_table_release(t);
			return NOTIFY_STATUS_FAILED;
		}

		*val = ntohl(shm_base[t->slot]);
		token_table_release(t);
		return NOTIFY_STATUS_OK;
	}

	token_table_release(t);
	return NOTIFY_STATUS_INVALID_REQUEST;
}

int *
notify_check_addr(int token)
{
	token_table_node_t *t;
	uint32_t slot;
	int *val;

	t = token_table_find_retain(token);
	if (t == NULL) return NULL;

	if (t->flags & TOKEN_TYPE_SELF)
	{
		/* _notify_lib_check_addr returns NOTIFY_STATUS_FAILED if self_state is NULL */
		val = _notify_lib_check_addr(self_state, t->client_id);
		token_table_release(t);
		return val;
	}

	if (t->flags & TOKEN_TYPE_MEMORY)
	{
		slot = t->slot;
		token_table_release(t);

		if (shm_base == NULL) return NULL;
		return (int *)&(shm_base[slot]);
	}

	token_table_release(t);
	return NULL;
}

uint32_t
notify_monitor_file(int token, char *path, int flags)
{
	kern_return_t kstatus;
	uint32_t status, len;
	token_table_node_t *t;

	t = token_table_find_retain(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & TOKEN_TYPE_SELF)
	{
		token_table_release(t);
		return NOTIFY_STATUS_INVALID_REQUEST;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0)
		{
			token_table_release(t);
			return NOTIFY_STATUS_FAILED;
		}
	}

	len = 0;
	if (path != NULL) len = strlen(path);

	kstatus = _notify_server_monitor_file(notify_server_port, t->client_id, path, len, flags, (int32_t *)&status);
	token_table_release(t);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_get_event(int token, int *ev, char *buf, int *len)
{
	if (ev != NULL) *ev = 0;
	if (len != NULL) *len = 0;

	return NOTIFY_STATUS_OK;
}

uint32_t
notify_get_state(int token, uint64_t *state)
{
	kern_return_t kstatus;
	uint32_t status;
	token_table_node_t *t;

	t = token_table_find_retain(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & TOKEN_TYPE_SELF)
	{
		/* _notify_lib_get_state returns NOTIFY_STATUS_FAILED if self_state is NULL */
		status = _notify_lib_get_state(self_state, t->client_id, state);
		token_table_release(t);
		return status;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0)
		{
			token_table_release(t);
			return NOTIFY_STATUS_FAILED;
		}
	}

	kstatus = _notify_server_get_state(notify_server_port, t->client_id, state, (int32_t *)&status);
	token_table_release(t);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_set_state(int token, uint64_t state)
{
	kern_return_t kstatus;
	uint32_t status;
	token_table_node_t *t;

	t = token_table_find_retain(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & TOKEN_TYPE_SELF)
	{
		/* _notify_lib_set_state returns NOTIFY_STATUS_FAILED if self_state is NULL */
		status = _notify_lib_set_state(self_state, t->client_id, state, 0, 0);
		token_table_release(t);
		return status;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0)
		{
			token_table_release(t);
			return NOTIFY_STATUS_FAILED;
		}
	}

	kstatus = _notify_server_set_state(notify_server_port, t->client_id, state, (int32_t *)&status);
	token_table_release(t);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_get_val(int token, int *val)
{
	kern_return_t kstatus;
	uint32_t status, slot;
	token_table_node_t *t;

	t = token_table_find_retain(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & TOKEN_TYPE_SELF)
	{
		/* _notify_lib_get_val returns NOTIFY_STATUS_FAILED if self_state is NULL */
		status = _notify_lib_get_val(self_state, t->client_id, val);
		token_table_release(t);
		return status;
	}

	if (t->flags & TOKEN_TYPE_MEMORY)
	{
		slot = t->slot;
		token_table_release(t);

		if (shm_base == NULL) return NOTIFY_STATUS_FAILED;

		*val = ntohl(shm_base[slot]);
		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0)
		{
			token_table_release(t);
			return NOTIFY_STATUS_FAILED;
		}
	}

	kstatus = _notify_server_get_val(notify_server_port, t->client_id, val, (int32_t *)&status);
	token_table_release(t);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_set_val(int token, int val)
{
	kern_return_t kstatus;
	uint32_t status;
	token_table_node_t *t;

	t = token_table_find_retain(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & TOKEN_TYPE_SELF)
	{
		/* _notify_lib_set_val returns NOTIFY_STATUS_FAILED if self_state is NULL */
		status = _notify_lib_set_val(self_state, t->client_id, val, 0, 0);
		token_table_release(t);
		return status;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0)
		{
			token_table_release(t);
			return NOTIFY_STATUS_FAILED;
		}
	}

	kstatus = _notify_server_set_val(notify_server_port, t->client_id, val, (int32_t *)&status);
	token_table_release(t);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_cancel(int token)
{
	token_table_node_t *t;
	uint32_t status;
	kern_return_t kstatus;

	t = token_table_find_retain(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & TOKEN_TYPE_SELF)
	{
		/*
		 * _notify_lib_cancel returns NOTIFY_STATUS_FAILED if self_state is NULL
		 * We let it fail quietly.
		 */
		_notify_lib_cancel(self_state, t->client_id);

		/* release twice: once for the token_table_find_retain, once to really release it */
		token_table_release(t);
		token_table_release(t);

		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0)
		{
			/* release twice: once for the token_table_find_retain, once to really release it */
			token_table_release(t);
			token_table_release(t);

			return NOTIFY_STATUS_FAILED;
		}
	}

	kstatus = _notify_server_cancel(notify_server_port, t->client_id, (int32_t *)&status);

	/* release twice: once for the token_table_find_retain, once to really release it */
	token_table_release(t);
	token_table_release(t);

	if ((kstatus == MIG_SERVER_DIED) || (kstatus == MACH_SEND_INVALID_DEST)) status = NOTIFY_STATUS_OK;
	else if (kstatus != KERN_SUCCESS) status = NOTIFY_STATUS_FAILED;

	return status;
}

uint32_t
notify_suspend(int token)
{
	token_table_node_t *t;
	uint32_t status;
	kern_return_t kstatus;

	t = token_table_find_retain(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & TOKEN_TYPE_SELF)
	{
		_notify_lib_suspend(self_state, t->client_id);
		token_table_release(t);
		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0)
		{
			token_table_release(t);
			return NOTIFY_STATUS_FAILED;
		}
	}

	status = NOTIFY_STATUS_OK;
	kstatus = _notify_server_suspend(notify_server_port, t->client_id, (int32_t *)&status);
	token_table_release(t);
	if (kstatus != KERN_SUCCESS) status = NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_resume(int token)
{
	token_table_node_t *t;
	uint32_t status;
	kern_return_t kstatus;

	t = token_table_find_retain(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & TOKEN_TYPE_SELF)
	{
		_notify_lib_resume(self_state, t->client_id);
		token_table_release(t);
		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0)
		{
			token_table_release(t);
			return NOTIFY_STATUS_FAILED;
		}
	}

	status = NOTIFY_STATUS_OK;
	kstatus = _notify_server_resume(notify_server_port, t->client_id, (int32_t *)&status);
	token_table_release(t);
	if (kstatus != KERN_SUCCESS) status = NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_suspend_pid(pid_t pid)
{
	uint32_t status;
	kern_return_t kstatus;

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0)
		{
			return NOTIFY_STATUS_FAILED;
		}
	}

	status = NOTIFY_STATUS_OK;
	kstatus = _notify_server_suspend_pid(notify_server_port, pid, (int32_t *)&status);

	if (kstatus != KERN_SUCCESS) status = NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_resume_pid(pid_t pid)
{
	uint32_t status;
	kern_return_t kstatus;

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0)
		{
			return NOTIFY_STATUS_FAILED;
		}
	}

	status = NOTIFY_STATUS_OK;
	kstatus = _notify_server_resume_pid(notify_server_port, pid, (int32_t *)&status);

	if (kstatus != KERN_SUCCESS) status = NOTIFY_STATUS_FAILED;
	return status;
}

/* SPI */
uint32_t
notify_simple_post(const char *name)
{
	notify_state_t *ns_self;
	kern_return_t kstatus;
	uint32_t status;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		ns_self = _notify_lib_self_state();
		if (ns_self == NULL) return NOTIFY_STATUS_FAILED;
		_notify_lib_post(ns_self, name, 0, 0);
		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(NOTIFY_SERVICE_NAME);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	kstatus = _notify_server_simple_post(notify_server_port, (caddr_t)name, strlen(name));
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;

	return NOTIFY_STATUS_OK;
}

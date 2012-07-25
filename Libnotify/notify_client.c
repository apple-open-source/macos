/*
 * Copyright (c) 2003-2011 Apple Inc. All rights reserved.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <asl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/ipc.h>
#include <sys/signal.h>
#include <sys/syslimits.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <sys/time.h>
#include <servers/bootstrap.h>
#include <errno.h>
#include <pthread.h>
#include <TargetConditionals.h>
#include <libkern/OSAtomic.h>
#include "notify.h"
#include "notify_ipc.h"
#include "libnotify.h"
#include "notify_private.h"
#include <Block.h>
#include <dispatch/dispatch.h>
#include <dispatch/private.h>

// <rdar://problem/10385540>
WEAK_IMPORT_ATTRIBUTE bool
_dispatch_is_multithreaded(void);

#define EVENT_INIT       0
#define EVENT_REGEN      1

static uint32_t notify_ipc_version = 0;
static pid_t notify_server_pid = 0;
static uint32_t client_opts = 0;

#define SELF_PREFIX "self."
#define SELF_PREFIX_LEN 5

#define COMMON_SELF_PORT_KEY "self.com.apple.system.notify.common"

#define NOTIFYD_PROCESS_FLAG 0x00000001

#define MULTIPLE_REGISTRATION_WARNING_TRIGGER 20

extern uint32_t _notify_lib_peek(notify_state_t *ns, pid_t pid, int token, int *val);
extern int *_notify_lib_check_addr(notify_state_t *ns, pid_t pid, int token);

extern int __notify_78945668_info__;

static notify_state_t *self_state = NULL;
static mach_port_t notify_server_port = MACH_PORT_NULL;
static mach_port_t notify_common_self_port = MACH_PORT_NULL;
static mach_port_t notify_common_port = MACH_PORT_NULL;
static int notify_common_self_token = -1;
static int notify_common_token = -1;
static dispatch_source_t notify_dispatch_self_source = NULL;
static dispatch_source_t notify_dispatch_source = NULL;
static dispatch_source_t server_proc_source = NULL;

#define CLIENT_TOKEN_TABLE_SIZE 256

#define NID_UNSET 0xffffffffffffffffL
#define NID_CALLED_ONCE 0xfffffffffffffffeL

#define NO_LOCK 1

typedef struct
{
	uint32_t refcount;
	uint64_t name_id;
} name_table_node_t;

typedef struct
{
	uint32_t refcount;
	const char *name;
	size_t namelen;
	name_table_node_t *name_node;
	uint32_t token;
	uint32_t slot;
	uint32_t val;
	uint32_t flags;
	int fd;
	int signal;
	mach_port_t mp;
	uint32_t client_id;
	uint64_t set_state_time;
	uint64_t set_state_val;
	char * path;
	int path_flags;
	dispatch_queue_t queue;
	notify_handler_t block;
} token_table_node_t;

static pthread_mutex_t notify_lock = PTHREAD_MUTEX_INITIALIZER;

static table_t *token_table = NULL;
static table_t *token_name_table = NULL;
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

/* FORWARD */
static void _notify_lib_regenerate(int src);
static void notify_retain_mach_port(mach_port_t mp, int mine);
static uint32_t _notify_register_primary_port(const char *name, mach_port_name_t *notify_port, int *out_token);
static void _notify_dispatch_handle(mach_port_t port);
static notify_state_t *_notify_lib_self_state();

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

/*
 * Initializations needed for self notifications.
 * Currently we only check to see if a common self port is
 * required (if demux is enabled).
 */
static uint32_t
_notify_lib_self_init(uint32_t event)
{
	uint32_t status = NOTIFY_STATUS_OK;

	if (client_opts & NOTIFY_OPT_DEMUX)
	{
		if (notify_common_self_port == MACH_PORT_NULL)
		{
			status = _notify_register_primary_port(COMMON_SELF_PORT_KEY, &notify_common_self_port, &notify_common_self_token);
			if (status == NOTIFY_STATUS_OK)
			{
				notify_dispatch_self_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, notify_common_self_port, 0, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0));
				dispatch_source_set_event_handler(notify_dispatch_self_source, ^{ _notify_dispatch_handle(notify_common_self_port); });
				dispatch_resume(notify_dispatch_self_source);
			}
			else
			{
				notify_common_self_port = MACH_PORT_NULL;
			}
		}
	}

	return status;
}

/*
 * _notify_lib_init is called for each new registration (event = EVENT_INIT).
 * It is also called to re-initialize when the library has detected that
 * notifyd has restarted (event = EVENT_REGEN).
 */
static uint32_t
_notify_lib_init(uint32_t event)
{
	__block kern_return_t kstatus;
	__block bool first = false;
	static dispatch_once_t nsp_once;
	int status, cid;
	uint64_t state;

	/*
	 * notifyd sets a bit (NOTIFYD_PROCESS_FLAG) in this global.  If some library routine
	 * calls into Libnotify from notifyd, we fail here.  This prevents deadlocks in notifyd.
	 */
	if (__notify_78945668_info__ & NOTIFYD_PROCESS_FLAG) return NOTIFY_STATUS_FAILED;

	/* Look up the notifyd server port just once. */
	kstatus = KERN_SUCCESS;
	dispatch_once(&nsp_once, ^{
		first = true;
		kstatus = bootstrap_look_up(bootstrap_port, NOTIFY_SERVICE_NAME, &notify_server_port);
	});

	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;

	if (event == EVENT_INIT) pthread_mutex_lock(&notify_lock);

	/*
	 * _dispatch_is_multithreaded() tells us if it is safe to use dispatch queues for
	 * a shared port for all registratios, and to watch for notifyd exiting / restarting.
	 */
	// _dispatch_is_multithreaded is weak imported, <rdar://problem/10385540>
	if (_dispatch_is_multithreaded) {
		if (_dispatch_is_multithreaded()) client_opts |= (NOTIFY_OPT_DEMUX | NOTIFY_OPT_REGEN);
	}

	/*
	 * Look up the server's PID and supported IPC version on the first call,
	 * and on a regeneration event (when the server has restarted).
	 */
	if (first || (event == EVENT_REGEN))
	{
		pid_t last_pid = notify_server_pid;

		notify_ipc_version = 0;
		notify_server_pid = 0;

		kstatus = _notify_server_register_plain(notify_server_port, NOTIFY_IPC_VERSION_NAME, NOTIFY_IPC_VERSION_NAME_LEN, &cid, &status);
		if ((kstatus == KERN_SUCCESS) && (status == NOTIFY_STATUS_OK))
		{
			kstatus = _notify_server_get_state(notify_server_port, cid, &state, &status);
			if ((kstatus == KERN_SUCCESS) && (status == NOTIFY_STATUS_OK))
			{
				notify_ipc_version = state;
				state >>= 32;
				notify_server_pid = state;
			}

			_notify_server_cancel(notify_server_port, cid, &status);

			if ((last_pid == notify_server_pid) && (event == EVENT_REGEN))
			{
				if (event == EVENT_INIT) pthread_mutex_unlock(&notify_lock);
				return NOTIFY_STATUS_INVALID_REQUEST;
			}
		}

		if (server_proc_source != NULL)
		{
			dispatch_source_cancel(server_proc_source);
			dispatch_release(server_proc_source);
			server_proc_source = NULL;
		}

		if (notify_dispatch_source != NULL)
		{
			dispatch_source_cancel(notify_dispatch_source);
			dispatch_release(notify_dispatch_source);
			notify_dispatch_source = NULL;
			notify_common_port = MACH_PORT_NULL;
		}
	}

	if (notify_ipc_version < 2)
	{
		/* regen is not supported below version 2 */
		client_opts &= ~NOTIFY_OPT_REGEN;
	}

	/*
	 * Create a source (DISPATCH_SOURCE_TYPE_PROC) to invoke _notify_lib_regenerate if notifyd restarts.
	 * Available in IPC version 2.
	 */
	if ((server_proc_source == NULL) && (client_opts & NOTIFY_OPT_REGEN) && (notify_server_pid != 0))
	{
		server_proc_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, (uintptr_t)notify_server_pid, DISPATCH_PROC_EXIT, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0));
		dispatch_source_set_event_handler(server_proc_source, ^{ _notify_lib_regenerate(1); });
		dispatch_resume(server_proc_source);
	}

	/*
	 * Create the shared multiplex ports if NOTIFY_OPT_DEMUX is set.
	 */
	if ((client_opts & NOTIFY_OPT_DEMUX) && (notify_common_port == MACH_PORT_NULL))
	{
		status = _notify_register_primary_port(COMMON_PORT_KEY, &notify_common_port, &notify_common_token);
		if (status == NOTIFY_STATUS_OK)
		{
			mach_port_t common = notify_common_port;

			notify_dispatch_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, common, 0, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0));
			dispatch_source_set_event_handler(notify_dispatch_source, ^{ _notify_dispatch_handle(common); });
			dispatch_source_set_cancel_handler(notify_dispatch_source, ^{
				mach_port_mod_refs(mach_task_self(), common, MACH_PORT_RIGHT_RECEIVE, -1);
			});
			dispatch_resume(notify_dispatch_source);
		}
		else
		{
			notify_common_port = MACH_PORT_NULL;
		}
	}

	if (event == EVENT_INIT) pthread_mutex_unlock(&notify_lock);

	return NOTIFY_STATUS_OK;
}

/* Reset all internal state at fork */
void
_notify_fork_child()
{
	self_state = NULL;
	notify_server_port = MACH_PORT_NULL;
	notify_ipc_version = 0;
	notify_server_pid = 0;
	client_opts = 0;

	token_table = NULL;
	token_name_table = NULL;

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
token_table_add(const char *name, size_t namelen, uint64_t nid, uint32_t token, uint32_t cid, uint32_t slot, uint32_t flags, int sig, int fd, mach_port_t mp, int lock)
{
	token_table_node_t *t;
	name_table_node_t *n;
	static dispatch_once_t once;
	uint32_t warn_count = 0;

	dispatch_once(&once, ^{
		token_table = _nc_table_new(CLIENT_TOKEN_TABLE_SIZE);
		token_name_table = _nc_table_new(CLIENT_TOKEN_TABLE_SIZE);
	});

	if (token_table == NULL) return -1;
	if (token_name_table == NULL) return -1;
	if (name == NULL) return -1;

	t = (token_table_node_t *)calloc(1, sizeof(token_table_node_t));
	if (t == NULL) return -1;

	t->refcount = 1;

	/* we will get t->name from the token_name_table */
	t->name = NULL;

	t->namelen = namelen;
	t->token = token;
	t->slot = slot;
	t->val = 0;
	t->flags = flags;
	t->fd = fd;
	t->mp = mp;
	t->client_id = cid;

	if (lock != NO_LOCK) pthread_mutex_lock(&notify_lock);
	_nc_table_insert_n(token_table, t->token, t);

	/* check if we have this name in the name table */
	n = _nc_table_find_get_key(token_name_table, name, &(t->name));
	if (n == NULL)
	{
		char *copy_name = strdup(name);
		if (copy_name == NULL)
		{
			free(t);
			if (lock != NO_LOCK) pthread_mutex_unlock(&notify_lock);
			return -1;
		}

		t->name = (const char *)copy_name;

		/* create a new name table node */
		n = (name_table_node_t *)calloc(1, sizeof(name_table_node_t));
		if (n != NULL)
		{
			n->refcount = 1;
			n->name_id = nid;

			/* the name table node "owns" the name */
			_nc_table_insert_pass(token_name_table, copy_name, n);
			t->name_node = n;
		}
	}
	else
	{
		/* this token retains the name table node */
		t->name_node = n;
		n->refcount++;

		if ((n->refcount % MULTIPLE_REGISTRATION_WARNING_TRIGGER) == 0)
		{
			warn_count = n->refcount;
		}
	}

	if (lock != NO_LOCK) pthread_mutex_unlock(&notify_lock);

	if (warn_count > 0)
	{
		aslclient a = asl_open(NULL, NULL, ASL_OPT_NO_REMOTE);
		asl_log(a, NULL, ASL_LEVEL_WARNING, "notify name \"%s\" has been registered %d times - this may be a leak", name, warn_count);
		asl_close(a);
	}

	return 0;
}

static token_table_node_t *
token_table_find_retain(uint32_t token)
{
	token_table_node_t *t;

	pthread_mutex_lock(&notify_lock);

	t = (token_table_node_t *)_nc_table_find_n(token_table, token);
	if (t != NULL) t->refcount++;

	pthread_mutex_unlock(&notify_lock);

	return t;
}

static token_table_node_t *
token_table_find_no_lock(uint32_t token)
{
	return (token_table_node_t *)_nc_table_find_n(token_table, token);
}

static name_table_node_t *
name_table_find_retain_no_lock(const char *name)
{
	name_table_node_t *n;

	n = (name_table_node_t *)_nc_table_find(token_name_table, name);
	if (n != NULL) n->refcount++;

	return n;
}

static void
name_table_release_no_lock(const char *name)
{
	name_table_node_t *n;

	n = (name_table_node_t *)_nc_table_find(token_name_table, name);
	if (n != NULL)
	{
		if (n->refcount > 0) n->refcount--;
		if (n->refcount == 0)
		{
			_nc_table_delete(token_name_table, name);
			free(n);
		}
	}
}

static void
name_table_set_nid(const char *name, uint64_t nid)
{
	name_table_node_t *n;

	pthread_mutex_lock(&notify_lock);

	n = (name_table_node_t *)_nc_table_find(token_name_table, name);
	if (n != NULL) n->name_id = nid;

	pthread_mutex_unlock(&notify_lock);
}

static uint32_t
_notify_register_primary_port(const char *name, mach_port_name_t *notify_port, int *out_token)
{
	notify_state_t *ns_self;
	kern_return_t kstatus;
	uint32_t status;
	uint64_t nid;
	task_t task;
	int token;
	size_t namelen;
	uint32_t cid;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;
	if (notify_port == NULL) return NOTIFY_STATUS_INVALID_PORT;

	namelen = strlen(name);

	task = mach_task_self();

	kstatus = mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, notify_port);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;

	kstatus = mach_port_insert_right(task, *notify_port, *notify_port, MACH_MSG_TYPE_MAKE_SEND);
	if (kstatus != KERN_SUCCESS)
	{
		mach_port_mod_refs(task, *notify_port, MACH_PORT_RIGHT_RECEIVE, -1);
		return NOTIFY_STATUS_FAILED;
	}

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		ns_self = _notify_lib_self_state();
		if (ns_self == NULL)
		{
			mach_port_mod_refs(task, *notify_port, MACH_PORT_RIGHT_RECEIVE, -1);
			mach_port_deallocate(task, *notify_port);
			return NOTIFY_STATUS_FAILED;
		}

		token = OSAtomicIncrement32((int32_t *)&token_id);
		status = _notify_lib_register_mach_port(ns_self, name, NOTIFY_CLIENT_SELF, token, *notify_port, 0, 0, &nid);
		if (status != NOTIFY_STATUS_OK)
		{
			mach_port_mod_refs(task, *notify_port, MACH_PORT_RIGHT_RECEIVE, -1);
			mach_port_deallocate(task, *notify_port);
			return status;
		}

		*out_token = token;

		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL) return NOTIFY_STATUS_FAILED;

	token = OSAtomicIncrement32((int32_t *)&token_id);

	if (notify_ipc_version == 0)
	{
		kstatus = _notify_server_register_mach_port(notify_server_port, (caddr_t)name, namelen, *notify_port, token, (int32_t *)&cid, (int32_t *)&status);
		if ((kstatus == KERN_SUCCESS) && (status != NOTIFY_STATUS_OK)) kstatus = KERN_FAILURE;
	}
	else
	{
		cid = token;
		kstatus = _notify_server_register_mach_port_2(notify_server_port, (caddr_t)name, namelen, token, *notify_port);
	}

	if (kstatus != KERN_SUCCESS)
	{
		mach_port_mod_refs(task, *notify_port, MACH_PORT_RIGHT_RECEIVE, -1);
		mach_port_deallocate(task, *notify_port);
		return NOTIFY_STATUS_FAILED;
	}

	*out_token = token;

	return NOTIFY_STATUS_OK;
}


static void
_notify_lib_regenerate_token(token_table_node_t *t)
{
	uint32_t type;
	int status, new_slot;
	kern_return_t kstatus;
	mach_port_t port;
	uint64_t new_nid;
	size_t pathlen;

	if (t == NULL) return;
	if (t->name == NULL) return;
	if (t->flags & NOTIFY_FLAG_SELF) return;
	if ((t->flags & NOTIFY_FLAG_REGEN) == 0) return;
	if (!strcmp(t->name, COMMON_PORT_KEY)) return;

	port = MACH_PORT_NULL;
	if (t->flags & NOTIFY_TYPE_PORT)
	{
		port = notify_common_port;
	}

	pathlen = 0;
	if (t->path != NULL) pathlen = strlen(t->path);
	type = t->flags & 0x000000ff;

	kstatus = _notify_server_regenerate(notify_server_port, (caddr_t)t->name, t->namelen, t->token, type, port, t->signal, t->slot, t->set_state_val, t->set_state_time, t->path, pathlen, t->path_flags, &new_slot, &new_nid, &status);

	if (kstatus != KERN_SUCCESS) status = NOTIFY_STATUS_FAILED;
	if (status != NOTIFY_STATUS_OK) return;

	t->slot = new_slot;

	/* reset the name_id in the name table node */
	if (t->name_node != NULL) t->name_node->name_id = new_nid;
}

/*
 * Invoked when server has died.
 * Regenerates all registrations and state.
 */
static void
_notify_lib_regenerate(int src)
{
	void *tt;
	token_table_node_t *t;

	if ((client_opts & NOTIFY_OPT_REGEN) == 0) return;

	pthread_mutex_lock(&notify_lock);

	/* _notify_lib_init returns an error if regeneration is unnecessary */

	if (_notify_lib_init(EVENT_REGEN) == NOTIFY_STATUS_OK)
	{
		tt = _nc_table_traverse_start(token_table);
		while (tt != NULL)
		{
			t = _nc_table_traverse(token_table, tt);
			if (t == NULL) break;
			_notify_lib_regenerate_token(t);
		}

		_nc_table_traverse_end(token_table, tt);
	}

	pthread_mutex_unlock(&notify_lock);
}

/*
 * Regenerate if the server PID (shared memory slot 0) has changed.
 */
static inline void
regenerate_check()
{
	if ((client_opts & NOTIFY_OPT_REGEN) == 0) return;

	if ((shm_base != NULL) && (shm_base[0] != notify_server_pid)) _notify_lib_regenerate(0);
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
		if (flags & NOTIFY_FLAG_SELF) mach_port_deallocate(mach_task_self(), mp);
	}

	if (flags & NOTIFY_FLAG_RELEASE_SEND)
	{
		/* multiplexed registration holds a send right in Libnotify */
		mach_port_deallocate(mach_task_self(), mp);
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
token_table_release_no_lock(token_table_node_t *t)
{
	if (t == NULL) return;

	if (t->refcount > 0) t->refcount--;
	if (t->refcount > 0) return;

	notify_release_file_descriptor(t->fd);
	notify_release_mach_port(t->mp, t->flags);

	if (t->queue != NULL) dispatch_release(t->queue);
	t->queue = NULL;
	if (t->block != NULL) Block_release(t->block);
	t->block = NULL;

	_nc_table_delete_n(token_table, t->token);
	name_table_release_no_lock(t->name);

	free(t->path);
	free(t);
}

static void
token_table_release(token_table_node_t *t)
{
	pthread_mutex_lock(&notify_lock);
	token_table_release_no_lock(t);
	pthread_mutex_unlock(&notify_lock);
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

/* SPI */
void
notify_set_options(uint32_t opts)
{
	client_opts = opts;

	/* call _notify_lib_init to create ports / dispatch sources as required */
	_notify_lib_init(EVENT_INIT);
}

/*
 * PUBLIC API
 */

/*
 * notify_post is a very simple API, but the implementation is
 * more complex to try to optimize the time it takes.
 *
 * The server - notifyd - keeps a unique ID number for each key
 * in the namespace.  Although it's reasonably fast to call
 * _notify_server_post_4 (a MIG simpleroutine), the MIG call
 * allocates VM and copies the name string.  It's much faster to
 * call using the ID number.  The problem is mapping from name to
 * ID number.  The token table keeps track of all registered names
 * (in the client), but the registration calls are simpleroutines,
 * except for notify_register_check.  notify_register_check saves
 * the name ID in the token table, but the other routines set it
 * to NID_UNSET.
 *
 * In notify_post, we check if the name is known.  If it is not,
 * then the client is doing a "cold call".  There may be no
 * clients for this name anywhere on the system.  In this case
 * we simply send the name.  We take the allocate/copy cost, but
 * the latency is still not too bad since we use a simpleroutine.
 *
 * If the name in registered and the ID number is known, we send
 * the ID using a simpleroutine.  This is very fast.
 *
 * If the name is registered but the ID number is NID_UNSET, we
 * send the name (as in a "cold call".  It *might* just be that
 * this client process just posts once, and we don't want to incur
 * any addition cost.  The ID number is reset to NID_CALLED_ONCE.
 *
 * If the client posts the same name again (the ID number is
 * NID_CALLED_ONCE, we do a synchronous call to notifyd, sending
 * the name string and getting back the name ID, whcih we save
 * in the token table.  This is simply a zero/one/many heuristic:
 * If the client posts the same name more than once, we make the
 * guess that it's going to do it more frequently, and it's worth
 * the time it takes to fetch the ID from notifyd.
 */
uint32_t
notify_post(const char *name)
{
	notify_state_t *ns_self;
	kern_return_t kstatus;
	uint32_t status;
	size_t namelen = 0;
	name_table_node_t *n;
	uint64_t nid = UINT64_MAX;

	regenerate_check();

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
		status = _notify_lib_init(EVENT_INIT);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	if (notify_ipc_version == 0)
	{
		namelen = strlen(name);
		kstatus = _notify_server_post(notify_server_port, (caddr_t)name, namelen, (int32_t *)&status);
		if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
		return status;
	}

	namelen = strlen(name);

	/* Lock to prevent a race with notify cancel over the use of name IDs */
	pthread_mutex_lock(&notify_lock);

	/* See if we have a name ID for this name. */
	n = name_table_find_retain_no_lock(name);
	if (n != NULL)
	{
		if (n->name_id == NID_UNSET)
		{
			/* First post goes using the name string. */
			kstatus = _notify_server_post_4(notify_server_port, (caddr_t)name, namelen);
			if (kstatus != KERN_SUCCESS)
			{
				name_table_release_no_lock(name);
				pthread_mutex_unlock(&notify_lock);
				return NOTIFY_STATUS_FAILED;
			}

			n->name_id = NID_CALLED_ONCE;
			name_table_release_no_lock(name);
			pthread_mutex_unlock(&notify_lock);
			return NOTIFY_STATUS_OK;
		}
		else if (n->name_id == NID_CALLED_ONCE)
		{
			/* Post and fetch the name ID.  Slow, but subsequent posts will be very fast. */
			kstatus = _notify_server_post_2(notify_server_port, (caddr_t)name, namelen, &nid, (int32_t *)&status);
			if (kstatus != KERN_SUCCESS)
			{
				name_table_release_no_lock(name);
				pthread_mutex_unlock(&notify_lock);
				return NOTIFY_STATUS_FAILED;
			}

			if (status == NOTIFY_STATUS_OK) n->name_id = nid;
			name_table_release_no_lock(name);
			pthread_mutex_unlock(&notify_lock);
			return status;
		}
		else
		{
			/* We have the name ID.  Do an async post using the name ID.  Very fast. */
			kstatus = _notify_server_post_3(notify_server_port, n->name_id);
			name_table_release_no_lock(name);
			pthread_mutex_unlock(&notify_lock);
			if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
			return NOTIFY_STATUS_OK;
		}
	}

	pthread_mutex_unlock(&notify_lock);

	/* Do an async post using the name string. Fast (but not as fast as using name ID). */
	kstatus = _notify_server_post_4(notify_server_port, (caddr_t)name, namelen);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return NOTIFY_STATUS_OK;
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
		status = _notify_lib_init(EVENT_INIT);
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
		status = _notify_lib_init(EVENT_INIT);
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
		status = _notify_lib_init(EVENT_INIT);
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
		status = _notify_lib_init(EVENT_INIT);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	kstatus = _notify_server_get_access(notify_server_port, (caddr_t)name, strlen(name), (int32_t *)access, (int32_t *)&status);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}

/* notifyd retains and releases a name when clients register and cancel. */
uint32_t
notify_release_name(const char *name)
{
	return NOTIFY_STATUS_OK;
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

	if (token == notify_common_token) _notify_lib_regenerate(3);
}

uint32_t
notify_register_dispatch(const char *name, int *out_token, dispatch_queue_t queue, notify_handler_t handler)
{
	__block uint32_t status;
	token_table_node_t *t;

	regenerate_check();

	if (queue == NULL) return NOTIFY_STATUS_FAILED;
	if (handler == NULL) return NOTIFY_STATUS_FAILED;

	/* client is using dispatch: enable local demux and regeneration */
	notify_set_options(NOTIFY_OPT_DEMUX | NOTIFY_OPT_REGEN);

	status = NOTIFY_STATUS_OK;

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		_notify_lib_self_init(EVENT_INIT);
		status = notify_register_mach_port(name, &notify_common_self_port, NOTIFY_REUSE, out_token);
	}
	else
	{
		status = notify_register_mach_port(name, &notify_common_port, NOTIFY_REUSE, out_token);
	}

	if (status != NOTIFY_STATUS_OK) return status;

	t = token_table_find_retain(*out_token);
	if (t == NULL) return NOTIFY_STATUS_FAILED;

	t->queue = queue;
	dispatch_retain(t->queue);
	t->block = Block_copy(handler);
	token_table_release(t);

	return NOTIFY_STATUS_OK;
}

/* note this does not get self names */
static uint32_t
notify_register_mux_fd(const char *name, int *out_token, int rfd, int wfd)
{
	__block uint32_t status;
	token_table_node_t *t;
	int val;

	status = NOTIFY_STATUS_OK;

	if (notify_common_port == MACH_PORT_NULL) return NOTIFY_STATUS_FAILED;

	status = notify_register_mach_port(name, &notify_common_port, NOTIFY_REUSE, out_token);

	t = token_table_find_retain(*out_token);
	if (t == NULL) return NOTIFY_STATUS_FAILED;

	t->token = *out_token;
	t->fd = rfd;
	t->queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0);
	dispatch_retain(t->queue);
	val = htonl(t->token);
	t->block = (notify_handler_t)Block_copy(^(int unused){ write(wfd, &val, sizeof(val)); });

	token_table_release(t);

	return NOTIFY_STATUS_OK;
}

uint32_t
notify_register_check(const char *name, int *out_token)
{
	notify_state_t *ns_self;
	kern_return_t kstatus;
	uint32_t status, token;
	uint64_t nid;
	int32_t slot, shmsize;
	size_t namelen;
	uint32_t cid;

	regenerate_check();

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;
	if (out_token == NULL) return NOTIFY_STATUS_FAILED;

	*out_token = -1;
	namelen = strlen(name);

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		ns_self = _notify_lib_self_state();
		if (ns_self == NULL) return NOTIFY_STATUS_FAILED;

		token = OSAtomicIncrement32((int32_t *)&token_id);
		status = _notify_lib_register_plain(ns_self, name, NOTIFY_CLIENT_SELF, token, SLOT_NONE, 0, 0, &nid);
		if (status != NOTIFY_STATUS_OK) return status;

		cid = token;
		token_table_add(name, namelen, nid, token, cid, SLOT_NONE, NOTIFY_FLAG_SELF | NOTIFY_TYPE_PLAIN, SIGNAL_NONE, FD_NONE, MACH_PORT_NULL, 0);

		*out_token = token;
		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(EVENT_INIT);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	token = OSAtomicIncrement32((int32_t *)&token_id);
	kstatus = KERN_SUCCESS;

	if (notify_ipc_version == 0)
	{
		nid = NID_UNSET;
		kstatus = _notify_server_register_check(notify_server_port, (caddr_t)name, namelen, &shmsize, &slot, (int32_t *)&cid, (int32_t *)&status);
	}
	else
	{
		cid = token;
		kstatus = _notify_server_register_check_2(notify_server_port, (caddr_t)name, namelen, token, &shmsize, &slot, &nid, (int32_t *)&status);
	}

	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	if (status != NOTIFY_STATUS_OK) return status;

	if (shmsize != -1)
	{
		if (shm_base == NULL)
		{
			if (shm_attach(shmsize) != 0) return NOTIFY_STATUS_FAILED;
			if (shm_base == NULL) return NOTIFY_STATUS_FAILED;
		}

		token_table_add(name, namelen, nid, token, cid, slot, NOTIFY_TYPE_MEMORY | NOTIFY_FLAG_REGEN, SIGNAL_NONE, FD_NONE, MACH_PORT_NULL, 0);
	}
	else
	{
		token_table_add(name, namelen, nid, token, cid, SLOT_NONE, NOTIFY_TYPE_PLAIN | NOTIFY_FLAG_REGEN, SIGNAL_NONE, FD_NONE, MACH_PORT_NULL, 0);
	}

	*out_token = token;
	return status;
}

uint32_t
notify_register_plain(const char *name, int *out_token)
{
	notify_state_t *ns_self;
	kern_return_t kstatus;
	uint32_t status;
	uint64_t nid;
	size_t namelen;
	int token;
	uint32_t cid;

	regenerate_check();

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	namelen = strlen(name);

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		ns_self = _notify_lib_self_state();
		if (ns_self == NULL) return NOTIFY_STATUS_FAILED;

		token = OSAtomicIncrement32((int32_t *)&token_id);
		status = _notify_lib_register_plain(ns_self, name, NOTIFY_CLIENT_SELF, token, SLOT_NONE, 0, 0, &nid);
		if (status != NOTIFY_STATUS_OK) return status;

		cid = token;
		token_table_add(name, namelen, nid, token, cid, SLOT_NONE, NOTIFY_FLAG_SELF | NOTIFY_TYPE_PLAIN, SIGNAL_NONE, FD_NONE, MACH_PORT_NULL, 0);

		*out_token = token;
		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(EVENT_INIT);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	token = OSAtomicIncrement32((int32_t *)&token_id);

	if (notify_ipc_version == 0)
	{
		kstatus = _notify_server_register_plain(notify_server_port, (caddr_t)name, namelen, (int32_t *)&cid, (int32_t *)&status);
		if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
		if (status != NOTIFY_STATUS_OK) return status;
	}
	else
	{
		cid = token;
		kstatus = _notify_server_register_plain_2(notify_server_port, (caddr_t)name, namelen, token);
		if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	}

	token_table_add(name, namelen, NID_UNSET, token, cid, SLOT_NONE, NOTIFY_TYPE_PLAIN | NOTIFY_FLAG_REGEN, SIGNAL_NONE, FD_NONE, MACH_PORT_NULL, 0);

	*out_token = token;
	return NOTIFY_STATUS_OK;
}

uint32_t
notify_register_signal(const char *name, int sig, int *out_token)
{
	notify_state_t *ns_self;
	kern_return_t kstatus;
	uint32_t status;
	uint64_t nid;
	size_t namelen;
	int token;
	uint32_t cid;

	regenerate_check();

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	namelen = strlen(name);

	if (!strncmp(name, SELF_PREFIX, SELF_PREFIX_LEN))
	{
		ns_self = _notify_lib_self_state();
		if (ns_self == NULL) return NOTIFY_STATUS_FAILED;

		token = OSAtomicIncrement32((int32_t *)&token_id);
		status = _notify_lib_register_signal(ns_self, name, NOTIFY_CLIENT_SELF, token, sig, 0, 0, &nid);
		if (status != NOTIFY_STATUS_OK) return status;

		cid = token;
		token_table_add(name, namelen, nid, token, cid, SLOT_NONE, NOTIFY_FLAG_SELF | NOTIFY_TYPE_SIGNAL, sig, FD_NONE, MACH_PORT_NULL, 0);

		*out_token = token;
		return NOTIFY_STATUS_OK;
	}

	if (client_opts & NOTIFY_OPT_DEMUX)
	{
		return notify_register_dispatch(name, out_token, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^(int unused){ kill(getpid(), sig); });
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(EVENT_INIT);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	token = OSAtomicIncrement32((int32_t *)&token_id);

	if (notify_ipc_version == 0)
	{
		kstatus = _notify_server_register_signal(notify_server_port, (caddr_t)name, namelen, sig, (int32_t *)&cid, (int32_t *)&status);
		if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
		if (status != NOTIFY_STATUS_OK) return status;
	}
	else
	{
		cid = token;
		kstatus = _notify_server_register_signal_2(notify_server_port, (caddr_t)name, namelen, token, sig);
		if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	}

	token_table_add(name, namelen, NID_UNSET, token, cid, SLOT_NONE, NOTIFY_TYPE_SIGNAL | NOTIFY_FLAG_REGEN, sig, FD_NONE, MACH_PORT_NULL, 0);

	*out_token = token;
	return NOTIFY_STATUS_OK;
}

uint32_t
notify_register_mach_port(const char *name, mach_port_name_t *notify_port, int flags, int *out_token)
{
	notify_state_t *ns_self;
	kern_return_t kstatus;
	uint32_t status;
	uint64_t nid;
	task_t task;
	int token, mine;
	size_t namelen;
	uint32_t cid, tflags;
	token_table_node_t *t;
	mach_port_name_t port;

	regenerate_check();

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;
	if (notify_port == NULL) return NOTIFY_STATUS_INVALID_PORT;

	mine = 0;
	namelen = strlen(name);

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

		token = OSAtomicIncrement32((int32_t *)&token_id);
		status = _notify_lib_register_mach_port(ns_self, name, NOTIFY_CLIENT_SELF, token, *notify_port, 0, 0, &nid);
		if (status != NOTIFY_STATUS_OK)
		{
			if (mine == 1)
			{
				mach_port_mod_refs(task, *notify_port, MACH_PORT_RIGHT_RECEIVE, -1);
			}

			mach_port_deallocate(task, *notify_port);
			return status;
		}

		cid = token;
		token_table_add(name, namelen, nid, token, cid, SLOT_NONE, NOTIFY_FLAG_SELF | NOTIFY_TYPE_PORT, SIGNAL_NONE, FD_NONE, *notify_port, 0);

		*out_token = token;
		notify_retain_mach_port(*notify_port, mine);

		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(EVENT_INIT);
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

	if ((client_opts & NOTIFY_OPT_DEMUX) && (*notify_port != notify_common_port))
	{
		port = notify_common_port;
		kstatus = mach_port_insert_right(task, notify_common_port, notify_common_port, MACH_MSG_TYPE_MAKE_SEND);
	}
	else
	{
		port = *notify_port;
		kstatus = KERN_SUCCESS;
	}

	if (kstatus == KERN_SUCCESS)
	{
		token = OSAtomicIncrement32((int32_t *)&token_id);

		if (notify_ipc_version == 0)
		{
			kstatus = _notify_server_register_mach_port(notify_server_port, (caddr_t)name, namelen, port, token, (int32_t *)&cid, (int32_t *)&status);
			if ((kstatus == KERN_SUCCESS) && (status != NOTIFY_STATUS_OK)) kstatus = KERN_FAILURE;
		}
		else
		{
			cid = token;
			kstatus = _notify_server_register_mach_port_2(notify_server_port, (caddr_t)name, namelen, token, port);
		}
	}

	if (kstatus != KERN_SUCCESS)
	{
		if (mine == 1)
		{
			mach_port_mod_refs(task, *notify_port, MACH_PORT_RIGHT_RECEIVE, -1);
		}

		mach_port_deallocate(task, *notify_port);
		return NOTIFY_STATUS_FAILED;
	}

	tflags = NOTIFY_TYPE_PORT;
	if (port == notify_common_port) tflags |= NOTIFY_FLAG_REGEN;
	token_table_add(name, namelen, NID_UNSET, token, cid, SLOT_NONE, tflags, SIGNAL_NONE, FD_NONE, *notify_port, 0);

	if ((client_opts & NOTIFY_OPT_DEMUX) && (*notify_port != notify_common_port))
	{
		t = token_table_find_retain(token);
		if (t == NULL) return NOTIFY_STATUS_FAILED;

		/* remember to release the send right when this gets cancelled */
		t->flags |= NOTIFY_FLAG_RELEASE_SEND;

		port = *notify_port;
		t->queue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0);
		dispatch_retain(t->queue);
		t->block = (notify_handler_t)Block_copy(^(int unused){
			mach_msg_empty_send_t msg;
			kern_return_t kstatus;

			/* send empty message to the port with msgh_id = token; */
			memset(&msg, 0, sizeof(msg));
			msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSGH_BITS_ZERO);
			msg.header.msgh_remote_port = port;
			msg.header.msgh_local_port = MACH_PORT_NULL;
			msg.header.msgh_size = sizeof(mach_msg_empty_send_t);
			msg.header.msgh_id = token;

			kstatus = mach_msg(&(msg.header), MACH_SEND_MSG | MACH_SEND_TIMEOUT, msg.header.msgh_size, 0, MACH_PORT_NULL, 0, MACH_PORT_NULL);
		});

		token_table_release(t);
	}

	*out_token = token;
	notify_retain_mach_port(*notify_port, mine);

	return NOTIFY_STATUS_OK;
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
	uint32_t i, status;
	uint64_t nid;
	int token, mine, fdpair[2];
	size_t namelen;
	fileport_t fileport;
	kern_return_t kstatus;
	uint32_t cid;

	regenerate_check();

	mine = 0;

	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;
	if (notify_fd == NULL) return NOTIFY_STATUS_INVALID_FILE;

	namelen = strlen(name);

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

		token = OSAtomicIncrement32((int32_t *)&token_id);
		status = _notify_lib_register_file_descriptor(ns_self, name, NOTIFY_CLIENT_SELF, token, fdpair[1], 0, 0, &nid);
		if (status != NOTIFY_STATUS_OK)
		{
			if (mine == 1)
			{
				close(fdpair[0]);
				close(fdpair[1]);
			}

			return status;
		}

		cid = token;
		token_table_add(name, namelen, nid, token, cid, SLOT_NONE, NOTIFY_FLAG_SELF | NOTIFY_TYPE_FILE, SIGNAL_NONE, *notify_fd, MACH_PORT_NULL, 0);

		*out_token = token;
		notify_retain_file_descriptor(fdpair[0], fdpair[1]);

		return NOTIFY_STATUS_OK;
	}

	if (client_opts & NOTIFY_OPT_DEMUX)
	{
		/*
		 * Use dispatch to do a write() on fdpair[1] when notified.
		 */
		status = notify_register_mux_fd(name, out_token, fdpair[0], fdpair[1]);
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
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(EVENT_INIT);
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

	token = OSAtomicIncrement32((int32_t *)&token_id);

	if (notify_ipc_version == 0)
	{
		kstatus = _notify_server_register_file_descriptor(notify_server_port, (caddr_t)name, namelen, (mach_port_t)fileport, token, (int32_t *)&cid, (int32_t *)&status);
		if ((kstatus == KERN_SUCCESS) && (status != NOTIFY_STATUS_OK)) kstatus = KERN_FAILURE;
	}
	else
	{
		kstatus = _notify_server_register_file_descriptor_2(notify_server_port, (caddr_t)name, namelen, token, (mach_port_t)fileport);
	}

	if (kstatus != KERN_SUCCESS)
	{
		if (mine == 1)
		{
			close(fdpair[0]);
			close(fdpair[1]);
		}

		return NOTIFY_STATUS_FAILED;
	}

	token_table_add(name, namelen, NID_UNSET, token, cid, SLOT_NONE, NOTIFY_TYPE_FILE, SIGNAL_NONE, *notify_fd, MACH_PORT_NULL, 0);

	*out_token = token;
	notify_retain_file_descriptor(fdpair[0], fdpair[1]);

	return NOTIFY_STATUS_OK;
}

uint32_t
notify_check(int token, int *check)
{
	kern_return_t kstatus;
	uint32_t status, val;
	token_table_node_t *t;
	uint32_t tid;

	regenerate_check();

	t = token_table_find_retain(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & NOTIFY_FLAG_SELF)
	{
		/* _notify_lib_check returns NOTIFY_STATUS_FAILED if self_state is NULL */
		status = _notify_lib_check(self_state, NOTIFY_CLIENT_SELF, token, check);
		token_table_release(t);
		return status;
	}

	if (t->flags & NOTIFY_TYPE_MEMORY)
	{
		if (shm_base == NULL) return NOTIFY_STATUS_FAILED;

		*check = 0;
		val = shm_base[t->slot];
		if (t->val != val)
		{
			*check = 1;
			t->val = val;
		}

		token_table_release(t);
		return NOTIFY_STATUS_OK;
	}

	tid = token;
	if (notify_ipc_version == 0) tid = t->client_id;

	token_table_release(t);

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(EVENT_INIT);
		if (status != 0) return NOTIFY_STATUS_FAILED;
	}

	kstatus = _notify_server_check(notify_server_port, tid, check, (int32_t *)&status);

	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_peek(int token, uint32_t *val)
{
	token_table_node_t *t;
	uint32_t status;

	regenerate_check();

	t = token_table_find_retain(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & NOTIFY_FLAG_SELF)
	{
		/* _notify_lib_peek returns NOTIFY_STATUS_FAILED if self_state is NULL */
		status = _notify_lib_peek(self_state, NOTIFY_CLIENT_SELF, token, (int *)val);
		token_table_release(t);
		return status;
	}

	if (t->flags & NOTIFY_TYPE_MEMORY)
	{
		if (shm_base == NULL)
		{
			token_table_release(t);
			return NOTIFY_STATUS_FAILED;
		}

		*val = shm_base[t->slot];
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

	regenerate_check();

	t = token_table_find_retain(token);
	if (t == NULL) return NULL;

	if (t->flags & NOTIFY_FLAG_SELF)
	{
		/* _notify_lib_check_addr returns NOTIFY_STATUS_FAILED if self_state is NULL */
		val = _notify_lib_check_addr(self_state, NOTIFY_CLIENT_SELF, token);
		token_table_release(t);
		return val;
	}

	if (t->flags & NOTIFY_TYPE_MEMORY)
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
	char *dup;

	regenerate_check();

	if (path == NULL) return NOTIFY_STATUS_INVALID_REQUEST;

	t = token_table_find_retain(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & NOTIFY_FLAG_SELF)
	{
		token_table_release(t);
		return NOTIFY_STATUS_INVALID_REQUEST;
	}

	/* can only monitor one path with a token */
	if (t->path != NULL)
	{
		token_table_release(t);
		return NOTIFY_STATUS_INVALID_REQUEST;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(EVENT_INIT);
		if (status != 0)
		{
			token_table_release(t);
			return NOTIFY_STATUS_FAILED;
		}
	}

	len = strlen(path);
	dup = strdup(path);
	if (dup == NULL) return NOTIFY_STATUS_FAILED;

	if (notify_ipc_version == 0)
	{
		kstatus = _notify_server_monitor_file(notify_server_port, t->client_id, path, len, flags, (int32_t *)&status);
		if ((kstatus == KERN_SUCCESS) && (status != NOTIFY_STATUS_OK)) kstatus = KERN_FAILURE;
	}
	else
	{
		kstatus = _notify_server_monitor_file_2(notify_server_port, token, path, len, flags);
	}

	t->path = dup;
	t->path_flags = flags;

	token_table_release(t);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return NOTIFY_STATUS_OK;
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
	uint64_t nid;

	regenerate_check();

	t = token_table_find_retain(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;
	if (t->name_node == NULL)
	{
		token_table_release(t);
		return NOTIFY_STATUS_INVALID_TOKEN;
	}

	if (t->flags & NOTIFY_FLAG_SELF)
	{
		/* _notify_lib_get_state returns NOTIFY_STATUS_FAILED if self_state is NULL */
		status = _notify_lib_get_state(self_state, t->name_node->name_id, state, 0, 0);
		token_table_release(t);
		return status;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(EVENT_INIT);
		if (status != 0)
		{
			token_table_release(t);
			return NOTIFY_STATUS_FAILED;
		}
	}

	if (notify_ipc_version == 0)
	{
		kstatus = _notify_server_get_state(notify_server_port, t->client_id, state, (int32_t *)&status);
		if ((kstatus == KERN_SUCCESS) && (status != NOTIFY_STATUS_OK)) kstatus = KERN_FAILURE;
	}
	else
	{
		if (t->name_node->name_id >= NID_CALLED_ONCE)
		{
			kstatus = _notify_server_get_state_3(notify_server_port, t->token, state, (uint64_t *)&nid, (int32_t *)&status);
			if ((kstatus == KERN_SUCCESS) && (status == NOTIFY_STATUS_OK)) name_table_set_nid(t->name, nid);
		}
		else
		{
			kstatus = _notify_server_get_state_2(notify_server_port, t->name_node->name_id, state, (int32_t *)&status);
		}
	}

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
	uint64_t nid;

	regenerate_check();

	t = token_table_find_retain(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;
	if (t->name_node == NULL)
	{
		token_table_release(t);
		return NOTIFY_STATUS_INVALID_TOKEN;
	}

	if (t->flags & NOTIFY_FLAG_SELF)
	{
		/* _notify_lib_set_state returns NOTIFY_STATUS_FAILED if self_state is NULL */
		status = _notify_lib_set_state(self_state, t->name_node->name_id, state, 0, 0);
		token_table_release(t);
		return status;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(EVENT_INIT);
		if (status != 0)
		{
			token_table_release(t);
			return NOTIFY_STATUS_FAILED;
		}
	}

	status = NOTIFY_STATUS_OK;

	if (notify_ipc_version == 0)
	{
		kstatus = _notify_server_set_state(notify_server_port, t->client_id, state, (int32_t *)&status);
		if ((kstatus == KERN_SUCCESS) && (status != NOTIFY_STATUS_OK)) kstatus = KERN_FAILURE;
	}
	else
	{
		if (t->name_node->name_id >= NID_CALLED_ONCE)
		{
			kstatus = _notify_server_set_state_3(notify_server_port, t->token, state, (uint64_t *)&nid, (int32_t *)&status);
			if ((kstatus == KERN_SUCCESS) && (status == NOTIFY_STATUS_OK)) name_table_set_nid(t->name, nid);
		}
		else
		{
			status = NOTIFY_STATUS_OK;
			kstatus = _notify_server_set_state_2(notify_server_port, t->name_node->name_id, state);
		}
	}

	if ((kstatus == KERN_SUCCESS) && (status == NOTIFY_STATUS_OK))
	{
		t->set_state_time = mach_absolute_time();
		t->set_state_val = state;
	}

	token_table_release(t);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return NOTIFY_STATUS_OK;
}

uint32_t
notify_get_val(int token, int *val)
{
	kern_return_t kstatus;
	uint32_t status, slot, tid;
	token_table_node_t *t;

	regenerate_check();

	t = token_table_find_retain(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & NOTIFY_FLAG_SELF)
	{
		/* _notify_lib_get_val returns NOTIFY_STATUS_FAILED if self_state is NULL */
		status = _notify_lib_get_val(self_state, NOTIFY_CLIENT_SELF, t->token, val);
		token_table_release(t);
		return status;
	}

	if (t->flags & NOTIFY_TYPE_MEMORY)
	{
		slot = t->slot;
		token_table_release(t);

		if (shm_base == NULL) return NOTIFY_STATUS_FAILED;

		*val = shm_base[slot];
		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(EVENT_INIT);
		if (status != 0)
		{
			token_table_release(t);
			return NOTIFY_STATUS_FAILED;
		}
	}

	tid = token;
	if (notify_ipc_version == 0) tid = t->client_id;

	kstatus = _notify_server_get_val(notify_server_port, tid, val, (int32_t *)&status);

	token_table_release(t);
	if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_set_val(int token, int val)
{
	kern_return_t kstatus;
	uint32_t status, tid;
	token_table_node_t *t;

	regenerate_check();

	t = token_table_find_retain(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & NOTIFY_FLAG_SELF)
	{
		/* _notify_lib_set_val returns NOTIFY_STATUS_FAILED if self_state is NULL */
		status = _notify_lib_set_val(self_state, NOTIFY_CLIENT_SELF, t->token, val, 0, 0);
		token_table_release(t);
		return status;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(EVENT_INIT);
		if (status != 0)
		{
			token_table_release(t);
			return NOTIFY_STATUS_FAILED;
		}
	}

	tid = token;
	if (notify_ipc_version == 0) tid = t->client_id;

	kstatus = _notify_server_set_val(notify_server_port, tid, val, (int32_t *)&status);

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

	regenerate_check();

	/*
	 * Lock to prevent a race with notify_post, which uses the name ID.
	 * If we are cancelling the last registration for this name, then we need
	 * to block those routines from getting the name ID from the name table.
	 * Once notifyd gets the cancellation, the name may vanish, and the name ID
	 * held in the name table would go stale.
	 *
	 * Uses token_table_find_no_lock() which does not retain, and
	 * token_table_release_no_lock() which releases the token.
	 */
	pthread_mutex_lock(&notify_lock);

	t = token_table_find_no_lock(token);
	if (t == NULL)
	{
		pthread_mutex_unlock(&notify_lock);
		return NOTIFY_STATUS_INVALID_TOKEN;
	}

	if (t->flags & NOTIFY_FLAG_SELF)
	{
		/*
		 * _notify_lib_cancel returns NOTIFY_STATUS_FAILED if self_state is NULL
		 * We let it fail quietly.
		 */
		_notify_lib_cancel(self_state, NOTIFY_CLIENT_SELF, t->token);

		token_table_release_no_lock(t);
		pthread_mutex_unlock(&notify_lock);
		return NOTIFY_STATUS_OK;
	}

	if (notify_ipc_version == 0)
	{
		kstatus = _notify_server_cancel(notify_server_port, t->client_id, (int32_t *)&status);
		if ((kstatus == KERN_SUCCESS) && (status != NOTIFY_STATUS_OK)) kstatus = KERN_FAILURE;
	}
	else
	{
		kstatus = _notify_server_cancel_2(notify_server_port, token);
	}

	token_table_release_no_lock(t);
	pthread_mutex_unlock(&notify_lock);

	if ((kstatus == MIG_SERVER_DIED) || (kstatus == MACH_SEND_INVALID_DEST)) return NOTIFY_STATUS_OK;
	else if (kstatus != KERN_SUCCESS) return NOTIFY_STATUS_FAILED;

	return NOTIFY_STATUS_OK;
}

uint32_t
notify_suspend(int token)
{
	token_table_node_t *t;
	uint32_t status, tid;
	kern_return_t kstatus;

	regenerate_check();

	t = token_table_find_retain(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & NOTIFY_FLAG_SELF)
	{
		_notify_lib_suspend(self_state, NOTIFY_CLIENT_SELF, t->token);
		token_table_release(t);
		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(EVENT_INIT);
		if (status != 0)
		{
			token_table_release(t);
			return NOTIFY_STATUS_FAILED;
		}
	}

	tid = token;
	if (notify_ipc_version == 0) tid = t->client_id;

	kstatus = _notify_server_suspend(notify_server_port, tid, (int32_t *)&status);

	token_table_release(t);
	if (kstatus != KERN_SUCCESS) status = NOTIFY_STATUS_FAILED;
	return status;
}

uint32_t
notify_resume(int token)
{
	token_table_node_t *t;
	uint32_t status, tid;
	kern_return_t kstatus;

	regenerate_check();

	t = token_table_find_retain(token);
	if (t == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (t->flags & NOTIFY_FLAG_SELF)
	{
		_notify_lib_resume(self_state, NOTIFY_CLIENT_SELF, t->token);
		token_table_release(t);
		return NOTIFY_STATUS_OK;
	}

	if (notify_server_port == MACH_PORT_NULL)
	{
		status = _notify_lib_init(EVENT_INIT);
		if (status != 0)
		{
			token_table_release(t);
			return NOTIFY_STATUS_FAILED;
		}
	}

	tid = token;
	if (notify_ipc_version == 0) tid = t->client_id;

	kstatus = _notify_server_resume(notify_server_port, tid, (int32_t *)&status);

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
		status = _notify_lib_init(EVENT_INIT);
		if (status != 0)
		{
			return NOTIFY_STATUS_FAILED;
		}
	}

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
		status = _notify_lib_init(EVENT_INIT);
		if (status != 0)
		{
			return NOTIFY_STATUS_FAILED;
		}
	}

	kstatus = _notify_server_resume_pid(notify_server_port, pid, (int32_t *)&status);

	if (kstatus != KERN_SUCCESS) status = NOTIFY_STATUS_FAILED;
	return status;
}

/* Deprecated SPI */
uint32_t
notify_simple_post(const char *name)
{
	return notify_post(name);
}

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
#include <errno.h>
#include <pthread.h>
#include "notify.h"
#include "notify_ipc.h"
#include "common.h"

/* Required to prevent deadlocks */
int __notify_78945668_info__ = 0;

notify_state_t *
_notify_lib_notify_state_new(uint32_t flags)
{
	notify_state_t *ns;

	ns = (notify_state_t *)calloc(1, sizeof(notify_state_t));
	if (ns == NULL) return NULL;

	ns->flags = flags;

	if (ns->flags & NOTIFY_STATE_USE_LOCKS) 
	{
		ns->name_lock = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(ns->name_lock, NULL);

		ns->client_lock = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(ns->client_lock, NULL);
	}

	ns->name_table = _nc_table_new(8192);
	ns->client_table = _nc_table_new(8192);

	ns->sock = -1;

	return ns;
}

void
_notify_lib_notify_state_free(notify_state_t *ns)
{
	client_t *n;
	list_t *c;

	if (ns == NULL) return;

	_nc_table_free(ns->name_table);
	_nc_table_free(ns->client_table);

	if (ns->name_lock != NULL) free(ns->name_lock);
	if (ns->client_lock != NULL) free(ns->client_lock);

	c = ns->free_client_list;
	while (c != NULL)
	{
		n = (client_t *)_nc_list_data(c);
		c = _nc_list_next(c);
		if (n != NULL) free(n);
	}

	_nc_list_release_list(ns->free_client_list);

	if (ns->sock != -1)
	{
		shutdown(ns->sock, 2);
		close(ns->sock);
	}

	if (ns->controlled_name != NULL) free(ns->controlled_name);
}

static client_t *
_notify_lib_client_new(notify_state_t *ns)
{
	client_t *c;

	if (ns == NULL) return NULL;

	if (ns->free_client_list != NULL)
	{
		c = _nc_list_data(ns->free_client_list);
		ns->free_client_list = _nc_list_chop(ns->free_client_list);
	}
	else
	{
		c = calloc(1, sizeof(client_t));
		ns->client_id++;
		c->client_id = ns->client_id;
	}
	
	c->info = (client_info_t *)calloc(1, sizeof(client_info_t));
	c->info->lastval = 0;

	_nc_table_insert_n(ns->client_table, c->client_id, c);

	return c;
}

static void
free_client_info(client_info_t *info)
{
	if (info == NULL) return;

	switch (info->notify_type)
	{
		case NOTIFY_TYPE_SIGNAL:
			break;
		case NOTIFY_TYPE_FD:
			break;
		case NOTIFY_TYPE_PORT:
			if (info->msg != NULL)
			{
				if (info->msg->header.msgh_remote_port != MACH_PORT_NULL)
					mach_port_destroy(mach_task_self(), info->msg->header.msgh_remote_port);
				free(info->msg);
			}
			break;
		default:
			break;	
	}

	free(info);
}

static void
_notify_lib_client_release(notify_state_t *ns, client_t *c)
{
	client_t *p;
	uint32_t x;
	list_t *a, *l;

	if (ns == NULL) return;
	if (c == NULL) return;

	free_client_info(c->info);
	x = c->client_id;
	_nc_table_delete_n(ns->client_table, x);

	if (x == ns->client_id)
	{
		/* Recover this client ID */
		free(c);
		ns->client_id--;
	}
	else
	{
		/* Put this client in the free list */
		memset(c, 0, sizeof(client_t));
		c->client_id = x;

		if (ns->free_client_list == NULL)
		{
			ns->free_client_list = _nc_list_new(c);
			return;
		}

		/* Insert in decreasing order by client_id */

		a = NULL;
		l = ns->free_client_list;
		p = _nc_list_data(l);

		while ((l != NULL) && (p->client_id > x))
		{
			a = l;
			l = _nc_list_next(l);
			p = _nc_list_data(l);
		}

		if (l == NULL)
		{
			_nc_list_append(a, _nc_list_new(c));
		}
		else if (a == NULL)
		{
			ns->free_client_list = _nc_list_prepend(ns->free_client_list, _nc_list_new(c));
		}
		else
		{
			_nc_list_prepend(l, _nc_list_new(c));
		}
	}

	/* sweep the client free list once to recover usable client ids */
	p = _nc_list_data(ns->free_client_list);

	while ((p != NULL) && (p->client_id == ns->client_id))
	{
		free(p);
		ns->client_id--;

		ns->free_client_list = _nc_list_chop(ns->free_client_list);
		p = _nc_list_data(ns->free_client_list);
	}
}

static name_info_t *
_notify_lib_new_name(notify_state_t *ns, const char *name)
{
	name_info_t *n;

	if (ns == NULL) return NULL;
	if (name == NULL) return NULL;

	n = (name_info_t *)calloc(1, sizeof(name_info_t));
	if (n == NULL) return NULL;

	n->name = strdup(name);

	n->access = NOTIFY_ACCESS_DEFAULT;

	n->slot = (uint32_t)-1;
	n->val = 1;

	if (ns->flags & NOTIFY_STATE_USE_LOCKS)
	{
		n->lock = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(n->lock, NULL);

		pthread_mutex_lock(ns->name_lock);
	}

	_nc_table_insert(ns->name_table, name, n);

	if (ns->flags & NOTIFY_STATE_USE_LOCKS) pthread_mutex_unlock(ns->name_lock);

	return n;
}

static name_info_t *
_notify_lib_lookup_name_info(notify_state_t *ns, const char *name)
{
	name_info_t *n;

	if (ns == NULL) return NULL;
	if (name == NULL) return NULL;

	if (ns->flags & NOTIFY_STATE_USE_LOCKS) pthread_mutex_lock(ns->name_lock);

	n = (name_info_t *)_nc_table_find(ns->name_table, name);

	if (ns->flags & NOTIFY_STATE_USE_LOCKS) pthread_mutex_unlock(ns->name_lock);

	return n;
}

uint32_t
_notify_lib_check_controlled_access(notify_state_t *ns, char *name, uint32_t uid, uint32_t gid, int req)
{
	uint32_t i, len, plen;
	name_info_t *p;

	if (ns == NULL) return NOTIFY_STATUS_FAILED;
	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	/* root can do anything */
	if (uid == 0) return NOTIFY_STATUS_OK;

	/* check if the name is in a reserved subspace */

	len = strlen(name);

	for (i = 0; i < ns->controlled_name_count; i++)
	{
		p = ns->controlled_name[i];
		if (p == NULL) break;

		plen = strlen(p->name);

		/* don't check the input name in this loop */
		if (!strcmp(p->name, name)) continue;

		/* check if this key is a prefix */
		if (plen >= len) continue;
		if (strncmp(p->name, name, plen)) continue;

		/* Found a prefix, check if restrictions apply to this uid/gid */
		if ((p->uid == uid) && (p->access & (req << NOTIFY_ACCESS_USER_SHIFT))) break;
		if ((p->gid == gid) && (p->access & (req << NOTIFY_ACCESS_GROUP_SHIFT))) break;
		if (p->access & (req << NOTIFY_ACCESS_OTHER_SHIFT)) break;

		/* prefix node blocks access */
		return NOTIFY_STATUS_NOT_AUTHORIZED;
	}

	return NOTIFY_STATUS_OK;
}

static uint32_t
_notify_lib_check_access(notify_state_t *ns, name_info_t *n, uint32_t uid, uint32_t gid, int req)
{
	uint32_t status;

	if (ns == NULL) return NOTIFY_STATUS_FAILED;
	if (n == NULL) return NOTIFY_STATUS_INVALID_NAME;

	/* root can do anything */
	if (uid == 0) return NOTIFY_STATUS_OK;

	status = _notify_lib_check_controlled_access(ns, n->name, uid, gid, req);
	if (status != NOTIFY_STATUS_OK) return status;

	/* check user access rights */
	if ((n->uid == uid) && (n->access & (req << NOTIFY_ACCESS_USER_SHIFT))) return NOTIFY_STATUS_OK;

	/* check group access rights */
	if ((n->gid == gid) && (n->access & (req << NOTIFY_ACCESS_GROUP_SHIFT))) return NOTIFY_STATUS_OK;

	/* check other access rights */
	if (n->access & (req << NOTIFY_ACCESS_OTHER_SHIFT)) return NOTIFY_STATUS_OK;

	return NOTIFY_STATUS_NOT_AUTHORIZED;
}

/*
 * Notify a client.
 */
static void
_notify_lib_send(notify_state_t *ns, client_t *c)
{
	uint32_t cid, status;
	struct sockaddr_in sin;
	kern_return_t kstatus;

	if (ns == NULL) return;
	if (c == NULL) return;

	switch (c->info->notify_type)
	{
		case NOTIFY_TYPE_SIGNAL:
			kill(c->info->pid, c->info->sig);
			break;
		case NOTIFY_TYPE_FD:
			if (ns->sock == -1)
			{
				if (ns->flags & NOTIFY_STATE_USE_LOCKS) pthread_mutex_lock(ns->name_lock);
				if (ns->sock == -1) ns->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
				if (ns->flags & NOTIFY_STATE_USE_LOCKS) pthread_mutex_unlock(ns->name_lock);
			}

			memset(&sin, 0, sizeof(struct sockaddr_in));
			sin.sin_family = AF_INET;
			sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			sin.sin_port = c->info->port;
			cid = c->info->token;
			status = sendto(ns->sock, &cid, 4, 0, (struct sockaddr *)&sin, sizeof(struct sockaddr_in));
			break;
		case NOTIFY_TYPE_PORT:
			c->info->msg->header.msgh_id = (mach_msg_id_t)(c->info->token);
			kstatus = mach_msg(&(c->info->msg->header), 
				MACH_SEND_MSG, 
				c->info->msg->header.msgh_size, 
				0, 
				MACH_PORT_NULL,
				MACH_MSG_TIMEOUT_NONE,
				MACH_PORT_NULL);
			if (kstatus == MACH_SEND_INVALID_DEST)
			{
				/* XXX clean up XXX */
			}
			break;
		default:
			break;	
	}
}

/*
 * Notify clients of this name.
 */
uint32_t
_notify_lib_post(notify_state_t *ns, const char *name, uint32_t uid, uint32_t gid)
{
	name_info_t *n;
	client_t *c;
	list_t *l;
	int auth;

	if (ns == NULL) return NOTIFY_STATUS_FAILED;

	n = _notify_lib_lookup_name_info(ns, name);
	if (n == NULL) return NOTIFY_STATUS_INVALID_NAME;

	auth = _notify_lib_check_access(ns, n, uid, gid, NOTIFY_ACCESS_WRITE);
	if (auth != 0) return NOTIFY_STATUS_NOT_AUTHORIZED;

	n->val++;

	if (ns->flags & NOTIFY_STATE_USE_LOCKS) pthread_mutex_lock(n->lock);

	l = n->client_list;

	if (ns->flags & NOTIFY_STATE_USE_LOCKS) pthread_mutex_unlock(n->lock);

	while (l != NULL)
	{
		c = _nc_list_data(l);
		_notify_lib_send(ns, c);
		l = _nc_list_next(l);
	}

	return NOTIFY_STATUS_OK;
}

/*
 * Cancel (delete) a client
 */
void
_notify_lib_cancel(notify_state_t *ns, uint32_t cid)
{
	client_t *c;
	name_info_t *n;

	if (ns == NULL) return;
	if (cid == 0) return;

	c = NULL;
	n = NULL;

	c = _nc_table_find_n(ns->client_table, cid);
	if (c == NULL) return;

	n = c->info->name_info;
	if (n == NULL) return;

	if (ns->flags & NOTIFY_STATE_USE_LOCKS) pthread_mutex_lock(n->lock);

	n->refcount--;

	n->client_list = _nc_list_find_release(n->client_list, c);

	_notify_lib_client_release(ns, c);

	if (ns->flags & NOTIFY_STATE_USE_LOCKS) pthread_mutex_unlock(n->lock);

	if (n->refcount == 0)
	{
		_nc_table_delete(ns->name_table, n->name);
		if (n->lock != NULL) free(n->lock);
		free(n->name);
		free(n);
	}
}

/*
 * Delete all clients for a session
 */
void
_notify_lib_cancel_session(notify_state_t *ns, task_t t)
{
	client_t *c, *a, *p;
	name_info_t *n;
	void *tt;
	list_t *l, *x;

	a = NULL;
	x = NULL;
	p = NULL;

	tt = _nc_table_traverse_start(ns->name_table);
	while (tt != NULL)
	{
		n = _nc_table_traverse(ns->name_table, tt);
		if (n == NULL) break;

		for (l = n->client_list; l != NULL; l = _nc_list_next(l))
		{
			c = _nc_list_data(l);
			if (c->info->session == t)
			{
				a = (client_t *)malloc(sizeof(client_t));
				a->client_id = c->client_id;
				a->info = c->info;

				x = _nc_list_prepend(x, _nc_list_new(a));
			}
		}
	}
	_nc_table_traverse_end(ns->name_table, tt);

	for (l = x; l != NULL; l = _nc_list_next(l))
	{
		c = _nc_list_data(l);
		_notify_lib_cancel(ns, c->client_id);
		free(c);
	}

	_nc_list_release_list(x);
}

/*
 * Check if a name has changed since the last time this client checked.
 * Returns true, false, or error.
 */
uint32_t
_notify_lib_check(notify_state_t *ns, uint32_t cid, int *check)
{
	client_t *c;

	if (ns == NULL) return NOTIFY_STATUS_FAILED;
	if (cid == 0) return NOTIFY_STATUS_INVALID_TOKEN;

	c = _nc_table_find_n(ns->client_table, cid);

	if (c == NULL) return NOTIFY_STATUS_INVALID_TOKEN;
	if (c->info->name_info == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	if (c->info->name_info->val == c->info->lastval)
	{
		*check = 0;
		return NOTIFY_STATUS_OK;
	}

	c->info->lastval = c->info->name_info->val;
	*check = 1;
	return NOTIFY_STATUS_OK;
}

/*
 * SPI: get velue for a name.
 */
uint32_t
_notify_lib_peek(notify_state_t *ns, uint32_t cid, int *val)
{
	client_t *c;

	if (ns == NULL) return NOTIFY_STATUS_FAILED;
	if (cid == 0) return NOTIFY_STATUS_INVALID_TOKEN;

	c = _nc_table_find_n(ns->client_table, cid);

	if (c == NULL) return NOTIFY_STATUS_INVALID_TOKEN;
	if (c->info->name_info == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	*val = c->info->name_info->val;
	return NOTIFY_STATUS_OK;
}

/*
 * Get state value for a name.
 */
uint32_t
_notify_lib_get_state(notify_state_t *ns, uint32_t cid, int *state)
{
	client_t *c;

	*state = 0;

	if (ns == NULL) return NOTIFY_STATUS_FAILED;
	if (cid == 0) return NOTIFY_STATUS_INVALID_TOKEN;

	c = _nc_table_find_n(ns->client_table, cid);

	if (c == NULL) return NOTIFY_STATUS_INVALID_TOKEN;
	if (c->info->name_info == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	*state = c->info->name_info->state;
	return NOTIFY_STATUS_OK;
}

/*
 * Set state value for a name.
 */
uint32_t
_notify_lib_set_state(notify_state_t *ns, uint32_t cid, int state, uint32_t uid, uint32_t gid)
{
	client_t *c;
	int auth;

	if (ns == NULL) return NOTIFY_STATUS_FAILED;
	if (cid == 0) return NOTIFY_STATUS_INVALID_TOKEN;

	c = _nc_table_find_n(ns->client_table, cid);

	if (c == NULL) return NOTIFY_STATUS_INVALID_TOKEN;
	if (c->info->name_info == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	auth = _notify_lib_check_access(ns, c->info->name_info, uid, gid, NOTIFY_ACCESS_WRITE);
	if (auth != 0) return NOTIFY_STATUS_NOT_AUTHORIZED;

	c->info->name_info->state = state;
	return NOTIFY_STATUS_OK;
}

/*
 * Register for signal.
 * Returns the client_id;
 */
uint32_t
_notify_lib_register_signal
(
	notify_state_t *ns,
	const char *name,
	task_t task,
	uint32_t sig,
	uint32_t uid,
	uint32_t gid,
	uint32_t *out_token
)
{
	name_info_t *n;
	client_t *c;
	int pid, auth;
	kern_return_t ks;

	if (ns == NULL) return 0;
	if (name == NULL) return 0;

	n = _notify_lib_lookup_name_info(ns, name);
	if (n == NULL) n = _notify_lib_new_name(ns, name);
	if (n == NULL) return NOTIFY_STATUS_FAILED;

	auth = _notify_lib_check_access(ns, n, uid, gid, NOTIFY_ACCESS_READ);
	if (auth != 0) return NOTIFY_STATUS_NOT_AUTHORIZED;

	ks = pid_for_task(task, &pid);
	if (ks != KERN_SUCCESS) return 0;

	n->refcount++;

	c = _notify_lib_client_new(ns);
	if (c == NULL) return 0;

	c->info->name_info = n;
	c->info->notify_type = NOTIFY_TYPE_SIGNAL;
	c->info->pid = pid;
	c->info->sig = sig;
	c->info->session = task;

	if (ns->flags & NOTIFY_STATE_USE_LOCKS) pthread_mutex_lock(n->lock);
	n->client_list = _nc_list_prepend(n->client_list, _nc_list_new(c));
	if (ns->flags & NOTIFY_STATE_USE_LOCKS) pthread_mutex_unlock(n->lock);

	*out_token = c->client_id;
	return NOTIFY_STATUS_OK;
}

/*
 * Register for notification on a file descriptor.
 * Returns the client_id;
 */
uint32_t
_notify_lib_register_file_descriptor
(
	notify_state_t *ns,
	const char *name,
	task_t task,
	uint32_t port,
	uint32_t token,
	uint32_t uid,
	uint32_t gid,
	uint32_t *out_token
)
{
	name_info_t *n;
	client_t *c;
	int auth;

	if (ns == NULL) return 0;
	if (name == NULL) return 0;

	n = _notify_lib_lookup_name_info(ns, name);
	if (n == NULL) n = _notify_lib_new_name(ns, name);
	if (n == NULL) return NOTIFY_STATUS_FAILED;

	auth = _notify_lib_check_access(ns, n, uid, gid, NOTIFY_ACCESS_READ);
	if (auth != 0) return NOTIFY_STATUS_NOT_AUTHORIZED;

	n->refcount++;

	c = _notify_lib_client_new(ns);
	if (c == NULL) return 0;

	c->info->name_info = n;
	c->info->notify_type = NOTIFY_TYPE_FD;
	c->info->port = port;
	c->info->token = token;
	c->info->session = task;

	if (ns->flags & NOTIFY_STATE_USE_LOCKS) pthread_mutex_lock(n->lock);
	n->client_list = _nc_list_prepend(n->client_list, _nc_list_new(c));
	if (ns->flags & NOTIFY_STATE_USE_LOCKS) pthread_mutex_unlock(n->lock);

	*out_token = c->client_id;
	return NOTIFY_STATUS_OK;
}

/*
 * Register for notification on a mach port.
 * Returns the client_id;
 */
uint32_t
_notify_lib_register_mach_port
(
	notify_state_t *ns,
	const char *name,
	task_t task,
	mach_port_t port,
	uint32_t token,
	uint32_t uid,
	uint32_t gid,
	uint32_t *out_token
)
{
	name_info_t *n;
	client_t *c;
	int auth;

	if (ns == NULL) return 0;
	if (name == NULL) return 0;

	n = _notify_lib_lookup_name_info(ns, name);
	if (n == NULL) n = _notify_lib_new_name(ns, name);
	if (n == NULL) return NOTIFY_STATUS_FAILED;

	auth = _notify_lib_check_access(ns, n, uid, gid, NOTIFY_ACCESS_READ);
	if (auth != 0) return NOTIFY_STATUS_NOT_AUTHORIZED;

	n->refcount++;

	c = _notify_lib_client_new(ns);
	if (c == NULL) return 0;

	c->info->name_info = n;
	c->info->notify_type = NOTIFY_TYPE_PORT;
	c->info->msg = (mach_msg_empty_send_t *)calloc(1, sizeof(mach_msg_empty_send_t));
	c->info->msg->header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSGH_BITS_ZERO);
	c->info->msg->header.msgh_remote_port = port;
	c->info->msg->header.msgh_local_port = MACH_PORT_NULL;
	c->info->msg->header.msgh_size = sizeof(mach_msg_empty_send_t);
	c->info->msg->header.msgh_id = token;
	c->info->token = token;
	c->info->session = task;

	if (ns->flags & NOTIFY_STATE_USE_LOCKS) pthread_mutex_lock(n->lock);
	n->client_list = _nc_list_prepend(n->client_list, _nc_list_new(c));
	if (ns->flags & NOTIFY_STATE_USE_LOCKS) pthread_mutex_unlock(n->lock);

	*out_token = c->client_id;
	return NOTIFY_STATUS_OK;
}

/*
 * Plain registration - only for notify_check()
 * Returns the client_id.
 */
uint32_t
_notify_lib_register_plain
(
	notify_state_t *ns,
	const char *name,
	task_t task,
	uint32_t slot,
	uint32_t uid,
	uint32_t gid,
	uint32_t *out_token
)
{
	name_info_t *n;
	client_t *c;
	int auth;

	if (ns == NULL) return NOTIFY_STATUS_FAILED;
	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;
	if (out_token == NULL) return NOTIFY_STATUS_INVALID_TOKEN;

	n = _notify_lib_lookup_name_info(ns, name);
	if (n == NULL) n = _notify_lib_new_name(ns, name);
	if (n == NULL) return NOTIFY_STATUS_FAILED;

	auth = _notify_lib_check_access(ns, n, uid, gid, NOTIFY_ACCESS_READ);
	if (auth != 0) return NOTIFY_STATUS_NOT_AUTHORIZED;

	n->refcount++;
	if (slot != (uint32_t)-1) n->slot = slot;

	c = _notify_lib_client_new(ns);
	if (c == NULL) return NOTIFY_STATUS_FAILED;

	c->info->name_info = n;
	if (slot == (uint32_t)-1) c->info->notify_type = NOTIFY_TYPE_PLAIN;
	else c->info->notify_type = NOTIFY_TYPE_MEMORY;
	c->info->session = task;

	if (ns->flags & NOTIFY_STATE_USE_LOCKS) pthread_mutex_lock(n->lock);
	n->client_list = _nc_list_prepend(n->client_list, _nc_list_new(c));
	if (ns->flags & NOTIFY_STATE_USE_LOCKS) pthread_mutex_unlock(n->lock);

	*out_token = c->client_id;
	return NOTIFY_STATUS_OK;
}

static void
insert_controlled_name(notify_state_t *ns, name_info_t *n)
{
	int i, j;

	for (i = 0; i < ns->controlled_name_count; i++)
	{
		if (ns->controlled_name[i] == n) return;
	}

	if (ns->controlled_name_count == 0)
	{
		ns->controlled_name = (name_info_t **)malloc(sizeof(name_info_t *));
	}
	else
	{
		ns->controlled_name = (name_info_t **)realloc(ns->controlled_name, (ns->controlled_name_count + 1) * sizeof(name_info_t *));
	}

	/*
	 * Insert name in reverse sorted order (longer names preceed shorter names).
	 * this means that in _notify_lib_check_controlled_access, we check
	 * subspaces from the bottom up - i.e. we check access for the "deepest"
	 * controlled subspace.
	 */

	for (i = 0; i < ns->controlled_name_count; i++)
	{
		if (strcmp(n->name, ns->controlled_name[i]->name) > 0) break;
	}

	for (j = ns->controlled_name_count; j > i; j--)
	{
		ns->controlled_name[j] = ns->controlled_name[j-1];
	}

	ns->controlled_name[i] = n;
	ns->controlled_name_count++;
}

uint32_t
_notify_lib_set_owner
(
	notify_state_t *ns,
	const char *name,
	uint32_t uid,
	uint32_t gid
)
{
	name_info_t *n;

	if (ns == NULL) return NOTIFY_STATUS_FAILED;
	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	n = _notify_lib_lookup_name_info(ns, name);
	if (n == NULL)
	{
		/* create new name */
		n = _notify_lib_new_name(ns, name);
		if (n == NULL) return NOTIFY_STATUS_FAILED;

		n->access = NOTIFY_ACCESS_DEFAULT;
		n->refcount++;
	}

	n->uid = uid;
	n->gid = gid;

	insert_controlled_name(ns, n);

	return NOTIFY_STATUS_OK;
}

uint32_t
_notify_lib_get_owner
(
	notify_state_t *ns,
	const char *name,
	uint32_t *uid,
	uint32_t *gid
)
{
	name_info_t *n;
	int i, nlen, len;

	n = _notify_lib_lookup_name_info(ns, name);
	if (n != NULL)
	{
		*uid = n->uid;
		*gid = n->gid;
		return NOTIFY_STATUS_OK;
	}

	len = strlen(name);

	for (i = 0; i < ns->controlled_name_count; i++)
	{
		n = ns->controlled_name[i];
		if (n == NULL) break;

		nlen = strlen(n->name);

		if (!strcmp(n->name, name))
		{
			*uid = n->uid;
			*gid = n->gid;
			return NOTIFY_STATUS_OK;
		}

		/* check if this key is a prefix */
		if (nlen >= len) continue;
		if (strncmp(n->name, name, nlen)) continue;

		*uid = n->uid;
		*gid = n->gid;
		return NOTIFY_STATUS_OK;
	}

	*uid = 0;
	*gid = 0;
	return NOTIFY_STATUS_OK;
}

uint32_t
_notify_lib_set_access
(
	notify_state_t *ns,
	const char *name,
	uint32_t mode
)
{
	name_info_t *n;

	if (ns == NULL) return NOTIFY_STATUS_FAILED;
	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	n = _notify_lib_lookup_name_info(ns, name);
	if (n == NULL)
	{
		/* create new name */
		n = _notify_lib_new_name(ns, name);
		if (n == NULL) return NOTIFY_STATUS_FAILED;

		n->refcount++;
	}

	n->access = mode;

	insert_controlled_name(ns, n);

	return NOTIFY_STATUS_OK;
}

uint32_t
_notify_lib_get_access
(
	notify_state_t *ns,
	const char *name,
	uint32_t *mode
)
{
	name_info_t *n;
	int i, nlen, len;

	n = _notify_lib_lookup_name_info(ns, name);
	if (n != NULL)
	{
		*mode = n->access;
		return NOTIFY_STATUS_OK;
	}

	len = strlen(name);

	for (i = 0; i < ns->controlled_name_count; i++)
	{
		n = ns->controlled_name[i];
		if (n == NULL) break;

		nlen = strlen(n->name);

		if (!strcmp(n->name, name))
		{
			*mode = n->access;
			return NOTIFY_STATUS_OK;
		}

		/* check if this key is a prefix */
		if (nlen >= len) continue;
		if (strncmp(n->name, name, nlen)) continue;

		*mode = n->access;
		return NOTIFY_STATUS_OK;
	}

	*mode = NOTIFY_ACCESS_DEFAULT;
	return NOTIFY_STATUS_OK;
}

uint32_t
_notify_lib_release_name
(
	notify_state_t *ns,
	const char *name,
	uint32_t uid,
	uint32_t gid
)
{
	name_info_t *n;
	uint32_t i, j;

	if (ns == NULL) return NOTIFY_STATUS_FAILED;
	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	n = _notify_lib_lookup_name_info(ns, name);
	if (n == NULL) return NOTIFY_STATUS_INVALID_NAME;

	/* Owner and root may release */
	if ((n->uid != uid) && (uid != 0)) return NOTIFY_STATUS_NOT_AUTHORIZED;

	for (i = 0; i < ns->controlled_name_count; i++)
	{
		if (ns->controlled_name[i] == n)
		{
			for (j = i + 1; j < ns->controlled_name_count; j++)
			{
				ns->controlled_name[j-1] = ns->controlled_name[j];
			}

			ns->controlled_name_count--;
			if (ns->controlled_name_count == 0)
			{
				free(ns->controlled_name);
				ns->controlled_name = NULL;
			}
			else
			{
				ns->controlled_name = (name_info_t **)realloc(ns->controlled_name, ns->controlled_name_count * sizeof(name_info_t *));
			}

			break;
		}
	}

	n->refcount--;

	if (n->refcount == 0)
	{
		_nc_table_delete(ns->name_table, n->name);
		free(n->name);
		if (n->lock != NULL) free(n->lock);
		free(n);
	}

	return NOTIFY_STATUS_OK;
}


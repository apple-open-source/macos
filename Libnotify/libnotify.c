/*
 * Copyright (c) 2003-2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 2003 Apple Computer, Inc.  All Rights
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

#include <sys/types.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/ipc.h>
#include <signal.h>
#include <mach/mach.h>
#include <errno.h>
#include <pthread.h>
#include "libnotify.h"
#include "notify.h"

#define MACH_PORT_SEND_TIMEOUT 50

#define USER_PROTECTED_UID_PREFIX "user.uid."
#define USER_PROTECTED_UID_PREFIX_LEN 9

/* Required to prevent deadlocks */
int __notify_78945668_info__ = 0;

notify_state_t *
_notify_lib_notify_state_new(uint32_t flags, uint32_t table_size)
{
	notify_state_t *ns;

	ns = (notify_state_t *)calloc(1, sizeof(notify_state_t));
	if (ns == NULL) return NULL;

	ns->flags = flags;

	if (ns->flags & NOTIFY_STATE_USE_LOCKS) 
	{
		ns->lock = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(ns->lock, NULL);
	}

	ns->name_table = _nc_table_new(table_size);
	ns->client_table = _nc_table_new(table_size);

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

	if (ns->lock != NULL)
	{
		pthread_mutex_destroy(ns->lock);
		free(ns->lock);
	}

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
_internal_client_new(notify_state_t *ns)
{
	client_t *c;

	if (ns == NULL) return NULL;

	c = NULL;

	if (ns->free_client_list != NULL)
	{
		c = _nc_list_data(ns->free_client_list);
		ns->free_client_list = _nc_list_chop(ns->free_client_list);
	}
	else
	{
		c = calloc(1, sizeof(client_t));
		if (c == NULL) return NULL;

		ns->client_id++;
		if (ns->client_id == NOTIFY_INVALID_CLIENT_ID) ns->client_id++;
		c->client_id = ns->client_id;
	}

	if (c == NULL) return NULL;

	c->info = (client_info_t *)calloc(1, sizeof(client_info_t));
	if (c->info == NULL) return NULL;

	c->info->lastval = 0;

	_nc_table_insert_n(ns->client_table, c->client_id, c);

	return c;
}

static void
_internal_free_client_info(client_info_t *info)
{
	if (info == NULL) return;

	switch (info->notify_type)
	{
		case NOTIFY_TYPE_SIGNAL:
		{
			break;
		}

		case NOTIFY_TYPE_FD:
		{
			if (info->fd > 0) close(info->fd);
			break;
		}

		case NOTIFY_TYPE_PORT:
		{
			if (info->msg != NULL)
			{
				if (info->msg->header.msgh_remote_port != MACH_PORT_NULL)
				{
					/* release my send right to the port */
					mach_port_deallocate(mach_task_self(), info->msg->header.msgh_remote_port);
				}
				free(info->msg);
			}
			break;
		}

		default:
		{
			break;
		}
	}

	if (info->session != TASK_NAME_NULL) 
	{
		mach_port_deallocate(mach_task_self(), info->session);
		info->session = TASK_NAME_NULL;
	}

	free(info);
}

static void
_internal_client_release(notify_state_t *ns, client_t *c)
{
	client_t *p;
	uint32_t x;
	list_t *a, *l;

	if (ns == NULL) return;
	if (c == NULL) return;

	_internal_free_client_info(c->info);
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
_internal_new_name(notify_state_t *ns, const char *name)
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

	_nc_table_insert(ns->name_table, name, n);

	return n;
}

static void
_internal_insert_controlled_name(notify_state_t *ns, name_info_t *n)
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
		ns->controlled_name = (name_info_t **)reallocf(ns->controlled_name, (ns->controlled_name_count + 1) * sizeof(name_info_t *));
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

static void
_internal_remove_controlled_name(notify_state_t *ns, name_info_t *n)
{
	uint32_t i, j;

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
				ns->controlled_name = (name_info_t **)reallocf(ns->controlled_name, ns->controlled_name_count * sizeof(name_info_t *));
			}

			return;
		}
	}
}

/* 
 * Only owner and root may access user.uid.<uid> or user.uid.<uid>.[...]
 */
static uint32_t
_internal_check_user_protected(const char *name, uid_t uid, int *is_protected)
{
	char str[64];
	uint32_t len;

	*is_protected = 0;

	/* if it doesn't have "user.uid." as a prefix, it's not in the user-protected namespace */
	if (strncmp(name, USER_PROTECTED_UID_PREFIX, USER_PROTECTED_UID_PREFIX_LEN)) return NOTIFY_STATUS_OK;

	*is_protected = 1;

	/* root may do anything */
	if (uid == 0) return NOTIFY_STATUS_OK;

	snprintf(str, sizeof(str) - 1, "%s%d", USER_PROTECTED_UID_PREFIX, uid);
	len = strlen(str);

	/* user may access user.uid.<uid> or a subtree name */
	if ((!strncmp(name, str, len)) && ((name[len] == '\0') || (name[len] == '.'))) return NOTIFY_STATUS_OK;
	return NOTIFY_STATUS_NOT_AUTHORIZED;
}

static uint32_t
_internal_check_controlled_access(notify_state_t *ns, char *name, uid_t uid, gid_t gid, int req)
{
	uint32_t status, i, len, plen;
	name_info_t *p;
	int is_protected;

	if (ns == NULL) return NOTIFY_STATUS_FAILED;
	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	/* root can do anything */
	if (uid == 0) return NOTIFY_STATUS_OK;

	/* check if name is user protected */
	is_protected = 0;
	status = _internal_check_user_protected(name, uid, &is_protected);
	if (status != NOTIFY_STATUS_OK) return status;

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

uint32_t
_notify_lib_check_controlled_access(notify_state_t *ns, char *name, uid_t uid, gid_t gid, int req)
{
	int status;

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);
	status = _internal_check_controlled_access(ns, name, uid, gid, req);
	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);

	return status;
}

static uint32_t
_internal_check_access(notify_state_t *ns, name_info_t *n, uid_t uid, gid_t gid, int req)
{
	uint32_t status;

	if (ns == NULL) return NOTIFY_STATUS_FAILED;
	if (n == NULL) return NOTIFY_STATUS_INVALID_NAME;

	/* root can do anything */
	if (uid == 0) return NOTIFY_STATUS_OK;

	status = _internal_check_controlled_access(ns, n->name, uid, gid, req);
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
_internal_send(notify_state_t *ns, client_t *c)
{
	uint32_t cid;
	ssize_t len;
	kern_return_t kstatus;

	if (ns == NULL) return;
	if (c == NULL) return;
	if (c->info == NULL) return;

	if (c->info->state & NOTIFY_CLIENT_STATE_SUSPENDED)
	{
		c->info->state |= NOTIFY_CLIENT_STATE_PENDING;
		return;
	}

	switch (c->info->notify_type)
	{
		case NOTIFY_TYPE_SIGNAL:
		{
			kill(c->info->pid, c->info->sig);
			break;
		}

		case NOTIFY_TYPE_FD:
		{
			if (c->info->fd >= 0)
			{
				cid = htonl(c->info->token);
				len = write(c->info->fd, &cid, sizeof(uint32_t));
				if (len != sizeof(uint32_t))
				{
					close(c->info->fd);
					c->info->fd = -1;
				}
			}
			break;
		}

		case NOTIFY_TYPE_PORT:
		{
			c->info->msg->header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSGH_BITS_ZERO);
			c->info->msg->header.msgh_local_port = MACH_PORT_NULL;
			c->info->msg->header.msgh_size = sizeof(mach_msg_empty_send_t);
			c->info->msg->header.msgh_id = (mach_msg_id_t)(c->info->token);

			kstatus = mach_msg(&(c->info->msg->header),
							   MACH_SEND_MSG | MACH_SEND_TIMEOUT,
							   c->info->msg->header.msgh_size,
							   0,
							   MACH_PORT_NULL,
							   MACH_PORT_SEND_TIMEOUT,
							   MACH_PORT_NULL);

			if (kstatus == MACH_SEND_INVALID_DEST)
			{
				/* XXX clean up XXX */
			}
			break;
		}

		default:
		{
			break;
		}
	}
}

uint32_t
_notify_lib_post_client(notify_state_t *ns, client_t *c)
{
	if (ns == NULL) return NOTIFY_STATUS_FAILED;
	if (c == NULL) return NOTIFY_STATUS_FAILED;

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);
	_internal_send(ns, c);
	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);

	return NOTIFY_STATUS_OK;
}

/*
 * Notify clients of this name.
 */
uint32_t
_notify_lib_post(notify_state_t *ns, const char *name, uid_t uid, gid_t gid)
{
	name_info_t *n;
	client_t *c;
	list_t *l;
	int auth;

	if (ns == NULL) return NOTIFY_STATUS_FAILED;

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);

	n = (name_info_t *)_nc_table_find(ns->name_table, name);
	if (n == NULL)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NOTIFY_STATUS_INVALID_NAME;
	}

	auth = _internal_check_access(ns, n, uid, gid, NOTIFY_ACCESS_WRITE);
	if (auth != 0)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NOTIFY_STATUS_NOT_AUTHORIZED;
	}

	n->val++;

	l = n->client_list;

	while (l != NULL)
	{
		c = _nc_list_data(l);
		_internal_send(ns, c);
		l = _nc_list_next(l);
	}

	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
	return NOTIFY_STATUS_OK;
}

static void
_internal_release_name_info(notify_state_t *ns, name_info_t *n)
{
	if (n->refcount > 0) n->refcount--;
	if (n->refcount == 0)
	{
		_internal_remove_controlled_name(ns, n);
		_nc_table_delete(ns->name_table, n->name);
		free(n->name);
		free(n);
	}
}

/*
 * Cancel (delete) a client
 */
static void
_internal_cancel(notify_state_t *ns, uint32_t cid)
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

	n->client_list = _nc_list_find_release(n->client_list, c);
	_internal_client_release(ns, c);
	_internal_release_name_info(ns, n);
}

void
_notify_lib_cancel(notify_state_t *ns, uint32_t cid)
{
	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);
	_internal_cancel(ns, cid);
	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
}

void
_notify_lib_suspend(notify_state_t *ns, uint32_t cid)
{
	client_t *c;

	if (ns == NULL) return;
	if (cid == 0) return;

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);

	c = _nc_table_find_n(ns->client_table, cid);
	if ((c != NULL) && (c->info != NULL))
	{
		c->info->state |= NOTIFY_CLIENT_STATE_SUSPENDED;
		if (c->info->suspend_count < UINT32_MAX) c->info->suspend_count++;
	}

	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
}

void
_notify_lib_suspend_session(notify_state_t *ns, task_name_t session)
{
	client_t *c;
	name_info_t *n;
	void *tt;
	list_t *l;

	if (ns == NULL) return;
	if (session == TASK_NAME_NULL) return;

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);

	/* Suspend all clients for this session */

	tt = _nc_table_traverse_start(ns->name_table);
	while (tt != NULL)
	{
		n = _nc_table_traverse(ns->name_table, tt);
		if (n == NULL) break;

		for (l = n->client_list; l != NULL; l = _nc_list_next(l))
		{
			c = _nc_list_data(l);
			if ((c != NULL) && (c->info != NULL) && (c->info->session == session))
			{
				c->info->state |= NOTIFY_CLIENT_STATE_SUSPENDED;
				if (c->info->suspend_count < UINT32_MAX) c->info->suspend_count++;
			}
		}
	}
	_nc_table_traverse_end(ns->name_table, tt);
	
	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
}

void
_notify_lib_resume(notify_state_t *ns, uint32_t cid)
{
	client_t *c;

	if (ns == NULL) return;
	if (cid == 0) return;

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);

	c = _nc_table_find_n(ns->client_table, cid);
	if ((c != NULL) && (c->info != NULL))
	{
		if (c->info->suspend_count > 0) c->info->suspend_count--;
		if (c->info->suspend_count == 0)
		{
			c->info->state &= ~NOTIFY_CLIENT_STATE_SUSPENDED;

			if (c->info->state & NOTIFY_CLIENT_STATE_PENDING)
			{
				_internal_send(ns, c);
				c->info->state &= ~NOTIFY_CLIENT_STATE_PENDING;
			}
		}
	}

	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
}

void
_notify_lib_resume_session(notify_state_t *ns, task_name_t session)
{
	client_t *c;
	name_info_t *n;
	void *tt;
	list_t *l;

	if (ns == NULL) return;
	if (session == TASK_NAME_NULL) return;

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);

	/* Suspend all clients for this session */

	tt = _nc_table_traverse_start(ns->name_table);
	while (tt != NULL)
	{
		n = _nc_table_traverse(ns->name_table, tt);
		if (n == NULL) break;

		for (l = n->client_list; l != NULL; l = _nc_list_next(l))
		{
			c = _nc_list_data(l);
			if ((c != NULL) && (c->info != NULL) && (c->info->session == session))
			{
				if (c->info->suspend_count > 0) c->info->suspend_count--;
				if (c->info->suspend_count == 0)
				{
					c->info->state &= ~NOTIFY_CLIENT_STATE_SUSPENDED;

					if (c->info->state & NOTIFY_CLIENT_STATE_PENDING)
					{
						_internal_send(ns, c);
						c->info->state &= ~NOTIFY_CLIENT_STATE_PENDING;
					}
				}
			}
		}
	}
	_nc_table_traverse_end(ns->name_table, tt);

	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
}

/*
 * Delete all clients for a session
 */
void
_notify_lib_cancel_session(notify_state_t *ns, task_name_t session)
{
	client_t *c, *a, *p;
	name_info_t *n;
	void *tt;
	list_t *l, *x;

	if (ns == NULL) return;
	if (session == TASK_NAME_NULL) return;

	a = NULL;
	x = NULL;
	p = NULL;

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);

	tt = _nc_table_traverse_start(ns->name_table);
	while (tt != NULL)
	{
		n = _nc_table_traverse(ns->name_table, tt);
		if (n == NULL) break;

		for (l = n->client_list; l != NULL; l = _nc_list_next(l))
		{
			c = _nc_list_data(l);
			if ((c->info != NULL) && (c->info->session == session))
			{
				a = (client_t *)malloc(sizeof(client_t));
				if (a == NULL)
				{
					if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
					return;
				}

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
		_internal_cancel(ns, c->client_id);
		free(c);
	}

	_nc_list_release_list(x);

	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
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

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);

	c = _nc_table_find_n(ns->client_table, cid);

	if (c == NULL)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NOTIFY_STATUS_INVALID_TOKEN;
	}

	if (c->info->name_info == NULL)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NOTIFY_STATUS_INVALID_TOKEN;
	}

	if (c->info->name_info->val == c->info->lastval)
	{
		*check = 0;
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NOTIFY_STATUS_OK;
	}

	c->info->lastval = c->info->name_info->val;
	*check = 1;

	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
	return NOTIFY_STATUS_OK;
}

/*
 * SPI: get value for a name.
 */
uint32_t
_notify_lib_peek(notify_state_t *ns, uint32_t cid, int *val)
{
	client_t *c;

	if (ns == NULL) return NOTIFY_STATUS_FAILED;
	if (cid == 0) return NOTIFY_STATUS_INVALID_TOKEN;

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);

	c = _nc_table_find_n(ns->client_table, cid);

	if (c == NULL)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NOTIFY_STATUS_INVALID_TOKEN;
	}

	if (c->info->name_info == NULL)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NOTIFY_STATUS_INVALID_TOKEN;
	}

	*val = c->info->name_info->val;

	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
	return NOTIFY_STATUS_OK;
}

int *
_notify_lib_check_addr(notify_state_t *ns, uint32_t cid)
{
	client_t *c;
	int *addr;

	if (ns == NULL) return NULL;
	if (cid == 0) return NULL;

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);

	c = _nc_table_find_n(ns->client_table, cid);

	if (c == NULL)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NULL;
	}

	if (c->info->name_info == NULL)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NULL;
	}

	addr = (int *)&(c->info->name_info->val);

	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
	return addr;
}

/*
 * Get state value for a name.
 */
uint32_t
_notify_lib_get_state(notify_state_t *ns, uint32_t cid, uint64_t *state)
{
	client_t *c;

	if (state == NULL) return NOTIFY_STATUS_FAILED;

	*state = 0;

	if (ns == NULL) return NOTIFY_STATUS_FAILED;
	if (cid == 0) return NOTIFY_STATUS_INVALID_TOKEN;

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);

	c = _nc_table_find_n(ns->client_table, cid);

	if (c == NULL)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NOTIFY_STATUS_INVALID_TOKEN;
	}

	if (c->info->name_info == NULL)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NOTIFY_STATUS_INVALID_TOKEN;
	}

	*state = c->info->name_info->state;

	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
	return NOTIFY_STATUS_OK;
}

/*
 * Set state value for a name.
 */
uint32_t
_notify_lib_set_state(notify_state_t *ns, uint32_t cid, uint64_t state, uid_t uid, gid_t gid)
{
	client_t *c;
	int auth;

	if (ns == NULL) return NOTIFY_STATUS_FAILED;
	if (cid == 0) return NOTIFY_STATUS_INVALID_TOKEN;

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);

	c = _nc_table_find_n(ns->client_table, cid);

	if (c == NULL)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NOTIFY_STATUS_INVALID_TOKEN;
	}

	if (c->info->name_info == NULL)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NOTIFY_STATUS_INVALID_TOKEN;
	}

	auth = _internal_check_access(ns, c->info->name_info, uid, gid, NOTIFY_ACCESS_WRITE);
	if (auth != 0)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NOTIFY_STATUS_NOT_AUTHORIZED;
	}

	c->info->name_info->state = state;

	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
	return NOTIFY_STATUS_OK;
}

/*
 * Get value for a name.
 */
uint32_t
_notify_lib_get_val(notify_state_t *ns, uint32_t cid, int *val)
{
	client_t *c;

	*val = 0;

	if (ns == NULL) return NOTIFY_STATUS_FAILED;
	if (cid == 0) return NOTIFY_STATUS_INVALID_TOKEN;

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);

	c = _nc_table_find_n(ns->client_table, cid);

	if (c == NULL)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NOTIFY_STATUS_INVALID_TOKEN;
	}

	if (c->info->name_info == NULL)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NOTIFY_STATUS_INVALID_TOKEN;
	}

	*val = c->info->name_info->val;

	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
	return NOTIFY_STATUS_OK;
}

/*
 * Set value for a name.
 */
uint32_t
_notify_lib_set_val(notify_state_t *ns, uint32_t cid, int val, uid_t uid, gid_t gid)
{
	client_t *c;
	int auth;

	if (ns == NULL) return NOTIFY_STATUS_FAILED;
	if (cid == 0) return NOTIFY_STATUS_INVALID_TOKEN;

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);

	c = _nc_table_find_n(ns->client_table, cid);

	if (c == NULL)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NOTIFY_STATUS_INVALID_TOKEN;
	}

	if (c->info->name_info == NULL)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NOTIFY_STATUS_INVALID_TOKEN;
	}

	auth = _internal_check_access(ns, c->info->name_info, uid, gid, NOTIFY_ACCESS_WRITE);
	if (auth != 0)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NOTIFY_STATUS_NOT_AUTHORIZED;
	}

	c->info->name_info->val = val;

	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
	return NOTIFY_STATUS_OK;
}

static uint32_t
_internal_register_common(notify_state_t *ns, const char *name, uid_t uid, gid_t gid, client_t **outc, name_info_t **outn)
{
	client_t *c;
	name_info_t *n;
	int auth, is_protected, is_new_name;
	uint32_t status;

	if (outc == NULL) return 0;

	*outc = NULL;
	if (outn != NULL) *outn = NULL;
	is_new_name = 0;
	is_protected = 0;

	n = (name_info_t *)_nc_table_find(ns->name_table, name);
	if (n == NULL)
	{
		is_new_name = 1;

		status = _internal_check_user_protected(name, uid, &is_protected);
		if (status != NOTIFY_STATUS_OK) return status;

		n = _internal_new_name(ns, name);
		if (n == NULL) return NOTIFY_STATUS_FAILED;

		if (is_protected == 1)
		{
			n->uid = uid;
			n->gid = gid;
			if (uid == 0)
			{
				n->uid = atoi(name + USER_PROTECTED_UID_PREFIX_LEN);
				n->gid = -1;
			}

			n->access = NOTIFY_ACCESS_USER_RW;
		}
	}

	auth = _internal_check_access(ns, n, uid, gid, NOTIFY_ACCESS_READ);
	if (auth != 0)
	{
		if (is_new_name == 1)
		{
			_nc_table_delete(ns->name_table, n->name);
			free(n->name);
			free(n);
		}

		return NOTIFY_STATUS_NOT_AUTHORIZED;
	}

	c = _internal_client_new(ns);
	if (c == NULL)
	{
		if (is_new_name == 1)
		{
			_nc_table_delete(ns->name_table, n->name);
			free(n->name);
			free(n);
		}

		return NOTIFY_STATUS_FAILED;
	}

	n->refcount++;

	c->info->name_info = n;
	n->client_list = _nc_list_prepend(n->client_list, _nc_list_new(c));

	if ((is_new_name == 1) && (is_protected == 1)) _internal_insert_controlled_name(ns, n);

	*outc = c;
	if (outn != NULL) *outn = n;

	return NOTIFY_STATUS_OK;
}

/*
 * Register for signal.
 * Returns the client_id;
 */
uint32_t
_notify_lib_register_signal(notify_state_t *ns, const char *name, task_name_t session, pid_t pid, uint32_t sig, uid_t uid, gid_t gid, uint32_t *out_token)
{
	client_t *c;
	uint32_t status;

	if (ns == NULL) return 0;
	if (name == NULL) return 0;

	c = NULL;

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);

	status = _internal_register_common(ns, name, uid, gid, &c, NULL);
	if (status != NOTIFY_STATUS_OK)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return status;
	}

	c->info->notify_type = NOTIFY_TYPE_SIGNAL;
	c->info->pid = pid;
	c->info->sig = sig;
	c->info->session = session;
	*out_token = c->client_id;

	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
	return NOTIFY_STATUS_OK;
}

/*
 * Register for notification on a file descriptor.
 * Returns the client_id;
 */
uint32_t
_notify_lib_register_file_descriptor(notify_state_t *ns, const char *name, task_name_t session, const char *path, uint32_t token, uid_t uid, gid_t gid, uint32_t *out_token)
{
	client_t *c;
	name_info_t *n;
	uint32_t status;
	struct sockaddr_un sun;
	int sock;

	if (ns == NULL) return 0;
	if (name == NULL) return 0;

	c = NULL;
	n = NULL;

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);

	sock = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sock < 0)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NOTIFY_STATUS_FAILED;
	}

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strncpy(sun.sun_path, path, sizeof(sun.sun_path));

	if (connect(sock, (const struct sockaddr *)&sun, sizeof(sun)) < 0)
	{
		close(sock);
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NOTIFY_STATUS_INVALID_FILE;
	}

	status = _internal_register_common(ns, name, uid, gid, &c, &n);
	if (status != NOTIFY_STATUS_OK)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return status;
	}

	c->info->name_info = n;
	c->info->notify_type = NOTIFY_TYPE_FD;
	c->info->fd = sock;
	c->info->token = token;
	c->info->session = session;
	*out_token = c->client_id;

	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
	return NOTIFY_STATUS_OK;
}

/*
 * Register for notification on a mach port.
 * Returns the client_id;
 */
uint32_t
_notify_lib_register_mach_port(notify_state_t *ns, const char *name, task_name_t session, mach_port_t port, uint32_t token, uid_t uid, gid_t gid, uint32_t *out_token)
{
	client_t *c;
	uint32_t status;

	if (ns == NULL) return 0;
	if (name == NULL) return 0;

	c = NULL;

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);

	status = _internal_register_common(ns, name, uid, gid, &c, NULL);
	if (status != NOTIFY_STATUS_OK)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return status;
	}

	c->info->notify_type = NOTIFY_TYPE_PORT;
	c->info->msg = (mach_msg_empty_send_t *)calloc(1, sizeof(mach_msg_empty_send_t));
	c->info->msg->header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSGH_BITS_ZERO);
	c->info->msg->header.msgh_remote_port = port;
	c->info->msg->header.msgh_local_port = MACH_PORT_NULL;
	c->info->msg->header.msgh_size = sizeof(mach_msg_empty_send_t);
	c->info->msg->header.msgh_id = token;
	c->info->token = token;
	c->info->session = session;
	*out_token = c->client_id;

	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
	return NOTIFY_STATUS_OK;
}

/*
 * Plain registration - only for notify_check()
 * Returns the client_id.
 */
uint32_t
_notify_lib_register_plain(notify_state_t *ns, const char *name, task_name_t session, uint32_t slot, uid_t uid, gid_t gid, uint32_t *out_token)
{
	client_t *c;
	name_info_t *n;
	uint32_t status;

	if (ns == NULL) return 0;
	if (name == NULL) return 0;

	c = NULL;
	n = NULL;

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);

	status = _internal_register_common(ns, name, uid, gid, &c, &n);
	if (status != NOTIFY_STATUS_OK)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return status;
	}

	if (slot == (uint32_t)-1)
	{
		c->info->notify_type = NOTIFY_TYPE_PLAIN;
	}
	else
	{
		c->info->notify_type = NOTIFY_TYPE_MEMORY;
		n->slot = slot;
	}

	c->info->session = session;
	*out_token = c->client_id;

	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
	return NOTIFY_STATUS_OK;
}

uint32_t
_notify_lib_set_owner(notify_state_t *ns, const char *name, uid_t uid, gid_t gid)
{
	name_info_t *n;

	if (ns == NULL) return NOTIFY_STATUS_FAILED;
	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);

	n = (name_info_t *)_nc_table_find(ns->name_table, name);
	if (n == NULL)
	{
		/* create new name */
		n = _internal_new_name(ns, name);
		if (n == NULL)
		{
			if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
			return NOTIFY_STATUS_FAILED;
		}

		/* 
		 * Setting the refcount here allows the namespace to be "pre-populated"
		 * with controlled names.  notifyd does this for reserved names in 
		 * its configuration file.
		 */
		n->refcount++;
	}

	n->uid = uid;
	n->gid = gid;

	_internal_insert_controlled_name(ns, n);

	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
	return NOTIFY_STATUS_OK;
}

uint32_t
_notify_lib_get_owner(notify_state_t *ns, const char *name, uint32_t *uid, uint32_t *gid)
{
	name_info_t *n;
	int i, nlen, len;

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);

	n = (name_info_t *)_nc_table_find(ns->name_table, name);
	if (n != NULL)
	{
		*uid = n->uid;
		*gid = n->gid;
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
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
			if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
			return NOTIFY_STATUS_OK;
		}

		/* check if this key is a prefix */
		if (nlen >= len) continue;
		if (strncmp(n->name, name, nlen)) continue;

		*uid = n->uid;
		*gid = n->gid;

		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NOTIFY_STATUS_OK;
	}

	*uid = 0;
	*gid = 0;

	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
	return NOTIFY_STATUS_OK;
}

uint32_t
_notify_lib_set_access(notify_state_t *ns, const char *name, uint32_t mode)
{
	name_info_t *n;

	if (ns == NULL) return NOTIFY_STATUS_FAILED;
	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);

	n = (name_info_t *)_nc_table_find(ns->name_table, name);
	if (n == NULL)
	{
		/* create new name */
		n = _internal_new_name(ns, name);
		if (n == NULL)
		{
			if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
			return NOTIFY_STATUS_FAILED;
		}

		/* 
		 * Setting the refcount here allows the namespace to be "pre-populated"
		 * with controlled names.  notifyd does this for reserved names in 
		 * its configuration file.
		 */
		n->refcount++;
	}

	n->access = mode;

	_internal_insert_controlled_name(ns, n);

	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
	return NOTIFY_STATUS_OK;
}

uint32_t
_notify_lib_get_access(notify_state_t *ns, const char *name, uint32_t *mode)
{
	name_info_t *n;
	int i, nlen, len;

	if (ns == NULL) return NOTIFY_STATUS_FAILED;
	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);

	n = (name_info_t *)_nc_table_find(ns->name_table, name);
	if (n != NULL)
	{
		*mode = n->access;

		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
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

			if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
			return NOTIFY_STATUS_OK;
		}

		/* check if this key is a prefix */
		if (nlen >= len) continue;
		if (strncmp(n->name, name, nlen)) continue;

		*mode = n->access;

		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NOTIFY_STATUS_OK;
	}

	*mode = NOTIFY_ACCESS_DEFAULT;

	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
	return NOTIFY_STATUS_OK;
}

uint32_t
_notify_lib_release_name(notify_state_t *ns, const char *name, uid_t uid, gid_t gid)
{
	name_info_t *n;

	if (ns == NULL) return NOTIFY_STATUS_FAILED;
	if (name == NULL) return NOTIFY_STATUS_INVALID_NAME;

	if (ns->lock != NULL) pthread_mutex_lock(ns->lock);

	n = (name_info_t *)_nc_table_find(ns->name_table, name);
	if (n == NULL)
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NOTIFY_STATUS_INVALID_NAME;
	}

	/* Owner and root may release */
	if ((n->uid != uid) && (uid != 0))
	{
		if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
		return NOTIFY_STATUS_NOT_AUTHORIZED;
	}

	_internal_release_name_info(ns, n);

	if (ns->lock != NULL) pthread_mutex_unlock(ns->lock);
	return NOTIFY_STATUS_OK;
}

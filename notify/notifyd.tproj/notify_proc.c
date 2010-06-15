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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <asl.h>
#include <bsm/libbsm.h>
#include "notify.h"
#include "daemon.h"
#include "service.h"
#include "notify_ipc.h"
#include <mach/mach_traps.h>
#include <pthread.h>
extern uint32_t _notify_lib_check_controlled_access();

static void
register_session(task_name_t tname)
{
	mach_port_t previous;

	if (ns == NULL) return;
	if (dead_session_port == MACH_PORT_NULL) return;

	/* register for port death notification */
	mach_port_request_notification(mach_task_self(), tname, MACH_NOTIFY_DEAD_NAME, 0, dead_session_port, MACH_MSG_TYPE_MAKE_SEND_ONCE, &previous);
	mach_port_deallocate(mach_task_self(), previous);
}

void
cancel_session(task_name_t tname)
{
	client_t *c, *a, *p;
	name_info_t *n;
	void *tt;
	list_t *l, *x;

	a = NULL;
	x = NULL;
	p = NULL;
	if (ns == NULL) return;

	/* Release all clients for this session */

	tt = _nc_table_traverse_start(ns->name_table);
	while (tt != NULL)
	{
		n = _nc_table_traverse(ns->name_table, tt);
		if (n == NULL) break;

		for (l = n->client_list; l != NULL; l = _nc_list_next(l))
		{
			c = _nc_list_data(l);
			if ((c->info != NULL) && (c->info->session == tname))
			{
				a = (client_t *)calloc(1, sizeof(client_t));
				if (a == NULL) return;

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

		if (c->info != NULL)
		{
			if (c->info->private != NULL)
			{
				service_close((svc_info_t *)c->info->private, NULL);
				c->info->private = NULL;
			}

			n = c->info->name_info;
			if ((n != NULL) && (n->refcount == 1))
			{
				service_close((svc_info_t *)n->private, n->name);
				n->private = NULL;
			}

			if (c->info->notify_type == NOTIFY_TYPE_MEMORY)
			{
				shm_refcount[n->slot]--;
			}
		}

		_notify_lib_cancel(ns, c->client_id);
		free(c);
	}

	_nc_list_release_list(x);

	/* Release the tname */
	mach_port_destroy(mach_task_self(), tname);
}

kern_return_t __notify_server_post
(
	mach_port_t server,
	caddr_t name,
	mach_msg_type_number_t nameCnt,
	int *status,
	audit_token_t token
)
{
	uid_t uid;
	gid_t gid;

	if (name == NULL)
	{
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	if (ns == NULL)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	if (name[nameCnt] != '\0')
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_post %s\n", name);

	uid = (uid_t)-1;
	gid = (gid_t)-1;
	audit_token_to_au32(token, NULL, &uid, &gid, NULL, NULL, NULL, NULL, NULL);

	*status = _notify_lib_check_controlled_access(ns, name, uid, gid, NOTIFY_ACCESS_WRITE);
	if (*status != NOTIFY_STATUS_OK)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		return KERN_SUCCESS;
	}

	*status = daemon_post(name, uid, gid);
	vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);

	return KERN_SUCCESS;
}

kern_return_t __notify_server_register_plain
(
	mach_port_t server,
	caddr_t name,
	mach_msg_type_number_t nameCnt,
	int *client_id,
	int *status,
	audit_token_t token
)
{
	uid_t uid;
	gid_t gid;
	pid_t pid;
	task_name_t tname;
	kern_return_t kstatus;
	client_t *c;

	*client_id = 0;

	if (name == NULL)
	{
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	if (ns == NULL)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	if (name[nameCnt] != '\0')
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_register_plain %s\n", name);

	uid = (uid_t)-1;
	gid = (gid_t)-1;
	pid = (pid_t)-1;
	audit_token_to_au32(token, NULL, &uid, &gid, NULL, NULL, &pid, NULL, NULL);

	tname = TASK_NAME_NULL;
	kstatus = task_name_for_pid(mach_task_self(), pid, &tname);
	if (kstatus != KERN_SUCCESS)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	*status = _notify_lib_register_plain(ns, name, tname, -1, uid, gid, (uint32_t *)client_id);
	vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
	if (*status != NOTIFY_STATUS_OK)
	{
		mach_port_deallocate(mach_task_self(), tname);
		return KERN_SUCCESS;
	}

	c = _nc_table_find_n(ns->client_table, *client_id);
	if ((c != NULL) && (c->info != NULL)) c->info->pid = pid;

	register_session(tname);

	return KERN_SUCCESS;
}

kern_return_t __notify_server_register_check
(
	mach_port_t server,
	caddr_t name,
	mach_msg_type_number_t nameCnt,
	int *size,
	int *slot,
	int *client_id,
	int *status,
	audit_token_t token
)
{
	name_info_t *n;
	uint32_t i, j, x, new_slot;
	uid_t uid;
	gid_t gid;
	pid_t pid;
	task_name_t tname;
	kern_return_t kstatus;
	client_t *c;

	*size = 0;
	*slot = 0;
	*client_id = 0;

	if (name == NULL)
	{
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	if (ns == NULL)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	if (name[nameCnt] != '\0')
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_register_check %s\n", name);

	if (shm_enabled == 0)
	{
		*size = -1;
		*slot = -1;
		return __notify_server_register_plain(server, name, nameCnt, client_id, status, token);
	}

	x = (uint32_t)-1;

	n = (name_info_t *)_nc_table_find(ns->name_table, name);
	if (n != NULL) x = n->slot;

	new_slot = 0;
	if (x == (uint32_t)-1) 
	{
		/* find a slot */
		new_slot = 1;

		for (i = 0, j = slot_id + 1; i < nslots; i++, j++)
		{
			if (j >= nslots) j = 0;
			if (shm_refcount[j] == 0)
			{
				x = j;
				break;
			}
		}

		if (x == (uint32_t)-1)
		{
			/* Ran out of slots! */
			slot_id++;
			if (slot_id >= nslots) slot_id = 0;
			log_message(ASL_LEVEL_DEBUG, "reused shared memory slot %u\n", slot_id);
			x = slot_id;
		}
		else
		{
			if (x == (slot_id + 1)) slot_id = x;
		}
	}

	if (new_slot == 1) shm_base[x] = 1;
	shm_refcount[x]++;

	uid = (uid_t)-1;
	gid = (gid_t)-1;
	pid = (pid_t)-1;
	audit_token_to_au32(token, NULL, &uid, &gid, NULL, NULL, &pid, NULL, NULL);

	tname = TASK_NAME_NULL;
	kstatus = task_name_for_pid(mach_task_self(), pid, &tname);
	if (kstatus != KERN_SUCCESS)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	*size = nslots * sizeof(uint32_t);
	*slot = x;
	*status = _notify_lib_register_plain(ns, name, tname, x, uid, gid, (uint32_t *)client_id);
	vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
	if (*status != NOTIFY_STATUS_OK)
	{
		mach_port_deallocate(mach_task_self(), tname);
		return KERN_SUCCESS;
	}

	c = _nc_table_find_n(ns->client_table, *client_id);
	if ((c != NULL) && (c->info != NULL)) c->info->pid = pid;

	register_session(tname);

	return KERN_SUCCESS;
}

kern_return_t __notify_server_register_signal
(
	mach_port_t server,
	caddr_t name,
	mach_msg_type_number_t nameCnt,
	int sig,
	int *client_id,
	int *status,
	audit_token_t token
)
{
	uid_t uid;
	gid_t gid;
	pid_t pid;
	task_name_t tname;
	kern_return_t kstatus;
	client_t *c;

	*client_id = 0;

	if (name == NULL)
	{
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	if (ns == NULL)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	if (name[nameCnt] != '\0')
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_register_signal %s\n", name);

	uid = (uid_t)-1;
	gid = (gid_t)-1;
	pid = (gid_t)-1;
	audit_token_to_au32(token, NULL, &uid, &gid, NULL, NULL, &pid, NULL, NULL);

	tname = TASK_NAME_NULL;
	kstatus = task_name_for_pid(mach_task_self(), pid, &tname);
	if (kstatus != KERN_SUCCESS)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	*status = _notify_lib_register_signal(ns, name, tname, pid, sig, uid, gid, (uint32_t *)client_id);
	vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
	if (*status != NOTIFY_STATUS_OK)
	{
		mach_port_deallocate(mach_task_self(), tname);
		return KERN_SUCCESS;
	}

	c = _nc_table_find_n(ns->client_table, *client_id);
	if ((c != NULL) && (c->info != NULL)) c->info->pid = pid;

	register_session(tname);

	return KERN_SUCCESS;
}

kern_return_t __notify_server_register_file_descriptor
(
	mach_port_t server,
	caddr_t name,
	mach_msg_type_number_t nameCnt,
	caddr_t path,
	mach_msg_type_number_t pathCnt,
	int ntoken,
	int *client_id,
	int *status,
	audit_token_t token
)
{
	uid_t uid;
	gid_t gid;
	pid_t pid;
	task_name_t tname;
	kern_return_t kstatus;
	client_t *c;

	*client_id = 0;

	if (name == NULL)
	{
		if (path != NULL) vm_deallocate(mach_task_self(), (vm_address_t)path, pathCnt);
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	if (path == NULL)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_INVALID_FILE;
		return KERN_SUCCESS;
	}

	if (ns == NULL)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		vm_deallocate(mach_task_self(), (vm_address_t)path, pathCnt);
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	if (name[nameCnt] != '\0')
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		vm_deallocate(mach_task_self(), (vm_address_t)path, pathCnt);
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	if (path[pathCnt] != '\0')
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		vm_deallocate(mach_task_self(), (vm_address_t)path, pathCnt);
		*status = NOTIFY_STATUS_INVALID_FILE;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_register_file_descriptor %s %s\n", name, path);

	uid = (uid_t)-1;
	gid = (gid_t)-1;
	pid = (gid_t)-1;
	audit_token_to_au32(token, NULL, &uid, &gid, NULL, NULL, &pid, NULL, NULL);

	tname = TASK_NAME_NULL;
	kstatus = task_name_for_pid(mach_task_self(), pid, &tname);
	if (kstatus != KERN_SUCCESS)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		vm_deallocate(mach_task_self(), (vm_address_t)path, pathCnt);
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	*status = _notify_lib_register_file_descriptor(ns, name, tname, path, ntoken, uid, gid, (uint32_t *)client_id);
	vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
	vm_deallocate(mach_task_self(), (vm_address_t)path, pathCnt);
	if (*status != NOTIFY_STATUS_OK)
	{
		mach_port_deallocate(mach_task_self(), tname);
		return KERN_SUCCESS;
	}

	c = _nc_table_find_n(ns->client_table, *client_id);
	if ((c != NULL) && (c->info != NULL)) c->info->pid = pid;

	register_session(tname);

	return KERN_SUCCESS;
}

kern_return_t __notify_server_register_mach_port
(
	mach_port_t server,
	caddr_t name,
	mach_msg_type_number_t nameCnt,
	mach_port_t port,
	int ntoken,
	int *client_id,
	int *status,
	audit_token_t token
)
{
	uid_t uid;
	gid_t gid;
	pid_t pid;
	task_name_t tname;
	kern_return_t kstatus;
	client_t *c;

	if (name == NULL)
	{
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	if (ns == NULL)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	if (name[nameCnt] != '\0')
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_register_mach_port %s\n", name);

	uid = (uid_t)-1;
	gid = (gid_t)-1;
	pid = (gid_t)-1;
	audit_token_to_au32(token, NULL, &uid, &gid, NULL, NULL, &pid, NULL, NULL);

	tname = TASK_NAME_NULL;
	kstatus = task_name_for_pid(mach_task_self(), pid, &tname);
	if (kstatus != KERN_SUCCESS)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	*status = _notify_lib_register_mach_port(ns, name, tname, port, ntoken, uid, gid, (uint32_t *)client_id);
	vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
	if (*status != NOTIFY_STATUS_OK)
	{
		mach_port_deallocate(mach_task_self(), tname);
		return KERN_SUCCESS;
	}

	c = _nc_table_find_n(ns->client_table, *client_id);
	if ((c != NULL) && (c->info != NULL)) c->info->pid = pid;

	register_session(tname);

	return KERN_SUCCESS;
}

kern_return_t __notify_server_cancel
(
	mach_port_t server,
	int client_id,
	int *status,
	audit_token_t token
)
{
	uid_t uid;
	pid_t pid;
	task_name_t tname;
	kern_return_t kstatus;
	client_t *c;
	name_info_t *n;

	if (ns == NULL)
	{
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_cancel %u\n", client_id);

	uid = (uid_t)-1;
	pid = (gid_t)-1;
	audit_token_to_au32(token, NULL, &uid, NULL, NULL, NULL, &pid, NULL, NULL);

	tname = TASK_NAME_NULL;
	kstatus = task_name_for_pid(mach_task_self(), pid, &tname);
	if (kstatus != KERN_SUCCESS)
	{
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	*status = NOTIFY_STATUS_OK;

	c = _nc_table_find_n(ns->client_table, client_id);
	if (c == NULL) return KERN_SUCCESS;
	if (c->info == NULL)
	{
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	if ((uid != 0) && (c->info->session != tname))
	{
		*status = NOTIFY_STATUS_NOT_AUTHORIZED;
		return KERN_SUCCESS;
	}

	service_close((svc_info_t *)c->info->private, NULL);

	c->info->private = NULL;

	n = c->info->name_info;
	if (n == NULL) return KERN_SUCCESS;

	if (c->info->notify_type == NOTIFY_TYPE_MEMORY)
	{
		shm_refcount[n->slot]--;
	}

	if (n->refcount == 1)
	{
		service_close((svc_info_t *)n->private, n->name);
		n->private = NULL;
	}

	_notify_lib_cancel(ns, client_id);

	return KERN_SUCCESS;
}

kern_return_t __notify_server_suspend
(
	mach_port_t server,
	int client_id,
	int *status,
	audit_token_t token
)
{
	uid_t uid;
	pid_t pid;
	task_name_t tname;
	kern_return_t kstatus;
	client_t *c;
	name_info_t *n;

	if (ns == NULL)
	{
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_suspend %u\n", client_id);

	uid = (uid_t)-1;
	pid = (gid_t)-1;
	audit_token_to_au32(token, NULL, &uid, NULL, NULL, NULL, &pid, NULL, NULL);

	tname = TASK_NAME_NULL;
	kstatus = task_name_for_pid(mach_task_self(), pid, &tname);
	if (kstatus != KERN_SUCCESS)
	{
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	*status = NOTIFY_STATUS_OK;

	c = _nc_table_find_n(ns->client_table, client_id);
	if (c == NULL) return KERN_SUCCESS;
	if (c->info == NULL)
	{
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	if ((uid != 0) && (c->info->session != tname))
	{
		*status = NOTIFY_STATUS_NOT_AUTHORIZED;
		return KERN_SUCCESS;
	}

	n = c->info->name_info;
	if (n == NULL) return KERN_SUCCESS;

	_notify_lib_suspend(ns, client_id);

	return KERN_SUCCESS;
}

kern_return_t __notify_server_resume
(
	mach_port_t server,
	int client_id,
	int *status,
	audit_token_t token
)
{
	uid_t uid;
	pid_t pid;
	task_name_t tname;
	kern_return_t kstatus;
	client_t *c;
	name_info_t *n;

	if (ns == NULL)
	{
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_resume %u\n", client_id);

	uid = (uid_t)-1;
	pid = (gid_t)-1;
	audit_token_to_au32(token, NULL, &uid, NULL, NULL, NULL, &pid, NULL, NULL);

	tname = TASK_NAME_NULL;
	kstatus = task_name_for_pid(mach_task_self(), pid, &tname);
	if (kstatus != KERN_SUCCESS)
	{
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	*status = NOTIFY_STATUS_OK;

	c = _nc_table_find_n(ns->client_table, client_id);
	if (c == NULL) return KERN_SUCCESS;
	if (c->info == NULL)
	{
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	if ((uid != 0) && (c->info->session != tname))
	{
		*status = NOTIFY_STATUS_NOT_AUTHORIZED;
		return KERN_SUCCESS;
	}

	n = c->info->name_info;
	if (n == NULL) return KERN_SUCCESS;

	_notify_lib_resume(ns, client_id);

	return KERN_SUCCESS;
}

kern_return_t __notify_server_suspend_pid
(
	mach_port_t server,
	int pid,
	int *status,
	audit_token_t token
)
{
	uid_t uid;
	task_name_t tname;
	kern_return_t kstatus;

	if (ns == NULL)
	{
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_suspend_pid %u\n", pid);

	uid = (uid_t)-1;
	audit_token_to_au32(token, NULL, &uid, NULL, NULL, NULL, NULL, NULL, NULL);
	if (uid != 0)
	{
		*status = NOTIFY_STATUS_NOT_AUTHORIZED;
		return KERN_SUCCESS;
	}

	tname = TASK_NAME_NULL;
	kstatus = task_name_for_pid(mach_task_self(), pid, &tname);
	if (kstatus != KERN_SUCCESS)
	{
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	*status = NOTIFY_STATUS_OK;

	_notify_lib_suspend_session(ns, tname);

	return KERN_SUCCESS;
}

kern_return_t __notify_server_resume_pid
(
	mach_port_t server,
	int pid,
	int *status,
	audit_token_t token
)
{
	uid_t uid;
	task_name_t tname;
	kern_return_t kstatus;

	if (ns == NULL)
	{
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_resume_pid %u\n", pid);

	uid = (uid_t)-1;
	audit_token_to_au32(token, NULL, &uid, NULL, NULL, NULL, NULL, NULL, NULL);
	if (uid != 0)
	{
		*status = NOTIFY_STATUS_NOT_AUTHORIZED;
		return KERN_SUCCESS;
	}

	tname = TASK_NAME_NULL;
	kstatus = task_name_for_pid(mach_task_self(), pid, &tname);
	if (kstatus != KERN_SUCCESS)
	{
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	*status = NOTIFY_STATUS_OK;

	_notify_lib_resume_session(ns, tname);

	return KERN_SUCCESS;
}

kern_return_t __notify_server_check
(
	mach_port_t server,
	int client_id,
	int *check,
	int *status,
	audit_token_t token
)
{
	log_message(ASL_LEVEL_DEBUG, "__notify_server_check %u\n", client_id);

	*status =  _notify_lib_check(ns, client_id, check);
	return KERN_SUCCESS;
}

kern_return_t __notify_server_get_state
(
	mach_port_t server,
	int client_id,
	uint64_t *state,
	int *status,
	audit_token_t token
)
{
	log_message(ASL_LEVEL_DEBUG, "__notify_server_get_state %u\n", client_id);

	*status = _notify_lib_get_state(ns, client_id, state);
	return KERN_SUCCESS;
}

kern_return_t __notify_server_set_state
(
	mach_port_t server,
	int client_id,
	uint64_t state,
	int *status,
	audit_token_t token
)
{
	uid_t uid;
	gid_t gid;

	log_message(ASL_LEVEL_DEBUG, "__notify_server_set_state %u\n", client_id);

	uid = (uid_t)-1;
	gid = (gid_t)-1;
	audit_token_to_au32(token, NULL, &uid, &gid, NULL, NULL, NULL, NULL, NULL);

	*status = _notify_lib_set_state(ns, client_id, state, uid, gid);
	return KERN_SUCCESS;
}

kern_return_t __notify_server_get_val
(
	mach_port_t server,
	int client_id,
	int *val,
	int *status,
	audit_token_t token
 )
{
	log_message(ASL_LEVEL_DEBUG, "__notify_server_get_val %u\n", client_id);

	*status = _notify_lib_get_val(ns, client_id, val);
	return KERN_SUCCESS;
}

kern_return_t __notify_server_set_val
(
	mach_port_t server,
	int client_id,
	int val,
	int *status,
	audit_token_t token
 )
{
	client_t *c;
	name_info_t *n;
	uid_t uid;
	gid_t gid;

	log_message(ASL_LEVEL_DEBUG, "__notify_server_set_val %u\n", client_id);

	uid = (uid_t)-1;
	gid = (gid_t)-1;
	audit_token_to_au32(token, NULL, &uid, &gid, NULL, NULL, NULL, NULL, NULL);

	*status = _notify_lib_set_val(ns, client_id, val, uid, gid);
	if (*status == NOTIFY_STATUS_OK)
	{
		c = _nc_table_find_n(ns->client_table, client_id);
		if ((c == NULL) || (c->info == NULL))
		{
			*status = NOTIFY_STATUS_FAILED;
			return KERN_SUCCESS;
		}

		n = c->info->name_info;
		if (n->slot == -1) return KERN_SUCCESS;
		shm_base[n->slot] = val;
	}

	return KERN_SUCCESS;
}

kern_return_t __notify_server_set_owner
(
	mach_port_t server,
	caddr_t name,
	mach_msg_type_number_t nameCnt,
	int uid,
	int gid,
	int *status,
	audit_token_t token
)
{
	uid_t auid;

	if (name == NULL)
	{
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	if (ns == NULL)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	if (name[nameCnt] != '\0')
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_set_owner %s %u %u\n", name, uid, gid);

	auid = (uid_t)-1;
	audit_token_to_au32(token, NULL, &auid, NULL, NULL, NULL, NULL, NULL, NULL);

	/* only root may set owner for names */
	if (auid != 0)
	{ 
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_NOT_AUTHORIZED;
		return KERN_SUCCESS;
	}

	*status = _notify_lib_set_owner(ns, name, uid, gid);
	vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
	return KERN_SUCCESS;
}

kern_return_t __notify_server_get_owner
(
	mach_port_t server,
	caddr_t name,
	mach_msg_type_number_t nameCnt,
	int *uid,
	int *gid,
	int *status,
	audit_token_t token
)
{
	if (name == NULL)
	{
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	if (ns == NULL)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	if (name[nameCnt] != '\0')
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_get_owner %s\n", name);

	*status = _notify_lib_get_owner(ns, name, (uint32_t *)uid, (uint32_t *)gid);
	vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
	return KERN_SUCCESS;
}

kern_return_t __notify_server_set_access
(
	mach_port_t server,
	caddr_t name,
	mach_msg_type_number_t nameCnt,
	int mode,
	int *status,
	audit_token_t token
)
{
	uint32_t u, g;
	uid_t uid;
	gid_t gid;

	if (name == NULL)
	{
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	if (ns == NULL)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	if (name[nameCnt] != '\0')
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_set_access %s 0x%03x\n", name, mode);

	_notify_lib_get_owner(ns, name, &u, &g);

	uid = (uid_t)-1;
	gid = (gid_t)-1;
	audit_token_to_au32(token, NULL, &uid, &gid, NULL, NULL, NULL, NULL, NULL);

	/* only root and owner may set access for names */
	if ((uid != 0) && (uid != u))
	{ 
		*status = NOTIFY_STATUS_NOT_AUTHORIZED;
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		return KERN_SUCCESS;
	}

	*status = _notify_lib_set_access(ns, name, mode);
	if ((u != 0) || (g != 0)) *status = _notify_lib_set_owner(ns, name, u, g);
	vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
	return KERN_SUCCESS;
}

kern_return_t __notify_server_get_access
(
	mach_port_t server,
	caddr_t name,
	mach_msg_type_number_t nameCnt,
	int *mode,
	int *status,
	audit_token_t token
)
{
	if (name == NULL)
	{
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	if (ns == NULL)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	if (name[nameCnt] != '\0')
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_get_access %s\n", name);

	*status = _notify_lib_get_access(ns, name, (uint32_t *)mode);
	vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
	return KERN_SUCCESS;
}

kern_return_t __notify_server_release_name
(
	mach_port_t server,
	caddr_t name,
	mach_msg_type_number_t nameCnt,
	int *status,
	audit_token_t token
)
{
	uint32_t u, g;
	uid_t uid;
	gid_t gid;

	if (name == NULL)
	{
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	if (ns == NULL)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	if (name[nameCnt] != '\0')
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_release_name %s\n", name);

	_notify_lib_get_owner(ns, name, &u, &g);

	uid = (uid_t)-1;
	gid = (gid_t)-1;
	audit_token_to_au32(token, NULL, &uid, &gid, NULL, NULL, NULL, NULL, NULL);

	/* only root and owner may release names */
	if ((uid != 0) && (uid != u))
	{ 
		*status = NOTIFY_STATUS_NOT_AUTHORIZED;
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		return KERN_SUCCESS;
	}

	*status = _notify_lib_release_name(ns, name, uid, gid);
	vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
	return KERN_SUCCESS;
}

kern_return_t __notify_server_monitor_file
(
	mach_port_t server,
	int client_id,
	caddr_t path,
	mach_msg_type_number_t pathCnt,
	int flags,
	int *status,
	audit_token_t token
)
{
	client_t *c;
	name_info_t *n;
	uid_t uid;
	gid_t gid;

	if (path == NULL)
	{
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	if (ns == NULL)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)path, pathCnt);
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	if (path[pathCnt] != '\0')
	{
		vm_deallocate(mach_task_self(), (vm_address_t)path, pathCnt);
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_monitor_file %d %s %d\n", client_id, path, flags);

	c = _nc_table_find_n(ns->client_table, client_id);
	if (c == NULL)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)path, pathCnt);
		*status = NOTIFY_STATUS_INVALID_REQUEST;
		return KERN_SUCCESS;
	}

	n = c->info->name_info;
	if (n == NULL)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)path, pathCnt);
		*status = NOTIFY_STATUS_INVALID_REQUEST;
		return KERN_SUCCESS;
	}

	uid = (uid_t)-1;
	gid = (gid_t)-1;
	audit_token_to_au32(token, NULL, &uid, &gid, NULL, NULL, NULL, NULL, NULL);

	*status = service_open_file(client_id, n->name, path, flags, uid, gid);
	vm_deallocate(mach_task_self(), (vm_address_t)path, pathCnt);
	return KERN_SUCCESS;
}

kern_return_t __notify_server_get_event
(
	mach_port_t server,
	int client_id,
	int *event_type,
	inline_data_t name,
	mach_msg_type_number_t *nameCnt,
	int *status,
	audit_token_t token
)
{
	client_t *c;
	svc_info_t *s;
	w_event_t *e;
	uint32_t len;

	if (ns == NULL)
	{
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_get_event %u\n", client_id);

	*status = NOTIFY_STATUS_INVALID_REQUEST;
	*event_type = 0;
	*nameCnt = 0;

	c = _nc_table_find_n(ns->client_table, client_id);
	if (c == NULL) return KERN_SUCCESS;

	s = (svc_info_t *)c->info->private;

	if (s == NULL) return KERN_SUCCESS;

	e = service_get_event(s);

	*status = NOTIFY_STATUS_OK;
	if (e == NULL) return KERN_SUCCESS;

	*event_type = e->type;
	len = strlen(e->name) + 1;
	memcpy(name, e->name, len);
	*nameCnt = len;

	w_event_release(e);

	return KERN_SUCCESS;
}

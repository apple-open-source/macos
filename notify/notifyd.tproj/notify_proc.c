/*
 * Copyright (c) 2003-2010 Apple Inc. All rights reserved.
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
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_traps.h>
#include <sys/sysctl.h>
#include <pthread.h>
#include <sys/fcntl.h>
#include "notify.h"
#include "notifyd.h"
#include "service.h"
#include "notify_ipc.h"

extern uint32_t _notify_lib_check_controlled_access();

void
cancel_client(client_t *c)
{
	name_info_t *n;

	if (global.notify_state == NULL) return;
	if (c == NULL) return;
	if (c->info == NULL) return;

	if (c->info->private != NULL)
	{
		service_close(c->info->private);
		c->info->private = NULL;
	}

	n = c->info->name_info;
	if ((n != NULL) && (n->refcount == 1) && (n->private != NULL))
	{
		service_close(n->private);
		n->private = NULL;
	}
	
	if (c->info->notify_type == NOTIFY_TYPE_MEMORY)
	{
		global.shared_memory_refcount[n->slot]--;
	}
	
	_notify_lib_cancel(global.notify_state, c->client_id);
}

static void
register_client(client_t *c)
{
	if (c == NULL) return;
	if (c->info == NULL) return;
	
	c->info->proc_src = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, c->info->pid, DISPATCH_PROC_EXIT, global.work_q);
	dispatch_source_set_event_handler_f(c->info->proc_src, (dispatch_function_t)cancel_client);
	dispatch_set_context(c->info->proc_src, c);

	dispatch_resume(c->info->proc_src);
}

static void
register_port(client_t *c)
{
	mach_port_t port;

	if (c == NULL) return;
	if (c->info == NULL) return;

	port = c->info->msg->header.msgh_remote_port;

	c->info->port_src = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_SEND, port, DISPATCH_MACH_SEND_DEAD, global.work_q);
	dispatch_source_set_event_handler_f(c->info->port_src, (dispatch_function_t)cancel_client);
	dispatch_set_context(c->info->port_src, c);

	dispatch_resume(c->info->port_src);
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

	if (global.notify_state == NULL)
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

	*status = _notify_lib_check_controlled_access(global.notify_state, name, uid, gid, NOTIFY_ACCESS_WRITE);
	if (*status != NOTIFY_STATUS_OK)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		return KERN_SUCCESS;
	}

	*status = daemon_post(name, uid, gid);
	vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);

	return KERN_SUCCESS;
}

kern_return_t __notify_server_simple_post
(
	mach_port_t server,
	caddr_t name,
	mach_msg_type_number_t nameCnt,
	audit_token_t token
)
{
	int status;

	return __notify_server_post(server, name, nameCnt, &status, token);
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
	client_t *c;

	*client_id = 0;

	if (name == NULL)
	{
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	if (global.notify_state == NULL)
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

	*status = _notify_lib_register_plain(global.notify_state, name, -1, uid, gid, (uint32_t *)client_id);
	if (*status != NOTIFY_STATUS_OK)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		return KERN_SUCCESS;
	}

	c = _nc_table_find_n(global.notify_state->client_table, *client_id);
	if ((c != NULL) && (c->info != NULL)) c->info->pid = pid;

	if (!strncmp(name, SERVICE_PREFIX, SERVICE_PREFIX_LEN)) service_open(name, c, uid, gid);
	vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);

	register_client(c);

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
	client_t *c;

	*size = 0;
	*slot = 0;
	*client_id = 0;

	if (name == NULL)
	{
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	if (global.notify_state == NULL)
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

	if (global.nslots == 0)
	{
		*size = -1;
		*slot = -1;
		return __notify_server_register_plain(server, name, nameCnt, client_id, status, token);
	}

	x = (uint32_t)-1;

	n = (name_info_t *)_nc_table_find(global.notify_state->name_table, name);
	if (n != NULL) x = n->slot;

	new_slot = 0;
	if (x == (uint32_t)-1) 
	{
		/* find a slot */
		new_slot = 1;

		for (i = 0, j = global.slot_id + 1; i < global.nslots; i++, j++)
		{
			if (j >= global.nslots) j = 0;
			if (global.shared_memory_refcount[j] == 0)
			{
				x = j;
				break;
			}
		}

		if (x == (uint32_t)-1)
		{
			/* Ran out of slots! */
			global.slot_id++;
			if (global.slot_id >= global.nslots) global.slot_id = 0;
			log_message(ASL_LEVEL_DEBUG, "reused shared memory slot %u\n", global.slot_id);
			x = global.slot_id;
		}
		else
		{
			if (x == (global.slot_id + 1)) global.slot_id = x;
		}
	}

	if (new_slot == 1) global.shared_memory_base[x] = 1;
	global.shared_memory_refcount[x]++;

	uid = (uid_t)-1;
	gid = (gid_t)-1;
	pid = (pid_t)-1;
	audit_token_to_au32(token, NULL, &uid, &gid, NULL, NULL, &pid, NULL, NULL);

	*size = global.nslots * sizeof(uint32_t);
	*slot = x;
	*status = _notify_lib_register_plain(global.notify_state, name, x, uid, gid, (uint32_t *)client_id);
	if (*status != NOTIFY_STATUS_OK)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		return KERN_SUCCESS;
	}

	c = _nc_table_find_n(global.notify_state->client_table, *client_id);
	if ((c != NULL) && (c->info != NULL)) c->info->pid = pid;

	if (!strncmp(name, SERVICE_PREFIX, SERVICE_PREFIX_LEN)) service_open(name, c, uid, gid);
	vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);

	register_client(c);

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
	client_t *c;

	*client_id = 0;

	if (name == NULL)
	{
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	if (global.notify_state == NULL)
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

	*status = _notify_lib_register_signal(global.notify_state, name, pid, sig, uid, gid, (uint32_t *)client_id);
	if (*status != NOTIFY_STATUS_OK)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		return KERN_SUCCESS;
	}

	c = _nc_table_find_n(global.notify_state->client_table, *client_id);
	if ((c != NULL) && (c->info != NULL)) c->info->pid = pid;

	if (!strncmp(name, SERVICE_PREFIX, SERVICE_PREFIX_LEN)) service_open(name, c, uid, gid);
	vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);

	register_client(c);

	return KERN_SUCCESS;
}

kern_return_t __notify_server_register_file_descriptor
(
	mach_port_t server,
	caddr_t name,
	mach_msg_type_number_t nameCnt,
	fileport_t fileport,
	int ntoken,
	int *client_id,
	int *status,
	audit_token_t token
)
{
	uid_t uid;
	gid_t gid;
	pid_t pid;
	client_t *c;
	int fd, flags;

	*client_id = 0;

	if (name == NULL)
	{
		mach_port_deallocate(mach_task_self(), fileport);
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	if (global.notify_state == NULL)
	{
		mach_port_deallocate(mach_task_self(), fileport);
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	if (name[nameCnt] != '\0')
	{
		mach_port_deallocate(mach_task_self(), fileport);
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_register_file_descriptor %s\n", name);

	uid = (uid_t)-1;
	gid = (gid_t)-1;
	pid = (gid_t)-1;
	audit_token_to_au32(token, NULL, &uid, &gid, NULL, NULL, &pid, NULL, NULL);

	fd = fileport_makefd(fileport);
	mach_port_deallocate(mach_task_self(), fileport);
	if (fd < 0)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_INVALID_FILE;
		return KERN_SUCCESS;
	}

	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) < 0)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	*status = _notify_lib_register_file_descriptor(global.notify_state, name, fd, ntoken, uid, gid, (uint32_t *)client_id);
	if (*status != NOTIFY_STATUS_OK)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		return KERN_SUCCESS;
	}

	c = _nc_table_find_n(global.notify_state->client_table, *client_id);
	if ((c != NULL) && (c->info != NULL)) c->info->pid = pid;

	if (!strncmp(name, SERVICE_PREFIX, SERVICE_PREFIX_LEN)) service_open(name, c, uid, gid);
	vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);

	register_client(c);

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
	client_t *c;

	if (name == NULL)
	{
		mach_port_deallocate(mach_task_self(), port);
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	if (global.notify_state == NULL)
	{
		mach_port_deallocate(mach_task_self(), port);
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	if (name[nameCnt] != '\0')
	{
		mach_port_deallocate(mach_task_self(), port);
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_register_mach_port %s\n", name);

	uid = (uid_t)-1;
	gid = (gid_t)-1;
	pid = (gid_t)-1;
	audit_token_to_au32(token, NULL, &uid, &gid, NULL, NULL, &pid, NULL, NULL);

	*status = _notify_lib_register_mach_port(global.notify_state, name, port, ntoken, uid, gid, (uint32_t *)client_id);
	if (*status != NOTIFY_STATUS_OK)
	{
		vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);
		mach_port_deallocate(mach_task_self(), port);
		return KERN_SUCCESS;
	}

	c = _nc_table_find_n(global.notify_state->client_table, *client_id);
	if ((c != NULL) && (c->info != NULL)) c->info->pid = pid;

#ifdef PORT_DEBUG
	log_message(ASL_LEVEL_NOTICE, "register com port 0x%08x for pid %d\n", port, pid);
#endif

	if (!strncmp(name, SERVICE_PREFIX, SERVICE_PREFIX_LEN)) service_open(name, c, uid, gid);
	vm_deallocate(mach_task_self(), (vm_address_t)name, nameCnt);

	register_client(c);
	register_port(c);

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
	client_t *c;

	if (global.notify_state == NULL)
	{
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_cancel %u\n", client_id);

	uid = (uid_t)-1;
	pid = (gid_t)-1;
	audit_token_to_au32(token, NULL, &uid, NULL, NULL, NULL, &pid, NULL, NULL);

	*status = NOTIFY_STATUS_OK;

	c = _nc_table_find_n(global.notify_state->client_table, client_id);
	if (c == NULL) return KERN_SUCCESS;

	if (c->info == NULL)
	{
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	if ((uid != 0) && (c->info->pid != pid))
	{
		*status = NOTIFY_STATUS_NOT_AUTHORIZED;
		return KERN_SUCCESS;
	}

	cancel_client(c);

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
	client_t *c;
	name_info_t *n;

	if (global.notify_state == NULL)
	{
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_suspend %u\n", client_id);

	uid = (uid_t)-1;
	pid = (gid_t)-1;
	audit_token_to_au32(token, NULL, &uid, NULL, NULL, NULL, &pid, NULL, NULL);

	*status = NOTIFY_STATUS_OK;

	c = _nc_table_find_n(global.notify_state->client_table, client_id);
	if (c == NULL) return KERN_SUCCESS;

	if (c->info == NULL)
	{
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	if ((uid != 0) && (c->info->pid != pid))
	{
		*status = NOTIFY_STATUS_NOT_AUTHORIZED;
		return KERN_SUCCESS;
	}

	n = c->info->name_info;
	if (n == NULL) return KERN_SUCCESS;

	_notify_lib_suspend(global.notify_state, client_id);

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
	client_t *c;
	name_info_t *n;

	if (global.notify_state == NULL)
	{
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_resume %u\n", client_id);

	uid = (uid_t)-1;
	pid = (gid_t)-1;
	audit_token_to_au32(token, NULL, &uid, NULL, NULL, NULL, &pid, NULL, NULL);

	*status = NOTIFY_STATUS_OK;

	c = _nc_table_find_n(global.notify_state->client_table, client_id);
	if (c == NULL)
	{
		return KERN_SUCCESS;
	}

	if (c->info == NULL)
	{
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	if ((uid != 0) && (c->info->pid != pid))
	{
		*status = NOTIFY_STATUS_NOT_AUTHORIZED;
		return KERN_SUCCESS;
	}

	n = c->info->name_info;
	if (n == NULL) return KERN_SUCCESS;

	_notify_lib_resume(global.notify_state, client_id);

	return KERN_SUCCESS;
}

static uid_t
uid_for_pid(pid_t pid)
{
	int mib[4];
	struct kinfo_proc info;
	size_t size = sizeof(struct kinfo_proc);

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_PID;
	mib[3] = pid;

	sysctl(mib, 4, &info, &size, 0, 0);

	return (uid_t)info.kp_eproc.e_ucred.cr_uid;
}

kern_return_t __notify_server_suspend_pid
(
	mach_port_t server,
	int pid,
	int *status,
	audit_token_t token
)
{
	uid_t uid, target_uid;

	if (global.notify_state == NULL)
	{
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_suspend_pid %u\n", pid);

	uid = (uid_t)-1;
	audit_token_to_au32(token, NULL, &uid, NULL, NULL, NULL, NULL, NULL, NULL);

	target_uid = uid_for_pid(pid);

	if ((uid != 0) && (target_uid != uid))
	{
		*status = NOTIFY_STATUS_NOT_AUTHORIZED;
		return KERN_SUCCESS;
	}

	*status = NOTIFY_STATUS_OK;

	_notify_lib_suspend_session(global.notify_state, pid);

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
	uid_t uid, target_uid;

	if (global.notify_state == NULL)
	{
		*status = NOTIFY_STATUS_FAILED;
		return KERN_SUCCESS;
	}

	log_message(ASL_LEVEL_DEBUG, "__notify_server_resume_pid %u\n", pid);

	uid = (uid_t)-1;
	audit_token_to_au32(token, NULL, &uid, NULL, NULL, NULL, NULL, NULL, NULL);

	target_uid = uid_for_pid(pid);

	if ((uid != 0) && (target_uid != uid))
	{
		*status = NOTIFY_STATUS_NOT_AUTHORIZED;
		return KERN_SUCCESS;
	}

	*status = NOTIFY_STATUS_OK;

	_notify_lib_resume_session(global.notify_state, pid);

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

	*status =  _notify_lib_check(global.notify_state, client_id, check);
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

	*status = _notify_lib_get_state(global.notify_state, client_id, state);
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

	*status = _notify_lib_set_state(global.notify_state, client_id, state, uid, gid);
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

	*status = _notify_lib_get_val(global.notify_state, client_id, val);
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

	*status = _notify_lib_set_val(global.notify_state, client_id, val, uid, gid);
	if (*status == NOTIFY_STATUS_OK)
	{
		c = _nc_table_find_n(global.notify_state->client_table, client_id);
		if ((c == NULL) || (c->info == NULL))
		{
			*status = NOTIFY_STATUS_FAILED;
			return KERN_SUCCESS;
		}

		n = c->info->name_info;
		if (n->slot == -1) return KERN_SUCCESS;
		global.shared_memory_base[n->slot] = val;
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

	if (global.notify_state == NULL)
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

	*status = _notify_lib_set_owner(global.notify_state, name, uid, gid);
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

	if (global.notify_state == NULL)
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

	*status = _notify_lib_get_owner(global.notify_state, name, (uint32_t *)uid, (uint32_t *)gid);
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

	if (global.notify_state == NULL)
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

	_notify_lib_get_owner(global.notify_state, name, &u, &g);

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

	*status = _notify_lib_set_access(global.notify_state, name, mode);
	if ((u != 0) || (g != 0)) *status = _notify_lib_set_owner(global.notify_state, name, u, g);
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

	if (global.notify_state == NULL)
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

	*status = _notify_lib_get_access(global.notify_state, name, (uint32_t *)mode);
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

	if (global.notify_state == NULL)
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

	_notify_lib_get_owner(global.notify_state, name, &u, &g);

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

	*status = _notify_lib_release_name(global.notify_state, name, uid, gid);
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
	uint32_t ubits = (uint32_t)flags;

	if (path == NULL)
	{
		*status = NOTIFY_STATUS_INVALID_NAME;
		return KERN_SUCCESS;
	}

	if (global.notify_state == NULL)
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

	log_message(ASL_LEVEL_DEBUG, "__notify_server_monitor_file %d %s 0x%08x\n", client_id, path, ubits);

	c = _nc_table_find_n(global.notify_state->client_table, client_id);
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

	*status = service_open_path_private(n->name, c, path, uid, gid, ubits);
	vm_deallocate(mach_task_self(), (vm_address_t)path, pathCnt);

	return KERN_SUCCESS;
}

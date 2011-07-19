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

#ifndef _NOTIFY_DAEMON_H_
#define _NOTIFY_DAEMON_H_

#include <libnotify.h>
#include <mach/mach.h>
#include <launch.h>
#include <dispatch/dispatch.h>

struct global_s
{
	mach_port_t server_port;
	launch_data_t launch_dict;
	notify_state_t *notify_state;
	dispatch_queue_t work_q;
	uint32_t request_size;
	uint32_t reply_size;
	uint32_t nslots;
	uint32_t slot_id;
	uint32_t *shared_memory_base;
	uint32_t *shared_memory_refcount;
	uint32_t log_cutoff;
	uint32_t log_default;
	FILE *log_file;
} global;

extern void log_message(int priority, const char *str, ...);
extern uint32_t daemon_post(const char *name, uint32_t u, uint32_t g);
extern void daemon_post_client(uint32_t cid);
extern void daemon_set_state(const char *name, uint64_t val);
extern void dump_status(uint32_t level);
extern void cancel_port(mach_port_t port);

#endif /* _NOTIFY_DAEMON_H_ */

/*
 * Copyright (c) 2007-2011 Apple Inc. All rights reserved.
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

#ifndef __ASL_PRIVATE_H__
#define __ASL_PRIVATE_H__

#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "asl_file.h"
#include "asl_msg.h"
#include <Availability.h>

#define ASL_QUERY_OP_NULL          0x00000

#define ASLMANAGER_SERVICE_NAME "com.apple.aslmanager"
#define NOTIFY_SYSTEM_MASTER "com.apple.system.syslog.master"
#define NOTIFY_SYSTEM_ASL_FILTER "com.apple.system.syslog.asl_filter"
#define NOTIFY_PREFIX_SYSTEM "com.apple.system.syslog"
#define NOTIFY_PREFIX_USER "user.syslog"
#define NOTIFY_RC "com.apple.asl.remote"

#define ASL_OPT_IGNORE "ignore"
#define ASL_OPT_STORE "store"

#define ASL_STORE_LOCATION_FILE 0
#define ASL_STORE_LOCATION_MEMORY 1

#define ASL_OPT_SYSLOG_LEGACY  0x00010000

#define ASL_KEY_SENDER_MACH_UUID "Sender_Mach_UUID"

/* SPI to enable ASL filter tunneling using asl_set_filter() */
#define ASL_FILTER_MASK_TUNNEL   0x100

typedef struct
{
	int fd;
	uint32_t encoding;
	uint32_t filter;
	char *mfmt;
	char *tfmt;
} asl_out_file_t;

typedef struct __aslclient
{
	uint32_t options;
	struct sockaddr_un server;
	int sock;
	pid_t pid;
	uid_t uid;
	gid_t gid;
	char *name;
	char *facility;
	uint32_t filter;
	int notify_token;
	int notify_master_token;
	uint32_t out_count;
	asl_out_file_t *out_list;
	asl_file_t *aslfile;
	uint64_t aslfileid;
	uint32_t reserved1;
	void *reserved2;
	int32_t refcount;
} asl_client_t;

__BEGIN_DECLS

int asl_add_output(aslclient asl, int fd, const char *msg_fmt, const char *time_fmt, uint32_t text_encoding) __OSX_AVAILABLE_STARTING(__MAC_10_5, __IPHONE_2_0);
int asl_remove_output(aslclient asl, int fd) __OSX_AVAILABLE_STARTING(__MAC_10_5, __IPHONE_2_0);
int asl_store_location() __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_4_3);
int asl_get_filter(aslclient asl, int *local, int *master, int *remote, int *active) __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_4_3);
char *asl_remote_notify_name() __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_4_3);
int asl_trigger_aslmanager(void) __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);
aslmsg _asl_server_control_query(void) __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);

__END_DECLS

#endif /* __ASL_PRIVATE_H__ */

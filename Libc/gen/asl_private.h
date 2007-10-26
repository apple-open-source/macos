/*
 * Copyright (c) 2007 Apple Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 2007 Apple Inc.  All Rights
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
#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define _PATH_ASL_OUT "/var/log/asl.log"

#define ASL_QUERY_OP_NULL          0x00000

#define NOTIFY_SYSTEM_MASTER "com.apple.system.syslog.master"
#define NOTIFY_SYSTEM_ASL_FILTER "com.apple.system.syslog.asl_filter"
#define NOTIFY_PREFIX_SYSTEM "com.apple.system.syslog"
#define NOTIFY_PREFIX_USER "user.syslog"

#define ASL_MSG_FMT_RAW "raw"
#define ASL_MSG_FMT_STD "std"
#define ASL_MSG_FMT_BSD "bsd"
#define ASL_MSG_FMT_XML "xml"
#define ASL_MSG_FMT_MSG "msg"

#define ASL_TIME_FMT_SEC "sec"
#define ASL_TIME_FMT_UTC "utc"
#define ASL_TIME_FMT_LCL "lcl"

#define ASL_KEY_REF_PID  "RefPID"
#define ASL_KEY_REF_PROC "RefProc"

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
	uint32_t fd_count;
	int *fd_list;
	char **fd_mfmt;
	char **fd_tfmt;
	uint32_t reserved1;
	void *reserved2;
} asl_client_t;

typedef struct __aslmsg
{
	uint32_t type;
	uint32_t count;
	char **key;
	char **val;
	uint32_t *op;
} asl_msg_t;

typedef struct __aslresponse
{
	uint32_t count;
	uint32_t curr;
	asl_msg_t **msg;
} asl_search_result_t;


__BEGIN_DECLS

int asl_add_output(aslclient asl, int fd, const char *msg_fmt, const char *time_fmt);
int asl_remove_output(aslclient asl, int fd);
char *asl_format_message(aslmsg msg, const char *msg_fmt, const char *time_fmt, uint32_t *outlen);

__END_DECLS


/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#import <mach/mach.h>
#ifndef __MACH30__
#import <libc.h>
#import <stdio.h>
#import <mach/mach_error.h>
#import <mach/message.h>
#import <servers/bootstrap.h>

#import "profileServer.h"

static port_t profile_server_port = PORT_NULL;

static msg_return_t send_request(
    port_t port,
    enum request_type request,
    const char *dylib,
    enum profile_state *profile_state,
    char *gmon_file);

boolean_t
profile_server_exists(
void)
{
    boolean_t service_active;

	if(bootstrap_status(bootstrap_port, PROFILE_SERVER_NAME,
	   &service_active) != BOOTSTRAP_SUCCESS)
	    return(FALSE);
	if(service_active == FALSE)
	    return(FALSE);
	return(TRUE);
}    

boolean_t
buffer_for_dylib(
const char *dylib_name, 
char *gmon_file)
{
    boolean_t service_active;
    enum profile_state state;
    enum request_type request;
    msg_return_t ret;

	if(bootstrap_status(bootstrap_port, PROFILE_SERVER_NAME,
	   &service_active) != BOOTSTRAP_SUCCESS)
	    return(FALSE);
	if(service_active == FALSE)
	    return(FALSE);

	if(profile_server_port == PORT_NULL){
	    if(bootstrap_look_up(bootstrap_port, PROFILE_SERVER_NAME,
				 &profile_server_port) != BOOTSTRAP_SUCCESS)
	    return(FALSE);
	}

	request = NSBufferFileForLibrary;
	ret = send_request(profile_server_port, request, dylib_name, &state,
			   gmon_file);
	if(ret != SEND_SUCCESS || state == NSBufferNotCreated)
	    return(FALSE);
	return(TRUE);
}

boolean_t
new_buffer_for_dylib(
const char *dylib_name,
char *gmon_file)
{
    boolean_t service_active;
    enum profile_state state;
    enum request_type request;
    msg_return_t ret;

	if(bootstrap_status(bootstrap_port, PROFILE_SERVER_NAME,
	   &service_active) != BOOTSTRAP_SUCCESS)
	    return(FALSE);
	if(service_active == FALSE)
	    return(FALSE);

	if(profile_server_port == PORT_NULL){
	    if(bootstrap_look_up(bootstrap_port, PROFILE_SERVER_NAME,
				 &profile_server_port) != BOOTSTRAP_SUCCESS)
	    return(FALSE);
	}

	request = NSCreateProfileBufferForLibrary;
	ret = send_request(profile_server_port, request, dylib_name, &state,
			   gmon_file);
	if(ret != SEND_SUCCESS || state == NSBufferNotCreated)
	    return(FALSE);
	return(TRUE);
}

boolean_t
start_profiling_for_dylib(
const char *dylib_name)
{
    boolean_t service_active;
    enum profile_state state;
    enum request_type request;
    msg_return_t ret;
    char dummy_gmon[MAXPATHLEN];

	if(bootstrap_status(bootstrap_port, PROFILE_SERVER_NAME,
	   &service_active) != BOOTSTRAP_SUCCESS)
	    return(FALSE);
	if(service_active == FALSE)
	    return(FALSE);

	if(profile_server_port == PORT_NULL){
	    if(bootstrap_look_up(bootstrap_port, PROFILE_SERVER_NAME,
				 &profile_server_port) != BOOTSTRAP_SUCCESS)
	    return(FALSE);
	}

	request = NSStartProfilingForLibrary;
	ret = send_request(profile_server_port, request, dylib_name, &state,
			   dummy_gmon);
	if(ret != SEND_SUCCESS || state == NSBufferNotCreated)
	    return(FALSE);
	return(TRUE);
}

boolean_t
stop_profiling_for_dylib(
const char *dylib_name)
{
    boolean_t service_active;
    enum profile_state state;
    enum request_type request;
    msg_return_t ret;
    char dummy_gmon[MAXPATHLEN];

	if(bootstrap_status(bootstrap_port, PROFILE_SERVER_NAME,
	   &service_active) != BOOTSTRAP_SUCCESS)
	    return(FALSE);
	if(service_active == FALSE)
	    return(FALSE);

	if(profile_server_port == PORT_NULL){
	    if(bootstrap_look_up(bootstrap_port, PROFILE_SERVER_NAME,
				 &profile_server_port) != BOOTSTRAP_SUCCESS)
	    return(FALSE);
	}

	request = NSStopProfilingForLibrary;
	ret = send_request(profile_server_port, request, dylib_name, &state,
			   dummy_gmon);
	if(ret != SEND_SUCCESS || state != NSProfilingStopped)
	    return(FALSE);
	return(TRUE);
}

static
msg_return_t
send_request(
port_t port,
enum request_type request,
const char *dylib,
enum profile_state *profile_state,
char *gmon_file)
{
    union {
	struct request_msg request;
	struct reply_msg reply;
    } msg;
    msg_return_t msg_ret;

	/*
	 * Cons up the header and type structs
	 */
	msg.request.hdr.msg_simple = TRUE;
	msg.request.hdr.msg_size = sizeof(struct request_msg);
	msg.request.hdr.msg_type = MSG_TYPE_NORMAL;
	msg.request.hdr.msg_local_port = thread_reply();
	msg.request.hdr.msg_remote_port = port;
	msg.request.hdr.msg_id = PROFILE_REQUEST_ID;
	msg.request.request_type.msg_type_name = MSG_TYPE_INTEGER_32;
	msg.request.request_type.msg_type_size = sizeof(enum request_type) * 8;
	msg.request.request_type.msg_type_number = 1;
	msg.request.request_type.msg_type_inline = TRUE;
	msg.request.request_type.msg_type_longform = FALSE;
	msg.request.request_type.msg_type_deallocate = FALSE;
	msg.request.dylib_type.msg_type_name = MSG_TYPE_CHAR;
	msg.request.dylib_type.msg_type_size = sizeof(char) * 8;
	msg.request.dylib_type.msg_type_number = strlen(dylib) + 1;
	msg.request.dylib_type.msg_type_inline = TRUE;
	msg.request.dylib_type.msg_type_longform = FALSE;
	msg.request.dylib_type.msg_type_deallocate = FALSE;
	
	strcpy(msg.request.dylib_file, dylib);
	msg.request.request = request;

	/*
	 * Send it off.
	 */
	msg_ret = msg_rpc(&msg.request.hdr, MSG_OPTION_NONE, sizeof(msg),
			  (msg_timeout_t)500, (msg_timeout_t)500);
	if(msg_ret != RPC_SUCCESS)
	    return(msg_ret);
	
	*profile_state = msg.reply.profile_state;
	strcpy(gmon_file, msg.reply.gmon_file);
	return(SEND_SUCCESS);
}
#endif /* __MACH30__ */

/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
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

/*
 * sys.h
 *
 * Miscellaneous OS functions
 *
 * Copyright (c) 1999 Apple Computer, Inc.  All Rights Reserved.
 * Written by Marc Majka
 */

#include <NetInfo/config.h>
#include <mach/mach.h>
#include <mach/mach_error.h>

#define SYS_PORT_NULL MACH_PORT_NULL
#define sys_msg_return_type mach_msg_return_t
#define sys_port_type mach_port_t
#define sys_task_port_type task_port_t
#define sys_msg_timeout_type mach_msg_timeout_t
#define sys_msg_header_type mach_msg_header_t
#define sys_strerror mach_error_string
#define sys_task_self mach_task_self
#define sys_task_for_pid task_for_pid
#define sys_msg_local_port msgh_local_port

#define SERVER_STATUS_ERROR -1
#define SERVER_STATUS_INACTIVE 0
#define SERVER_STATUS_ACTIVE 1
#define SERVER_STATUS_ON_DEMAND 2

kern_return_t sys_create_service(char *name, sys_port_type *p, int restart);
kern_return_t sys_destroy_service(char *name);

kern_return_t sys_receive_port_alloc(sys_msg_header_type *h, unsigned int size);

kern_return_t sys_port_alloc(sys_port_type *p);

void sys_port_free(sys_port_type p);

sys_msg_return_type
sys_receive_message(sys_msg_header_type *h, unsigned int size, sys_port_type p, sys_msg_timeout_type t);

kern_return_t
sys_send_message(sys_msg_header_type *h, sys_msg_timeout_type t);

sys_port_type
sys_message_port(sys_msg_header_type *h);

kern_return_t
sys_port_extract_receive_right(sys_task_port_type et, sys_port_type ep, sys_port_type *p);

sys_port_type lookupd_port(char *name);

int sys_server_status(char *name);

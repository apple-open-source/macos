/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#import <mach/mach.h>
#import <mach/message.h>
#import <mach/mach_error.h>

#ifdef MOSX
#define task_self	mach_task_self
#define MSG_OPTION_NONE	0
#define SEND_SUCCESS	MACH_MSG_SUCCESS
#define RCV_SUCCESS	MACH_MSG_SUCCESS
#define RCV_TIMED_OUT	MACH_RCV_TIMED_OUT
#define RCV_TIMEOUT	MACH_RCV_TIMEOUT
#define port_t		mach_port_t
#define port_all_t	mach_port_t
#define msg_header_t	mach_msg_header_t
#define msg_timeout_t	mach_msg_timeout_t
#define msg_return_t	mach_msg_return_t

static __inline__ kern_return_t
msg_send(mach_msg_header_t * header, mach_msg_option_t options,
	 mach_msg_timeout_t msecs)
{
    return(mach_msg(header, MACH_SEND_MSG | options, header->msgh_size, 0,
		    MACH_PORT_NULL, msecs, MACH_PORT_NULL));
}

static __inline__ kern_return_t
msg_receive(mach_msg_header_t * header, mach_msg_option_t options,
	    mach_msg_timeout_t msecs)
{
    return (mach_msg(header, MACH_RCV_MSG | options, 0, header->msgh_size, 
		     header->msgh_local_port, msecs, MACH_PORT_NULL));
}

static __inline__ kern_return_t
port_deallocate(task_port_t task, mach_port_name_t name)
{
    return (mach_port_destroy(task, name));
}

static __inline__ kern_return_t
port_allocate(task_port_t task, mach_port_name_t * name)
{
    kern_return_t ret;

    ret = mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE,
			     name);
    if (ret != KERN_SUCCESS)
	return (ret);
    return mach_port_insert_right(task, *name, *name, MACH_MSG_TYPE_MAKE_SEND);
}

#endif MOSX

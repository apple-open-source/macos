/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <mach/mach.h>
#include <mach/notify.h>
#include <mach/mach_error.h>
#include <pthread.h>

#include "SCDPrivate.h"


__private_extern__ mach_msg_id_t
_waitForMachMessage(mach_port_t port)
{
	kern_return_t 		status;
	mach_msg_empty_rcv_t	*buf;

	mach_msg_size_t		size = sizeof(mach_msg_empty_t) + MAX_TRAILER_SIZE;

	status = vm_allocate(mach_task_self(), (vm_address_t *)&buf, size, TRUE);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("vm_allocate(): %s"), mach_error_string(status));
		return -1;
	}

	status = mach_msg(&buf->header,			/* msg */
			  MACH_RCV_MSG,			/* options */
			  0,				/* send_size */
			  size,				/* rcv_size */
			  port,				/* rcv_name */
			  MACH_MSG_TIMEOUT_NONE,	/* timeout */
			  MACH_PORT_NULL);		/* notify */
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("mach_msg(): %s"), mach_error_string(status));
		return -1;
	}

	return buf->header.msgh_id;
}


void
_showMachPortStatus()
{
#ifdef	DEBUG
	/* print status of in-use mach ports */
	if (SCDOptionGet(NULL, kSCDOptionDebug) && SCDOptionGet(NULL, kSCDOptionVerbose)) {
		kern_return_t		status;
		mach_port_name_array_t	ports;
		mach_port_type_array_t	types;
		int			pi, pn, tn;
		CFMutableStringRef	str;

		SCDLog(LOG_DEBUG, CFSTR("----------"));

		/* report on ALL mach ports associated with this task */
		status = mach_port_names(mach_task_self(), &ports, &pn, &types, &tn);
		if (status == MACH_MSG_SUCCESS) {
			str = CFStringCreateMutable(NULL, 0);
			for (pi=0; pi < pn; pi++) {
				char	rights[16], *rp = &rights[0];

				if (types[pi] != MACH_PORT_TYPE_NONE) {
					*rp++ = ' ';
					*rp++ = '(';
					if (types[pi] & MACH_PORT_TYPE_SEND)
						*rp++ = 'S';
					if (types[pi] & MACH_PORT_TYPE_RECEIVE)
						*rp++ = 'R';
					if (types[pi] & MACH_PORT_TYPE_SEND_ONCE)
						*rp++ = 'O';
					if (types[pi] & MACH_PORT_TYPE_PORT_SET)
						*rp++ = 'P';
					if (types[pi] & MACH_PORT_TYPE_DEAD_NAME)
						*rp++ = 'D';
					*rp++ = ')';
				}
				*rp = '\0';
				CFStringAppendFormat(str, NULL, CFSTR(" %d%s"), ports[pi], rights);
			}
			SCDLog(LOG_DEBUG, CFSTR("Task ports (n=%d):%@"), pn, str);
			CFRelease(str);
		} else {
			/* log (but ignore) errors */
			SCDLog(LOG_DEBUG, CFSTR("mach_port_names(): %s"), mach_error_string(status));
		}
	}
#endif	/* DEBUG */
	return;
}


void
_showMachPortReferences(mach_port_t port)
{
#ifdef	DEBUG
	kern_return_t		status;
	mach_port_urefs_t	refs_send	= 0;
	mach_port_urefs_t	refs_recv	= 0;
	mach_port_urefs_t	refs_once	= 0;
	mach_port_urefs_t	refs_pset	= 0;
	mach_port_urefs_t	refs_dead	= 0;

	SCDLog(LOG_DEBUG, CFSTR("user references for mach port %d"), port);

	status = mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND,      &refs_send);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("  mach_port_get_refs(MACH_PORT_RIGHT_SEND): %s"), mach_error_string(status));
		return;
	}

	status = mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE,   &refs_recv);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("  mach_port_get_refs(MACH_PORT_RIGHT_RECEIVE): %s"), mach_error_string(status));
		return;
	}

	status = mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND_ONCE, &refs_once);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("  mach_port_get_refs(MACH_PORT_RIGHT_SEND_ONCE): %s"), mach_error_string(status));
		return;
	}

	status = mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_PORT_SET,  &refs_pset);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("  mach_port_get_refs(MACH_PORT_RIGHT_PORT_SET): %s"), mach_error_string(status));
		return;
	}

	status = mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_DEAD_NAME, &refs_dead);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("  mach_port_get_refs(MACH_PORT_RIGHT_DEAD_NAME): %s"), mach_error_string(status));
		return;
	}

	SCDLog(LOG_DEBUG,
	       CFSTR("  send = %d, receive = %d, send once = %d, port set = %d, dead name = %d"),
	       refs_send,
	       refs_recv,
	       refs_once,
	       refs_pset,
	       refs_dead);

#endif	/* DEBUG */
	return;
}

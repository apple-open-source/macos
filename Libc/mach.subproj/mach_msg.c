/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#include <mach/port.h>
#include <mach/message.h>

#define LIBMACH_OPTIONS	(MACH_SEND_INTERRUPT|MACH_RCV_INTERRUPT)

mach_msg_return_t
mach_msg(msg, option, send_size, rcv_size, rcv_name, timeout, notify)
	mach_msg_header_t *msg;
	mach_msg_option_t option;
	mach_msg_size_t send_size;
	mach_msg_size_t rcv_size;
	mach_port_t rcv_name;
	mach_msg_timeout_t timeout;
	mach_port_t notify;
{
	mach_msg_return_t mr;

	/*
	 * Consider the following cases:
	 *	1) Errors in pseudo-receive (eg, MACH_SEND_INTERRUPTED
	 *	plus special bits).
	 *	2) Use of MACH_SEND_INTERRUPT/MACH_RCV_INTERRUPT options.
	 *	3) RPC calls with interruptions in one/both halves.
	 *
	 * We refrain from passing the option bits that we implement
	 * to the kernel.  This prevents their presence from inhibiting
	 * the kernel's fast paths (when it checks the option value).
	 */

	mr = mach_msg_overwrite_trap(msg, option &~ LIBMACH_OPTIONS,
			   send_size, rcv_size, rcv_name,
			   timeout, notify, MACH_MSG_NULL, 0);
	if (mr == MACH_MSG_SUCCESS)
		return MACH_MSG_SUCCESS;

	if ((option & MACH_SEND_INTERRUPT) == 0)
		while (mr == MACH_SEND_INTERRUPTED)
			mr = mach_msg_overwrite_trap(msg,
				option &~ LIBMACH_OPTIONS,
				send_size, rcv_size, rcv_name,
				timeout, notify, MACH_MSG_NULL, 0);

	if ((option & MACH_RCV_INTERRUPT) == 0)
		while (mr == MACH_RCV_INTERRUPTED)
			mr = mach_msg_overwrite_trap(msg,
				option &~ (LIBMACH_OPTIONS|MACH_SEND_MSG),
				0, rcv_size, rcv_name,
				timeout, notify, MACH_MSG_NULL, 0);

	return mr;
}

mach_msg_return_t
mach_msg_overwrite(msg, option, send_size, rcv_limit, rcv_name, timeout, 
		   notify, rcv_msg, rcv_msg_size)
	mach_msg_header_t *msg;
	mach_msg_option_t option;
	mach_msg_size_t send_size;
	mach_msg_size_t rcv_limit;
	mach_port_t rcv_name;
	mach_msg_timeout_t timeout;
	mach_port_t notify;
	mach_msg_header_t *rcv_msg;
	mach_msg_size_t rcv_msg_size;
{
	mach_msg_return_t mr;

	/*
	 * Consider the following cases:
	 *	1) Errors in pseudo-receive (eg, MACH_SEND_INTERRUPTED
	 *	plus special bits).
	 *	2) Use of MACH_SEND_INTERRUPT/MACH_RCV_INTERRUPT options.
	 *	3) RPC calls with interruptions in one/both halves.
	 *
	 * We refrain from passing the option bits that we implement
	 * to the kernel.  This prevents their presence from inhibiting
	 * the kernel's fast paths (when it checks the option value).
	 */

	mr = mach_msg_overwrite_trap(msg, option &~ LIBMACH_OPTIONS,
			   send_size, rcv_limit, rcv_name,
			   timeout, notify, rcv_msg, rcv_msg_size);
	if (mr == MACH_MSG_SUCCESS)
		return MACH_MSG_SUCCESS;

	if ((option & MACH_SEND_INTERRUPT) == 0)
		while (mr == MACH_SEND_INTERRUPTED)
			mr = mach_msg_overwrite_trap(msg,
				option &~ LIBMACH_OPTIONS,
				send_size, rcv_limit, rcv_name,
				timeout, notify, rcv_msg, rcv_msg_size);

	if ((option & MACH_RCV_INTERRUPT) == 0)
		while (mr == MACH_RCV_INTERRUPTED)
			mr = mach_msg_overwrite_trap(msg,
				option &~ (LIBMACH_OPTIONS|MACH_SEND_MSG),
				0, rcv_limit, rcv_name,
				timeout, notify, rcv_msg, rcv_msg_size);

	return mr;
}

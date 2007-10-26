/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef _LIBPROC_APPLE_H
#define _LIBPROC_APPLE_H

#ifdef	__cplusplus
extern "C" {
#endif
		
/*
 * APPLE NOTE: 
 *
 * This file exists to expose the innards of ps_prochandle.
 * We cannot place this in libproc.h, because it refers to
 * objc and mach specific classes and types.
 *
 * The Apple emulation of /proc control requires access to
 * this structure.
 */
	
struct ps_prochandle {
	task_t task;
	pstatus_t status;
	VMUSymbolicator* symbolicator;
	VMUSymbolicator* prev_symbolicator;
	NSMutableDictionary* prmap_dictionary;
	mach_port_t dtrace_dyld_port;
	Phandler_func_t *activity_handler_func;
	void *activity_handler_arg;
	rd_event_msg_t activity_handler_rdm;
};

extern void Pmothball_syms(struct ps_prochandle *);
	
#ifdef  __cplusplus
}
#endif

#endif  /* _LIBPROC_APPLE_H */

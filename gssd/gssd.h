/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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

// Hack for now. This is (will be) part of <mach/task_special_ports.h>
#ifndef	_GSSD__H_
#define _GSSD_H_

#include <mach/task_special_ports.h>

#ifndef TASK_GSSD_PORT
#define TASK_GSSD_PORT		8	/* GSSD port for security context */

#define task_get_gssd_port(task, port)	\
		(task_get_special_port((task), TASK_GSSD_PORT, (port)))

#define task_set_gssd_port(task, port)	\
		(task_set_special_port((task), TASK_GSSD_PORT, (port)))
#endif
#endif	/* _GSSD_H_ */

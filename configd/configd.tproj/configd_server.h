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

#ifndef _S_CONFIGD_SERVER_H
#define _S_CONFIGD_SERVER_H

#include <sys/cdefs.h>
#include <mach/mach.h>

#include <CoreFoundation/CFRunLoop.h>
#include <CoreFoundation/CFMachPort.h>

extern	CFMachPortRef		configd_port;		/* configd server port (for new session requests) */

__BEGIN_DECLS

void		configdCallback	__P((CFMachPortRef		port,
				     void			*msg,
				     CFIndex			size,
				     void			*info));

boolean_t	server_active	__P(());
void		server_init	__P(());

void		server_loop	__P(());

kern_return_t	_snapshot	__P((mach_port_t server, int *scd_status));

kern_return_t	_configopen	__P((mach_port_t		server,
				     xmlData_t			name,
				     mach_msg_type_number_t	nameCnt,
				     mach_port_t		*newServer,
				     int			*scd_status));

kern_return_t	_configclose	__P((mach_port_t server, int *scd_status));

kern_return_t	_configlock	__P((mach_port_t server, int *scd_status));

kern_return_t	_configunlock	__P((mach_port_t server, int *scd_status));

kern_return_t	_configlist	__P((mach_port_t server,
				     xmlData_t			key,
				     mach_msg_type_number_t	keyCnt,
				     int			regexOptions,
				     xmlDataOut_t		*list,
				     mach_msg_type_number_t	*listCnt,
				     int			*scd_status));

kern_return_t	_configadd	__P((mach_port_t 		server,
				     xmlData_t			key,
				     mach_msg_type_number_t	keyCnt,
				     xmlData_t			data,
				     mach_msg_type_number_t	dataCnt,
				     int			*newInstance,
				     int			*scd_status));

kern_return_t	_configadd_s	__P((mach_port_t 		server,
				     xmlData_t			key,
				     mach_msg_type_number_t	keyCnt,
				     xmlData_t			data,
				     mach_msg_type_number_t	dataCnt,
				     int			*newInstance,
				     int			*scd_status));

kern_return_t	_configget	__P((mach_port_t		server,
				     xmlData_t			key,
				     mach_msg_type_number_t	keyCnt,
				     xmlDataOut_t		*data,
				     mach_msg_type_number_t	*dataCnt,
				     int			*newInstance,
				     int			*scd_status));

kern_return_t	_configset	__P((mach_port_t		server,
				     xmlData_t			key,
				     mach_msg_type_number_t	keyCnt,
				     xmlData_t			data,
				     mach_msg_type_number_t	dataCnt,
				     int			*newInstance,
				     int			*scd_status));

kern_return_t	_configremove	__P((mach_port_t		server,
				     xmlData_t			key,
				     mach_msg_type_number_t	keyCnt,
				     int			*scd_status));

kern_return_t	_configtouch	__P((mach_port_t 		server,
				     xmlData_t			key,
				     mach_msg_type_number_t	keyCnt,
				     int			*scd_status));

kern_return_t	_notifyadd	__P((mach_port_t		server,
				     xmlData_t			key,
				     mach_msg_type_number_t	keyCnt,
				     int			regexOptions,
				     int			*status));

kern_return_t	_notifyremove	__P((mach_port_t		server,
				     xmlData_t			key,
				     mach_msg_type_number_t	keyCnt,
				     int			regexOptions,
				     int			*status));

kern_return_t	_notifychanges	__P((mach_port_t		server,
				     xmlDataOut_t		*list,
				     mach_msg_type_number_t	*listCnt,
				     int			*status));

kern_return_t	_notifyviaport	__P((mach_port_t		server,
				     mach_port_t		port,
				     mach_msg_id_t		msgid,
				     int			*status));

kern_return_t	_notifyviafd	__P((mach_port_t		server,
				     xmlData_t			path,
				     mach_msg_type_number_t	pathCnt,
				     int			identifier,
				     int			*status));

kern_return_t	_notifyviasignal
				__P((mach_port_t		server,
				     task_t			task,
				     int			signal,
				     int			*status));

kern_return_t	_notifycancel	__P((mach_port_t		server,
				     int			*scd_status));

__END_DECLS

#endif /* !_S_CONFIGD_SERVER_H */

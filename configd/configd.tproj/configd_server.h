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

/*
 * Modification History
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * March 24, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#ifndef _S_CONFIGD_SERVER_H
#define _S_CONFIGD_SERVER_H

#include <sys/cdefs.h>
#include <mach/mach.h>

#include <CoreFoundation/CFRunLoop.h>
#include <CoreFoundation/CFMachPort.h>

extern	CFMachPortRef		configd_port;		/* configd server port (for new session requests) */

__BEGIN_DECLS

void		configdCallback	(CFMachPortRef		port,
				 void			*msg,
				 CFIndex		size,
				 void			*info);

boolean_t	server_active	();

void		server_init	();

void		server_loop	();

kern_return_t	_snapshot	(mach_port_t		server,
				 int			*sc_status);

kern_return_t	_configopen	(mach_port_t		server,
				 xmlData_t		nameRef,
				 mach_msg_type_number_t	nameLen,
				 mach_port_t		*newServer,
				 int			*sc_status);

kern_return_t	_configclose	(mach_port_t		server,
				 int			*sc_status);

kern_return_t	_configlock	(mach_port_t		server,
				 int			*sc_status);

kern_return_t	_configunlock	(mach_port_t		server,
				 int			*sc_status);

kern_return_t	_configlist	(mach_port_t server,
				 xmlData_t		keyRef,
				 mach_msg_type_number_t	keyLen,
				 int			isRegex,
				 xmlDataOut_t		*listRef,
				 mach_msg_type_number_t	*listLen,
				 int			*sc_status);

kern_return_t	_configadd	(mach_port_t 		server,
				 xmlData_t		keyRef,
				 mach_msg_type_number_t	keyLen,
				 xmlData_t		dataRef,
				 mach_msg_type_number_t	dataLen,
				 int			*newInstance,
				 int			*sc_status);

kern_return_t	_configadd_s	(mach_port_t 		server,
				 xmlData_t		keyRef,
				 mach_msg_type_number_t	keyLen,
				 xmlData_t		dataRef,
				 mach_msg_type_number_t	dataLen,
				 int			*newInstance,
				 int			*sc_status);

kern_return_t	_configget	(mach_port_t		server,
				 xmlData_t		keyRef,
				 mach_msg_type_number_t	keyLen,
				 xmlDataOut_t		*dataRef,
				 mach_msg_type_number_t	*dataLen,
				 int			*newInstance,
				 int			*sc_status);

kern_return_t	_configset	(mach_port_t		server,
				 xmlData_t		keyRef,
				 mach_msg_type_number_t	keyLen,
				 xmlData_t		dataRef,
				 mach_msg_type_number_t	dataLen,
				 int			*newInstance,
				 int			*sc_status);

kern_return_t	_configremove	(mach_port_t		server,
				 xmlData_t		keyRef,
				 mach_msg_type_number_t	keyLen,
				 int			*sc_status);

kern_return_t	_configtouch	(mach_port_t 		server,
				 xmlData_t		keyRef,
				 mach_msg_type_number_t	keyLen,
				 int			*sc_status);

kern_return_t	_confignotify	(mach_port_t 		server,
				 xmlData_t		keyRef,
				 mach_msg_type_number_t	keyLen,
				 int			*sc_status);

kern_return_t	_configget_m	(mach_port_t		server,
				 xmlData_t		keysRef,
				 mach_msg_type_number_t	keysLen,
				 xmlData_t		patternsRef,
				 mach_msg_type_number_t	patternsLen,
				 xmlDataOut_t		*dataRef,
				 mach_msg_type_number_t	*dataLen,
				 int			*sc_status);

kern_return_t	_configset_m	(mach_port_t		server,
				 xmlData_t		dataRef,
				 mach_msg_type_number_t	dataLen,
				 xmlData_t		removeRef,
				 mach_msg_type_number_t	removeLen,
				 xmlData_t		notifyRef,
				 mach_msg_type_number_t	notifyLen,
				 int			*sc_status);

kern_return_t	_notifyadd	(mach_port_t		server,
				 xmlData_t		keyRef,
				 mach_msg_type_number_t	keyLen,
				 int			isRegex,
				 int			*status);

kern_return_t	_notifyremove	(mach_port_t		server,
				 xmlData_t		keyRef,
				 mach_msg_type_number_t	keyLen,
				 int			isRegex,
				 int			*status);

kern_return_t	_notifychanges	(mach_port_t		server,
				 xmlDataOut_t		*listRef,
				 mach_msg_type_number_t	*listLen,
				 int			*status);

kern_return_t	_notifyviaport	(mach_port_t		server,
				 mach_port_t		port,
				 mach_msg_id_t		msgid,
				 int			*status);

kern_return_t	_notifyviafd	(mach_port_t		server,
				 xmlData_t		pathRef,
				 mach_msg_type_number_t	pathLen,
				 int			identifier,
				 int			*status);

kern_return_t	_notifyviasignal
				(mach_port_t		server,
				 task_t			task,
				 int			signal,
				 int			*status);

kern_return_t	_notifycancel	(mach_port_t		server,
				 int			*sc_status);

__END_DECLS

#endif /* !_S_CONFIGD_SERVER_H */

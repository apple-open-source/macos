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

#ifndef _S_SESSION_H
#define _S_SESSION_H

#include <sys/cdefs.h>

/* Per client server state */
typedef struct {

	/* mach port used as the key to this session */
	mach_port_t		key;

	/* mach port associated with this session */
	CFMachPortRef		serverPort;
	CFRunLoopSourceRef	serverRunLoopSource;

	/* data associated with this "open" session */
	SCDynamicStoreRef	store;

	/* credentials associated with this "open" session */
	int			callerEUID;
	int			callerEGID;

} serverSession, *serverSessionRef;

__BEGIN_DECLS

serverSessionRef	getSession	(mach_port_t	server);

serverSessionRef	addSession	(CFMachPortRef	server);

void			removeSession	(mach_port_t	server);

void			cleanupSession	(mach_port_t	server);

void			listSessions	();

__END_DECLS

#endif /* !_S_SESSION_H */

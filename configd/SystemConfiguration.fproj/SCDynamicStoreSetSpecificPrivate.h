/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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

#ifndef _SCDYNAMICSTORESETSPECIFICPRIVATE_H
#define _SCDYNAMICSTORESETSPECIFICPRIVATE_H

#include <sys/cdefs.h>
#include <SystemConfiguration/SCDynamicStore.h>


__BEGIN_DECLS

/*!
	@function SCDynamicStoreSetConsoleUser
	@discussion Sets the name, user ID, and group ID of the currently
		logged in user.
	@param session An SCDynamicStoreRef that should be used for communication
		with the server.
		If NULL, a temporary session will be used.
	@param user A pointer to a character buffer containing the name of
		the current "Console" user. If NULL, any current "Console"
		user information will be reset.
	@param uid The user ID of the current "Console" user.
	@param gid The group ID of the current "Console" user.
	@result A boolean indicating the success (or failure) of the call.
 */
Boolean
SCDynamicStoreSetConsoleUser		(
					SCDynamicStoreRef	session,
					const char		*user,
					uid_t			uid,
					gid_t			gid
					);

__END_DECLS

#endif /* _SCDYNAMICSTORESETSPECIFICPRIVATE_H */

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

#ifndef _SCDHOSTNAME_H
#define _SCDHOSTNAME_H

#include <sys/cdefs.h>

/*!
	@header SCDHostName.h
	The SystemConfiguration framework provides access to the data used
		to configure a running system.

	Specifically, the SCDHostNameXXX() API's allow an application
		to determine (or set) the login/user currently using the
		console.

	The APIs provided by this framework communicate with the "configd"
		daemon to obtain information regarding the systems current
		configuration.
 */


__BEGIN_DECLS

/*!
	@function SCDKeyCreateHostName
	@discussion Creates a key which can be used by the SCDNotifierAdd()
		function to receive notifications when the current
		computer/host name changes.
	@result A notification string for the current computer/host name".
*/
CFStringRef	SCDKeyCreateHostName	();

/*!
	@function SCDHostNameGet
	@discussion Gets the current computer/host name.
	@param name A pointer to memory which will be filled with the current
		computer/host name.
	@param nameEncoding A pointer to memory which, if non-NULL, will be
		filled with the encoding associated with the computer/host name.
	@result A constant of type SCDStatus indicating the success (or failure) of
		the call.
 */
SCDStatus	SCDHostNameGet		(CFStringRef		*name,
					 CFStringEncoding	*nameEncoding);

__END_DECLS

#endif /* _SCDHOSTNAME_H */

/*
 * Copyright (c) 2003,2011,2014 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
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


//
// privPort.h - shared definitions between port server client and server side
//
#ifndef _H_PRIVPORT
#define _H_PRIVPORT



//
// UNIX domain port name for server connection
//
#define kPrivilegedPortBinder	"/var/run/bindprivport"


//
// Authorization Rights used
//
#define ACCEPTRIGHT "system.privilege.port.accept"
#define CONNECTRIGHT "system.privilege.port.connect"


//
// Structure of a request
//
struct Request {
	AuthorizationExternalForm authForm;
	struct sockaddr_in requestedName;
};

struct Reply {
	OSStatus status;
};


#endif //_H_PRIVPORT

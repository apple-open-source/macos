/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
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

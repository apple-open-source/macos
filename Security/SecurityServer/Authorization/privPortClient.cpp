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
// privPortClient - Authorization privileged-port binding client side
//
#include <Security/AuthorizationPriv.h>
#include <Security/fdmover.h>
#include <Security/debugging.h>
#include "privPort.h"

using namespace UnixPlusPlus;
using namespace IPPlusPlus;


//
// Where is the trampoline itself?
//
#if !defined(PORTSERVERPATH)
# define PORTSERVERPATH "/System/Library/CoreServices/privportserver" /* fallback */
#endif


//
// Local functions
//
static void startServer();


//
// The official client function. This does all the work
//
OSStatus AuthorizationBindPrivilegedPort(int fileDescriptor,
	const struct sockaddr_in *name,
	AuthorizationRef authorization,
	AuthorizationFlags flags)
{
	BEGIN_API
	unsigned short port = ntohs(name->sin_port);
	secdebug("portserve", "bind request fd=%d port=%d", fileDescriptor, port);
	
	// must request a privileged port (don't check for debug version)
#if defined(NDEBUG)
	if (port <= 0 || port >= IPPORT_RESERVED)
		return errAuthorizationBadAddress;
#endif //NEBUG
	
	// no flags recognized so far
	if (flags)
		return errAuthorizationInvalidFlags;

	// form the request message
	Request request;
	if (OSStatus err = AuthorizationMakeExternalForm(authorization, &request.authForm))
		return err;
	request.requestedName = *name;
	
	// connect to the port server (start it if we must)
	UNSockAddress serverAddress(kPrivilegedPortBinder);
	FdMover server;
	server.open(AF_UNIX, SOCK_STREAM);
	try {
		server.connect(serverAddress);
	} catch (const UnixError &error) {
		switch (error.error) {
		case ENOENT:			// not bound
		case ECONNREFUSED:		// was bound, server is dead
			startServer();
			server.connect(serverAddress);
			break;
		default:
			throw;
		}
	}
	
	// send request
	secdebug("portserve", "sending request");
	FdVector fds;
	fds.push_back(fileDescriptor);
	if (server.send(&request, sizeof(request), fds) != sizeof(request))
		UnixError::throwMe(EIO);
	
	// read reply
	secdebug("portserve", "getting reply");
	Reply reply;
	if (server.read(&reply, sizeof(reply)) != sizeof(reply))
		UnixError::throwMe(EIO);

	secdebug("portserve", "server replied %ld", reply.status);
	return ntohl(reply.status);
	
	END_API(CSSM)
}


//
// Start the server (fork/exec)
//
static void startServer()
{
	const char *serverpath = PORTSERVERPATH;
#if !defined(NDEBUG)
	if (const char *override = getenv("PRIVPORTSERVER"))
		serverpath = override;
#endif //!NDEBUG

	secdebug("portserve", "starting %s", serverpath);
	switch (fork()) {
	case -1:
		UnixError::throwMe();
	case 0:			// child
		execl(serverpath, serverpath, NULL);
		secdebug("portserve", "cannot exec %s (errno=%d)", serverpath, errno);
		_exit(1);
	default:		// parent
		sleep(1);
		break;
	}
}


//
// A pure-C interface helper function for overriding bind(2).
// This obtains the authorization, then calls AuthorizationBindPrivilegedPort
//
int __authorization_bind(int s, const struct sockaddr_in *name)
{
    // obtain authorization
    AuthorizationItem rights[] = {
        { "system.privilege.port.connect", 0, NULL, 0 }
    };
    AuthorizationRights rightSet = 
        { sizeof(rights) / sizeof(rights[0]), rights };
    AuthorizationRef auth;
    if (AuthorizationCreate(&rightSet, NULL,
        kAuthorizationFlagInteractionAllowed |
        kAuthorizationFlagExtendRights |
        kAuthorizationFlagPreAuthorize,
        &auth)) {
		errno = EPERM;
		return -1;
	}
	
	// bind
	OSStatus err = AuthorizationBindPrivilegedPort(s, name, auth, 0);
	AuthorizationFree(auth, 0);	// ignore errors

	if (err) {
		errno = (err >= errSecErrnoBase && err <= errSecErrnoLimit) ?
			errno = err - errSecErrnoBase :
			EPERM;
		return -1;
	}
	return 0;
}

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
// privportserver.cpp - tool-side trampoline support functions
//
#include <cstdlib>
#include <unistd.h>
#include <Security/Authorization.h>
#include <Security/unix++.h>
#include <Security/fdmover.h>
#include <Security/debugging.h>
#include "privPort.h"

using namespace UnixPlusPlus;
using namespace IPPlusPlus;


//
// Default values
//
#define DEFAULT_TIMEOUT	(2 * 60)	/* seconds */


//
// Local functions
//
void usage(const char *me);
void serve(FdMover client);
void sigALRM(int signo);


//
// Send a reply (successful or otherwise) to the client and clean up.
// 
void reply(Socket s, OSStatus error)
{
	Reply reply;
	reply.status = htonl(error);
	s.write(&reply, sizeof(reply));
	close(s);
}


//
// Server main program
//
int main(int argc, char *argv[])
{
	unsigned timeout = DEFAULT_TIMEOUT;

	extern int optind;
	extern char *optarg;
	int arg;
	while ((arg = getopt(argc, argv, "t:")) != -1)
		switch (arg) {
		case 't':
			timeout = atoi(optarg);
			break;
		case '?':
			usage(argv[0]);
		}
	if (optind < argc)
		usage(argv[0]);

	umask(0111);	// allow world-write on created socket
	Socket server(AF_UNIX, SOCK_STREAM);
	UNSockAddress serverAddress(kPrivilegedPortBinder);
	try {
		server.bind(serverAddress);
	} catch (const UnixError &error) {
		switch (error.error) {
		case EADDRINUSE:
			unlink(kPrivilegedPortBinder);
			server.bind(serverAddress);
			break;
		default:
			throw;
		}
	}
	server.listen(5);
	
	// prepare signals
	SigSet signals;
	signals += SIGALRM;
	sigMask(signals, SIG_BLOCK);
	if (signal(SIGALRM, sigALRM) == SIG_ERR) {
		perror("SIGALRM");
		exit(1);
	}
	
	// server loop
	for (;;) {
		FdMover s;
		alarm(timeout);
		sigMask(signals, SIG_UNBLOCK);
		server.accept(s);
		sigMask(signals, SIG_BLOCK);
		try {
			serve(s);
		} catch (const CssmCommonError &error) {
			reply(s, error.cssmError());
		} catch (...) {
			reply(s, -1);
		}
	} 

	// go away
	return 0;
}


void usage(const char *me)
{
	fprintf(stderr, "Usage: %s [-t timeout-seconds]\n", me);
	exit(2);
}


void sigALRM(int signo)
{
	secdebug("portserver", "timeout; quitting");
	unlink(kPrivilegedPortBinder);
	exit(0);
}


//
// Serve one request
void serve(FdMover client)
{
	secdebug("portserver", "processing port request");
	
	// receive request message
	Request request;
	FdVector fds;
	if (client.receive(&request, sizeof(request), fds) != sizeof(request)) {
		// short receive - assume sender protocol error
		return reply(client, 11111);
		return;
	}
	if (fds.size() != 1) {
		// must send exactly one socket to find
		return reply(client, 11111);
		return;
	}
	
	// internalize the authorization
	AuthorizationRef auth;
	if (OSStatus err = AuthorizationCreateFromExternalForm(&request.authForm, &auth))
		return reply(client, err);
	
	// validate authorization; punt if not authorized
	AuthorizationItem rights[] = {
		{ CONNECTRIGHT, 0, NULL, 0 },
		{ ACCEPTRIGHT, 0, NULL, 0 }
	};
	AuthorizationRights rightSet = { sizeof(rights) / sizeof(rights[0]), rights };
	AuthorizationRights *result;
	if (OSStatus err = AuthorizationCopyRights(auth, &rightSet, NULL,
		kAuthorizationFlagInteractionAllowed | kAuthorizationFlagExtendRights |
			kAuthorizationFlagPartialRights,
		&result))
		return reply(client, err);
	UInt32 count = result ? result->count : 0;
	AuthorizationFreeItemSet(result);
	if (count == 0)
		return reply(client, errAuthorizationDenied);
	
	// okay, bind as requested
	Socket s; s = fds[0];
	s.bind(request.requestedName);
	
	// all is well. Tell the client
	return reply(client, 0);
}

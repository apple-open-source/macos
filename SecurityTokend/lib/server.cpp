/*
 *  Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */
//
// server - master server loop for tokend
//
#include "server.h"
#include "tdclient.h"
#include <securityd_client/ssclient.h>
#include <security_cdsa_client/mdsclient.h>

using namespace MachPlusPlus;


//
// The MIG-generated server dispatch function
//
extern boolean_t tokend_server(mach_msg_header_t *in, mach_msg_header_t *out);


namespace Security {
namespace Tokend {


//
// The server singleton 
//
Server *server = NULL;


//
// Drive the server
//
int Server::operator () (int argc, const char *argv[], SecTokendCallbackFlags flags)
{
	// process command-line arguments as sent by securityd
	if (argc != 4) {
		secdebug("tokenlib", "invalid argument count (%d)", argc);
		Syslog::notice("token daemon invoked with invalid arguments");
		return 2;
	}

	// argv[1] == version of wire protocol
	const char *versionString = argv[1];
	if (atol(versionString) != TDPROTOVERSION) {
		// This would be a mismatch between securityd and the SecurityTokend.framework
		// It's NOT a "binary compatibility" mismatch; that got checked for by our caller.
		secdebug("tokenlib", "incoming protocol %s expected %d - aborting",
			versionString, TDPROTOVERSION);
		return 2;
	}

	// argv[2] == reader name
	mReaderName = argv[2];
	secdebug("tokenlib", "reader '%s' wire protocol %s", mReaderName, versionString);
	
	// argv[3] == hex coding of reader state, with name() undefined
	CssmData::wrap(mStartupReaderState).fromHex(argv[3]); 
	mStartupReaderState.name(readerName());	// fix name pointer (shipped separately)
	
#if !defined(NDEBUG)
	// stop right here; do not run the server. We're being run from the command line
	if (flags & kSecTokendCallbacksTestNoServer)
		return 0;
#endif
	
	// handshake with securityd
	secdebug("tokenlib", "checking in with securityd");
	SecurityServer::ClientSession client(Allocator::standard(), Allocator::standard());
	client.childCheckIn(primaryServicePort(), TaskPort());
	
	// start dispatch; from here on securityd is driving us
	secdebug("tokenlib", "starting server loop");
	run();
	
	// we don't usually return from run(), but just in case
	termination(0, 0);
}


//
// Handle the terminate message
//
void Server::termination(uint32 reason, uint32 options)
{
	secdebug("tokenlib", "terminate(%ld,0x%lx) received", reason, options);
	if (terminate)
		terminate(reason, options);	// ignore return code
	exit(0);
}


//
// MIG handler hook
//
boolean_t Server::handle(mach_msg_header_t *in, mach_msg_header_t *out)
{
	return tokend_server(in, out);
}


}	// namespace Tokend
}	// namespace Security

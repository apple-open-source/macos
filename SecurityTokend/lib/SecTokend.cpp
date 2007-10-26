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

#include "SecTokend.h"
#include "server.h"
#include <securityd_client/ssclient.h>


namespace Security {
namespace Tokend {


using namespace MachPlusPlus;


//
// Support callbacks provided for the implementation
//
static const SCARD_READERSTATE *cbStartupReaderInfo()
{
	return &server->startupReaderState();
}

static const char *cbTokenUid()
{
	return server->tokenUid();
}

static void *cbMalloc(uint32 size)
{
	return malloc(size);
}

static void cbFree(void *ptr)
{
	return free(ptr);
}

static void *cbMallocSensitive(uint32 size)
{
	return malloc(size);
}

static void cbFreeSensitive(void *ptr)
{
	return free(ptr);
}


//
// Vector of support functions passed to implementation
//
static SecTokendSupport supportVector = {
	cbStartupReaderInfo,
	cbTokenUid,
	cbMalloc, cbFree,
	cbMallocSensitive, cbFreeSensitive
};


extern "C" {


//
// The main driver function.
// This is called from the daemon's main() and takes over from there.
//
int SecTokendMain(int argc, const char * argv[],
	const SecTokendCallbacks *callbacks, SecTokendSupport *support)
{
	// first, check interface version and abort if we don't support it
	if (!callbacks) {
		secdebug("tokenlib", "NULL callback structure");
		exit(1);
	}
	if (callbacks->version != kSecTokendCallbackVersion) {
		secdebug("tokenlib", "callback structure is version %d (supporting %d)",
			callbacks->version, kSecTokendCallbackVersion);
		exit(1);
	}
	secdebug("tokenlib", "API interface version %d", callbacks->version);

	server = new Server();
	if (!server)
	{
		secdebug("tokenlib", "can't create server object");
		exit(1);
	}
	
	// set globals (we know by now that the version is okay)
	server->callbacks() = *callbacks;
	if (support)
		*support = supportVector;

	try {
		return (*server)(argc, argv, callbacks->flags);
	} catch (...) {
		secdebug("tokenlib", "server aborted with exception");
		return 1;
	}
}

}	// extern "C"


}	// namespace Tokend
}	// namespace Security

/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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


/*
 * YarrowServer - Yarrow RNG server object, OSX version
 */
#include "YarrowServer_OSX.h"
#include <mach/mach_error.h>
#include <sys/errno.h>
#include <stdio.h>				// debug
#include <yarrowMigTypes.h>
#include "MacYarrow_OSX.h"
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <Security/cssmalloc.h>

#define YS_DEBUG		0 
#if		YS_DEBUG
#define ysprintf(x)		printf x
#else
#define ysprintf(x)
#endif

using MachPlusPlus::MachServer;

/*
 * A timer for this module. Just one, and it's always active subsequent to startup.
 */
class YarrowTimer : public MachServer::Timer {
public:
	YarrowTimer(MachPlusPlus::MachServer &server) :
		MachServer::Timer(),
		mServer(server) {}
		
	void action();
	void scheduleTimer(unsigned msFromNow);
private:
	MachPlusPlus::MachServer	&mServer;		// to which we do setTimer()
};

/* 
 * Timeout event, the sole purpose of this class. Pass on to MacYarrow module.
 */
void YarrowTimer::action()
{
	unsigned nextTimeout = yarrowTimerEvent();
	scheduleTimer(nextTimeout);
}

void YarrowTimer::scheduleTimer(unsigned msFromNow)
{
    mServer.setTimer(this, Time::Interval(msFromNow / 1000.0));
}

/* global, one per process, therefore one per system */
static YarrowTimer 		*yarrowTimer;
static CssmAllocator	*cssmAlloc;		// for temp alloc in 
										//   yarrow_server_getRandomBytes()

YarrowServer::YarrowServer(MachPlusPlus::MachServer &globalServer, const char *entropyFilePath) : 
	MachServer(YARROW_SERVER_NAME)
{
#ifdef TEMPORARY_SEMANTICS
        MutexLocker ml (gYarrowMutex);
#endif
	unsigned firstTimeout;
	yarrowTimer = new YarrowTimer(globalServer);
	cssmAlloc = &CssmAllocator::standard();
	yarrowServerInit(entropyFilePath, &firstTimeout);
	yarrowTimer->scheduleTimer(firstTimeout);
}


/*
 * Clean up the server object
 */
YarrowServer::~YarrowServer()
{
	delete yarrowTimer;		// FIXME - is this safe? Does MachServer do this?
}

/*
 * Thread::run() specific to this class, needed because both MachServer
 * and Thread have a run() method. Called from main(). 
 */
void YarrowServer::runYarrow()
{
	Thread::run();
}
 
/*
 * Run the server. This will not return until the server is forced to exit.
 */
void YarrowServer::action() 
{
	ysprintf(("YarrowServer: running MachServer\n"));
	MachServer::run();
}


//
// The server run-loop function, called out from MachServer
//
boolean_t yarrow_server(mach_msg_header_t *, mach_msg_header_t *);

boolean_t YarrowServer::handle(mach_msg_header_t *in, mach_msg_header_t *out)
{
	return yarrow_server(in, out);
}


//
// Handling dead-port notifications
// FIXME - how is this used?
//
void YarrowServer::notifyDeadName(MachPlusPlus::Port port)
{
	// forcibly end the Connection
	// FIXME....endConnection(port, true);
}

/*
 * Functions called from server side of MIG interface. 
 * As far as I can tell, MIG doesn't generate
 * prototypes for the server side...FIXME....
 */
 
/* Bracket Macros */
#define UCSP_ARGS	mach_port_t sport, mach_port_t rport, OSStatus *rcode

kern_return_t 
yarrow_server_addEntropy(
	UCSP_ARGS,
	Data bytes,
	mach_msg_type_number_t bytesCnt, 
	UInt32 entBits)
{
	unsigned nextTimeout;
	ysprintf(("yarrow server addEntropy(%02X %02X %02X %02X...) called\n",
		((UInt8 *)bytes)[0], ((UInt8 *)bytes)[1], ((UInt8 *)bytes)[2],
		((UInt8 *)bytes)[3]));
	*rcode = yarrowAddEntropy(static_cast<UInt8 *>(bytes), bytesCnt, entBits,
		&nextTimeout);
	if(nextTimeout != 0) {
		yarrowTimer->scheduleTimer(nextTimeout);
	}
	return KERN_SUCCESS;
}


kern_return_t 
yarrow_server_getRandomBytes(
	UCSP_ARGS,
	UInt32 numBytes,					// in
	Data *bytes,						// out
	mach_msg_type_number_t *bytesCnt)	// out
{
	/*
	 * We have to allocate here; MIG does a virtual copy back to 
	 * client. MachServer releases later. 
	 */
	void *tempPtr;
	try {
		tempPtr = cssmAlloc->malloc(numBytes);
	}
	catch(...) {
		return unix_err(ENOMEM);
	}
	MachPlusPlus::MachServer::active().releaseWhenDone(*cssmAlloc, tempPtr);
	*rcode = yarrowGetRandomBytes(reinterpret_cast<UInt8 *>(tempPtr), numBytes);
	if(*rcode == noErr) {
		*bytes = reinterpret_cast<Data>(tempPtr);
		*bytesCnt = numBytes; 
	}
	else {
		*bytesCnt = 0;
	}
	ysprintf(("yarrow server getRandomBytes called; data %02X %02X %02X %02X...\n",
		((UInt8 *)*bytes)[0], ((UInt8 *)*bytes)[1], ((UInt8 *)*bytes)[2],
		((UInt8 *)*bytes)[3]));
	return KERN_SUCCESS;
}

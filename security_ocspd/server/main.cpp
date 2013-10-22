/*
 * Copyright (c) 2004-2011 Apple Inc. All Rights Reserved.
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

/*
 * main.cpp - main() for OCSP helper daemon
 */

#include <stdlib.h>
#include "ocspdServer.h"
#include <security_ocspd/ocspdTypes.h>
#include <security_ocspd/ocspdDebug.h>
#include <security_utilities/daemon.h>
#include <security_utilities/logging.h>
#include <CoreFoundation/CoreFoundation.h>

using namespace Security;

Mutex gTimeMutex;

const CFAbsoluteTime kTimeoutInterval = 300;
const int kTimeoutCheckTime = 60;

extern void enableAutoreleasePool(int enable);

static void usage(char **argv)
{
	printf("Usage: %s [option...]\n", argv[0]);
	printf("Options:\n");
	printf("  -d                  -- Debug mode, do not run as forked daemon\n");
	printf("  -n bootstrapName    -- specify alternate bootstrap name\n");
	exit(1);
}

void HandleSigTerm (int sig)
{
	exit (1);
}

CFAbsoluteTime gLastActivity;

void ServerActivity()
{
	StLock<Mutex> _mutexLock(gTimeMutex);
	gLastActivity = CFAbsoluteTimeGetCurrent();
}

class TimeoutTimer : public MachPlusPlus::MachServer::Timer
{
protected:
	OcspdServer &mServer;

public:
	TimeoutTimer(OcspdServer &server) : mServer(server) {}
	void action();
};


void TimeoutTimer::action()
{
	bool doExit = false;
	{
		StLock<Mutex> _mutexLock(gTimeMutex);
		CFAbsoluteTime thisTime = CFAbsoluteTimeGetCurrent();
		if (thisTime - gLastActivity > kTimeoutInterval)
		{
			doExit = true;
		}
	
		if (!doExit)
		{
			// reinstall us as a timer
			mServer.setTimer(this, Time::Interval(kTimeoutCheckTime));
		}
	}

	if (doExit)
	{
		exit(0);
	}
}

int main(int argc, char **argv)
{
	signal (SIGTERM, HandleSigTerm);
	enableAutoreleasePool(1);

	/* user-specified variables */
	const char *bootStrapName = NULL;
	bool debugMode = false;
	
	extern char *optarg;
	int arg;
	while ((arg = getopt(argc, argv, "dn:h")) != -1) {
		switch(arg) {
			case 'd':
				debugMode = true;
				break;
			case 'n':
				bootStrapName = optarg;
				break;
			case 'h':
			default:
				usage(argv);
		}
	}
	
	/* no non-option arguments */
	if (optind < argc) {
		usage(argv);
	}
	
	/* bootstrap name override for debugging only */
	#ifndef	NDEBUG
	if(bootStrapName == NULL) {
		bootStrapName = getenv(OCSPD_BOOTSTRAP_ENV);
	}
	#endif	/* NDEBUG */
	if(bootStrapName == NULL) {
		bootStrapName = OCSPD_BOOTSTRAP_NAME;
	}
	
    /* if we're not running as root in production mode, fail */
	#if defined(NDEBUG)
    if (uid_t uid = getuid()) {
        Syslog::alert("Tried to run ocspd as user %d: aborted", uid);
        fprintf(stderr, "You are not allowed to run securityd\n");
        exit(1);
    }
	#endif //NDEBUG

    /* turn into a properly diabolical daemon unless debugMode is on */
    if (!debugMode) {
		if (!Daemon::incarnate(false))
			exit(1);	// can't daemonize
	}

    // Declare the server here.  That way if something throws underneath its state wont
    // fall out of scope, taking the server global state with it.  That will let us shut
    // down more peacefully.
	OcspdServer server(bootStrapName);

	try {
		/* create the main server object and register it */

		/* FIXME - any signal handlers? */

		ocspdDebug("ocspd: starting main run loop");

		ServerActivity();
		TimeoutTimer tt(server);
		/* These options copied from securityd - they enable the audit trailer */
		server.setTimer(&tt, Time::Interval(kTimeoutCheckTime));

		server.run(4096,		// copied from machserver default
			MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0) |
			MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT));
	}
	catch(...) {}
	/* fell out of runloop (should not happen) */
	enableAutoreleasePool(0);
	#ifndef NDEBUG
	Syslog::alert("Aborting");
	#endif
	return 1;
}

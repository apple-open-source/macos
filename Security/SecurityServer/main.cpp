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


//
// SecurityServer - Apple security services daemon.
//
#include "securityserver.h"
#include "server.h"
#include <MacYarrow/yarrowseed.h>

#include <Security/daemon.h>
#include <Security/osxsigner.h>
#include "authority.h"
#include "session.h"

#include <unistd.h>
#include <Security/machserver.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

// ACL subject types (their makers are instantiated here)
#include <Security/acl_any.h>
#include <Security/acl_password.h>
#include <Security/acl_threshold.h>
#include <Security/acl_codesigning.h>
#include <Security/acl_comment.h>
#include "acl_keychain.h"


namespace Security
{

//
// Program options (set by argument scan and environment)
//
uint32 debugMode = 0;

} // end namespace Security


//
// Local functions of the main program driver
//
static void usage(const char *me);
static void handleSIGCHLD(int);
static void handleSIGOther(int);


//
// Main driver
//
int main(int argc, char *argv[])
{
	// program arguments (preset to defaults)
	bool forceCssmInit = false;
	int workerTimeout = 0;
	int maxThreads = 0;
	const char *authorizationConfig = "/etc/authorization";
	const char *bootstrapName = "SecurityServer";

	// parse command line arguments
	extern char *optarg;
	extern int optind;
	int arg;
	while ((arg = getopt(argc, argv, "a:dfN:t:T:")) != -1) {
		switch (arg) {
		case 'a':
			authorizationConfig = optarg;
			break;
		case 'd':
			debugMode++;
			break;
        case 'f':
            forceCssmInit = true;
            break;
		case 'N':
			bootstrapName = optarg;
			break;
		case 't':
			if ((maxThreads = atoi(optarg)) < 0)
				maxThreads = 0;
			break;
		case 'T':
			if ((workerTimeout = atoi(optarg)) < 0)
				workerTimeout = 0;
			break;
		default:
			usage(argv[0]);
		}
	}
	
	// take no non-option arguments
	if (optind < argc)
		usage(argv[0]);
		
	// configure logging
	if (debugMode) {
		Syslog::open(argv[0], LOG_AUTHPRIV, LOG_PERROR);
		Syslog::notice("SecurityServer started in debug mode");
	} else {
		Syslog::open(argv[0], LOG_AUTHPRIV, LOG_CONS);
	}
    
    // if we're not running as root in production mode, fail
    // in debug mode, issue a warning
    if (uid_t uid = getuid()) {
#if defined(NDEBUG)
        Syslog::alert("Unprivileged SecurityServer aborted (uid=%d)", uid);
        fprintf(stderr, "You are not allowed to run SecurityServer\n");
        exit(1);
#else
        debug("SS", "Running unprivileged (uid=%d); some features may not work", uid);
#endif //NDEBUG
    }
    
    // turn into a properly diabolical daemon unless debugMode is on
    if (!debugMode && !Daemon::incarnate())
        exit(1);
	
	// create a code signing engine
	CodeSigning::OSXSigner signer;
	
	// create an Authorization engine
	Authority authority(authorizationConfig);
	
	// establish the ACL machinery
	new AnyAclSubject::Maker();
	new PasswordAclSubject::Maker();
	new ThresholdAclSubject::Maker();
    new KeychainPromptAclSubject::Maker();
    new CommentAclSubject::Maker();
    new CodeSignatureAclSubject::Maker(signer);
    
    // create the RootSession object
    RootSession rootSession;
	
    // create the main server object and register it
 	Server server(authority, bootstrapName);

    // set server configuration from arguments, if specified
	if (workerTimeout)
		server.timeout(workerTimeout);
	if (maxThreads)
		server.maxThreads(maxThreads);
    
	// add the RNG seed timer to it
    YarrowTimer yarrow(server);
        
    // set up signal handlers
    if (signal(SIGCHLD, handleSIGCHLD) == SIG_ERR)
        debug("SS", "Cannot ignore SIGCHLD: errno=%d", errno);
    if (signal(SIGINT, handleSIGOther) == SIG_ERR)
        debug("SS", "Cannot handle SIGINT: errno=%d", errno);
    if (signal(SIGTERM, handleSIGOther) == SIG_ERR)
        debug("SS", "Cannot handle SIGTERM: errno=%d", errno);
    
    // initialize CSSM now if requested
    if (forceCssmInit)
        server.loadCssm();
    
	Syslog::notice("Entering service");
	debug("SS", "Entering service run loop");
	server.run();
	
	// fell out of runloop (should not happen)
	Syslog::alert("Aborting");
    return 1;
}


//
// Issue usage message and die
//
static void usage(const char *me)
{
	fprintf(stderr, "Usage: %s [-df] [-t maxthreads] [-T threadTimeout]"
		"\t[-N bootstrapName] [-a authConfigFile]\n", me);
	exit(2);
}


//
// Handle SIGCHLD signals to reap our children (zombie cleanup)
//
static void handleSIGCHLD(int)
{
    int status;
    switch (pid_t pid = waitpid(-1, &status, WNOHANG)) {
    case 0:
        debug("SS", "Spurious SIGCHLD ignored");
        return;
    case -1:
        debug("SS", "waitpid after SIGCHLD failed: errno=%d", errno);
        return;
    default:
        debug("SS", "Reaping child pid=%d", pid);
        return;
    }
}


//
// Handle some other signals to shut down cleanly (and with logging)
//
static void handleSIGOther(int sig)
{
    switch (sig) {
    case SIGINT:
        debug("SS", "Interrupt signal; terminating");
        exit(0);
    case SIGTERM:
        debug("SS", "Termination signal; terminating");
        exit(0);
    }
}

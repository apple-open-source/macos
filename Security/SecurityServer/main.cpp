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
#include "entropy.h"

#include <Security/daemon.h>
#include <Security/osxsigner.h>
#include "authority.h"
#include "session.h"

#include <unistd.h>
#include <Security/machserver.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

#include "ktracecodes.h"

// ACL subject types (their makers are instantiated here)
#include <Security/acl_any.h>
#include <Security/acl_password.h>
#include <Security/acl_protectedpw.h>
#include <Security/acl_threshold.h>
#include <Security/acl_codesigning.h>
#include <Security/acl_process.h>
#include <Security/acl_comment.h>
#include "acl_keychain.h"


namespace Security
{

//
// Program options (set by argument scan and environment)
//
uint32 debugMode = 0;
const char *bootstrapName = NULL;

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
    Debug::trace (kSecTraceSecurityServerStart);
    
	// program arguments (preset to defaults)
	bool forceCssmInit = false;
	bool reExecute = false;
	int workerTimeout = 0;
	int maxThreads = 0;
	const char *authorizationConfig = "/etc/authorization";
    const char *entropyFile = "/var/db/SystemEntropyCache";
	const char *equivDbFile = EQUIVALENCEDBPATH;

	// parse command line arguments
	extern char *optarg;
	extern int optind;
	int arg;
	while ((arg = getopt(argc, argv, "a:de:E:fN:t:T:X")) != -1) {
		switch (arg) {
		case 'a':
			authorizationConfig = optarg;
			break;
		case 'd':
			debugMode++;
			break;
		case 'e':
			equivDbFile = optarg;
			break;
        case 'E':
            entropyFile = optarg;
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
		case 'X':
			reExecute = true;
			break;
		default:
			usage(argv[0]);
		}
	}
	
	// take no non-option arguments
	if (optind < argc)
		usage(argv[0]);
	
	// figure out the bootstrap name
    IFDEBUG(if (!bootstrapName) bootstrapName = getenv(SECURITYSERVER_BOOTSTRAP_ENV));

	if (!bootstrapName) {
		bootstrapName = SECURITYSERVER_BOOTSTRAP_NAME;
	}
		
	// configure logging first
	if (debugMode) {
		Syslog::open(bootstrapName, LOG_AUTHPRIV, LOG_PERROR);
		Syslog::notice("SecurityServer started in debug mode");
	} else {
		Syslog::open(bootstrapName, LOG_AUTHPRIV, LOG_CONS);
	}
    
    // if we're not running as root in production mode, fail
    // in debug mode, issue a warning
    if (uid_t uid = getuid()) {
#if defined(NDEBUG)
        Syslog::alert("Tried to run SecurityServer as user %d: aborted", uid);
        fprintf(stderr, "You are not allowed to run SecurityServer\n");
        exit(1);
#else
        fprintf(stderr, "SecurityServer is unprivileged; some features may not work.\n");
        secdebug("SS", "Running as user %d (you have been warned)", uid);
#endif //NDEBUG
    }
    
    // turn into a properly diabolical daemon unless debugMode is on
    if (!debugMode) {
		if (!Daemon::incarnate())
			exit(1);	// can't daemonize
		
		if (reExecute && !Daemon::executeSelf(argv))
			exit(1);	// can't self-execute
	}
	
	// create a code signing engine
	CodeSigning::OSXSigner signer;
	
	// create an Authorization engine
	Authority authority(authorizationConfig);
	
	// establish the ACL machinery
	new AnyAclSubject::Maker();
	new PasswordAclSubject::Maker();
    new ProtectedPasswordAclSubject::Maker();
	new ThresholdAclSubject::Maker();
    new CommentAclSubject::Maker();
 	new ProcessAclSubject::Maker();
	new CodeSignatureAclSubject::Maker(signer);
    new KeychainPromptAclSubject::Maker();
	
	// add a temporary registration for a subject type that went out in 10.2 seed 1
	// this should probably be removed for the next major release >10.2
	new KeychainPromptAclSubject::Maker(CSSM_WORDID__RESERVED_1);
	
	// establish the code equivalents database
	CodeSignatures codeSignatures(equivDbFile);
	
    // create the main server object and register it
 	Server server(authority, codeSignatures, bootstrapName);

    // set server configuration from arguments, if specified
	if (workerTimeout)
		server.timeout(workerTimeout);
	if (maxThreads)
		server.maxThreads(maxThreads);
    
	// add the RNG seed timer to it
# if defined(NDEBUG)
    EntropyManager entropy(server, entropyFile);
# else
    if (!getuid()) new EntropyManager(server, entropyFile);
# endif
    
    // create the RootSession object (if -d, give it graphics and tty attributes)
    RootSession rootSession(server.primaryServicePort(),
		debugMode ? (sessionHasGraphicAccess | sessionHasTTY) : 0);
        
    // set up signal handlers
    if (signal(SIGCHLD, handleSIGCHLD) == SIG_ERR)
        secdebug("SS", "Cannot ignore SIGCHLD: errno=%d", errno);
    if (signal(SIGINT, handleSIGOther) == SIG_ERR)
        secdebug("SS", "Cannot handle SIGINT: errno=%d", errno);
    if (signal(SIGTERM, handleSIGOther) == SIG_ERR)
        secdebug("SS", "Cannot handle SIGTERM: errno=%d", errno);
    
    // initialize CSSM now if requested
    if (forceCssmInit)
        server.loadCssm();
    
	Syslog::notice("Entering service");
	secdebug("SS", "%s initialized", bootstrapName);

    Debug::trace (kSecTraceSecurityServerStart);
    
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
	pid_t pid = waitpid(-1, &status, WNOHANG);
    switch (pid) {
    case 0:
        //secdebug("SS", "Spurious SIGCHLD ignored");
        return;
    case -1:
        //secdebug("SS", "waitpid after SIGCHLD failed: errno=%d", errno);
        return;
    default:
        //secdebug("SS", "Reaping child pid=%d", pid);
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
        //secdebug("SS", "Interrupt signal; terminating");
        Syslog::notice("received interrupt signal; terminating");
        exit(0);
    case SIGTERM:
        //secdebug("SS", "Termination signal; terminating");
        Syslog::notice("received termination signal; terminating");
        exit(0);
    }
}

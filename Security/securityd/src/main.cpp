/*
 * Copyright (c) 2000-2009,2014 Apple Inc. All Rights Reserved.
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
// securityd - Apple security services daemon.
//
#include <securityd_client/ucsp.h>

#include "server.h"
#include "session.h"
#include "notifications.h"
#include "pcscmonitor.h"
#include "auditevents.h"
#include "self.h"

#include <security_utilities/daemon.h>
#include <security_utilities/machserver.h>
#include <security_utilities/logging.h>

#include <Security/SecKeychainPriv.h>

#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <syslog.h>

// ACL subject types (their makers are instantiated here)
#include <security_cdsa_utilities/acl_any.h>
#include <security_cdsa_utilities/acl_password.h>
#include <security_cdsa_utilities/acl_prompted.h>
#include <security_cdsa_utilities/acl_protectedpw.h>
#include <security_cdsa_utilities/acl_threshold.h>
#include <security_cdsa_utilities/acl_codesigning.h>
#include <security_cdsa_utilities/acl_process.h>
#include <security_cdsa_utilities/acl_comment.h>
#include <security_cdsa_utilities/acl_preauth.h>
#include "acl_keychain.h"
#include "acl_partition.h"


//
// Local functions of the main program driver
//
static void usage(const char *me) __attribute__((noreturn));
static void handleSignals(int sig);
static PCSCMonitor::ServiceLevel scOptions(const char *optionString);


static Port gMainServerPort;
PCSCMonitor *gPCSC;


//
// Main driver
//
int main(int argc, char *argv[])
{
	// clear the umask - we know what we're doing
	secnotice("SS", "starting umask was 0%o", ::umask(0));
	::umask(0);

	// tell the keychain (client) layer to turn off the server interface
	SecKeychainSetServerMode();
	
	// program arguments (preset to defaults)
	bool debugMode = false;
	const char *bootstrapName = NULL;
	const char* messagingName = SharedMemoryCommon::kDefaultSecurityMessagesName;
	bool doFork = false;
	bool reExecute = false;
	int workerTimeout = 0;
	int maxThreads = 0;
	bool waitForClients = true;
    bool mdsIsInstalled = false;
	const char *tokenCacheDir = "/var/db/TokenCache";
	const char *smartCardOptions = getenv("SMARTCARDS");
	uint32_t keychainAclDefault = CSSM_ACL_KEYCHAIN_PROMPT_INVALID | CSSM_ACL_KEYCHAIN_PROMPT_UNSIGNED;
	unsigned int verbose = 0;
	
	// check for the Installation-DVD environment and modify some default arguments if found
	if (access("/etc/rc.cdrom", F_OK) == 0) {	// /etc/rc.cdrom exists
        secnotice("SS", "starting in installmode");
		smartCardOptions = "off";	// needs writable directories that aren't
	}

	// parse command line arguments
	extern char *optarg;
	extern int optind;
	int arg;
	while ((arg = getopt(argc, argv, "c:dE:imN:s:t:T:uvWX")) != -1) {
		switch (arg) {
		case 'c':
			tokenCacheDir = optarg;
			break;
		case 'd':
			debugMode = true;
			break;
        case 'E':
            /* was entropyFile, kept to preserve ABI */
            break;
		case 'i':
			keychainAclDefault &= ~CSSM_ACL_KEYCHAIN_PROMPT_INVALID;
			break;
        case 'm':
            mdsIsInstalled = true;
            break;
		case 'N':
			bootstrapName = optarg;
			break;
		case 's':
			smartCardOptions = optarg;
			break;
		case 't':
			if ((maxThreads = atoi(optarg)) < 0)
				maxThreads = 0;
			break;
		case 'T':
			if ((workerTimeout = atoi(optarg)) < 0)
				workerTimeout = 0;
			break;
		case 'W':
			waitForClients = false;
			break;
		case 'u':
			keychainAclDefault &= ~CSSM_ACL_KEYCHAIN_PROMPT_UNSIGNED;
			break;
		case 'v':
			verbose++;
			break;
		case 'X':
			doFork = true;
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
    if (!bootstrapName) {
		bootstrapName = getenv(SECURITYSERVER_BOOTSTRAP_ENV);
		if (!bootstrapName)
		{
			bootstrapName = SECURITYSERVER_BOOTSTRAP_NAME;
		}
		else
		{
			messagingName = bootstrapName;
		}
	}
	else
	{
		messagingName = bootstrapName;
	}
	
	// configure logging first
	if (debugMode) {
		Syslog::open(bootstrapName, LOG_AUTHPRIV, LOG_PERROR);
		Syslog::notice("%s started in debug mode", argv[0]);
	} else {
		Syslog::open(bootstrapName, LOG_AUTHPRIV, LOG_CONS);
	}
    
    // if we're not running as root in production mode, fail
    // in debug mode, issue a warning
    if (uid_t uid = getuid()) {
#if defined(NDEBUG)
        Syslog::alert("Tried to run securityd as user %d: aborted", uid);
        fprintf(stderr, "You are not allowed to run securityd\n");
        exit(1);
#else
        fprintf(stderr, "securityd is unprivileged (uid=%d); some features may not work.\n", uid);
#endif //NDEBUG
    }
    
    // turn into a properly diabolical daemon unless debugMode is on
    if (!debugMode && getppid() != 1) {
		if (!Daemon::incarnate(doFork))
			exit(1);	// can't daemonize
		
		if (reExecute && !Daemon::executeSelf(argv))
			exit(1);	// can't self-execute
	}
        
    // arm signal handlers; code below may generate signals we want to see
    if (signal(SIGCHLD, handleSignals) == SIG_ERR
		|| signal(SIGINT, handleSignals) == SIG_ERR
		|| signal(SIGTERM, handleSignals) == SIG_ERR
		|| signal(SIGPIPE, handleSignals) == SIG_ERR
#if !defined(NDEBUG)
		|| signal(SIGUSR1, handleSignals) == SIG_ERR
#endif //NDEBUG
		|| signal(SIGUSR2, handleSignals) == SIG_ERR) {
		perror("signal");
		exit(1);
	}

	// introduce all supported ACL subject types
	new AnyAclSubject::Maker();
	new PasswordAclSubject::Maker();
    new ProtectedPasswordAclSubject::Maker();
    new PromptedAclSubject::Maker();
	new ThresholdAclSubject::Maker();
	new CommentAclSubject::Maker();
 	new ProcessAclSubject::Maker();
	new CodeSignatureAclSubject::Maker();
	new KeychainPromptAclSubject::Maker(keychainAclDefault);
	new PartitionAclSubject::Maker();
	new PreAuthorizationAcls::OriginMaker();
    new PreAuthorizationAcls::SourceMaker();

    // establish the code equivalents database
    CodeSignatures codeSignatures;

    // create the main server object and register it
 	Server server(codeSignatures, bootstrapName);

    // Remember the primary service port to send signal events to
    gMainServerPort = server.primaryServicePort();

    // set server configuration from arguments, if specified
	if (workerTimeout)
		server.timeout(workerTimeout);
	if (maxThreads)
		server.maxThreads(maxThreads);
	server.floatingThread(true);
	server.waitForClients(waitForClients);
	server.verbosity(verbose);
    
	// create a smartcard monitor to manage external token devices
	gPCSC = new PCSCMonitor(server, tokenCacheDir, scOptions(smartCardOptions));
    
    // create the RootSession object (if -d, give it graphics and tty attributes)
    RootSession rootSession(debugMode ? (sessionHasGraphicAccess | sessionHasTTY) : 0, server);
	
	// create a monitor thread to watch for audit session events
	AuditMonitor audits(gMainServerPort);
	audits.run();
    
    // install MDS (if needed) and initialize the local CSSM
    server.loadCssm(mdsIsInstalled);
    
	// create the shared memory notification hub
#ifndef __clang_analyzer__
	new SharedMemoryListener(messagingName, kSharedMemoryPoolSize);
#endif // __clang_analyzer__

	// okay, we're ready to roll
    secnotice("SS", "Entering service as %s", (char*)bootstrapName);
	Syslog::notice("Entering service");
    
	// go
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
	fprintf(stderr, "Usage: %s [-dwX]"
		"\n\t[-c tokencache]                        smartcard token cache directory"
		"\n\t[-e equivDatabase] 					path to code equivalence database"
		"\n\t[-N serviceName]                       MACH service name"
		"\n\t[-s off|on|conservative|aggressive]    smartcard operation level"
		"\n\t[-t maxthreads] [-T threadTimeout]     server thread control"
		"\n", me);
	exit(2);
}


//
// Translate strings (e.g. "conservative") into PCSCMonitor service levels
//
static PCSCMonitor::ServiceLevel scOptions(const char *optionString)
{
	if (optionString)
		if (!strcmp(optionString, "off"))
			return PCSCMonitor::forcedOff;
		else if (!strcmp(optionString, "on"))
			return PCSCMonitor::externalDaemon;
		else if (!strcmp(optionString, "conservative"))
			return PCSCMonitor::externalDaemon;
		else if (!strcmp(optionString, "aggressive"))
			return PCSCMonitor::externalDaemon;
		else if (!strcmp(optionString, "external"))
			return PCSCMonitor::externalDaemon;
		else
			usage("securityd");
	else
		return PCSCMonitor::externalDaemon;
}


//
// Handle signals.
// We send ourselves a message (through the "self" service), so actual
// actions happen on the normal event loop path. Note that another thread
// may be picking up the message immediately.
//
static void handleSignals(int sig)
{
    secnotice("SS", "signal received: %d", sig);
	if (kern_return_t rc = self_client_handleSignal(gMainServerPort, mach_task_self(), sig))
		Syslog::error("self-send failed (mach error %d)", rc);
}

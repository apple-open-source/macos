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
// SecurityAgentClient - client interface to SecurityAgent
//
// This file changes behavior depending on two environment variables.
// AGENTNAME/AGENTPATH: if defined, is the name and path to
// the SecurityAgent binary to autolaunch. If undefined, SecurityAgent must be running.
// NOSA: If set, check for NOSA environment variable and if set, simulate the Agent
//  using stdio in the client.
//
// A note on message flow: the combined send/receive operation at the heart of each
// secagent_client_* call can receive three types of message:
// (o) SecurityAgent reply -- ok, process
// (o) Dead port notification -- agent died, translated to NO_USER_INTERACTION error thrown
// (o) Cancel message -- will come out as INVALID_ID error and throw
//
// @@@ SA keepalive option.
//
#include "SecurityAgentClient.h"
#include "secagent.h"
#include <Security/utilities.h>
#include <Security/Authorization.h>
#include <cstdio>
#include <unistd.h>
#include <mach/mach_error.h>
#include <mach/ndr.h>
#include <Security/debugging.h>
#include <sys/wait.h>
#include <sys/syslimits.h>
#include <time.h>
#include <signal.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <Security/ktracecodes.h>

#include <sys/types.h>
#include <grp.h>

// @@@ Should be in <time.h> but it isn't as of Puma5F22
extern "C" int nanosleep(const struct timespec *rqtp, struct timespec *rmtp);

namespace Security {
namespace SecurityAgent {


using namespace Security;
using namespace MachPlusPlus;


// pass structured arguments in/out of IPC calls. See "data walkers" for details
#define COPY(copy)			copy, copy.length(), copy
#define COPY_OUT(copy)		&copy, &copy##Length, &copy##Base
#define COPY_OUT_DECL(type,name) type *name, *name##Base; mach_msg_type_number_t name##Length


//
// Encode a requestor
//
class Requestor {
public:
	Requestor(const OSXCode *code)	{ if (code) extForm = code->encode(); }
	operator const char * () const	{ return extForm.c_str(); }

	// use this for debugging only
	const char *c_str() const		{ return extForm.empty() ? "(unknown)" : extForm.c_str(); }

private:
	string extForm;
};


//
// Check a return from a MIG client call
//
void Client::check(kern_return_t error)
{
	// first check the Mach IPC return code
	switch (error) {
	case KERN_SUCCESS:				// peachy
		break;
	case MIG_SERVER_DIED:			// explicit can't-send-it's-dead
		stage = mainStage;
		CssmError::throwMe(CSSM_ERRCODE_NO_USER_INTERACTION);
	default:						// some random Mach error
		stage = mainStage;
		MachPlusPlus::Error::throwMe(error);
	}
	
	// now check the OSStatus return from the server side
	switch (status) {
	case noErr:
	case errAuthorizationDenied:
		break;
	case userCanceledErr:
		unstage();
		CssmError::throwMe(CSSM_ERRCODE_USER_CANCELED);
	default:
		unstage();
		MacOSError::throwMe(status);
	}
}

void Client::unstage()
{
	if (stage != mainStage) {
		mStagePort.deallocate();
		stage = mainStage;
	}
}


//
// NOSA support functions. This is a test mode where the SecurityAgent
// is simulated via stdio in the client. Good for running automated tests
// of client programs. Only available if -DNOSA when compiling.
//
#if defined(NOSA)

#include <cstdarg>

static void getNoSA(char *buffer, size_t bufferSize, const char *fmt, ...)
{
	// write prompt
	va_list args;
	va_start(args, fmt);
	vfprintf(stdout, fmt, args);
	va_end(args);

	// read reply
	memset(buffer, 0, bufferSize);
	const char *nosa = getenv("NOSA");
	if (!strcmp(nosa, "-")) {
		if (fgets(buffer, bufferSize-1, stdin) == NULL)
			CssmError::throwMe(CSSM_ERRCODE_NO_USER_INTERACTION);
		buffer[strlen(buffer)-1] = '\0';	// remove trailing newline
		if (!isatty(fileno(stdin)))
			printf("%s\n", buffer);			// echo to output if input not terminal
	} else {
		strncpy(buffer, nosa, bufferSize-1);
		printf("%s\n", buffer);
	}
	if (buffer[0] == '\0')				// empty input -> cancellation
		CssmError::throwMe(CSSM_ERRCODE_USER_CANCELED);
}

#endif //NOSA


//
// Initialize our CSSM interface
//
Client::Client() : mActive(false), desktopUid(0), mKeepAlive(false), stage(mainStage), mAgentName("com.apple.SecurityAgent")
{
}

/*
 * The new, preferred way to activate the Security Agent.  The Security
 * Server will take advantage of this interface; the old constructor is
 * kept around for compatibility with the only other client, DiskCopy.
 * DiskCopy needs to be fixed to use the Security Server itself rather
 * than this library.
 */
Client::Client(uid_t clientUID, Bootstrap clientBootstrap, const char *agentName) :
    mActive(false), desktopUid(clientUID),
    mClientBootstrap(clientBootstrap), mKeepAlive(false), stage(mainStage), mAgentName(agentName)
{
}

Client::~Client()
{
	terminate();
}


//
// Activate a session
//
void Client::activate()
{
	if (!mActive) {

		establishServer();
		
		// create reply port
		mClientPort.allocate(MACH_PORT_RIGHT_RECEIVE);
		mClientPort.insertRight(MACH_MSG_TYPE_MAKE_SEND);
		
		// get notified if the server dies (shouldn't happen, but...)
		mServerPort.requestNotify(mClientPort, MACH_NOTIFY_DEAD_NAME, true);
		
		// ready
		mActive = true;
	}
}

void Client::terminate()
{
	if (mActive) {
		mServerPort.deallocate();
		mClientPort.destroy();
		mActive = false;
	}
}


//
// Cancel a client call.
// This actually sends a reply message to the thread waiting for a reply,
// thereby unblocking it.
// @@@ Theoretically we should thread-lock this so only one cancel message
// ever gets sent. But right now, this is only used to completely tear down
// a client session, so duplicate replies don't bother us.
//
void Client::cancel()
{
	// this is the common prefix of SecurityAgent client call replies
	struct {
			mach_msg_header_t Head;
			NDR_record_t NDR;
			kern_return_t result;
			OSStatus status;
	} request;

	request.Head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MAKE_SEND, 0);
	request.Head.msgh_remote_port = mClientPort;
	request.Head.msgh_local_port = MACH_PORT_NULL;
	request.Head.msgh_id = cancelMessagePseudoID;
	request.NDR = NDR_record;
	
	// set call succeeded, no error status
	request.result = KERN_SUCCESS;
	request.status = noErr;

	// send it (do not receive a reply). Use zero timeout to avoid hangs
	MachPlusPlus::check(mach_msg_overwrite(&request.Head, MACH_SEND_MSG|MACH_SEND_TIMEOUT, 
		sizeof(request), 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, 
		MACH_PORT_NULL, (mach_msg_header_t *) NULL, 0));
}


//
// Get the port for the SecurityAgent.
// Start it if necessary (and possible). Throw an exception if we can't get to it.
// Sets mServerPort on success.
//
void Client::establishServer()
{
    // if the server is already running, we're done
	if (mServerPort = mClientBootstrap.lookupOptional(mAgentName.c_str()))
		return;
    
#if defined(AGENTNAME) && defined(AGENTPATH)
    // switch the bootstrap port to that of the logged-in user
	
    StBootstrap bootSaver(mClientBootstrap);

    // try to start the agent
    switch (pid_t pid = fork()) {
    case 0:		// child
	{
		// Setup the environment for the SecurityAgent
		unsetenv("USER");
		unsetenv("LOGNAME");
		unsetenv("HOME");

		// tell agent which name to register
		setenv("AGENTNAME", mAgentName.c_str(), 1);

		if (desktopUid) // if the user is running as root, or we're not told what uid to use, we stick with what we are
		{
			struct group *grent = getgrnam("nobody");
			gid_t desktopGid = grent ? grent->gr_gid : unsigned(-2);	//@@@ questionable
			endgrent();
			secdebug("SAclnt", "setgid(%d)", desktopGid);
			setgid(desktopGid);	// switch to login-user gid
			secdebug("SAclnt", "setuid(%d)", desktopUid);
			// Must be setuid and not seteuid since we do not want the agent to be able
			// to call seteuid(0) successfully.
			setuid(desktopUid);	// switch to login-user uid
		}
        // close down any files that might have been open at this point
        int maxDescriptors = getdtablesize ();
        int i;
        
        for (i = 3; i < maxDescriptors; ++i)
        {
            close (i);
        }
        
        // construct path to SecurityAgent
        char agentExecutable[PATH_MAX + 1];
        const char *path = getenv("SECURITYAGENT");
        if (!path)
            path = AGENTPATH;
        snprintf(agentExecutable, sizeof(agentExecutable), "%s/Contents/MacOS/" AGENTNAME, path);
        secdebug("SAclnt", "execl(%s)", agentExecutable);
        execl(agentExecutable, agentExecutable, NULL);
		secdebug("SAclnt", "execl of SecurityAgent failed, errno=%d", errno);

        // Unconditional suicide follows.
		// See comments below on why we can't use abort()
#if 1
		_exit(1);
#else
        // NOTE: OS X abort() is implemented as kill(getuid()), which fails
        // for a setuid-root process that has setuid'd. Go back to root to die...
        setuid(0);
        abort();
#endif
	}
    case -1:	// error (in parent)
        UnixError::throwMe();
    default:	// parent
        {
			static const int timeout = 300;

            secdebug("SAclnt", "Starting security agent (%d seconds timeout)", timeout);
			struct timespec rqtp;
			memset(&rqtp, 0, sizeof(rqtp));
			rqtp.tv_nsec = 100000000; /* 10^8 nanaseconds = 1/10th of a second */
            for (int n = timeout; n > 0; nanosleep(&rqtp, NULL), n--) {
                if (mServerPort = mClientBootstrap.lookupOptional(mAgentName.c_str()))
                    break;
                int status;
                switch (IFDEBUG(pid_t rc =) waitpid(pid, &status, WNOHANG)) {
                case 0:	// child still running
                    continue;
                case -1:	// error
                    switch (errno) {
                    case EINTR:
                    case EAGAIN:	// transient
                        continue;
                    case ECHILD:	// no such child (dead; already reaped elsewhere)
                        secdebug("SAclnt", "child is dead (reaped elsewhere)");
                        CssmError::throwMe(CSSM_ERRCODE_NO_USER_INTERACTION);
                    default:
                        secdebug("SAclnt", "waitpid failed: errno=%d", errno);
                        UnixError::throwMe();
                    }
                default:
                    assert(rc == pid);
                    secdebug("SAclnt", "child died without claiming the SecurityAgent port");
                    CssmError::throwMe(CSSM_ERRCODE_NO_USER_INTERACTION);
                }
            }

            if (mServerPort == 0) {		// couldn't contact Security Agent
				secdebug("SAclnt", "Autolaunch failed");
                CssmError::throwMe(CSSM_ERRCODE_NO_USER_INTERACTION);
			}
            secdebug("SAclnt", "SecurityAgent located");
            return;
        }
    }
#endif

    // well, this didn't work. Too bad
	secdebug("SAclnt", "Cannot contact SecurityAgent");
    CssmError::throwMe(CSSM_ERRCODE_NO_USER_INTERACTION);	//@@@ or INTERNAL_ERROR?
}


//
// Staged query maintainance
//
void Client::finishStagedQuery()
{
	if (stage == mainStage)
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR); //@@@ invent a "state mismatch error"?
#if defined(NOSA)
	if (getenv("NOSA")) {
		printf(" [query done]\n");
		stage = mainStage;
		return;
	}
#endif		
	check(secagent_client_finishStagedQuery(mStagePort, mClientPort, &status));
	unstage();
    terminate();
}

void Client::cancelStagedQuery(Reason reason)
{
	if (stage == mainStage)
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR); //@@@ invent a "state mismatch error"?
#if defined(NOSA)
	if (getenv("NOSA")) {
		printf(" [query canceled; reason=%d]\n", reason);
		stage = mainStage;
		return;
	}
#endif
	check(secagent_client_cancelStagedQuery(mStagePort, mClientPort, &status, reason));
	unstage();
    terminate();
}


//
// Query the user to unlock a keychain. This is a staged protocol with a private side-port.
//
void Client::queryUnlockDatabase(const OSXCode *requestor, pid_t requestPid,
    const char *database, char passphrase[maxPassphraseLength])
{
	Requestor req(requestor);
	
#if defined(NOSA)
	if (getenv("NOSA")) {
		getNoSA(passphrase, maxPassphraseLength, "Unlock %s [<CR> to cancel]: ", database);
		stage = unlockStage;
		return;
	}
#endif
	activate();
	check(secagent_client_unlockDatabase(mServerPort, mClientPort,
		&status, req, requestPid, database, &mStagePort.port(), passphrase));
	stage = unlockStage;
}

void Client::retryUnlockDatabase(Reason reason, char passphrase[maxPassphraseLength])
{
	if (stage != unlockStage)
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);	//@@@ invent a "state mismatch error"?
#if defined(NOSA)
	if (getenv("NOSA")) {
		getNoSA(passphrase, maxPassphraseLength, "Retry unlock [<CR> to cancel]: ");
		return;
	}
#endif
	check(secagent_client_retryUnlockDatabase(mStagePort, mClientPort,
		&status, reason, passphrase));
}


//
// Ask for a (new) password for something.
//
void Client::queryNewPassphrase(const OSXCode *requestor, pid_t requestPid,
    const char *database, Reason reason, char passphrase[maxPassphraseLength], char oldPassphrase[maxPassphraseLength])
{
	Requestor req(requestor);
	
#if defined(NOSA)
	if (getenv("NOSA")) {
		getNoSA(passphrase, maxPassphraseLength,
			"New passphrase for %s (reason %d) [<CR> to cancel]: ",
			(database ? database : "[NULL database]"), reason);
		stage = newPassphraseStage;
		return;
	}
#endif
	activate();
	check(secagent_client_queryNewPassphrase(mServerPort, mClientPort,
		&status, req, requestPid, database, reason,
        &mStagePort.port(), passphrase, oldPassphrase));
	stage = newPassphraseStage;
}

void Client::retryNewPassphrase(Reason reason, char passphrase[maxPassphraseLength], char oldPassphrase[maxPassphraseLength])
{
	if (stage != newPassphraseStage)
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);	//@@@ invent a "state mismatch error"?
#if defined(NOSA)
	if (getenv("NOSA")) {
		getNoSA(passphrase, maxPassphraseLength,
			"retry new passphrase (reason %d) [<CR> to cancel]: ", reason);
		return;
	}
#endif
	check(secagent_client_retryNewPassphrase(mStagePort, mClientPort,
		&status, reason, passphrase, oldPassphrase));
}


//
// Ask the user permission to use an item.
// This is used by the keychain-style ACL subject type (only).
//
void Client::queryKeychainAccess(const OSXCode *requestor, pid_t requestPid,
	const char *database, const char *itemName, AclAuthorization action,
	bool needPassphrase, KeychainChoice &choice)
{
    Debug::trace (kSecTraceSecurityServerQueryKeychainAccess);
    
	Requestor req(requestor);

#if defined(NOSA)
	if (getenv("NOSA")) {
		char answer[maxPassphraseLength+10];
		getNoSA(answer, sizeof(answer), "Allow %s to do %d on %s in %s? [yn][g]%s ",
			req.c_str(), int(action), (itemName ? itemName : "[NULL item]"),
			(database ? database : "[NULL database]"),
			needPassphrase ? ":passphrase" : "");
		// turn passphrase (no ':') into y:passphrase
		if (needPassphrase && !strchr(answer, ':')) {
			memmove(answer+2, answer, strlen(answer)+1);
			memcpy(answer, "y:", 2);
		}
		choice.allowAccess = answer[0] == 'y';
		choice.continueGrantingToCaller = answer[1] == 'g';
		if (const char *colon = strchr(answer, ':'))
			strncpy(choice.passphrase, colon+1, maxPassphraseLength);
		else
			choice.passphrase[0] = '\0';
		return;
	}
#endif
	activate();
	check(secagent_client_queryKeychainAccess(mServerPort, mClientPort,
		&status, req, requestPid, (database ? database : ""), itemName, action, 
		needPassphrase, &mStagePort.port(), &choice));
    
    stage = queryKeychainAccessStage;
}


void Client::retryQueryKeychainAccess (Reason reason, Choice &choice)
{
	if (stage != queryKeychainAccessStage)
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);	//@@@ invent a "state mismatch error"?

    check(secagent_client_retryQueryKeychainAccess (mStagePort, mClientPort, &status, reason, &choice));
}



//
// Ask the user whether a somewhat (but not cleanly) matching code identity
// should be accepted for access control purposes.
//
void Client::queryCodeIdentity(const OSXCode *requestor, pid_t requestPid,
	const char *aclPath, KeychainChoice &choice)
{
	Requestor req(requestor);
	
#if defined(NOSA)
	if (getenv("NOSA")) {
		char answer[10];
		getNoSA(answer, sizeof(answer),
			"Allow %s to match an ACL for %s [yn][g]? ",
			req.c_str(), aclPath ? aclPath : "(unknown)");
		choice.allowAccess = answer[0] == 'y';
		choice.continueGrantingToCaller = answer[1] == 'g';
		return;
	}
#endif
	activate();
	check(secagent_client_queryCodeIdentity(mServerPort, mClientPort,
		&status, req, requestPid, aclPath, &choice));
	terminate();
}


//
// Query the user for a generic existing passphrase, with selectable prompt.
//
void Client::queryOldGenericPassphrase(const OSXCode *requestor, pid_t requestPid,
    const char *prompt,
    KeychainBox &addToKeychain, char passphrase[maxPassphraseLength])
{
	Requestor req(requestor);
	
#if defined(NOSA)
	if (getenv("NOSA")) {
		getNoSA(passphrase, maxPassphraseLength,
            "Old passphrase (\"%s\") [<CR> to cancel]: ", prompt);
        // @@@ addToKeychain not hooked up; stays unchanged
		stage = oldGenericPassphraseStage;
		return;
	}
#endif
	activate();
    MigBoolean addBox = addToKeychain.setting;
	check(secagent_client_queryOldGenericPassphrase(mServerPort, mClientPort,
		&status, req, requestPid, prompt, &mStagePort.port(),
        addToKeychain.show, &addBox, passphrase));
    addToKeychain.setting = addBox;
	stage = oldGenericPassphraseStage;
}

void Client::retryOldGenericPassphrase(Reason reason, 
    bool &addToKeychain, char passphrase[maxPassphraseLength])
{
	if (stage != oldGenericPassphraseStage)
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR); //@@@ invent a "state mismatch error"?
#if defined(NOSA)
	if (getenv("NOSA")) {
		getNoSA(passphrase, maxPassphraseLength,
            "Retry old passphrase [<CR> to cancel]: ");
		return;
	}
#endif
    MigBoolean addBox;
	check(secagent_client_retryOldGenericPassphrase(mStagePort, mClientPort,
		&status, reason, &addBox, passphrase));
    addToKeychain = addBox;
}


//
// Ask for a new passphrase for something (with selectable prompt).
//
void Client::queryNewGenericPassphrase(const OSXCode *requestor, pid_t requestPid,
    const char *prompt, Reason reason,
    KeychainBox &addToKeychain, char passphrase[maxPassphraseLength])
{
	Requestor req(requestor);
	
#if defined(NOSA)
	if (getenv("NOSA")) {
		getNoSA(passphrase, maxPassphraseLength,
			"New passphrase (\"%s\") (reason %d) [<CR> to cancel]: ",
			prompt, reason);
        // @@@ addToKeychain not hooked up; stays unchanged
		stage = newGenericPassphraseStage;
		return;
	}
#endif
	activate();
    MigBoolean addBox = addToKeychain.setting;
	check(secagent_client_queryNewGenericPassphrase(mServerPort, mClientPort,
		&status, req, requestPid, prompt, reason,
        &mStagePort.port(), addToKeychain.show, &addBox, passphrase));
    addToKeychain.setting = addBox;
	stage = newGenericPassphraseStage;
}

void Client::retryNewGenericPassphrase(Reason reason,
    bool &addToKeychain, char passphrase[maxPassphraseLength])
{
	if (stage != newGenericPassphraseStage)
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR); //@@@ invent a "state mismatch error"?
#if defined(NOSA)
	if (getenv("NOSA")) {
		getNoSA(passphrase, maxPassphraseLength,
			"retry new passphrase (reason %d) [<CR> to cancel]: ", reason);
		return;
	}
#endif
    MigBoolean addBox;
	check(secagent_client_retryNewGenericPassphrase(mStagePort, mClientPort,
		&status, reason, &addBox, passphrase));
    addToKeychain = addBox;
}


//
// Authorization by authentication
//
bool Client::authorizationAuthenticate(const OSXCode *requestor, pid_t requestPid,
	const char *neededGroup, const char *candidateUser,
	char user[maxUsernameLength], char passphrase[maxPassphraseLength])
{
	Requestor req(requestor);

#if defined(NOSA)
	if (getenv("NOSA")) {
		getNoSA(user, maxUsernameLength,
			"User to authenticate for group %s (try \"%s\" [\"-\" to deny]): ",
			neededGroup, (candidateUser ? candidateUser : "[NULL]"));
		if (strcmp(user, "-"))
                    getNoSA(passphrase, maxPassphraseLength,
                            "Passphrase for user %s: ", user);
		stage = authorizeStage;
		return strcmp(user, "-");
	}
#endif
	activate();
	check(secagent_client_authorizationAuthenticate(mServerPort, mClientPort,
			&status, req, requestPid, neededGroup, candidateUser, &mStagePort.port(), user, passphrase));
    stage = authorizeStage;
	return status == noErr;
}

bool Client::retryAuthorizationAuthenticate(Reason reason, char user[maxUsernameLength],
        char passphrase[maxPassphraseLength])
{
	if (stage != authorizeStage)
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);	//@@@ invent a "state mismatch error"?
#if defined(NOSA)
	if (getenv("NOSA")) {
		getNoSA(user, maxUsernameLength,
			"Retry authenticate (reason=%d) ([\"-\" to deny again]): ", reason);
		if (strcmp(user, "-"))
                    getNoSA(passphrase, maxPassphraseLength,
                            "Passphrase for user %s: ", user);
		return strcmp(user, "-");
	}
#endif
	check(secagent_client_retryAuthorizationAuthenticate(mStagePort, mClientPort,
			&status, reason, user, passphrase));
	return status == noErr;
}

//
// invokeMechanism old style
//
bool Client::invokeMechanism(const string &inPluginId, const string &inMechanismId, const AuthValueVector &inArguments, AuthItemSet &inHints, AuthItemSet &inContext, AuthorizationResult *outResult)
{
	AuthorizationValueVector *inArgumentVector;
	AuthorizationItemSet *inHintsSet, *inContextSet;
	size_t inArgumentVectorLength, inHintsSetLength, inContextSetLength;
	
	CssmAllocator &alloc = CssmAllocator::standard();
	inArguments.copy(&inArgumentVector, &inArgumentVectorLength);
	CssmAutoPtr<AuthorizationValueVector> argGuard(alloc, inArgumentVector);
	inHints.copy(inHintsSet, inHintsSetLength, alloc);
	CssmAutoPtr<AuthorizationItemSet> hintGuard(alloc, inHintsSet);
	inContext.copy(inContextSet, inContextSetLength, alloc);
	CssmAutoPtr<AuthorizationItemSet> contextGuard(alloc, inContextSet);

    COPY_OUT_DECL(AuthorizationItemSet, outHintsSet);
    COPY_OUT_DECL(AuthorizationItemSet, outContextSet);

    activate();

    // either noErr (user cancel, allow) or throws authInternal
	
	check(secagent_client_invokeMechanism(mServerPort, mClientPort,
		&status, &mStagePort.port(), inPluginId.c_str(), inMechanismId.c_str(),
		inArgumentVector, inArgumentVectorLength, inArgumentVector,
		inHintsSet, inHintsSetLength, inHintsSet,
		inContextSet, inContextSetLength, inContextSet,
		outResult,
		COPY_OUT(outHintsSet),
		COPY_OUT(outContextSet)));
	
	VMGuard _(outHintsSet, outHintsSetLength);
	VMGuard _2(outContextSet, outContextSetLength);

    if (status != errAuthorizationDenied)
    {
        relocate(outHintsSet, outHintsSetBase);
		inHints = *outHintsSet;
		relocate(outContextSet, outContextSetBase);
		inContext = *outContextSet;
    }

    return (status == noErr);
}


void Client::terminateAgent()
{
	// If the agent is (still) running, kill it
	if (mClientBootstrap.lookupOptional(mAgentName.c_str()))
    {
        activate();
        check(secagent_client_terminate(mServerPort, mClientPort));
    }
}

} // end namespace SecurityAgent
} // end namespace Security

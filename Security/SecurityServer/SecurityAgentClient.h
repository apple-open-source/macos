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
#ifndef _H_SECURITYAGENTCLIENT
#define _H_SECURITYAGENTCLIENT

#if defined(__cplusplus)
#include <string>
#include <Security/mach++.h>
#include <Security/osxsigning.h>
#include <Security/cssmacl.h>
#include <Security/cssm.h>

namespace Security {

using MachPlusPlus::Port;
using CodeSigning::OSXCode;


namespace SecurityAgent {

#endif //C++ only

// Note: Following section also available to C code for inclusion

static const unsigned int maxPassphraseLength = 1024;
static const unsigned int maxUsernameLength = 80;


//
// Unified reason codes transmitted to SecurityAgent (and internationalized there)
//
enum Reason {
	noReason = 0,			// no reason (not used, used as a NULL)
	unknownReason,			// something else (catch-all internal error)
	
	// reasons for asking for a new passphrase
	newDatabase = 11,		// need passphrase for a new database
	changePassphrase,		// changing passphrase for existing database
	
	// reasons for retrying an unlock query
	invalidPassphrase = 21,	// passphrase was wrong
	
	// reasons for retrying a new passphrase query
	passphraseIsNull = 31,	// empty passphrase
	passphraseTooSimple,	// passphrase is not complex enough
	passphraseRepeated,		// passphrase was used before (must use new one)
	passphraseUnacceptable, // passphrase unacceptable for some other reason
	
	// reasons for retrying an authorization query
	userNotInGroup = 41,	// authenticated user not in needed group
	unacceptableUser,		// authenticated user unacceptable for some other reason
	
	// reasons for canceling a staged query
	tooManyTries = 61,		// too many failed attempts to get it right
	noLongerNeeded,			// the queried item is no longer needed
	keychainAddFailed,		// the requested itemed couldn't be added to the keychain
	generalErrorCancel		// something went wrong so we have to give up now
};

#if defined(__cplusplus)


//
// The client interface to the SecurityAgent.
//
class Client {
public:
	Client();
	virtual ~Client();
	
	void activate(const char *bootstrapName = NULL);
	void terminate();
	
	bool keepAlive() const		{ return mKeepAlive; }
	void keepAlive(bool ka)		{ mKeepAlive = ka; }
	
	// common stage termination calls
	void finishStagedQuery();
	void cancelStagedQuery(Reason reason);
    
public:
    struct KeychainBox {
        bool show;				// show the "save in keychain" checkbox (in)
        bool setting;			// value of the checkbox (in/out)
    };
	
public:
	// ask to unlock an existing database. Staged protocol
	void queryUnlockDatabase(const OSXCode *requestor, pid_t requestPid,
        const char *database, char passphrase[maxPassphraseLength]);
	void retryUnlockDatabase(Reason reason, char passphrase[maxPassphraseLength]);
	
	// ask for a new passphrase for a database. Not yet staged
	void queryNewPassphrase(const OSXCode *requestor, pid_t requestPid,
        const char *database, Reason reason, char passphrase[maxPassphraseLength]);
	void retryNewPassphrase(Reason reason, char passphrase[maxPassphraseLength]);
	
	// ask permission to use an item in a database
    struct KeychainChoice {
        bool allowAccess;
        bool continueGrantingToCaller;
    };
    void queryKeychainAccess(const OSXCode *requestor, pid_t requestPid,
        const char *database, const char *itemName, AclAuthorization action,
		KeychainChoice &choice);
        
    // generic old passphrase query
    void queryOldGenericPassphrase(const OSXCode *requestor, pid_t requestPid,
        const char *prompt,
        KeychainBox &addToKeychain, char passphrase[maxPassphraseLength]);
    void retryOldGenericPassphrase(Reason reason,
        bool &addToKeychain, char passphrase[maxPassphraseLength]);
        
    // generic new passphrase query
    void queryNewGenericPassphrase(const OSXCode *requestor, pid_t requestPid,
        const char *prompt, Reason reason,
        KeychainBox &addToKeychain, char passphrase[maxPassphraseLength]);
    void retryNewGenericPassphrase(Reason reason,
        bool &addToKeychain, char passphrase[maxPassphraseLength]);
	
	// authenticate a user for the purpose of authorization
	bool authorizationAuthenticate(const OSXCode *requestor, pid_t requestPid,
		const char *neededGroup, const char *candidateUser,
                char username[maxUsernameLength], char passphrase[maxPassphraseLength]);
	bool retryAuthorizationAuthenticate(Reason reason,
                char username[maxUsernameLength], char passphrase[maxPassphraseLength]);
	
	// Cancel a pending client call in another thread by sending a cancel message.
	// This call (only) may be made from another thread.
	void cancel();
		
private:
	// used by client call wrappers to receive IPC return-status
	OSStatus status;

private:
    Port mServerPort;
    Port mClientPort;
	bool mActive;
    uid_t desktopUid;
    gid_t desktopGid;
    mach_port_t pbsBootstrap;
	bool mKeepAlive;

	enum Stage {
		mainStage,				// in between requests
		unlockStage,			// in unlock sub-protocol
		newPassphraseStage,		// in get-new-passphrase sub-protocol
        newGenericPassphraseStage, // in get-new-generic-passphrase sub-protocol
        oldGenericPassphraseStage, // in get-old-generic-passphrase sub-protocol
		authorizeStage			// in authorize-by-group-membership sub-protocol
	} stage;
	Port mStagePort;

    void locateDesktop();
    void establishServer(const char *name);
	void check(kern_return_t error);
	void unstage();
	
private:
	static const int cancelMessagePseudoID = 1200;
};

}; // end namespace SecurityAgent

} // end namespace Security

#endif //C++ only

#endif //_H_SECURITYAGENTCLIENT

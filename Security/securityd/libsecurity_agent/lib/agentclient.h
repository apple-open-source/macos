/*
 *  agentclient.h
 *  SecurityAgent
 *
 *  Copyright (c) 2002,2008,2011-2013 Apple Inc.. All Rights Reserved.
 *
 */

#ifndef _H_AGENTCLIENT
#define _H_AGENTCLIENT

#include <Security/Authorization.h>
#include <Security/AuthorizationPlugin.h>
#include <Security/AuthorizationTags.h>
#include <Security/AuthorizationTagsPriv.h>

#include <security_agent_client/sa_types.h>

#if defined(__cplusplus)

#include <string>
#include <security_utilities/mach++.h>
#include <security_cdsa_utilities/AuthorizationData.h>

namespace SecurityAgent {
#endif /* __cplusplus__ */

// Manimum number of failed authentications before
// SecurityAgent dialog is killed.
#define kMaximumAuthorizationTries 10000

// Number of failed authentications before a password
// hint is displayed.
#define kAuthorizationTriesBeforeHint 3

#define maxPassphraseLength 1024
    
//
// Unified reason codes transmitted to SecurityAgent (and internationalized there)
//
enum Reason {
    noReason = 0,                   // no reason (not used, used as a NULL)
    unknownReason,                  // something else (catch-all internal error)

    // reasons for asking for a new passphrase
    newDatabase = 11,               // need passphrase for a new database
    changePassphrase,               // changing passphrase for existing database

    // reasons for retrying an unlock query
    invalidPassphrase = 21,         // passphrase was wrong

    // reasons for retrying a new passphrase query
    passphraseIsNull = 31,          // empty passphrase
    passphraseTooSimple,            // passphrase is not complex enough
    passphraseRepeated,             // passphrase was used before (must use new one)
    passphraseUnacceptable,         // passphrase unacceptable for some other reason
    oldPassphraseWrong,             // the old passphrase given is wrong

    // reasons for retrying an authorization query
    userNotInGroup = 41,            // authenticated user not in needed group
    unacceptableUser,               // authenticated user unacceptable for some other reason

    // reasons for canceling a staged query
    tooManyTries = 61,              // too many failed attempts to get it right
    noLongerNeeded,                 // the queried item is no longer needed
    keychainAddFailed,              // the requested itemed couldn't be added to the keychain
    generalErrorCancel,              // something went wrong so we have to give up now
    resettingPassword,              // The user has indicated that they wish to reset their password
	
	worldChanged = 101
};

typedef enum {
	tool = 'TOOL',
	bundle = 'BNDL',
	unknown = 'UNKN'
} RequestorType;

#if defined(__cplusplus)

using MachPlusPlus::Port;
using MachPlusPlus::PortSet;
using MachPlusPlus::Bootstrap;
using MachPlusPlus::ReceivePort;
using MachPlusPlus::Message;
using Authorization::AuthItemSet;
using Authorization::AuthValueVector;

class Clients;

class Client
{
friend class Clients;

enum MessageType { requestInterruptMessage, didDeactivateMessage, reportErrorMessage };

public:
	Client();
	virtual ~Client();

    static AuthItemSet clientHints(SecurityAgent::RequestorType type, std::string &path, pid_t clientPid, uid_t clientUid);
    
    static OSStatus startTransaction(Port serverPort);
    static OSStatus endTransaction(Port serverPort);
	
protected:
	void establishServer();
	
public:
	void activate(Port serverPort);

    OSStatus contact(mach_port_t jobId, Bootstrap processBootstrap, mach_port_t userPrefs);
	OSStatus create(const char *pluginId, const char *mechanismId, const SessionId inSessionId);
    void setArguments(const Authorization::AuthValueVector& inArguments) { mArguments = inArguments; }
    void setInput(const Authorization::AuthItemSet& inHints, const Authorization::AuthItemSet& inContext) { mInHints = inHints; mInContext = inContext; }
    OSStatus invoke();
	OSStatus deactivate();
	OSStatus destroy();
	OSStatus terminate();
    void receive();
	
	void didCreate(const mach_port_t inStagePort);
    void setResult(const AuthorizationResult inResult, const AuthorizationItemSet *inHints, const AuthorizationItemSet *inContext);
	void requestInterrupt(); // setMessageType(requestInterrupt);
	void didDeactivate(); // setMessageType(didDeactivate);

	void setError(const OSStatus inMechanismError); // setMessageType(reportError); setError(mechanismError);
    OSStatus getError();
    AuthorizationResult result() { return mResult; }

	typedef enum _PluginState {
		init,
		created,
		current,
		deactivating,
		active,
		interrupting,
		dead
	} PluginState;
    PluginState state() { return mState; }

protected:
	void setMessageType(const MessageType inMessageType);
	// allow didCreate to set stagePort 
	void setStagePort(const mach_port_t inStagePort);
	// allow server routines to use request port to find instance 

	// @@@ implement lessThan operator for set in terms of instance	

protected:
	void setup();
	void teardown() throw();

    Port mServerPort;
	Port mStagePort;
    Port mClientPort;

	MessageType mMessageType;
    
    OSStatus mErrorState;

    AuthorizationResult mResult;
    AuthValueVector mArguments;
    AuthItemSet mInHints;
    AuthItemSet mInContext;
    AuthItemSet mOutHints;
    AuthItemSet mOutContext;
	
	PluginState mState;
	void setState(PluginState mState);
    
    bool mTerminateOnSleep;

public:
	mach_port_t instance() const { return mClientPort; }
//	bool operator == (const Client &other) const { return this->instance() == other.instance(); }
	bool operator < (const Client &other) const { return this->instance() < other.instance(); }

    AuthItemSet &inHints() { return mInHints; }
    AuthItemSet &inContext() { return mInContext; }
    AuthItemSet &outHints() { return mOutHints; }
    AuthItemSet &outContext() { return mOutContext; }

    void setTerminateOnSleep(bool terminateOnSleep) {mTerminateOnSleep = terminateOnSleep;}
    bool getTerminateOnSleep() {return mTerminateOnSleep;}

public:
    void check(mach_msg_return_t returnCode);
    void checkResult();
};

class Clients
{
friend class Client;

protected:
	set<Client*> mClients;
    PortSet mClientPortSet;
public:
    Clients() {}
    void create(); // create an agentclient
    void insert(Client *agent) { StLock<Mutex> _(mLock); mClients.insert(agent); mClientPortSet += agent->instance(); }
    void remove(Client *agent) { StLock<Mutex> _(mLock); mClientPortSet -= agent->instance(); mClients.erase(agent); }
    Client &find(const mach_port_t instance) const;
    bool receive();
    bool compare(const Client * client, mach_port_t instance);

    mutable Mutex mLock;
    static ThreadNexus<Clients> gClients;
    static ModuleNexus<RecursiveMutex> gAllClientsMutex;
    static ModuleNexus<set<Client*> > allClients;
    static void killAllClients();
};

} // end namespace Authorization

#endif /* __cplusplus__ */

#endif /* _H_AGENTCLIENT */


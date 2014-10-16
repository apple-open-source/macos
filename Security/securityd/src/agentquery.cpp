/*
 * Copyright (c) 2000-2013 Apple Inc. All Rights Reserved.
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
// passphrases - canonical code to obtain passphrases
//
#include "agentquery.h"
#include "authority.h"
#include "ccaudit_extensions.h"

#include <Security/AuthorizationTags.h>
#include <Security/AuthorizationTagsPriv.h>
#include <Security/checkpw.h>
#include <Security/Security.h>
#include <System/sys/fileport.h>
#include <bsm/audit.h>
#include <bsm/audit_uevents.h>      // AUE_ssauthint
#include <security_utilities/logging.h>
#include <security_utilities/mach++.h>
#include <stdlib.h>
#include <xpc/xpc.h>
#include <xpc/private.h>
#include "securityd_service/securityd_service/securityd_service_client.h"

#define SECURITYAGENT_BOOTSTRAP_NAME_BASE       "com.apple.security.agentMain"
#define SECURITYAGENT_STUB_BOOTSTRAP_NAME_BASE       "com.apple.security.agentStub"
#define AUTHORIZATIONHOST_BOOTSTRAP_NAME_BASE   "com.apple.security.authhost"

#define AUTH_XPC_ITEM_NAME  "_item_name"
#define AUTH_XPC_ITEM_FLAGS "_item_flags"
#define AUTH_XPC_ITEM_VALUE "_item_value"
#define AUTH_XPC_ITEM_TYPE  "_item_type"

#define AUTH_XPC_REQUEST_METHOD_KEY "_agent_request_key"
#define AUTH_XPC_REQUEST_METHOD_CREATE "_agent_request_create"
#define AUTH_XPC_REQUEST_METHOD_INVOKE "_agent_request_invoke"
#define AUTH_XPC_REQUEST_METHOD_DEACTIVATE "_agent_request_deactivate"
#define AUTH_XPC_REQUEST_METHOD_DESTROY "_agent_request_destroy"
#define AUTH_XPC_REPLY_METHOD_KEY "_agent_reply_key"
#define AUTH_XPC_REPLY_METHOD_RESULT "_agent_reply_result"
#define AUTH_XPC_REPLY_METHOD_INTERRUPT "_agent_reply_interrupt"
#define AUTH_XPC_REPLY_METHOD_CREATE "_agent_reply_create"
#define AUTH_XPC_REPLY_METHOD_DEACTIVATE "_agent_reply_deactivate"
#define AUTH_XPC_PLUGIN_NAME "_agent_plugin"
#define AUTH_XPC_MECHANISM_NAME "_agent_mechanism"
#define AUTH_XPC_HINTS_NAME "_agent_hints"
#define AUTH_XPC_CONTEXT_NAME "_agent_context"
#define AUTH_XPC_IMMUTABLE_HINTS_NAME "_agent_immutable_hints"
#define AUTH_XPC_REQUEST_INSTANCE "_agent_instance"
#define AUTH_XPC_REPLY_RESULT_VALUE "_agent_reply_result_value"
#define AUTH_XPC_AUDIT_SESSION_PORT "_agent_audit_session_port"
#define AUTH_XPC_BOOTSTRAP_PORT "_agent_bootstrap_port"
#define AUTH_XPC_SESSION_UUID "_agent_session_uuid"
#define AUTH_XPC_SESSION_PREFS "_agent_session_prefs"
#define AUTH_XPC_SESSION_INPUT_METHOD "_agent_session_inputMethod"

#define UUID_INITIALIZER_FROM_SESSIONID(sessionid) \
{ 0,0,0,0, 0,0,0,0, 0,0,0,0, (unsigned char)((0xff000000 & (sessionid))>>24), (unsigned char)((0x00ff0000 & (sessionid))>>16), (unsigned char)((0x0000ff00 & (sessionid))>>8),  (unsigned char)((0x000000ff & (sessionid))) }

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


// SecurityAgentConnection

SecurityAgentConnection::SecurityAgentConnection(const AuthHostType type, Session &session)
: mAuthHostType(type),
mHostInstance(session.authhost(mAuthHostType)),
mConnection(&Server::connection()),
mAuditToken(Server::connection().auditToken())
{
	// this may take a while
	Server::active().longTermActivity();
    secdebug("SecurityAgentConnection", "new SecurityAgentConnection(%p)", this);
}

SecurityAgentConnection::~SecurityAgentConnection()
{
    secdebug("SecurityAgentConnection", "SecurityAgentConnection(%p) dying", this);
	mConnection->useAgent(NULL);
}

void
SecurityAgentConnection::activate()
{
    secdebug("SecurityAgentConnection", "activate(%p)", this);
	
    Session &session = mHostInstance->session();
    SessionId targetSessionId = session.sessionId();
    MachPlusPlus::Bootstrap processBootstrap = Server::process().taskPort().bootstrap();
    fileport_t userPrefsFP = MACH_PORT_NULL;
	
    // send the the userPrefs to SecurityAgent
    if (mAuthHostType == securityAgent || mAuthHostType == userAuthHost) {
		CFRef<CFDataRef> userPrefs(mHostInstance->session().copyUserPrefs());
		if (0 != userPrefs)
		{
			FILE *mbox = NULL;
			int fd = 0;
			mbox = tmpfile();
			if (NULL != mbox)
			{
				fd = dup(fileno(mbox));
				fclose(mbox);
				if (fd != -1)
				{
					CFIndex length = CFDataGetLength(userPrefs);
					if (write(fd, CFDataGetBytePtr(userPrefs), length) != length)
						Syslog::error("could not write userPrefs");
					else
					{
						if (0 == fileport_makeport(fd, &userPrefsFP))
							secdebug("SecurityAgentConnection", "stashed the userPrefs file descriptor");
						else
							Syslog::error("failed to stash the userPrefs file descriptor");
					}
					close(fd);
				}
			}
		}
		if (MACH_PORT_NULL == userPrefsFP)
		{
			secdebug("SecurityAgentConnection", "could not read userPrefs");
		}
    }
    
	mConnection->useAgent(this);
	try
    {
        StLock<Mutex> _(*mHostInstance);
		
        mach_port_t lookupPort = mHostInstance->lookup(targetSessionId);
        if (MACH_PORT_NULL == lookupPort)
        {
			Syslog::error("could not find real service, bailing");
			MacOSError::throwMe(CSSM_ERRCODE_SERVICE_NOT_AVAILABLE);
        }
        // reset Client contact info
        mPort = lookupPort;
        SecurityAgent::Client::activate(mPort);
        
        secdebug("SecurityAgentConnection", "%p activated", this);
	}
    catch (MacOSError &err)
    {
		mConnection->useAgent(NULL);	// guess not
        Syslog::error("SecurityAgentConnection: error activating %s instance %p",
                      mAuthHostType == privilegedAuthHost
                      ? "authorizationhost"
                      : "SecurityAgent", this);
		throw;
	}
	
    secdebug("SecurityAgentConnection", "contacting service (%p)", this);
	mach_port_name_t jobPort;
	if (0 > audit_session_port(session.sessionId(), &jobPort))
		Syslog::error("audit_session_port failed: %m");
    MacOSError::check(SecurityAgent::Client::contact(jobPort, processBootstrap, userPrefsFP));
    secdebug("SecurityAgentConnection", "contact didn't throw (%p)", this);
	
    if (userPrefsFP != MACH_PORT_NULL)
        mach_port_deallocate(mach_task_self(), userPrefsFP);
}

void
SecurityAgentConnection::reconnect()
{
    // if !mHostInstance throw()?
    if (mHostInstance)
    {
        activate();
    }
}

void
SecurityAgentConnection::terminate()
{
	activate();
    
    // @@@ This happens already in the destructor; presumably we do this to tear things down orderly
	mConnection->useAgent(NULL);
}


// SecurityAgentConnection

SecurityAgentXPCConnection::SecurityAgentXPCConnection(const AuthHostType type, Session &session)
: mAuthHostType(type),
mHostInstance(session.authhost(mAuthHostType)),
mSession(session),
mConnection(&Server::connection()),
mAuditToken(Server::connection().auditToken())
{
	// this may take a while
	Server::active().longTermActivity();
    secdebug("SecurityAgentConnection", "new SecurityAgentConnection(%p)", this);
    mXPCConnection = NULL;
    mXPCStubConnection = NULL;
    mNobodyUID = -2;
    struct passwd *pw = getpwnam("nobody");
    if (NULL != pw) {
        mNobodyUID = pw->pw_uid;
    }
}

SecurityAgentXPCConnection::~SecurityAgentXPCConnection()
{
    secdebug("SecurityAgentConnection", "SecurityAgentConnection(%p) dying", this);
	mConnection->useAgent(NULL);
    
    // If a connection has been established, we need to tear it down.
    if (NULL != mXPCConnection) {
        // Tearing this down is a multi-step process. First, request a cancellation.
        // This is safe even if the connection is already in the cancelled state.
        xpc_connection_cancel(mXPCConnection);
        
        // Then release the XPC connection
        xpc_release(mXPCConnection);
        mXPCConnection = NULL;

        if (NULL != mXPCStubConnection) {
            // We may or may not have one of these
            xpc_release(mXPCStubConnection);
            mXPCStubConnection = NULL;
        }
    }
}

bool SecurityAgentXPCConnection::inDarkWake()
{
	return mSession.server().inDarkWake();
}

void
SecurityAgentXPCConnection::activate(bool ignoreUid)
{
    secdebug("SecurityAgentConnection", "activate(%p)", this);
    
	mConnection->useAgent(this);
    if (mXPCConnection != NULL) {
        // If we already have an XPC connection, there's nothing to do.
        return;
    }
	try
    {
        if (mAuthHostType == securityAgent) {
            uuid_t sessionUUID = UUID_INITIALIZER_FROM_SESSIONID(mSession.sessionId());
            // Yes, these need to be throws, as we're still in securityd, and thus still have to do flow control with exceptions.
            if (!(mSession.attributes() & sessionHasGraphicAccess))
                CssmError::throwMe(CSSM_ERRCODE_NO_USER_INTERACTION);
            if (inDarkWake())
                CssmError::throwMe(CSSM_ERRCODE_IN_DARK_WAKE);
            uid_t targetUid = mHostInstance->session().originatorUid();
            secdebug("SecurityAgentXPCConnection","Retrieved UID %d for this session", targetUid);
            if ((int32_t)targetUid != -1) {
                mXPCStubConnection = xpc_connection_create_mach_service(SECURITYAGENT_STUB_BOOTSTRAP_NAME_BASE, NULL, 0);
                xpc_connection_set_target_uid(mXPCStubConnection, targetUid);
                secdebug("SecurityAgentXPCConnection", "Creating a security agent stub");
                xpc_connection_set_event_handler(mXPCStubConnection, ^(xpc_object_t object){}); // Yes, this is a dummy handler, we never ever care about any responses from the stub. It can die in a fire for all I care.
                xpc_connection_resume(mXPCStubConnection);
                
                xpc_object_t wakeupMessage = xpc_dictionary_create(NULL, NULL, 0);
                xpc_dictionary_set_data(wakeupMessage, AUTH_XPC_SESSION_UUID, sessionUUID, sizeof(uuid_t));
                xpc_object_t responseMessage = xpc_connection_send_message_with_reply_sync(mXPCStubConnection, wakeupMessage);
                if (xpc_get_type(responseMessage) == XPC_TYPE_DICTIONARY) {
                    secdebug("SecurityAgentXPCConnection", "Valid response received from stub");
                } else {
                    secdebug("SecurityAgentXPCConnection", "Error response received from stub");
                }
                xpc_release(wakeupMessage);
                xpc_release(responseMessage);
            }
            
            mXPCConnection = xpc_connection_create_mach_service(SECURITYAGENT_BOOTSTRAP_NAME_BASE, NULL,0);
            xpc_connection_set_instance(mXPCConnection, sessionUUID);
            secdebug("SecurityAgentXPCConnection", "Creating a security agent");
        } else {
            mXPCConnection = xpc_connection_create_mach_service(AUTHORIZATIONHOST_BOOTSTRAP_NAME_BASE, NULL, 0);
            secdebug("SecurityAgentXPCConnection", "Creating a standard authhost");
        }
        
        xpc_connection_set_event_handler(mXPCConnection, ^(xpc_object_t object) {
            if (xpc_get_type(object) == XPC_TYPE_ERROR) {
                secdebug("SecurityAgentXPCConnection", "error during xpc: %s", xpc_dictionary_get_string(object, XPC_ERROR_KEY_DESCRIPTION));
            }
        });
        
        xpc_connection_resume(mXPCConnection);
        
        secdebug("SecurityAgentXPCConnection", "%p activated", this);
	}
    catch (MacOSError &err)
    {
		mConnection->useAgent(NULL);	// guess not
        Syslog::error("SecurityAgentConnection: error activating %s instance %p",
                      mAuthHostType == privilegedAuthHost
                      ? "authorizationhost"
                      : "SecurityAgent", this);
		throw;
	}
	
    secdebug("SecurityAgentXPCConnection", "contact didn't throw (%p)", this);
}

void
SecurityAgentXPCConnection::reconnect()
{
}

void
SecurityAgentXPCConnection::terminate()
{
	activate(false);
    
    // @@@ This happens already in the destructor; presumably we do this to tear things down orderly
	mConnection->useAgent(NULL);
}


using SecurityAgent::Reason;
using namespace Authorization;

SecurityAgentQuery::SecurityAgentQuery(const AuthHostType type, Session &session)
: SecurityAgentConnection(type, session)
{
    secdebug("SecurityAgentQuery", "new SecurityAgentQuery(%p)", this);
}

SecurityAgentQuery::~SecurityAgentQuery()
{
    secdebug("SecurityAgentQuery", "SecurityAgentQuery(%p) dying", this);
    
#if defined(NOSA)
	if (getenv("NOSA")) {
		printf(" [query done]\n");
		return;
	}
#endif
    
    if (SecurityAgent::Client::state() != SecurityAgent::Client::dead)
        destroy();
}

void
SecurityAgentQuery::inferHints(Process &thisProcess)
{
    string guestPath;
	{
		StLock<Mutex> _(thisProcess);
		if (SecCodeRef clientCode = thisProcess.currentGuest())
			guestPath = codePath(clientCode);
	}
	AuthItemSet processHints = clientHints(SecurityAgent::bundle, guestPath,
                                           thisProcess.pid(), thisProcess.uid());
	mClientHints.insert(processHints.begin(), processHints.end());
}

void SecurityAgentQuery::addHint(const char *name, const void *value, UInt32 valueLen, UInt32 flags)
{
    AuthorizationItem item = { name, valueLen, const_cast<void *>(value), flags };
    mClientHints.insert(AuthItemRef(item));
}


void
SecurityAgentQuery::readChoice()
{
    allow = false;
    remember = false;
    
	AuthItem *allowAction = outContext().find(AGENT_CONTEXT_ALLOW);
	if (allowAction)
	{
        string allowString;
		if (allowAction->getString(allowString)
            && (allowString == "YES"))
            allow = true;
	}
	
	AuthItem *rememberAction = outContext().find(AGENT_CONTEXT_REMEMBER_ACTION);
	if (rememberAction)
	{
        string rememberString;
        if (rememberAction->getString(rememberString)
            && (rememberString == "YES"))
            remember = true;
	}
}

void
SecurityAgentQuery::disconnect()
{
    SecurityAgent::Client::destroy();
}

void
SecurityAgentQuery::terminate()
{
    // you might think these are called in the wrong order, but you'd be wrong
    SecurityAgentConnection::terminate();
	SecurityAgent::Client::terminate();
}

void
SecurityAgentQuery::create(const char *pluginId, const char *mechanismId, const SessionId inSessionId)
{
	activate();
	OSStatus status = SecurityAgent::Client::create(pluginId, mechanismId, inSessionId);
	if (status)
	{
		secdebug("SecurityAgentQuery", "agent went walkabout, restarting");
        reconnect();
		status = SecurityAgent::Client::create(pluginId, mechanismId, inSessionId);
	}
	if (status) MacOSError::throwMe(status);
}

ModuleNexus<RecursiveMutex> gAllXPCClientsMutex;
ModuleNexus<set<SecurityAgentXPCQuery*> > allXPCClients;

void
SecurityAgentXPCQuery::killAllXPCClients()
{
    // grab the lock for the client list -- we need to make sure no one modifies the structure while we are iterating it.
    StLock<Mutex> _(gAllXPCClientsMutex());
    
    set<SecurityAgentXPCQuery*>::iterator clientIterator = allXPCClients().begin();
    while (clientIterator != allXPCClients().end())
    {
        set<SecurityAgentXPCQuery*>::iterator thisClient = clientIterator++;
        if ((*thisClient)->getTerminateOnSleep())
        {
            (*thisClient)->terminate();
        }
    }
}


SecurityAgentXPCQuery::SecurityAgentXPCQuery(const AuthHostType type, Session &session)
: SecurityAgentXPCConnection(type, session), mAgentConnected(false), mTerminateOnSleep(false)
{
    secdebug("SecurityAgentXPCQuery", "new SecurityAgentXPCQuery(%p)", this);
}

SecurityAgentXPCQuery::~SecurityAgentXPCQuery()
{
    secdebug("SecurityAgentXPCQuery", "SecurityAgentXPCQuery(%p) dying", this);
    if (mAgentConnected) {
        this->disconnect();
    }
}

void
SecurityAgentXPCQuery::inferHints(Process &thisProcess)
{
    string guestPath;
	if (SecCodeRef clientCode = thisProcess.currentGuest())
		guestPath = codePath(clientCode);

    AuthItemSet clientHints;
    SecurityAgent::RequestorType type = SecurityAgent::bundle;
    pid_t clientPid = thisProcess.pid();
    uid_t clientUid = thisProcess.uid();
    
	clientHints.insert(AuthItemRef(AGENT_HINT_CLIENT_TYPE, AuthValueOverlay(sizeof(type), &type)));
	clientHints.insert(AuthItemRef(AGENT_HINT_CLIENT_PATH, AuthValueOverlay(guestPath)));
	clientHints.insert(AuthItemRef(AGENT_HINT_CLIENT_PID, AuthValueOverlay(sizeof(clientPid), &clientPid)));
	clientHints.insert(AuthItemRef(AGENT_HINT_CLIENT_UID, AuthValueOverlay(sizeof(clientUid), &clientUid)));
    

	mClientHints.insert(clientHints.begin(), clientHints.end());

    bool validSignature = thisProcess.checkAppleSigned();
    AuthItemSet clientImmutableHints;
    
	clientImmutableHints.insert(AuthItemRef(AGENT_HINT_PROCESS_SIGNED, AuthValueOverlay(sizeof(validSignature), &validSignature)));

	mImmutableHints.insert(clientImmutableHints.begin(), clientImmutableHints.end());
}

void SecurityAgentXPCQuery::addHint(const char *name, const void *value, UInt32 valueLen, UInt32 flags)
{
    AuthorizationItem item = { name, valueLen, const_cast<void *>(value), flags };
    mClientHints.insert(AuthItemRef(item));
}


void
SecurityAgentXPCQuery::readChoice()
{
    allow = false;
    remember = false;
    
	AuthItem *allowAction = mOutContext.find(AGENT_CONTEXT_ALLOW);
	if (allowAction)
	{
        string allowString;
		if (allowAction->getString(allowString)
            && (allowString == "YES"))
            allow = true;
	}
	
	AuthItem *rememberAction = mOutContext.find(AGENT_CONTEXT_REMEMBER_ACTION);
	if (rememberAction)
	{
        string rememberString;
        if (rememberAction->getString(rememberString)
            && (rememberString == "YES"))
            remember = true;
	}
}

void
SecurityAgentXPCQuery::disconnect()
{
    if (NULL != mXPCConnection) {
        xpc_object_t requestObject = xpc_dictionary_create(NULL, NULL, 0);
        xpc_dictionary_set_string(requestObject, AUTH_XPC_REQUEST_METHOD_KEY, AUTH_XPC_REQUEST_METHOD_DESTROY);
        xpc_connection_send_message(mXPCConnection, requestObject);
        xpc_release(requestObject);
    }

    StLock<Mutex> _(gAllXPCClientsMutex());
    allXPCClients().erase(this);
}

void
SecurityAgentXPCQuery::terminate()
{
    this->disconnect();
}

static void xpcArrayToAuthItemSet(AuthItemSet *setToBuild, xpc_object_t input) {
    setToBuild->clear();
    
    xpc_array_apply(input,  ^bool(size_t index, xpc_object_t item) {
        const char *name = xpc_dictionary_get_string(item, AUTH_XPC_ITEM_NAME);
        
        size_t length;
        const void *data = xpc_dictionary_get_data(item, AUTH_XPC_ITEM_VALUE, &length);
        void *dataCopy = malloc(length);
        memcpy(dataCopy, data, length);
        
        uint64_t flags = xpc_dictionary_get_uint64(item, AUTH_XPC_ITEM_FLAGS);
        AuthItemRef nextItem(name, AuthValueOverlay((uint32_t)length, dataCopy), (uint32_t)flags);
        setToBuild->insert(nextItem);
        memset(dataCopy, 0, length); // The authorization items contain things like passwords, so wiping clean is important.
        free(dataCopy);
        return true;
    });
}

void
SecurityAgentXPCQuery::create(const char *pluginId, const char *mechanismId, const SessionId inSessionId)
{
    bool ignoreUid = false;
    
    do {
        activate(ignoreUid);
        
        mAgentConnected = false;
        
        xpc_object_t requestObject = xpc_dictionary_create(NULL, NULL, 0);
        xpc_dictionary_set_string(requestObject, AUTH_XPC_REQUEST_METHOD_KEY, AUTH_XPC_REQUEST_METHOD_CREATE);
        xpc_dictionary_set_string(requestObject, AUTH_XPC_PLUGIN_NAME, pluginId);
        xpc_dictionary_set_string(requestObject, AUTH_XPC_MECHANISM_NAME, mechanismId);
        
        uid_t targetUid = Server::process().uid();
        bool doSwitchAudit =  true; // (ignoreUid) || ((targetUid == 0) || (targetUid == mNobodyUID));
        bool doSwitchBootstrap = true; // (ignoreUid) || ((targetUid == 0) || (targetUid == mNobodyUID));
        
        if (doSwitchAudit) {
            mach_port_name_t jobPort;
            if (0 == audit_session_port(mSession.sessionId(), &jobPort)) {
                secdebug("SecurityAgentXPCQuery", "attaching an audit session port because the uid was %d", targetUid);
                xpc_dictionary_set_mach_send(requestObject, AUTH_XPC_AUDIT_SESSION_PORT, jobPort);
                if (mach_port_mod_refs(mach_task_self(), jobPort, MACH_PORT_RIGHT_SEND, -1) != KERN_SUCCESS) {
                    secdebug("SecurityAgentXPCQuery", "unable to release send right for audit session, leaking");
                }
            }
        }
        
        if (doSwitchBootstrap) {
            secdebug("SecurityAgentXPCQuery", "attaching a bootstrap port because the uid was %d", targetUid);
            MachPlusPlus::Bootstrap processBootstrap = Server::process().taskPort().bootstrap();
            xpc_dictionary_set_mach_send(requestObject, AUTH_XPC_BOOTSTRAP_PORT, processBootstrap);
        }
        
        xpc_object_t object = xpc_connection_send_message_with_reply_sync(mXPCConnection, requestObject);
        if (xpc_get_type(object) == XPC_TYPE_DICTIONARY) {
            const char *replyType = xpc_dictionary_get_string(object, AUTH_XPC_REPLY_METHOD_KEY);
            if (0 == strcmp(replyType, AUTH_XPC_REPLY_METHOD_CREATE)) {
                uint64_t status = xpc_dictionary_get_uint64(object, AUTH_XPC_REPLY_RESULT_VALUE);
                if (status == kAuthorizationResultAllow) {
                    mAgentConnected = true;
                } else {
                    secdebug("SecurityAgentXPCQuery", "plugin create failed in SecurityAgent");
                    MacOSError::throwMe(errAuthorizationInternal);
                }
            }
        } else if (xpc_get_type(object) == XPC_TYPE_ERROR) {
            if (XPC_ERROR_CONNECTION_INVALID == object) {
                // If we get an error before getting the create response, try again without the UID
                if (ignoreUid) {
                    secdebug("SecurityAgentXPCQuery", "failed to establish connection, no retries left");
                    xpc_release(object);
                    MacOSError::throwMe(errAuthorizationInternal);
                } else {
                    secdebug("SecurityAgentXPCQuery", "failed to establish connection, retrying with no UID");
                    ignoreUid = true;
                    xpc_release(mXPCConnection);
                    mXPCConnection = NULL;
                }
            } else if (XPC_ERROR_CONNECTION_INTERRUPTED == object) {
                // If we get an error before getting the create response, try again
            }
        }
        xpc_release(object);
        xpc_release(requestObject);
    } while (!mAgentConnected);

    StLock<Mutex> _(gAllXPCClientsMutex());
    allXPCClients().insert(this);
}

static xpc_object_t authItemSetToXPCArray(AuthItemSet input) {
    xpc_object_t outputArray = xpc_array_create(NULL, 0);
    for (AuthItemSet::iterator i = input.begin(); i != input.end(); i++) {
        AuthItemRef item = *i;
        
        xpc_object_t xpc_data = xpc_dictionary_create(NULL, NULL, 0);
        xpc_dictionary_set_string(xpc_data, AUTH_XPC_ITEM_NAME, item->name());
        AuthorizationValue value = item->value();
        if (value.data != NULL) {
            xpc_dictionary_set_data(xpc_data, AUTH_XPC_ITEM_VALUE, value.data, value.length);
        }
        xpc_dictionary_set_uint64(xpc_data, AUTH_XPC_ITEM_FLAGS, item->flags());
        xpc_array_append_value(outputArray, xpc_data);
        xpc_release(xpc_data);
    }
    return outputArray;
}

OSStatus
SecurityAgentXPCQuery::invoke() {
    __block OSStatus status = kAuthorizationResultUndefined;
    
    xpc_object_t hintsArray = authItemSetToXPCArray(mInHints);
    xpc_object_t contextArray = authItemSetToXPCArray(mInContext);
    xpc_object_t immutableHintsArray = authItemSetToXPCArray(mImmutableHints);
    
    xpc_object_t requestObject = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_string(requestObject, AUTH_XPC_REQUEST_METHOD_KEY, AUTH_XPC_REQUEST_METHOD_INVOKE);
    xpc_dictionary_set_value(requestObject, AUTH_XPC_HINTS_NAME, hintsArray);
    xpc_dictionary_set_value(requestObject, AUTH_XPC_CONTEXT_NAME, contextArray);
    xpc_dictionary_set_value(requestObject, AUTH_XPC_IMMUTABLE_HINTS_NAME, immutableHintsArray);
    
    xpc_object_t object = xpc_connection_send_message_with_reply_sync(mXPCConnection, requestObject);
    if (xpc_get_type(object) == XPC_TYPE_DICTIONARY) {
        const char *replyType = xpc_dictionary_get_string(object, AUTH_XPC_REPLY_METHOD_KEY);
        if (0 == strcmp(replyType, AUTH_XPC_REPLY_METHOD_RESULT)) {
            xpc_object_t xpcHints = xpc_dictionary_get_value(object, AUTH_XPC_HINTS_NAME);
            xpc_object_t xpcContext = xpc_dictionary_get_value(object, AUTH_XPC_CONTEXT_NAME);
            AuthItemSet tempHints, tempContext;
            xpcArrayToAuthItemSet(&tempHints, xpcHints);
            xpcArrayToAuthItemSet(&tempContext, xpcContext);
            mOutHints = tempHints;
            mOutContext = tempContext;
            mLastResult = xpc_dictionary_get_uint64(object, AUTH_XPC_REPLY_RESULT_VALUE);
        }
    } else if (xpc_get_type(object) == XPC_TYPE_ERROR) {
        if (XPC_ERROR_CONNECTION_INVALID == object) {
            // If the connection drops, return an "auth undefined" result, because we cannot continue
        } else if (XPC_ERROR_CONNECTION_INTERRUPTED == object) {
            // If the agent dies, return an "auth undefined" result, because we cannot continue
        }
    }
    xpc_release(object);
    
    xpc_release(hintsArray);
    xpc_release(contextArray);
    xpc_release(immutableHintsArray);
    xpc_release(requestObject);
    
    return status;
}

void SecurityAgentXPCQuery::checkResult()
{
    // now check the OSStatus return from the server side
    switch (mLastResult) {
        case kAuthorizationResultAllow: return;
        case kAuthorizationResultDeny:
        case kAuthorizationResultUserCanceled: CssmError::throwMe(CSSM_ERRCODE_USER_CANCELED);
        default: MacOSError::throwMe(errAuthorizationInternal);
    }
}

//
// Perform the "rogue app" access query dialog
//
QueryKeychainUse::QueryKeychainUse(bool needPass, const Database *db)
	: mPassphraseCheck(NULL)
{
	// if passphrase checking requested, save KeychainDatabase reference
	// (will quietly disable check if db isn't a keychain)
	if (needPass)
		mPassphraseCheck = dynamic_cast<const KeychainDatabase *>(db);
    
    setTerminateOnSleep(true);
}

Reason QueryKeychainUse::queryUser (const char *database, const char *description, AclAuthorization action)
{
    Reason reason = SecurityAgent::noReason;
    int retryCount = 0;
	OSStatus status;
	AuthValueVector arguments;
	AuthItemSet hints, context;

#if defined(NOSA)
	if (getenv("NOSA")) {
		char answer[maxPassphraseLength+10];
		
        string applicationPath;
        AuthItem *applicationPathItem = mClientHints.find(AGENT_HINT_APPLICATION_PATH);
		if (applicationPathItem)
		  applicationPathItem->getString(applicationPath);

		getNoSA(answer, sizeof(answer), "Allow %s to do %d on %s in %s? [yn][g]%s ",
			applicationPath.c_str(), int(action), (description ? description : "[NULL item]"),
			(database ? database : "[NULL database]"),
			mPassphraseCheck ? ":passphrase" : "");
		// turn passphrase (no ':') into y:passphrase
		if (mPassphraseCheck && !strchr(answer, ':')) {
			memmove(answer+2, answer, strlen(answer)+1);
			memcpy(answer, "y:", 2);
		}

		allow = answer[0] == 'y';
		remember = answer[1] == 'g';
		return SecurityAgent::noReason;
	}
#endif

	// prepopulate with client hints
	hints.insert(mClientHints.begin(), mClientHints.end());
	
	// put action/operation (sint32) into hints
	hints.insert(AuthItemRef(AGENT_HINT_ACL_TAG, AuthValueOverlay(sizeof(action), static_cast<sint32*>(&action))));
	
	// item name into hints

	hints.insert(AuthItemRef(AGENT_HINT_KEYCHAIN_ITEM_NAME, AuthValueOverlay(description ? (uint32_t)strlen(description) : 0, const_cast<char*>(description))));
	
	// keychain name into hints
	hints.insert(AuthItemRef(AGENT_HINT_KEYCHAIN_PATH, AuthValueOverlay(database ? (uint32_t)strlen(database) : 0, const_cast<char*>(database))));
	
	if (mPassphraseCheck)
	{
		create("builtin", "confirm-access-password", noSecuritySession);
		
		CssmAutoData data(Allocator::standard(Allocator::sensitive));

		do 
		{

            AuthItemRef triesHint(AGENT_HINT_TRIES, AuthValueOverlay(sizeof(retryCount), &retryCount));
            hints.erase(triesHint); hints.insert(triesHint); // replace
            
            if (retryCount++ > kMaximumAuthorizationTries)
			{
                reason = SecurityAgent::tooManyTries;
			}
				
            AuthItemRef retryHint(AGENT_HINT_RETRY_REASON, AuthValueOverlay(sizeof(reason), &reason));
            hints.erase(retryHint); hints.insert(retryHint); // replace

            setInput(hints, context);
			status = invoke();

            if (retryCount > kMaximumAuthorizationTries)
			{
                return reason;
			}
			
			checkResult();
			
			AuthItem *passwordItem = mOutContext.find(kAuthorizationEnvironmentPassword);
			if (!passwordItem)
				continue;
						
			passwordItem->getCssmData(data);
		} 
		while ((reason = (const_cast<KeychainDatabase*>(mPassphraseCheck)->decode(data) ? SecurityAgent::noReason : SecurityAgent::invalidPassphrase)));
	}
	else
	{
		create("builtin", "confirm-access", noSecuritySession);
        setInput(hints, context);
		invoke();
	}
	
    readChoice();
    
	return reason;
}

//
// Perform code signature ACL access adjustment dialogs
//
bool QueryCodeCheck::operator () (const char *aclPath)
{
	OSStatus status;
	AuthValueVector arguments;
	AuthItemSet hints, context;
	
#if defined(NOSA)
	if (getenv("NOSA")) {
		char answer[10];
		
        string applicationPath;
        AuthItem *applicationPathItem = mClientHints.find(AGENT_HINT_APPLICATION_PATH);
		if (applicationPathItem)
		  applicationPathItem->getString(applicationPath);

		getNoSA(answer, sizeof(answer),
				"Allow %s to match an ACL for %s [yn][g]? ",
				applicationPath.c_str(), aclPath ? aclPath : "(unknown)");
		allow = answer[0] == 'y';
		remember = answer[1] == 'g';
		return;
	}
#endif

	// prepopulate with client hints
	hints.insert(mClientHints.begin(), mClientHints.end());
	
	hints.insert(AuthItemRef(AGENT_HINT_APPLICATION_PATH, AuthValueOverlay((uint32_t)strlen(aclPath), const_cast<char*>(aclPath))));
    
	create("builtin", "code-identity", noSecuritySession);

    setInput(hints, context);
	status = invoke();
		
    checkResult();

//	MacOSError::check(status);

    return kAuthorizationResultAllow == mLastResult;
}


//
// Obtain passphrases and submit them to the accept() method until it is accepted
// or we can't get another passphrase. Accept() should consume the passphrase
// if it is accepted. If no passphrase is acceptable, throw out of here.
//
Reason QueryOld::query()
{
	Reason reason = SecurityAgent::noReason;
	OSStatus status;
	AuthValueVector arguments;
	AuthItemSet hints, context;
	CssmAutoData passphrase(Allocator::standard(Allocator::sensitive));
	int retryCount = 0;
	
#if defined(NOSA)
    // return the passphrase
	if (getenv("NOSA")) {
        char passphrase_[maxPassphraseLength];
		getNoSA(passphrase, maxPassphraseLength, "Unlock %s [<CR> to cancel]: ", database.dbName());
	passphrase.copy(passphrase_, strlen(passphrase_));
    	return database.decode(passphrase) ? SecurityAgent::noReason : SecurityAgent::invalidPassphrase;
	}
#endif
	
	// prepopulate with client hints

    const char *keychainPath = database.dbName();
    hints.insert(AuthItemRef(AGENT_HINT_KEYCHAIN_PATH, AuthValueOverlay((uint32_t)strlen(keychainPath), const_cast<char*>(keychainPath))));

	hints.insert(mClientHints.begin(), mClientHints.end());
    
	create("builtin", "unlock-keychain", noSecuritySession);

	do
	{
        AuthItemRef triesHint(AGENT_HINT_TRIES, AuthValueOverlay(sizeof(retryCount), &retryCount));
        hints.erase(triesHint); hints.insert(triesHint); // replace
		
        ++retryCount;
            
        if (retryCount > maxTries)
		{
			reason = SecurityAgent::tooManyTries;
        }
            
        AuthItemRef retryHint(AGENT_HINT_RETRY_REASON, AuthValueOverlay(sizeof(reason), &reason));
        hints.erase(retryHint); hints.insert(retryHint); // replace

        setInput(hints, context);
        status = invoke();

        if (retryCount > maxTries)
        {
			return reason;
		}

        checkResult();
		
		AuthItem *passwordItem = mOutContext.find(kAuthorizationEnvironmentPassword);
		if (!passwordItem)
			continue;
		
		passwordItem->getCssmData(passphrase);
		
	}
	while ((reason = accept(passphrase)));

	return SecurityAgent::noReason;
}


//
// Get existing passphrase (unlock) Query
//
Reason QueryOld::operator () ()
{
	return query();
}


//
// End-classes for old secrets
//
Reason QueryUnlock::accept(CssmManagedData &passphrase)
{
	if (safer_cast<KeychainDatabase &>(database).decode(passphrase))
		return SecurityAgent::noReason;
	else
		return SecurityAgent::invalidPassphrase;
}

Reason QueryUnlock::retrievePassword(CssmOwnedData &passphrase) {
    CssmAutoData pass(Allocator::standard(Allocator::sensitive));

    AuthItem *passwordItem = mOutContext.find(kAuthorizationEnvironmentPassword);
    if (!passwordItem)
        return SecurityAgent::invalidPassphrase;
    
    passwordItem->getCssmData(pass);
    
    passphrase = pass;

   return SecurityAgent::noReason;
}

QueryKeybagPassphrase::QueryKeybagPassphrase(Session & session, int32_t tries) : mSession(session), mContext(), mRetries(tries)
{
    setTerminateOnSleep(true);
    mContext = mSession.get_current_service_context();
}

Reason QueryKeybagPassphrase::query()
{
	Reason reason = SecurityAgent::noReason;
	OSStatus status;
	AuthValueVector arguments;
	AuthItemSet hints, context;
	CssmAutoData passphrase(Allocator::standard(Allocator::sensitive));
	int retryCount = 0;

	// prepopulate with client hints

    const char *keychainPath = "iCloud";
    hints.insert(AuthItemRef(AGENT_HINT_KEYCHAIN_PATH, AuthValueOverlay((uint32_t)strlen(keychainPath), const_cast<char*>(keychainPath))));

	hints.insert(mClientHints.begin(), mClientHints.end());

	create("builtin", "unlock-keychain", noSecuritySession);

	do
	{
        if (retryCount > mRetries)
		{
			return SecurityAgent::tooManyTries;
        }

        AuthItemRef triesHint(AGENT_HINT_TRIES, AuthValueOverlay(sizeof(retryCount), &retryCount));
        hints.erase(triesHint); hints.insert(triesHint); // replace

        AuthItemRef retryHint(AGENT_HINT_RETRY_REASON, AuthValueOverlay(sizeof(reason), &reason));
        hints.erase(retryHint); hints.insert(retryHint); // replace

        setInput(hints, context);
        status = invoke();

        checkResult();

		AuthItem *passwordItem = mOutContext.find(kAuthorizationEnvironmentPassword);
		if (!passwordItem)
			continue;

		passwordItem->getCssmData(passphrase);

        ++retryCount;
	}
	while ((reason = accept(passphrase)));

	return SecurityAgent::noReason;
}

Reason QueryKeybagPassphrase::accept(Security::CssmManagedData & password)
{
	if (service_client_kb_unlock(&mContext, password.data(), (int)password.length()) == 0) {
		mSession.keybagSetState(session_keybag_unlocked);
        return SecurityAgent::noReason;
    } else
		return SecurityAgent::invalidPassphrase;
}

QueryKeybagNewPassphrase::QueryKeybagNewPassphrase(Session & session) : QueryKeybagPassphrase(session) {}

Reason QueryKeybagNewPassphrase::query(CssmOwnedData &oldPassphrase, CssmOwnedData &passphrase)
{
    CssmAutoData pass(Allocator::standard(Allocator::sensitive));
    CssmAutoData oldPass(Allocator::standard(Allocator::sensitive));
    Reason reason = SecurityAgent::noReason;
	OSStatus status;
	AuthValueVector arguments;
	AuthItemSet hints, context;
	int retryCount = 0;

	// prepopulate with client hints

    const char *keychainPath = "iCloud";
    hints.insert(AuthItemRef(AGENT_HINT_KEYCHAIN_PATH, AuthValueOverlay((uint32_t)strlen(keychainPath), const_cast<char*>(keychainPath))));

    const char *showResetString = "YES";
    hints.insert(AuthItemRef(AGENT_HINT_SHOW_RESET, AuthValueOverlay((uint32_t)strlen(showResetString), const_cast<char*>(showResetString))));

	hints.insert(mClientHints.begin(), mClientHints.end());

	create("builtin", "change-passphrase", noSecuritySession);

    AuthItem *resetPassword = NULL;
	do
	{
        if (retryCount > mRetries)
		{
			return SecurityAgent::tooManyTries;
        }

        AuthItemRef triesHint(AGENT_HINT_TRIES, AuthValueOverlay(sizeof(retryCount), &retryCount));
        hints.erase(triesHint); hints.insert(triesHint); // replace

        AuthItemRef retryHint(AGENT_HINT_RETRY_REASON, AuthValueOverlay(sizeof(reason), &reason));
        hints.erase(retryHint); hints.insert(retryHint); // replace

        setInput(hints, context);
        status = invoke();

        checkResult();

        resetPassword = mOutContext.find(AGENT_CONTEXT_RESET_PASSWORD);
        if (resetPassword != NULL) {
            return SecurityAgent::resettingPassword;
        }
        
        AuthItem *oldPasswordItem = mOutContext.find(AGENT_PASSWORD);
        if (!oldPasswordItem)
            continue;

        oldPasswordItem->getCssmData(oldPass);

        ++retryCount;
	}
	while ((reason = accept(oldPass)));

    if (reason == SecurityAgent::noReason) {
		AuthItem *passwordItem = mOutContext.find(AGENT_CONTEXT_NEW_PASSWORD);
		if (!passwordItem)
            return SecurityAgent::invalidPassphrase;

		passwordItem->getCssmData(pass);

        oldPassphrase = oldPass;
        passphrase = pass;
    }

	return SecurityAgent::noReason;
}

QueryPIN::QueryPIN(Database &db)
	: QueryOld(db), mPin(Allocator::standard())
{
	this->inferHints(Server::process());
}


Reason QueryPIN::accept(CssmManagedData &pin)
{
	// no retries for now
	mPin = pin;
	return SecurityAgent::noReason;
}


//
// Obtain passphrases and submit them to the accept() method until it is accepted
// or we can't get another passphrase. Accept() should consume the passphrase
// if it is accepted. If no passphrase is acceptable, throw out of here.
//
Reason QueryNewPassphrase::query()
{
	Reason reason = initialReason;
	CssmAutoData passphrase(Allocator::standard(Allocator::sensitive));
	CssmAutoData oldPassphrase(Allocator::standard(Allocator::sensitive));

    OSStatus status;
	AuthValueVector arguments;
	AuthItemSet hints, context;
    
	int retryCount = 0;

#if defined(NOSA)
	if (getenv("NOSA")) {
        char passphrase_[maxPassphraseLength];
		getNoSA(passphrase_, maxPassphraseLength,
			"New passphrase for %s (reason %d) [<CR> to cancel]: ",
			database.dbName(), reason);
		return SecurityAgent::noReason;
	}
#endif

	// prepopulate with client hints
	hints.insert(mClientHints.begin(), mClientHints.end());

	// keychain name into hints
	hints.insert(AuthItemRef(AGENT_HINT_KEYCHAIN_PATH, AuthValueOverlay(database.dbName())));
    
    switch (initialReason)
    {
        case SecurityAgent::newDatabase: 
            create("builtin", "new-passphrase", noSecuritySession);
            break;
        case SecurityAgent::changePassphrase:
            create("builtin", "change-passphrase", noSecuritySession);
            break;
        default:
            assert(false);
    }

	do
	{
        AuthItemRef triesHint(AGENT_HINT_TRIES, AuthValueOverlay(sizeof(retryCount), &retryCount));
        hints.erase(triesHint); hints.insert(triesHint); // replace

		if (++retryCount > maxTries)
		{
			reason = SecurityAgent::tooManyTries;
		}

        AuthItemRef retryHint(AGENT_HINT_RETRY_REASON, AuthValueOverlay(sizeof(reason), &reason));
        hints.erase(retryHint); hints.insert(retryHint); // replace

        setInput(hints, context);
		status = invoke();

		if (retryCount > maxTries)
		{
            return reason;
        }
        
        checkResult();

		if (SecurityAgent::changePassphrase == initialReason)
        {
            AuthItem *oldPasswordItem = mOutContext.find(AGENT_PASSWORD);
            if (!oldPasswordItem)
                continue;
            
            oldPasswordItem->getCssmData(oldPassphrase);
        }
        
		AuthItem *passwordItem = mOutContext.find(AGENT_CONTEXT_NEW_PASSWORD);
		if (!passwordItem)
			continue;
		
		passwordItem->getCssmData(passphrase);

    }
	while ((reason = accept(passphrase, (initialReason == SecurityAgent::changePassphrase) ? &oldPassphrase.get() : NULL)));
    
	return SecurityAgent::noReason;
}


//
// Get new passphrase Query
//
Reason QueryNewPassphrase::operator () (CssmOwnedData &oldPassphrase, CssmOwnedData &passphrase)
{
	if (Reason result = query())
		return result;	// failed
	passphrase = mPassphrase;
    oldPassphrase = mOldPassphrase;
	return SecurityAgent::noReason;	// success
}

Reason QueryNewPassphrase::accept(CssmManagedData &passphrase, CssmData *oldPassphrase)
{
	//@@@ acceptance criteria are currently hardwired here
	//@@@ This validation presumes ASCII - UTF8 might be more lenient
	
	// if we have an old passphrase, check it
	if (oldPassphrase && !safer_cast<KeychainDatabase&>(database).validatePassphrase(*oldPassphrase))
		return SecurityAgent::oldPassphraseWrong;
	
	// sanity check the new passphrase (but allow user override)
	if (!(mPassphraseValid && passphrase.get() == mPassphrase)) {
		mPassphrase = passphrase;
        if (oldPassphrase) mOldPassphrase = *oldPassphrase;
		mPassphraseValid = true;
		if (mPassphrase.length() == 0)
			return SecurityAgent::passphraseIsNull;
		if (mPassphrase.length() < 6)
			return SecurityAgent::passphraseTooSimple;
	}
	
	// accept this
	return SecurityAgent::noReason;
}

// 
// Get a passphrase for unspecified use
// 
Reason QueryGenericPassphrase::operator () (const CssmData *prompt, bool verify,
                                            string &passphrase)
{
    return query(prompt, verify, passphrase);
}

Reason QueryGenericPassphrase::query(const CssmData *prompt, bool verify,
                                     string &passphrase)
{
    Reason reason = SecurityAgent::noReason;
    OSStatus status;    // not really used; remove?  
    AuthValueVector arguments;
    AuthItemSet hints, context;
	
#if defined(NOSA)
    if (getenv("NOSA")) {
		// FIXME  3690984
		return SecurityAgent::noReason;
    }
#endif
	
    hints.insert(mClientHints.begin(), mClientHints.end());
    hints.insert(AuthItemRef(AGENT_HINT_CUSTOM_PROMPT, AuthValueOverlay(prompt ? (UInt32)prompt->length() : 0, prompt ? prompt->data() : NULL)));
    // XXX/gh  defined by dmitch but no analogous hint in
    // AuthorizationTagsPriv.h:
    // CSSM_ATTRIBUTE_ALERT_TITLE (optional alert panel title)
	
    if (false == verify) {  // import
		create("builtin", "generic-unlock", noSecuritySession);
    } else {		// verify passphrase (export)
					// new-passphrase-generic works with the pre-4 June 2004 agent; 
					// generic-new-passphrase is required for the new agent
		create("builtin", "generic-new-passphrase", noSecuritySession);
    }
    
    AuthItem *passwordItem;
    
    do {
        setInput(hints, context);
		status = invoke();
		checkResult();
		passwordItem = mOutContext.find(AGENT_PASSWORD);
		
    } while (!passwordItem);
	
    passwordItem->getString(passphrase);
    
    return reason;
}


// 
// Get a DB blob's passphrase--keychain synchronization
// 
Reason QueryDBBlobSecret::operator () (DbHandle *dbHandleArray, uint8 dbHandleArrayCount, DbHandle *dbHandleAuthenticated)
{
    return query(dbHandleArray, dbHandleArrayCount, dbHandleAuthenticated);
}

Reason QueryDBBlobSecret::query(DbHandle *dbHandleArray, uint8 dbHandleArrayCount, DbHandle *dbHandleAuthenticated)
{
    Reason reason = SecurityAgent::noReason;
	CssmAutoData passphrase(Allocator::standard(Allocator::sensitive));
    OSStatus status;    // not really used; remove?  
    AuthValueVector arguments;
    AuthItemSet hints/*NUKEME*/, context;
	
#if defined(NOSA)
    if (getenv("NOSA")) {
		// FIXME  akin to 3690984
		return SecurityAgent::noReason;
    }
#endif

	hints.insert(mClientHints.begin(), mClientHints.end());
	create("builtin", "generic-unlock-kcblob", noSecuritySession);
    
    AuthItem *secretItem;
    
	int retryCount = 0;
	
    do {
        AuthItemRef triesHint(AGENT_HINT_TRIES, AuthValueOverlay(sizeof(retryCount), &retryCount));
        hints.erase(triesHint); hints.insert(triesHint); // replace

		if (++retryCount > maxTries)
		{
			reason = SecurityAgent::tooManyTries;
		}		
		
        AuthItemRef retryHint(AGENT_HINT_RETRY_REASON, AuthValueOverlay(sizeof(reason), &reason));
        hints.erase(retryHint); hints.insert(retryHint); // replace
		
        setInput(hints, context);
		status = invoke();
		checkResult();
		secretItem = mOutContext.find(AGENT_PASSWORD);
		if (!secretItem)
			continue;
		secretItem->getCssmData(passphrase);
		
    } while ((reason = accept(passphrase, dbHandleArray, dbHandleArrayCount, dbHandleAuthenticated)));
	    
    return reason;
}

Reason QueryDBBlobSecret::accept(CssmManagedData &passphrase, 
								 DbHandle *dbHandlesToAuthenticate, uint8 dbHandleCount, DbHandle *dbHandleAuthenticated)
{
	DbHandle *currHdl = dbHandlesToAuthenticate;
	short index;
	Boolean authenticated = false;
	for (index=0; index < dbHandleCount && !authenticated; index++)
	{
		try 
		{
			RefPointer<KeychainDatabase> dbToUnlock = Server::keychain(*currHdl);
			dbToUnlock->unlockDb(passphrase);
			authenticated = true;
			*dbHandleAuthenticated = *currHdl; // return the DbHandle that 'passphrase' authenticated with.
		} 
		catch (const CommonError &err) 
		{
			currHdl++; // we failed to authenticate with this one, onto the next one.  
		}
	}
	if ( !authenticated )
		return SecurityAgent::invalidPassphrase;
	
	return SecurityAgent::noReason;
}

QueryInvokeMechanism::QueryInvokeMechanism(const AuthHostType type, Session &session) :
    SecurityAgentQuery(type, session) { }

void QueryInvokeMechanism::initialize(const string &inPluginId, const string &inMechanismId, const AuthValueVector &inArguments, const SessionId inSessionId)
{
    if (SecurityAgent::Client::init == SecurityAgent::Client::state())
    {
        create(inPluginId.c_str(), inMechanismId.c_str(), inSessionId);
        mArguments = inArguments;
    }
}

// XXX/cs should return AuthorizationResult
void QueryInvokeMechanism::run(const AuthValueVector &inArguments, AuthItemSet &inHints, AuthItemSet &inContext, AuthorizationResult *outResult)
{
    // prepopulate with client hints
	inHints.insert(mClientHints.begin(), mClientHints.end());

	if (Server::active().inDarkWake())
		CssmError::throwMe(CSSM_ERRCODE_IN_DARK_WAKE);

    setArguments(inArguments);
    setInput(inHints, inContext);
    MacOSError::check(invoke());
    
	if (outResult) *outResult = result();
    
    inHints = outHints();
    inContext = outContext();
}

void QueryInvokeMechanism::terminateAgent()
{
    terminate();
}

// @@@  no pluggable authentication possible!  
Reason
QueryKeychainAuth::operator () (const char *database, const char *description, AclAuthorization action, const char *prompt)
{
    Reason reason = SecurityAgent::noReason;
    AuthItemSet hints, context;
	AuthValueVector arguments;
	int retryCount = 0;
	string username;
	string password;
    
    using CommonCriteria::Securityd::KeychainAuthLogger;
    KeychainAuthLogger logger(mAuditToken, AUE_ssauthint, database, description);
	
#if defined(NOSA)
    /* XXX/gh  probably not complete; stolen verbatim from rogue-app query */
    if (getenv("NOSA")) {
		char answer[maxPassphraseLength+10];
		
        string applicationPath;
        AuthItem *applicationPathItem = mClientHints.find(AGENT_HINT_APPLICATION_PATH);
		if (applicationPathItem)
		  applicationPathItem->getString(applicationPath);

		getNoSA(answer, sizeof(answer), "Allow %s to do %d on %s in %s? [yn][g]%s ",
			applicationPath.c_str(), int(action), (description ? description : "[NULL item]"),
			(database ? database : "[NULL database]"),
			mPassphraseCheck ? ":passphrase" : "");
		// turn passphrase (no ':') into y:passphrase
		if (mPassphraseCheck && !strchr(answer, ':')) {
			memmove(answer+2, answer, strlen(answer)+1);
			memcpy(answer, "y:", 2);
		}

		allow = answer[0] == 'y';
		remember = answer[1] == 'g';
		return SecurityAgent::noReason;
    }
#endif
	
    hints.insert(mClientHints.begin(), mClientHints.end());

	// put action/operation (sint32) into hints
	hints.insert(AuthItemRef(AGENT_HINT_ACL_TAG, AuthValueOverlay(sizeof(action), static_cast<sint32*>(&action))));

    hints.insert(AuthItemRef(AGENT_HINT_CUSTOM_PROMPT, AuthValueOverlay(prompt ? (uint32_t)strlen(prompt) : 0, const_cast<char*>(prompt))));
	
	// item name into hints
	hints.insert(AuthItemRef(AGENT_HINT_KEYCHAIN_ITEM_NAME, AuthValueOverlay(description ? (uint32_t)strlen(description) : 0, const_cast<char*>(description))));
	
	// keychain name into hints
	hints.insert(AuthItemRef(AGENT_HINT_KEYCHAIN_PATH, AuthValueOverlay(database ? (uint32_t)strlen(database) : 0, const_cast<char*>(database))));
	
    create("builtin", "confirm-access-user-password", noSecuritySession);
    
    AuthItem *usernameItem;
    AuthItem *passwordItem;
    
    do {

        AuthItemRef triesHint(AGENT_HINT_TRIES, AuthValueOverlay(sizeof(retryCount), &retryCount));
        hints.erase(triesHint); hints.insert(triesHint); // replace
        
		if (++retryCount > maxTries)
			reason = SecurityAgent::tooManyTries;
		
        if (SecurityAgent::noReason != reason)
        {
            if (SecurityAgent::tooManyTries == reason)
                logger.logFailure(NULL,  CommonCriteria::errTooManyTries);
            else
                logger.logFailure();
        }

        AuthItemRef retryHint(AGENT_HINT_RETRY_REASON, AuthValueOverlay(sizeof(reason), &reason));
        hints.erase(retryHint); hints.insert(retryHint); // replace
		
        setInput(hints, context);
        try
        {
            invoke();
            checkResult();
        }
        catch (...)     // user probably clicked "deny"
        {
            logger.logFailure();
            throw;
        }
        usernameItem = mOutContext.find(AGENT_USERNAME);
		passwordItem = mOutContext.find(AGENT_PASSWORD);
		if (!usernameItem || !passwordItem)
			continue;
        usernameItem->getString(username);
        passwordItem->getString(password);
    } while ((reason = accept(username, password)));

    if (SecurityAgent::noReason == reason)
        logger.logSuccess();
    // else we logged the denial in the loop
    
    return reason;
}

Reason 
QueryKeychainAuth::accept(string &username, string &passphrase)
{
    const char *user = username.c_str();
    const char *passwd = passphrase.c_str();
    int checkpw_status = checkpw(user, passwd);
    
    if (checkpw_status != CHECKPW_SUCCESS)
		return SecurityAgent::invalidPassphrase;

	return SecurityAgent::noReason;
}


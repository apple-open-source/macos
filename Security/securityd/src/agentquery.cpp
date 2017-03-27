/*
 * Copyright (c) 2000-2015 Apple Inc. All Rights Reserved.
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
#define __STDC_WANT_LIB_EXT1__ 1
#include <string.h>

#include "agentquery.h"
#include "ccaudit_extensions.h"

#include <Security/AuthorizationTags.h>
#include <Security/AuthorizationTagsPriv.h>
#include <Security/checkpw.h>
#include <Security/Security.h>
#include <System/sys/fileport.h>
#include <bsm/audit.h>
#include <bsm/audit_uevents.h>      // AUE_ssauthint
#include <membership.h>
#include <membershipPriv.h>
#include <security_utilities/logging.h>
#include <security_utilities/mach++.h>
#include <stdlib.h>
#include <xpc/xpc.h>
#include <xpc/private.h>
#include "securityd_service/securityd_service/securityd_service_client.h"

#define SECURITYAGENT_BOOTSTRAP_NAME_BASE               "com.apple.security.agent"
#define SECURITYAGENT_LOGINWINDOW_BOOTSTRAP_NAME_BASE   "com.apple.security.agent.login"

#define AUTH_XPC_ITEM_NAME  "_item_name"
#define AUTH_XPC_ITEM_FLAGS "_item_flags"
#define AUTH_XPC_ITEM_VALUE "_item_value"
#define AUTH_XPC_ITEM_TYPE  "_item_type"
#define AUTH_XPC_ITEM_SENSITIVE_VALUE_LENGTH "_item_sensitive_value_length"

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

#define UUID_INITIALIZER_FROM_SESSIONID(sessionid) \
{ 0,0,0,0, 0,0,0,0, 0,0,0,0, (unsigned char)((0xff000000 & (sessionid))>>24), (unsigned char)((0x00ff0000 & (sessionid))>>16), (unsigned char)((0x0000ff00 & (sessionid))>>8),  (unsigned char)((0x000000ff & (sessionid))) }


// SecurityAgentXPCConnection

SecurityAgentXPCConnection::SecurityAgentXPCConnection(Session &session)
: mHostInstance(session.authhost()),
mSession(session),
mConnection(&Server::connection()),
mAuditToken(Server::connection().auditToken())
{
	// this may take a while
	Server::active().longTermActivity();
    secnotice("SecurityAgentConnection", "new SecurityAgentConnection(%p)", this);
    mXPCConnection = NULL;
    mNobodyUID = -2;
    struct passwd *pw = getpwnam("nobody");
    if (NULL != pw) {
        mNobodyUID = pw->pw_uid;
    }
}

SecurityAgentXPCConnection::~SecurityAgentXPCConnection()
{
    secnotice("SecurityAgentConnection", "SecurityAgentConnection(%p) dying", this);
	mConnection->useAgent(NULL);

    // If a connection has been established, we need to tear it down.
    if (NULL != mXPCConnection) {
        // Tearing this down is a multi-step process. First, request a cancellation.
        // This is safe even if the connection is already in the cancelled state.
        xpc_connection_cancel(mXPCConnection);

        // Then release the XPC connection
        xpc_release(mXPCConnection);
        mXPCConnection = NULL;
    }
}

bool SecurityAgentXPCConnection::inDarkWake()
{
	return mSession.server().inDarkWake();
}

void
SecurityAgentXPCConnection::activate(bool ignoreUid)
{
    secnotice("SecurityAgentConnection", "activate(%p)", this);

	mConnection->useAgent(this);
    if (mXPCConnection != NULL) {
        // If we already have an XPC connection, there's nothing to do.
        return;
    }

	try {
		uuid_t sessionUUID = UUID_INITIALIZER_FROM_SESSIONID(mSession.sessionId());

		// Yes, these need to be throws, as we're still in securityd, and thus still have to do flow control with exceptions.
		if (!(mSession.attributes() & sessionHasGraphicAccess))
			CssmError::throwMe(CSSM_ERRCODE_NO_USER_INTERACTION);
		if (inDarkWake())
			CssmError::throwMe(CSSM_ERRCODE_IN_DARK_WAKE);
		uid_t targetUid = mHostInstance->session().originatorUid();

		secnotice("SecurityAgentXPCConnection","Retrieved UID %d for this session", targetUid);
		if (!ignoreUid && targetUid != 0 && targetUid != mNobodyUID) {
			mXPCConnection = xpc_connection_create_mach_service(SECURITYAGENT_BOOTSTRAP_NAME_BASE, NULL, 0);
			xpc_connection_set_target_uid(mXPCConnection, targetUid);
			secnotice("SecurityAgentXPCConnection", "Creating a standard security agent");
		} else {
			mXPCConnection = xpc_connection_create_mach_service(SECURITYAGENT_LOGINWINDOW_BOOTSTRAP_NAME_BASE, NULL, 0);
			xpc_connection_set_instance(mXPCConnection, sessionUUID);
			secnotice("SecurityAgentXPCConnection", "Creating a loginwindow security agent");
		}

		xpc_connection_set_event_handler(mXPCConnection, ^(xpc_object_t object) {
			if (xpc_get_type(object) == XPC_TYPE_ERROR) {
				secnotice("SecurityAgentXPCConnection", "error during xpc: %s", xpc_dictionary_get_string(object, XPC_ERROR_KEY_DESCRIPTION));
			}
		});
		xpc_connection_resume(mXPCConnection);
		secnotice("SecurityAgentXPCConnection", "%p activated", this);
	}
    catch (MacOSError &err) {
		mConnection->useAgent(NULL);	// guess not
        Syslog::error("SecurityAgentConnection: error activating SecurityAgent instance %p", this);
		throw;
	}

    secnotice("SecurityAgentXPCConnection", "contact didn't throw (%p)", this);
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


SecurityAgentXPCQuery::SecurityAgentXPCQuery(Session &session)
: SecurityAgentXPCConnection(session), mAgentConnected(false), mTerminateOnSleep(false)
{
    secnotice("SecurityAgentXPCQuery", "new SecurityAgentXPCQuery(%p)", this);
}

SecurityAgentXPCQuery::~SecurityAgentXPCQuery()
{
    secnotice("SecurityAgentXPCQuery", "SecurityAgentXPCQuery(%p) dying", this);
    if (mAgentConnected) {
        this->disconnect();
    }
}

void
SecurityAgentXPCQuery::inferHints(Process &thisProcess)
{
    AuthItemSet clientHints;
    SecurityAgent::RequestorType type = SecurityAgent::bundle;
    pid_t clientPid = thisProcess.pid();
    uid_t clientUid = thisProcess.uid();
    string guestPath = thisProcess.getPath();
    Boolean ignoreSession = TRUE;

	clientHints.insert(AuthItemRef(AGENT_HINT_CLIENT_TYPE, AuthValueOverlay(sizeof(type), &type)));
	clientHints.insert(AuthItemRef(AGENT_HINT_CLIENT_PATH, AuthValueOverlay(guestPath)));
	clientHints.insert(AuthItemRef(AGENT_HINT_CLIENT_PID, AuthValueOverlay(sizeof(clientPid), &clientPid)));
	clientHints.insert(AuthItemRef(AGENT_HINT_CLIENT_UID, AuthValueOverlay(sizeof(clientUid), &clientUid)));

    /*
     * If its loginwindow that's asking, override the loginwindow shield detection
     * up front so that it can trigger SecurityAgent dialogs (like password change)
     * for when the OD password and keychain password is out of sync.
     */

    if (guestPath == "/System/Library/CoreServices/loginwindow.app") {
        clientHints.insert(AuthItemRef(AGENT_HINT_IGNORE_SESSION, AuthValueOverlay(sizeof(ignoreSession), &ignoreSession)));
    }

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
        void *dataCopy = 0;

        // <rdar://problem/13033889> authd is holding on to multiple copies of my password in the clear
        bool sensitive = xpc_dictionary_get_value(item, AUTH_XPC_ITEM_SENSITIVE_VALUE_LENGTH);
        if (sensitive) {
            size_t sensitiveLength = (size_t)xpc_dictionary_get_uint64(item, AUTH_XPC_ITEM_SENSITIVE_VALUE_LENGTH);
            dataCopy = malloc(sensitiveLength);
            memcpy(dataCopy, data, sensitiveLength);
            memset_s((void *)data, length, 0, sensitiveLength); // clear the sensitive data, memset_s is never optimized away
            length = sensitiveLength;
        } else {
            dataCopy = malloc(length);
            memcpy(dataCopy, data, length);
        }

        uint64_t flags = xpc_dictionary_get_uint64(item, AUTH_XPC_ITEM_FLAGS);
        AuthItemRef nextItem(name, AuthValueOverlay((uint32_t)length, dataCopy), (uint32_t)flags);
        setToBuild->insert(nextItem);
        memset(dataCopy, 0, length); // The authorization items contain things like passwords, so wiping clean is important.
        free(dataCopy);
        return true;
    });
}

void
SecurityAgentXPCQuery::create(const char *pluginId, const char *mechanismId)
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
		bool doSwitchAudit     = (ignoreUid || targetUid == 0 || targetUid == mNobodyUID);
		bool doSwitchBootstrap = (ignoreUid || targetUid == 0 || targetUid == mNobodyUID);

        if (doSwitchAudit) {
            mach_port_name_t jobPort;
            if (0 == audit_session_port(mSession.sessionId(), &jobPort)) {
                secnotice("SecurityAgentXPCQuery", "attaching an audit session port because the uid was %d", targetUid);
                xpc_dictionary_set_mach_send(requestObject, AUTH_XPC_AUDIT_SESSION_PORT, jobPort);
                if (mach_port_mod_refs(mach_task_self(), jobPort, MACH_PORT_RIGHT_SEND, -1) != KERN_SUCCESS) {
                    secnotice("SecurityAgentXPCQuery", "unable to release send right for audit session, leaking");
                }
            }
        }

        if (doSwitchBootstrap) {
            secnotice("SecurityAgentXPCQuery", "attaching a bootstrap port because the uid was %d", targetUid);
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
                    secnotice("SecurityAgentXPCQuery", "plugin create failed in SecurityAgent");
                    MacOSError::throwMe(errAuthorizationInternal);
                }
            }
        } else if (xpc_get_type(object) == XPC_TYPE_ERROR) {
            if (XPC_ERROR_CONNECTION_INVALID == object) {
                // If we get an error before getting the create response, try again without the UID
                if (ignoreUid) {
                    secnotice("SecurityAgentXPCQuery", "failed to establish connection, no retries left");
                    xpc_release(object);
                    MacOSError::throwMe(errAuthorizationInternal);
                } else {
                    secnotice("SecurityAgentXPCQuery", "failed to establish connection, retrying with no UID");
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
    uint32_t retryCount = 0;
	OSStatus status;
	AuthItemSet hints, context;

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
		create("builtin", "confirm-access-password");

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
		create("builtin", "confirm-access");
        setInput(hints, context);
		invoke();
	}

    readChoice();

	return reason;
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
	AuthItemSet hints, context;
	CssmAutoData passphrase(Allocator::standard(Allocator::sensitive));
	int retryCount = 0;

	// prepopulate with client hints

    const char *keychainPath = database.dbName();
    hints.insert(AuthItemRef(AGENT_HINT_KEYCHAIN_PATH, AuthValueOverlay((uint32_t)strlen(keychainPath), const_cast<char*>(keychainPath))));

	hints.insert(mClientHints.begin(), mClientHints.end());

	create("builtin", "unlock-keychain");

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
	AuthItemSet hints, context;
	CssmAutoData passphrase(Allocator::standard(Allocator::sensitive));
	int retryCount = 0;

	// prepopulate with client hints

    const char *keychainPath = "iCloud";
    hints.insert(AuthItemRef(AGENT_HINT_KEYCHAIN_PATH, AuthValueOverlay((uint32_t)strlen(keychainPath), const_cast<char*>(keychainPath))));

	hints.insert(mClientHints.begin(), mClientHints.end());

	create("builtin", "unlock-keychain");

    int currentTry = 0;
	do
	{
        currentTry = retryCount;
        if (retryCount > mRetries)
		{
			return SecurityAgent::tooManyTries;
        }
        retryCount++;

        AuthItemRef triesHint(AGENT_HINT_TRIES, AuthValueOverlay(sizeof(currentTry), &currentTry));
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
	AuthItemSet hints, context;
	int retryCount = 0;

	// prepopulate with client hints

    const char *keychainPath = "iCloud";
    hints.insert(AuthItemRef(AGENT_HINT_KEYCHAIN_PATH, AuthValueOverlay((uint32_t)strlen(keychainPath), const_cast<char*>(keychainPath))));

    const char *showResetString = "YES";
    hints.insert(AuthItemRef(AGENT_HINT_SHOW_RESET, AuthValueOverlay((uint32_t)strlen(showResetString), const_cast<char*>(showResetString))));

	hints.insert(mClientHints.begin(), mClientHints.end());

	create("builtin", "change-passphrase");

    int currentTry = 0;
    AuthItem *resetPassword = NULL;
	do
	{
        currentTry = retryCount;
        if (retryCount > mRetries)
		{
			return SecurityAgent::tooManyTries;
        }
        retryCount++;

        AuthItemRef triesHint(AGENT_HINT_TRIES, AuthValueOverlay(sizeof(currentTry), &currentTry));
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
	AuthItemSet hints, context;

	int retryCount = 0;

	// prepopulate with client hints
	hints.insert(mClientHints.begin(), mClientHints.end());

	// keychain name into hints
	hints.insert(AuthItemRef(AGENT_HINT_KEYCHAIN_PATH, AuthValueOverlay(database.dbName())));

    switch (initialReason)
    {
        case SecurityAgent::newDatabase:
            create("builtin", "new-passphrase");
            break;
        case SecurityAgent::changePassphrase:
            create("builtin", "change-passphrase");
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
    AuthItemSet hints, context;

    hints.insert(mClientHints.begin(), mClientHints.end());
    hints.insert(AuthItemRef(AGENT_HINT_CUSTOM_PROMPT, AuthValueOverlay(prompt ? (UInt32)prompt->length() : 0, prompt ? prompt->data() : NULL)));
    // XXX/gh  defined by dmitch but no analogous hint in
    // AuthorizationTagsPriv.h:
    // CSSM_ATTRIBUTE_ALERT_TITLE (optional alert panel title)

    if (false == verify) {  // import
		create("builtin", "generic-unlock");
    } else {		// verify passphrase (export)
		create("builtin", "generic-new-passphrase");
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
    AuthItemSet hints/*NUKEME*/, context;

	hints.insert(mClientHints.begin(), mClientHints.end());
	create("builtin", "generic-unlock-kcblob");

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
			dbToUnlock->unlockDb(passphrase, false);
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

// @@@  no pluggable authentication possible!
Reason
QueryKeychainAuth::operator () (const char *database, const char *description, AclAuthorization action, const char *prompt)
{
    Reason reason = SecurityAgent::noReason;
    AuthItemSet hints, context;
	int retryCount = 0;
	string username;
	string password;

    using CommonCriteria::Securityd::KeychainAuthLogger;
    KeychainAuthLogger logger(mAuditToken, AUE_ssauthint, database, description);

    hints.insert(mClientHints.begin(), mClientHints.end());

	// put action/operation (sint32) into hints
	hints.insert(AuthItemRef(AGENT_HINT_ACL_TAG, AuthValueOverlay(sizeof(action), static_cast<sint32*>(&action))));

    hints.insert(AuthItemRef(AGENT_HINT_CUSTOM_PROMPT, AuthValueOverlay(prompt ? (uint32_t)strlen(prompt) : 0, const_cast<char*>(prompt))));

	// item name into hints
	hints.insert(AuthItemRef(AGENT_HINT_KEYCHAIN_ITEM_NAME, AuthValueOverlay(description ? (uint32_t)strlen(description) : 0, const_cast<char*>(description))));

	// keychain name into hints
	hints.insert(AuthItemRef(AGENT_HINT_KEYCHAIN_PATH, AuthValueOverlay(database ? (uint32_t)strlen(database) : 0, const_cast<char*>(database))));

    create("builtin", "confirm-access-user-password");

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
	// Note: QueryKeychainAuth currently requires that the
	// specified user be in the admin group. If this requirement
	// ever needs to change, the group name should be passed as
	// a separate argument to this method.

	const char *user = username.c_str();
	const char *passwd = passphrase.c_str();
	int checkpw_status = checkpw(user, passwd);

	if (checkpw_status != CHECKPW_SUCCESS) {
		return SecurityAgent::invalidPassphrase;
	}

	const char *group = "admin";
	if (group) {
		int rc, ismember;
		uuid_t group_uuid, user_uuid;
		rc = mbr_group_name_to_uuid(group, group_uuid);
		if (rc) { return SecurityAgent::userNotInGroup; }

		rc = mbr_user_name_to_uuid(user, user_uuid);
		if (rc) { return SecurityAgent::userNotInGroup; }

		rc = mbr_check_membership(user_uuid, group_uuid, &ismember);
		if (rc || !ismember) { return SecurityAgent::userNotInGroup; }
	}

	return SecurityAgent::noReason;
}


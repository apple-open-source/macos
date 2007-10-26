/*
 *  Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
 *
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 *
 *  AuthorizationMechEval.cpp
 *  securityd
 *
 */
#include "AuthorizationMechEval.h"
#include <security_utilities/logging.h>
#include <bsm/audit_uevents.h>
#include <security_utilities/ccaudit.h>

namespace Authorization {

AgentMechanismRef::AgentMechanismRef(const AuthHostType type, Session &session) : 
    RefPointer<QueryInvokeMechanism>(new QueryInvokeMechanism(type, session)) {}

// we need the vector<string> of mechanisms
AgentMechanismEvaluator::AgentMechanismEvaluator(uid_t uid, Session& session, const vector<string>& inMechanisms) : 
    mMechanisms(inMechanisms), mClientUid(uid), mSession(session)
{
    //set up environment
}

OSStatus
AgentMechanismEvaluator::run(const AuthValueVector &inArguments, const AuthItemSet &inHints, const AuthorizationToken &auth)
{
    const AuthItemSet &inContext = const_cast<AuthorizationToken &>(auth).infoSet();
    
    // add process specifics to context?

    vector<std::string>::const_iterator currentMechanism = mMechanisms.begin();
    
    AuthorizationResult result = kAuthorizationResultAllow;
    
    AuthItemSet hints = inHints;
    AuthItemSet context = inContext;
    // add saved-off sticky context values to context for evaluation
    context.insert(mStickyContext.begin(), mStickyContext.end());
	
    while ( (result == kAuthorizationResultAllow)  &&
            (currentMechanism != mMechanisms.end()) ) // iterate mechanisms
    {
        ClientMap::iterator iter = mClients.find(*currentMechanism);
        if (iter == mClients.end())
        {
            string::size_type extPlugin = currentMechanism->find(':');
            if (extPlugin != string::npos)
            {
                // no whitespace removal
                string pluginIn(currentMechanism->substr(0, extPlugin));
				string mechanismIn, authhostIn;
				
				string::size_type extMechanism = currentMechanism->rfind(',');
				AuthHostType hostType = securityAgent;
				
				if (extMechanism != string::npos)
				{
					if (extMechanism < extPlugin)
						return errAuthorizationInternal;
						
					mechanismIn = currentMechanism->substr(extPlugin + 1, extMechanism - extPlugin - 1);
					authhostIn = currentMechanism->substr(extMechanism + 1);
					if (authhostIn == "privileged")
						hostType = privilegedAuthHost;
				}
				else
					mechanismIn = currentMechanism->substr(extPlugin + 1);
					
                secdebug("AuthEvalMech", "external mechanism %s:%s", pluginIn.c_str(), mechanismIn.c_str());
                
                AgentMechanismRef client(hostType, mSession);
                client->initialize(pluginIn, mechanismIn, inArguments);
                mClients.insert(ClientMap::value_type(*currentMechanism, client));
            }
            else if (*currentMechanism == "authinternal")
            {
                secdebug("AuthEvalMech", "performing authentication");
                result = authinternal(context);

                AuthItem *rightItem = hints.find(AGENT_HINT_AUTHORIZE_RIGHT);
                string right = (rightItem == NULL) ? string("<unknown right>") : rightItem->stringValue();
				CommonCriteria::AuditRecord auditrec(auth.creatorAuditToken());
				if (kAuthorizationResultAllow == result)
					auditrec.submit(AUE_ssauthint, CommonCriteria::errNone, right.c_str());
				else	// kAuthorizationResultDeny
					auditrec.submit(AUE_ssauthint, CommonCriteria::errInvalidCredential, right.c_str());
            }
            else if (*currentMechanism == "push_hints_to_context")
            {
                secdebug("AuthEvalMech", "evaluate push_hints_to_context");
				// doesn't block evaluation, ever
                result = kAuthorizationResultAllow; 
                context = hints;
            }
            else
                return errAuthorizationInternal;
        }

        iter = mClients.find(*currentMechanism);
        if (iter != mClients.end())
        {
            try
            {
                AgentMechanismRef &client = iter->second;
                client->run(inArguments, hints, context, &result);

				bool interrupted = false;
				while (client->state() == client->current)
				{
					// check for interruption
					vector<std::string>::const_iterator checkMechanism = mMechanisms.begin();
					while (*checkMechanism != *currentMechanism) {
						ClientMap::iterator iter2 = mClients.find(*checkMechanism);
						if (iter2->second->state() == iter2->second->interrupting)
						{
							client->deactivate();
							// nothing can happen until the client mechanism returns control to us
							while (client->state() == client->deactivating)
								client->receive();
								
							secdebug("AuthEvalMech", "evaluate(%s) interrupted by %s.", (iter->first).c_str(), (iter2->first).c_str());

							interrupted = true;
							hints = iter2->second->inHints();
							context = iter2->second->inContext();
							currentMechanism = checkMechanism;
							break;
						}
						else
							checkMechanism++;
					}
					if (client->state() == client->current)
						client->receive();
				} 
				
                if (interrupted)
                {
					// clear reason for restart from interrupt
					uint32_t reason = SecurityAgent::worldChanged;
					AuthItemRef retryHint(AGENT_HINT_RETRY_REASON, AuthValueOverlay(sizeof(reason), &reason));
					hints.erase(retryHint); hints.insert(retryHint); // replace

                    result = kAuthorizationResultAllow;
                    continue;
                }
				else
					secdebug("AuthEvalMech", "evaluate(%s) with result: %lu.", (iter->first).c_str(), result);
            }
            catch (...) {
                secdebug("AuthEvalMech", "exception during evaluate(%s).", (iter->first).c_str());
                result = kAuthorizationResultUndefined;
            }
        }
    
        if (result == kAuthorizationResultAllow)
            currentMechanism++;
    }
    
    if ((result == kAuthorizationResultUserCanceled) ||
        (result == kAuthorizationResultAllow))
    {
        mHints = hints;
        mContext.clear(); 
        // only make non-sticky context values available externally
        AuthItemSet::const_iterator end = context.end();
        for (AuthItemSet::const_iterator it = context.begin(); it != end; ++it) {
            const AuthItemRef &item = *it;
            if (item->flags() != kAuthorizationContextFlagSticky)
                mContext.insert(item);
        }
    }
    else if (result == kAuthorizationResultDeny)
    {
        // save off sticky values in context
        mStickyContext.clear();
        AuthItemSet::const_iterator end = context.end();
        for (AuthItemSet::const_iterator it = context.begin(); it != end; ++it) {
            const AuthItemRef &item = *it;
            if (item->flags() == kAuthorizationContextFlagSticky)
                mStickyContext.insert(item);
        }
    }
    
    // convert AuthorizationResult to OSStatus
    switch(result)
    {
        case kAuthorizationResultDeny:
            return errAuthorizationDenied;
        case kAuthorizationResultUserCanceled:
            return errAuthorizationCanceled;
        case kAuthorizationResultAllow:
            return errAuthorizationSuccess;
        default:
            return errAuthorizationInternal;
    }    
}

AuthorizationResult AgentMechanismEvaluator::authinternal(AuthItemSet &context)
{
    secdebug("AuthEvalMech", "evaluate authinternal");
    do {
        AuthItemSet::iterator found = find_if(context.begin(), context.end(), FindAuthItemByRightName(kAuthorizationEnvironmentUsername) );
        if (found == context.end())
            break;
        string username(static_cast<const char *>((*found)->value().data), (*found)->value().length);
        secdebug("AuthEvalMech", "found username");
        found = find_if(context.begin(), context.end(), FindAuthItemByRightName(kAuthorizationEnvironmentPassword) );
        if (found == context.end())
            break;
        string password(static_cast<const char *>((*found)->value().data), (*found)->value().length);
        secdebug("AuthEvalMech", "found password");

        Credential newCredential(username, password, true); // create a new shared credential
        if (newCredential->isValid())
            return kAuthorizationResultAllow;
        
    } while (0);
    
    return kAuthorizationResultDeny;
}

/*
AuthItemSet &
AgentMechanismEvaluator::commonHints(const AuthorizationToken &auth)
{
    
}
*/

} /* namespace Authorization */

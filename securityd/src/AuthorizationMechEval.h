/*
 *  Copyright (c) 2003-2004,2008 Apple Inc. All Rights Reserved.
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
 *  AuthorizationMechEval.h
 *  securityd
 *
 */

#include <string>
#include <map>
#include <security_utilities/refcount.h>
#include "agentquery.h"
#include "AuthorizationRule.h"
#include "authority.h"
#include "session.h"


namespace Authorization {
   
class AgentMechanismRef : public RefPointer<QueryInvokeMechanism>
{
public:
    AgentMechanismRef(const AuthHostType type, Session &session);
};
    
class AgentMechanismEvaluator
{
public:
    AgentMechanismEvaluator(uid_t uid, Session &session, const vector<string>& inMechanisms);
    OSStatus run(const AuthValueVector &inArguments, const AuthItemSet &inHints, const AuthorizationToken &auth);

    AuthorizationResult authinternal(AuthItemSet &context);
    
    AuthItemSet &hints() { return mHints; }
    AuthItemSet &context() { return mContext; }
    
private:
    vector<std::string> mMechanisms;
    typedef map<std::string, AgentMechanismRef> ClientMap;
    ClientMap mClients;
    
    uid_t mClientUid;
    Session &mSession;
    
    AuthItemSet mHints;
    AuthItemSet mContext;
    AuthItemSet mStickyContext;
};

} /* namespace Authorization */

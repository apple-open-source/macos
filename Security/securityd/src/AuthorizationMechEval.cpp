/*
 *  Copyright (c) 2003-2009 Apple Inc. All Rights Reserved.
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
#include "ccaudit_extensions.h"

namespace Authorization {

using namespace CommonCriteria::Securityd;

AgentMechanismRef::AgentMechanismRef(const AuthHostType type, Session &session) {}

// we need the vector<string> of mechanisms
AgentMechanismEvaluator::AgentMechanismEvaluator(uid_t uid, Session& session, const vector<string>& inMechanisms) : 
    mMechanisms(inMechanisms), mClientUid(uid), mSession(session)
{
    //set up environment
}

OSStatus
AgentMechanismEvaluator::run(const AuthValueVector &inArguments, const AuthItemSet &inHints, const AuthorizationToken &auth)
{
	return errAuthorizationDenied;
}

AuthorizationResult AgentMechanismEvaluator::authinternal(AuthItemSet &context)
{
    return kAuthorizationResultDeny;
}

} /* namespace Authorization */

/*
 * Copyright (c) 2000-2007,2009-2010,2012 Apple Inc. All Rights Reserved.
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

#include <paths.h>
#include <fcntl.h>
#include "authhost.h"
#include "server.h"
#include <security_utilities/logging.h>
#include <security_utilities/debugging.h>
#include <bsm/audit.h>
#include <bootstrap_priv.h>

#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <syslog.h>
#include <pthread.h>

static pthread_once_t agent_cred_init = PTHREAD_ONCE_INIT; 
static gid_t agent_gid = 92;
static uid_t agent_uid = 92;

static void initialize_agent_creds()
{
    struct passwd *agentUser = getpwnam("securityagent");
    if (agentUser)
    {
        agent_uid = agentUser->pw_uid;
        agent_gid = agentUser->pw_gid;
        endpwent();
    }
}
  
AuthHostInstance::AuthHostInstance(Session &session)
{
	secinfo("authhost", "authhost born (%p)", this);
	referent(session);
	session.addReference(*this);
	pthread_once(&agent_cred_init, initialize_agent_creds);
}

AuthHostInstance::~AuthHostInstance()
{ 
	secinfo("authhost", "authhost died (%p)", this);
}

Session &AuthHostInstance::session() const
{
	return referent<Session>();
}

bool AuthHostInstance::inDarkWake()
{
	return session().server().inDarkWake();
}

void
AuthHostInstance::childAction()
{
	secinfo("AuthHostInstance", "authhostinstance not supported");
	// Unconditional suicide follows.
	_exit(1);
}

/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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

#include <grp.h>
#include <pwd.h>


AuthHostInstance::AuthHostInstance(Session &session, AuthHostType host) :
	mHostType(host)
{
	secdebug("authhost", "authhost born (%p)", this);
	referent(session);
	session.addReference(*this);
}

AuthHostInstance::~AuthHostInstance()
{ 
	secdebug("authhost", "authhost died (%p)", this);
	
	// clean up
	servicePort ().destroy ();
}

Session &AuthHostInstance::session() const
{
	return referent<Session>();
}

void
AuthHostInstance::childAction()
{
	// Setup the environment for the SecurityAgent
	unsetenv("USER");
	unsetenv("LOGNAME");
	unsetenv("HOME");
				
	// close down any files that might have been open at this point
	int maxDescriptors = getdtablesize ();
	int i;
	
	int devnull = open(_PATH_DEVNULL, O_RDWR, 0);
	if (devnull >= 0) for (i = 0; i < 3; ++i)
	{
		dup2(devnull, i);
	}
	
	for (i = 3; i < maxDescriptors; ++i)
	{
		close (i);
	}
	
	// construct path to SecurityAgent
	char agentExecutable[PATH_MAX + 1];
	const char *path = getenv("SECURITYAGENT");
	if (!path)
		path = "/System/Library/CoreServices/SecurityAgent.app";

	if ((mHostType == userAuthHost) || (mHostType == privilegedAuthHost))
	{
		snprintf(agentExecutable, sizeof(agentExecutable), "%s/Contents/Resources/authorizationhost", path);
	
		secdebug("AuthHostInstance", "execl(%s)", agentExecutable);
		execl(agentExecutable, agentExecutable, NULL);
	}
	else
	{
		snprintf(agentExecutable, sizeof(agentExecutable), "%s/Contents/MacOS/SecurityAgent", path);

		struct group *agentGroup = getgrnam("securityagent");
		gid_t agentGID = static_cast<gid_t>(-2);
		if (agentGroup)
		{
			agentGID = agentGroup->gr_gid;
			endgrent();
		}

		struct passwd *agentUser = getpwnam("securityagent");
		uid_t agentUID = static_cast<uid_t>(-2);
		if (agentUser)
		{
			agentUID = agentUser->pw_uid;
			endpwent();
		}

		setuid(agentUID);
		setgid(agentGID);
	
		CFRef<CFDataRef> userPrefs(session().copyUserPrefs());
		
		FILE *mbox = tmpfile();
		
		if (userPrefs && mbox)
		{
			if (fwrite(CFDataGetBytePtr(userPrefs), CFDataGetLength(userPrefs), 1, mbox) != 1)
				fclose(mbox);
			else
			{
				char mboxFdString[20];
				fflush(mbox);
				if ((int)sizeof(mboxFdString) > snprintf(mboxFdString, sizeof(mboxFdString), "%d", fileno(mbox)))
					setenv("SECURITYAGENT_USERPREFS_FD", mboxFdString, 1);
			}
		}
		
		secdebug("AuthHostInstance", "execl(%s) as user (%d,%d)", agentExecutable, agentUID, agentGID);
		execl(agentExecutable, agentExecutable, NULL);
	}

	secdebug("AuthHostInstance", "execl failed, errno=%d", errno);
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

Port
AuthHostInstance::activate()
{
	StLock<Mutex> _(*this);
	if (state() != alive)
	{
		if (!(session().attributes() & sessionHasGraphicAccess))
			CssmError::throwMe(CSSM_ERRCODE_NO_USER_INTERACTION);

		Security::MachPlusPlus::StBootstrap bootSaver(session().bootstrapPort());

		fork();
		switch (ServerChild::state()) {
		case Child::alive:
			secdebug("AuthHostInstance", "%p (pid %d) has launched", this, pid());
			break;
		case Child::dead:
			secdebug("AuthHostInstance", "%p (pid %d) failed on startup", this, pid());
			break;
		default:
			assert(false);
		}
	}

	if (!ready())
		CssmError::throwMe(CSSM_ERRCODE_NO_USER_INTERACTION);

	return servicePort();
}

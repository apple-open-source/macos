/*
 * Copyright (c) 2004,2009-2010,2012 Apple Inc. All Rights Reserved.
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
#ifndef _H_AUTHHOST
#define _H_AUTHHOST

#include <security_agent_client/sa_types.h>

#include "structure.h"
#include "child.h"

class Session;

typedef enum {
	privilegedAuthHost,
	securityAgent,
	userAuthHost
} AuthHostType;

class AuthHostInstance : public PerSession, public ServerChild {
public:
	AuthHostInstance(Session &session, AuthHostType host);
	virtual ~AuthHostInstance();

	Session &session() const;
	mach_port_t lookup(SessionId jobId);
	Port activate();
		
protected:
	void childAction();

private:
	AuthHostType mHostType;

	bool inDarkWake();
};

#endif /* _H_AUTHHOST */

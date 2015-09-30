/*
 * Copyright (c) 2003-2004,2006,2011,2014 Apple Inc. All Rights Reserved.
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

#ifndef _H_EVENTLISTENER
#define _H_EVENTLISTENER

#include <securityd_client/ssclient.h>
#include <security_utilities/cfmach++.h>
#include <security_utilities/refcount.h>

namespace Security {
namespace SecurityServer {


//
// A CFNotificationDispatcher registers with the local CFRunLoop to automatically
// receive notification messages and dispatch them.
//
class EventListener : public RefCount
{
protected:
	NotificationDomain mDomain;
	NotificationMask mMask;

public:
	EventListener(NotificationDomain domain, NotificationMask eventMask);
	virtual ~EventListener();

	virtual void consume(NotificationDomain domain, NotificationEvent event, const CssmData& data);
	
	NotificationDomain GetDomain () {return mDomain;}
	NotificationMask GetMask () {return mMask;}
    
    static void FinishedInitialization(EventListener* eventListener);
};


} // end namespace SecurityServer
} // end namespace Security


#endif

/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 *  KCEventObserver.h -- OS X CF Observer for Keychain Events
 */
#ifndef _SECURITY_KCEVENTOBSERVER_H_
#define _SECURITY_KCEVENTOBSERVER_H_

#include "dictionary.h"
#include "eventlistener.h"
#include "notifications.h"


namespace Security
{

class Observer : private SecurityServer::EventListener
{
protected:
	void EventReceived (Listener::Domain domain, Listener::Event event, const void* data, size_t dataLength);

public:
    Observer (Listener::Domain whichDomain, Listener::EventMask whichEvents);
    virtual	~Observer ();
    virtual void Event (Listener::Domain domain, Listener::Event whichEvent, NameValueDictionary &dictionary) = 0;
};

} // end namespace Security

#endif // !_SECURITY_KCEVENTOBSERVER_H_

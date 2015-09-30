/*
 * Copyright (c) 1998-2004,2011,2014 Apple Inc. All Rights Reserved.
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


/*
 *  CCallbackMgr.h -- Code that communicates with processes that install a callback
 *  with the Keychain Manager to receive keychain events.
 */
#ifndef _SECURITY_CCALLBACKMGR_H_
#define _SECURITY_CCALLBACKMGR_H_

#include <security_keychain/Keychains.h>
#include <security_utilities/cfmach++.h>
#include <securityd_client/ssnotify.h>
#include <securityd_client/dictionary.h>
#include <securityd_client/eventlistener.h>
#include <list>
#include "KCEventNotifier.h"

namespace Security
{

namespace KeychainCore
{

class CallbackInfo;
class CCallbackMgr;

class CallbackInfo
{
public:
	~CallbackInfo();
	CallbackInfo();
	CallbackInfo(SecKeychainCallback inCallbackFunction,SecKeychainEventMask inEventMask,void *inContext);
	
	bool operator ==(const CallbackInfo& other) const;
	bool operator !=(const CallbackInfo& other) const;

	SecKeychainCallback mCallback;
	SecKeychainEventMask mEventMask;
	void *mContext;
};

// typedefs
typedef CallbackInfo *CallbackInfoPtr;
typedef CallbackInfo const *ConstCallbackInfoPtr;

typedef list<CallbackInfo>::iterator CallbackInfoListIterator;
typedef list<CallbackInfo>::const_iterator ConstCallbackInfoListIterator;


class CCallbackMgr : public SecurityServer::EventListener
{
public:
	CCallbackMgr();
	~CCallbackMgr();
	
	static CCallbackMgr& Instance();

	static void AddCallback( SecKeychainCallback inCallbackFunction, SecKeychainEventMask inEventMask, void* inContext);

	static void RemoveCallback( SecKeychainCallback inCallbackFunction );
    //static void RemoveCallbackUPP(KCCallbackUPP inCallbackFunction);
	static bool HasCallbacks()
	{ return CCallbackMgr::Instance().mEventCallbacks.size() > 0; };
	
private:

	void consume (SecurityServer::NotificationDomain domain, SecurityServer::NotificationEvent whichEvent,
				  const CssmData &data);
	
	static void AlertClients(const list<CallbackInfo> &eventCallbacks, SecKeychainEvent inEvent, pid_t inPid,
							 const Keychain& inKeychain, const Item &inItem);

	list<CallbackInfo> 		mEventCallbacks;
};

} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_CCALLBACKMGR_H_

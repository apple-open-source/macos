/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
	File:		CCallbackMgr.h

	Contains:	Code that communicates with processes that install a callback
                with the Keychain Manager to receive keychain events.

	Written by:	Sari Harrison, Craig Mortensen

	Copyright:	© 1998-2000 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

	To Do:
*/

#ifndef __CCALLBACKMGR__
#define __CCALLBACKMGR__

#include <Security/SecKeychainAPI.h>
#include <Security/KCEventObserver.h>
#include <Security/KCEventNotifier.h>
#include <Security/Keychains.h>
#include <list>

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
	CallbackInfo(SecKeychainCallbackProcPtr inCallbackFunction,SecKeychainEventMask inEventMask,void *inContext);
	
	bool operator ==(const CallbackInfo& other) const;
	bool operator !=(const CallbackInfo& other) const;

	SecKeychainCallbackProcPtr			mCallback;
	SecKeychainEventMask				mEventMask;
	void						*mContext;
};

// typedefs
typedef CallbackInfo *CallbackInfoPtr;
typedef CallbackInfo const *ConstCallbackInfoPtr;

typedef list<CallbackInfo>::iterator CallbackInfoListIterator;
typedef list<CallbackInfo>::const_iterator ConstCallbackInfoListIterator;

#ifdef _CPP_CCALLBACKMGR
# pragma export on
#endif


class CCallbackMgr : Observer
{
public:
	
	CCallbackMgr();
	~CCallbackMgr();
	
	static CCallbackMgr& Instance();

	static void AddCallback( SecKeychainCallbackProcPtr inCallbackFunction, SecKeychainEventMask inEventMask, void* inContext);
	//static void AddCallbackUPP(KCCallbackUPP inCallbackFunction, KCEventMask inEventMask, void* inContext);

	static void RemoveCallback( SecKeychainCallbackProcPtr inCallbackFunction );
    //static void RemoveCallbackUPP(KCCallbackUPP inCallbackFunction);
	static bool HasCallbacks() { return CCallbackMgr::Instance().mEventCallbacks.size() > 0; };
	static bool ThisProcessUsesSystemEvtCallback();
	static bool ThisProcessCanDisplayUI();
	
	static void AlertClients( SecKeychainEvent inEvent, bool inOKToAllocateMemory);
#if 0
	static void Idle();
#endif

private:

    virtual void 	Event ( CFNotificationCenterRef center, 
                            CFStringRef 			name, 
                            const void*				object, 
                            CFDictionaryRef 		userInfo );

	static void AlertClients( SecKeychainEvent inEvent, const Keychain& inKeychain,
                                const Item &inItem, bool inOKToAllocateMemory = true);

	list<CallbackInfo> 		mEventCallbacks;
	static CCallbackMgr* 	mCCallbackMgr;
};

} // end namespace KeychainCore

} // end namespace Security

#endif // __CCALLBACKMGR__

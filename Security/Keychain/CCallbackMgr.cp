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
	File:		CCallbackMgr.cp

	Contains:	Code that communicates with processes that install a callback
                with the Keychain Manager to receive keychain events.

	Written by:	Sari Harrison, Craig Mortensen

	Copyright:	© 1998-2000 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

	To Do:
*/

#include "CCallbackMgr.h"

#include <algorithm>
#include <list>

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>
#include "Globals.h"
#include <Security/DLDBListCFPref.h>

//using namespace std;
using namespace KeychainCore;
using namespace CssmClient;

static const UInt32 kTicksBetweenIdleEvents = 5L;

#pragma mark ÑÑÑÑ CallbackInfo ÑÑÑÑ

CallbackInfo::CallbackInfo() : mCallback(NULL),mEventMask(0),mContext(NULL)
{
}

CallbackInfo::CallbackInfo(SecKeychainCallbackProcPtr inCallbackFunction,SecKeychainEventMask inEventMask,void *inContext)
	 : mCallback(inCallbackFunction),mEventMask(inEventMask),mContext(inContext)
{
}

CallbackInfo::~CallbackInfo()
{
}

bool CallbackInfo::operator==(const CallbackInfo& other) const
{
    return mCallback==other.mCallback;
}

bool CallbackInfo::operator!=(const CallbackInfo& other) const
{
	return !(*this==other);
}


#pragma mark ÑÑÑÑ CCallbackMgr ÑÑÑÑ

CCallbackMgr *CCallbackMgr::mCCallbackMgr;

CCallbackMgr::CCallbackMgr() :
    // register for receiving Keychain events via CF
    Observer( kSecEventNotificationName, NULL, CFNotificationSuspensionBehaviorDeliverImmediately )
{
}

CCallbackMgr::~CCallbackMgr()
{
}

CCallbackMgr& CCallbackMgr::Instance()
{
	if (!mCCallbackMgr)
		mCCallbackMgr = new CCallbackMgr();	

	return *mCCallbackMgr;
}

void CCallbackMgr::AddCallback( SecKeychainCallbackProcPtr inCallbackFunction, 
                             SecKeychainEventMask 	inEventMask,
                             void* 			inContext)

{
	CallbackInfo info( inCallbackFunction, inEventMask, inContext );
	CallbackInfo existingInfo;


    CallbackInfoListIterator ix = find( CCallbackMgr::Instance().mEventCallbacks.begin(),
                                        CCallbackMgr::Instance().mEventCallbacks.end(), info );
	
	// make sure it is not already there
	if ( ix!=CCallbackMgr::Instance().mEventCallbacks.end() )
    {
        // It's already there. This could mean that the old process died unexpectedly,
        // so we need to validate the process ID of the existing callback.
        // On Mac OS X this list is per process so this is always a duplicate
		MacOSError::throwMe(errSecDuplicateCallback);
	}
	
	CCallbackMgr::Instance().mEventCallbacks.push_back(info);
}

#if 0
void CCallbackMgr::AddCallbackUPP(KCCallbackUPP 	inCallbackFunction,
                               KCEventMask 		inEventMask,
                               void*			inContext)
{
	CallbackInfo info( reinterpret_cast<SecKeychainCallbackProcPtr>(inCallbackFunction), inEventMask, inContext );
	CallbackInfo existingInfo;

#if TARGET_API_MAC_OS8
    OSErr err = noErr;
	err = ::GetCurrentProcess( &info.mProcessID );
	KCThrowIf_( err );
#endif

    CallbackInfoListIterator ix = find( CCallbackMgr::Instance().mEventCallbacks.begin(),
                                        CCallbackMgr::Instance().mEventCallbacks.end(), info );
	
	// make sure it is not already there
	if ( ix!=CCallbackMgr::Instance().mEventCallbacks.end() )
    {
        // It's already there. This could mean that the old process died unexpectedly,
        // so we need to validate the process ID of the existing callback.
#if TARGET_API_MAC_OS8
		if (ValidProcess(ix->mProcessID))	// existing callback is OK, so don't add this one.
			MacOSError::throwMe(errKCDuplicateCallback);

		// Process is gone, so remove the old entry
		CCallbackMgr::Instance().mEventCallbacks.erase(ix);
#else
        // On Mac OS X this list is per process so this is always a duplicate
		MacOSError::throwMe(errKCDuplicateCallback);
#endif
	}
	
	CCallbackMgr::Instance().mEventCallbacks.push_back(info);
}
#endif


class Predicate
{
	SecKeychainCallbackProcPtr mCallbackFunction;
public:
	Predicate(SecKeychainCallbackProcPtr inCallbackFunction) : mCallbackFunction(inCallbackFunction) {}
	bool operator()(const CallbackInfo &cbInfo) { return cbInfo.mCallback == mCallbackFunction; }
};

void CCallbackMgr::RemoveCallback(SecKeychainCallbackProcPtr inCallbackFunction)
{
	size_t oldSize = CCallbackMgr::Instance().mEventCallbacks.size();
	Predicate predicate(inCallbackFunction);
	CCallbackMgr::Instance().mEventCallbacks.remove_if(predicate);

	if (oldSize == CCallbackMgr::Instance().mEventCallbacks.size())
		MacOSError::throwMe(errSecInvalidCallback);
}

#if 0
void CCallbackMgr::RemoveCallbackUPP(KCCallbackUPP inCallbackFunction)
{
	size_t oldSize = CCallbackMgr::Instance().mEventCallbacks.size();
	Predicate predicate(reinterpret_cast<SecKeychainCallbackProcPtr>(inCallbackFunction));
	CCallbackMgr::Instance().mEventCallbacks.remove_if(predicate);

	if (oldSize == CCallbackMgr::Instance().mEventCallbacks.size())
		MacOSError::throwMe(errKCInvalidCallback);
}
#endif

bool CCallbackMgr::ThisProcessUsesSystemEvtCallback()
{
	const SecKeychainEventMask theMask = 1 << kSecSystemEvent;


	for ( CallbackInfoListIterator ix = CCallbackMgr::Instance().mEventCallbacks.begin();
		  ix!=CCallbackMgr::Instance().mEventCallbacks.end(); ++ix )
	{
        if ( ix->mEventMask & theMask)
            return true;
	}
	return false;
}

//%%% jch move this function to SecurityHI
bool CCallbackMgr::ThisProcessCanDisplayUI()
{
    return true;
}

#if 0
void CCallbackMgr::Idle()
{
	static unsigned long lastTickCount = 0;
	unsigned long tickCount = ::TickCount( );
	
	if (tickCount > lastTickCount+kTicksBetweenIdleEvents)
	{
    	lastTickCount = tickCount;
	}
}
#endif

void CCallbackMgr::AlertClients(SecKeychainEvent inEvent, bool inOKToAllocateMemory)
{
    AlertClients(inEvent, Keychain(), Item(), inOKToAllocateMemory);
}

void CCallbackMgr::AlertClients(SecKeychainEvent inEvent,
                                const Keychain &inKeychain,
                                const Item &inItem,
                                bool inOKToAllocateMemory)
{
    // Deal with events that we care about ourselves first.
    if (inEvent == kSecDefaultChangedEvent)
        globals().defaultKeychain.reload(true);
    else if (inEvent == kSecKeychainListChangedEvent)
        globals().storageManager.reload(true);

	// Iterate through callbacks, looking for those registered for inEvent
	const SecKeychainEventMask theMask = 1U << inEvent;

	for ( CallbackInfoListIterator ix = CCallbackMgr::Instance().mEventCallbacks.begin();
		ix != CCallbackMgr::Instance().mEventCallbacks.end(); ++ix )
	{
		if (!(ix->mEventMask & theMask))
			continue;

		SecKeychainCallbackInfo	cbInfo;
		cbInfo.version = 0; // @@@ kKeychainAPIVersion;
		cbInfo.item = inItem ? ItemRef::handle(inItem) : 0;
		cbInfo.keychain = inKeychain ? KeychainRef::handle(inKeychain) : 0;

#if 0
        //%%%cpm- need to change keychaincore.i so we don't to the reinterpret_cast
        // we need a carbon-version of the callbackmgr to register for events
        // and call the "C" real callback mgr (use the ix->mCallback when this is ready)
        
        // until then, we rely on CarbonCore for the UPP stuff
        InvokeKCCallbackUPP(inEvent,reinterpret_cast<KCCallbackInfo*>(&cbInfo),ix->mContext,
                            reinterpret_cast<KCCallbackUPP>(ix->mCallback));
#else
		ix->mCallback(inEvent,&cbInfo,ix->mContext);
#endif
	}
}

/***********************************************************************************
*	Event() - Overriden function of the KCEventObserver object.
*			  Each instance of KeychainCore will receive events from CF
*			  that was initiated by another KeychainCore instance that
*			  triggered the event.
*
* 	We <could> care about which KeychainCore posted the event: 
* 		Example (KCDeleteItem event):
*			If it was 'us', we don't do anything; we already processed the event. 
* 			If it wasn't 'us', we should remove our cached reference to the item that was deleted.
*
***********************************************************************************/
void CCallbackMgr::Event(CFNotificationCenterRef center, 
                         CFStringRef name, 
                         const void *object, 
                         CFDictionaryRef userInfo)
{
    // Decode from userInfo the event type, 'keychain' CFDict, and 'item' CFDict
    CCFValue event(CFDictionaryGetValue( userInfo, kSecEventTypeKey ));
	SecKeychainEvent	thisEvent = 0;
    if (!event.hasValue())
		return;

	thisEvent = sint32( event );

    CFDictionaryRef kc = reinterpret_cast<CFDictionaryRef>
                            (CFDictionaryGetValue(userInfo, kSecEventKeychainKey));
    Keychain thisKeychain;
    if (kc)
    {
        thisKeychain = globals().storageManager.keychain	
                            (DLDbListCFPref::cfDictionaryRefToDLDbIdentifier(kc));
    }

    CFDataRef item = reinterpret_cast<CFDataRef>
                            (CFDictionaryGetValue(userInfo, kSecEventItemKey));
    Item thisItem;
    if (item && thisKeychain)
    {
        const CssmData pkData(const_cast<UInt8*>(CFDataGetBytePtr(item)), CFDataGetLength(item));
        PrimaryKey pk(pkData);
		thisItem = thisKeychain->item(pk);
    }

    // Notify our process of this event.
	CCallbackMgr::AlertClients(thisEvent, thisKeychain, thisItem);
}

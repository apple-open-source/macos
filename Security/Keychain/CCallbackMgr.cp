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
	File:		CCallbackMgr.cp

	Contains:	Code that communicates with processes that install a callback
                with the Keychain Manager to receive keychain events.

*/

#include "CCallbackMgr.h"

#include <algorithm>
#include <list>

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>
#include "Globals.h"
#include <Security/DLDBListCFPref.h>
#include <Security/SecCFTypes.h>
//#include <Security/Keychain.h>

using namespace KeychainCore;
using namespace CssmClient;

#pragma mark ÑÑÑÑ CallbackInfo ÑÑÑÑ

CallbackInfo::CallbackInfo() : mCallback(NULL),mEventMask(0),mContext(NULL)
{
}

CallbackInfo::CallbackInfo(SecKeychainCallback inCallbackFunction,
	SecKeychainEventMask inEventMask, void *inContext)
	: mCallback(inCallbackFunction), mEventMask(inEventMask), mContext(inContext)
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
    Observer(Listener::databaseNotifications, Listener::allEvents)
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

void CCallbackMgr::AddCallback( SecKeychainCallback inCallbackFunction, 
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


class Predicate
{
	SecKeychainCallback mCallbackFunction;
public:
	Predicate(SecKeychainCallback inCallbackFunction) : mCallbackFunction(inCallbackFunction) {}
	bool operator()(const CallbackInfo &cbInfo) { return cbInfo.mCallback == mCallbackFunction; }
};

void CCallbackMgr::RemoveCallback(SecKeychainCallback inCallbackFunction)
{
	size_t oldSize = CCallbackMgr::Instance().mEventCallbacks.size();
	Predicate predicate(inCallbackFunction);
	CCallbackMgr::Instance().mEventCallbacks.remove_if(predicate);

	if (oldSize == CCallbackMgr::Instance().mEventCallbacks.size())
		MacOSError::throwMe(errSecInvalidCallback);
}

void CCallbackMgr::AlertClients(const list<CallbackInfo> &eventCallbacks,
								SecKeychainEvent inEvent,
								pid_t inPid,
                                const Keychain &inKeychain,
                                const Item &inItem)
{
    secdebug("kcnotify", "dispatch event %ld pid %d keychain %p item %p",
        inEvent, inPid, &inKeychain, !!inItem ? &*inItem : NULL);

	// Iterate through callbacks, looking for those registered for inEvent
	const SecKeychainEventMask theMask = 1U << inEvent;

	for (ConstCallbackInfoListIterator ix = eventCallbacks.begin(); ix != eventCallbacks.end(); ++ix)
	{
		if (!(ix->mEventMask & theMask))
			continue;

		SecKeychainCallbackInfo	cbInfo;
		cbInfo.version = 0; // @@@ kKeychainAPIVersion;
		cbInfo.item = inItem ? inItem->handle() : 0;
		cbInfo.keychain = inKeychain ? inKeychain->handle() : 0;
		cbInfo.pid = inPid;

		ix->mCallback(inEvent, &cbInfo, ix->mContext);
		if (cbInfo.item) CFRelease(cbInfo.item);
		if (cbInfo.keychain) CFRelease(cbInfo.keychain);
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
void CCallbackMgr::Event(Listener::Domain domain, Listener::Event whichEvent, NameValueDictionary &dictionary)
{
    // Decode from userInfo the event type, 'keychain' CFDict, and 'item' CFDict
	SecKeychainEvent	thisEvent = whichEvent;

    pid_t thisPid;
	const NameValuePair* pidRef = dictionary.FindByName(PID_KEY);
	if (pidRef == 0)
	{
		thisPid = 0;
	}
	else
	{
		thisPid = *reinterpret_cast<pid_t*>(pidRef->Value().data());
	}

	Keychain thisKeychain = 0;
    Item thisItem;
	list<CallbackInfo> eventCallbacks;

	{
		// Lock the global API lock before doing stuff with StorageManager.
		StLock<Mutex> _(globals().apiLock);

		// make sure we have a database identifier
		if (dictionary.FindByName (SSUID_KEY) != 0)
		{
			DLDbIdentifier dbid = NameValueDictionary::MakeDLDbIdentifierFromNameValueDictionary (dictionary);
			thisKeychain = globals().storageManager.keychain(dbid);
		}
		
		const NameValuePair* item = dictionary.FindByName(ITEM_KEY);
		
		if (item && thisKeychain)
		{
			PrimaryKey pk(item->Value());
			thisItem = thisKeychain->item(pk);
		}

		// Deal with events that we care about ourselves first.
		if (thisEvent == kSecDeleteEvent && thisKeychain.get() && thisItem.get())
			thisKeychain->didDeleteItem(thisItem.get());

		eventCallbacks = CCallbackMgr::Instance().mEventCallbacks;
		// We can safely release the global API lock now since thisKeychain and thisItem
		// are CFRetained and will be until they go out of scope.
	}

    // Notify our process of this event.
	CCallbackMgr::AlertClients(eventCallbacks, thisEvent, thisPid, thisKeychain, thisItem);
}

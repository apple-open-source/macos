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
#include <security_keychain/SecCFTypes.h>
#include <securityd_client/SharedMemoryCommon.h>
#include <securityd_client/ssnotify.h>
#include <notify.h>

using namespace KeychainCore;
using namespace CssmClient;
using namespace SecurityServer;

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


class CallbackMaker
{
protected:
	RefPointer<CCallbackMgr> mCallbackManager;

public:
	CallbackMaker();
	CCallbackMgr& instance() {return *mCallbackManager;}
};


CallbackMaker::CallbackMaker()
{
	CCallbackMgr* manager = new CCallbackMgr();
	mCallbackManager = manager;
}



ModuleNexus<CallbackMaker> gCallbackMaker;

CCallbackMgr::CCallbackMgr() : EventListener (kNotificationDomainDatabase, kNotificationAllEvents)
{
    EventListener::FinishedInitialization(this);
}

CCallbackMgr::~CCallbackMgr()
{
}

CCallbackMgr& CCallbackMgr::Instance()
{
	return gCallbackMaker().instance();
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



void CCallbackMgr::consume (SecurityServer::NotificationDomain domain, SecurityServer::NotificationEvent whichEvent, const CssmData &data)
{
	NameValueDictionary dictionary (data);

    // Decode from userInfo the event type, 'keychain' CFDict, and 'item' CFDict
	SecKeychainEvent thisEvent = whichEvent;

    pid_t thisPid;
	const NameValuePair* pidRef = dictionary.FindByName(PID_KEY);
	if (pidRef == 0)
	{
		thisPid = 0;
	}
	else
	{
		thisPid = n2h(*reinterpret_cast<pid_t*>(pidRef->Value().data ()));
	}

	Keychain thisKeychain;
    Item thisItem;
	list<CallbackInfo> eventCallbacks;
	{
		// Lock the global API lock before doing stuff with StorageManager.
		// make sure we have a database identifier
		if (dictionary.FindByName (SSUID_KEY) != 0)
		{
			DLDbIdentifier dbid = NameValueDictionary::MakeDLDbIdentifierFromNameValueDictionary(dictionary);
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
		else if (thisEvent == kSecKeychainListChangedEvent)
			globals().storageManager.forceUserSearchListReread();

		eventCallbacks = CCallbackMgr::Instance().mEventCallbacks;
		// We can safely release the global API lock now since thisKeychain and thisItem
		// are CFRetained and will be until they go out of scope.
	}

    // Notify our process of this event.
	CCallbackMgr::AlertClients(eventCallbacks, thisEvent, thisPid, thisKeychain, thisItem);
}

/*
 * Copyright (c) 2000-2004,2011-2016 Apple Inc. All Rights Reserved.
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

#include "Globals.h"
#include <security_cdsa_utilities/Schema.h>
#include <security_keychain/SecCFTypes.h>
#include <securityd_client/SharedMemoryCommon.h>
#include <securityd_client/ssnotify.h>
#include <utilities/SecCFRelease.h>
#include <notify.h>
#include <Security/SecCertificatePriv.h>

using namespace KeychainCore;
using namespace CssmClient;
using namespace SecurityServer;

#pragma mark ÑÑÑÑ CallbackInfo ÑÑÑÑ

CallbackInfo::CallbackInfo() : mCallback(NULL),mEventMask(0),mContext(NULL), mRunLoop(NULL), mActive(false)
{
}

CallbackInfo::CallbackInfo(SecKeychainCallback inCallbackFunction,
	SecKeychainEventMask inEventMask, void *inContext, CFRunLoopRef runLoop)
	: mCallback(inCallbackFunction), mEventMask(inEventMask), mContext(inContext), mRunLoop(NULL), mActive(false)
{
    mRunLoop = runLoop;
    CFRetainSafe(mRunLoop);
}

CallbackInfo::CallbackInfo(const CallbackInfo& cb) {
    mCallback = cb.mCallback;
    mEventMask = cb.mEventMask;
    mContext = cb.mContext;
    mActive = cb.mActive;

    mRunLoop = cb.mRunLoop;
    CFRetainSafe(mRunLoop);
}

CallbackInfo::~CallbackInfo()
{
    CFReleaseNull(mRunLoop);
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
    mInitialized = true;
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
	CallbackInfo info( inCallbackFunction, inEventMask, inContext, CFRunLoopGetCurrent() );

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

    // We want to deliver these notifications if the CFRunLoop we just wrote down is actually actively serviced.
    // Otherwise, it'll be a continuous (undetectable) leak.
    CFRunLoopTimerContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.info = info.mRunLoop;

    CFRunLoopTimerRef timerRef = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent(), 0, 0, 0, CCallbackMgr::cfrunLoopActive, &ctx);
    secdebug("kcnotify", "adding an activate callback on run loop %p", info.mRunLoop);
    CFRunLoopAddTimer(info.mRunLoop, timerRef, kCFRunLoopDefaultMode);
}

void CCallbackMgr::cfrunLoopActive(CFRunLoopTimerRef timer, void* info) {
    CFRunLoopRef runLoop = (CFRunLoopRef) info;
    secdebug("kcnotify", "activating run loop %p", runLoop);

    // Use the notification queue to serialize setting the mActive bits
    static dispatch_queue_t notification_queue = EventListener::getNotificationQueue();
    dispatch_async(notification_queue, ^() {
        // Iterate through list, and activate every notification on this run loop
        for(CallbackInfoListIterator ix = CCallbackMgr::Instance().mEventCallbacks.begin(); ix != CCallbackMgr::Instance().mEventCallbacks.end(); ix++) {
            // pointer comparison, not CFEqual.
            if(ix->mRunLoop == runLoop) {
                secdebug("kcnotify", "activating callback on run loop %p", runLoop);
                ix->mActive = true;
            }
        }
    });

    CFRelease(timer);
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

struct CallbackMgrInfo {
    SecKeychainEvent event;
    SecKeychainCallbackInfo secKeychainCallbackInfo;
    SecKeychainCallback callback;
    void *callbackContext;
};

void CCallbackMgr::tellClient(CFRunLoopTimerRef timer, void* info) {
    CallbackMgrInfo* cbmInfo = (CallbackMgrInfo*) info;
    if(!cbmInfo || !(cbmInfo->callback)) {
        return;
    }

    cbmInfo->callback(cbmInfo->event, &(cbmInfo->secKeychainCallbackInfo), cbmInfo->callbackContext);
    if (cbmInfo->secKeychainCallbackInfo.item) CFRelease(cbmInfo->secKeychainCallbackInfo.item);
    if (cbmInfo->secKeychainCallbackInfo.keychain) CFRelease(cbmInfo->secKeychainCallbackInfo.keychain);
    free(cbmInfo);
    CFRelease(timer);
}

static SecKeychainItemRef createItemReference(const Item &inItem)
{
	SecKeychainItemRef itemRef = (inItem) ? inItem->handle() : 0;
	if(!itemRef) { return NULL; }

	SecItemClass itemClass = Schema::itemClassFor(inItem->recordType());
	if (itemClass == kSecCertificateItemClass) {
		SecCertificateRef certRef = SecCertificateCreateFromItemImplInstance((SecCertificateRef)itemRef);
		CFRelease(itemRef); /* certRef maintains its own internal reference to itemRef */
		itemRef = (SecKeychainItemRef) certRef;
	}

	return itemRef;
}

static SecKeychainRef createKeychainReference(const Keychain &inKeychain)
{
	return (inKeychain) ? inKeychain->handle() : 0;
}

void CCallbackMgr::AlertClients(const list<CallbackInfo> &eventCallbacks,
								SecKeychainEvent inEvent,
								pid_t inPid,
                                const Keychain &inKeychain,
                                const Item &inItem)
{
    secinfo("kcnotify", "dispatch event %ld pid %d keychain %p item %p",
        (unsigned long)inEvent, inPid, &inKeychain, !!inItem ? &*inItem : NULL);

	// Iterate through callbacks, looking for those registered for inEvent
	const SecKeychainEventMask theMask = 1U << inEvent;

	for (ConstCallbackInfoListIterator ix = eventCallbacks.begin(); ix != eventCallbacks.end(); ++ix)
	{
		if (!(ix->mEventMask & theMask))
			continue;

        if(!(ix->mActive)) {
            // We haven't received our callback from this CFRunLoop yet. Assume it's not being pumped, and don't schedule.
            secdebug("kcnotify", "not sending event to run loop %p", ix->mRunLoop);
            continue;
        }

        // The previous notification system required a CFRunLoop to be executing. Schedule the client's notifications back on their CFRunLoop, just in case it's important.
        CFRunLoopRef runLoop = ix->mRunLoop;
        secdebug("kcnotify", "sending event to runloop %p", runLoop);

        // Set up our callback structures
        CallbackMgrInfo* cbmInfo = (CallbackMgrInfo*) calloc(sizeof(CallbackMgrInfo), 1);

        cbmInfo->secKeychainCallbackInfo.version = 0; // @@@ kKeychainAPIVersion;
        cbmInfo->secKeychainCallbackInfo.item = createItemReference(inItem);
        cbmInfo->secKeychainCallbackInfo.keychain = createKeychainReference(inKeychain);
        cbmInfo->secKeychainCallbackInfo.pid = inPid;

        cbmInfo->event = inEvent;
        cbmInfo->callback = ix->mCallback;
        cbmInfo->callbackContext = ix->mContext;

        CFRunLoopTimerContext ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.info = cbmInfo;

        // make a run loop timer
        CFRunLoopTimerRef timerRef = CFRunLoopTimerCreate(NULL, CFAbsoluteTimeGetCurrent(), 0, 0, 0, CCallbackMgr::tellClient, &ctx);

        // Actually call the callback the next time the run loop fires
        CFRunLoopAddTimer(runLoop, timerRef, kCFRunLoopDefaultMode);
	}
}



void CCallbackMgr::consume (SecurityServer::NotificationDomain domain, SecurityServer::NotificationEvent whichEvent, const CssmData &data)
{
    NameValueDictionary dictionary (data);

    // Decode from userInfo the event type, 'keychain' CFDict, and 'item' CFDict
	SecKeychainEvent thisEvent = (SecKeychainEvent) whichEvent;

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
            StLock<Mutex>_(*globals().storageManager.getStorageManagerMutex());
			DLDbIdentifier dbid = NameValueDictionary::MakeDLDbIdentifierFromNameValueDictionary(dictionary);
			thisKeychain = globals().storageManager.keychain(dbid);
            globals().storageManager.tickleKeychain(thisKeychain);
		}

		const NameValuePair* item = dictionary.FindByName(ITEM_KEY);

		if (item && thisKeychain)
		{
            PrimaryKey pk(item->Value());

            // if this is a deletion event, do the lookup slightly differently
            if(thisEvent != kSecDeleteEvent) {
                thisItem = thisKeychain->item(pk);
            } else {
                thisItem = thisKeychain->itemdeleted(pk);
            }
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

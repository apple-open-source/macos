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
	File:		KCEventNotifier.cpp

	Contains:	OS X CF Notifier for Keychain Events

	Written by:	Craig Mortensen

	Copyright:	2000 by Apple Computer, Inc., All rights reserved.

	Change History (most recent first):

	To Do:
*/

#include "KCEventNotifier.h"
#include "KCExceptions.h"
#include "Keychains.h"
#include <Security/cfutilities.h>

using namespace KeychainCore;

void KCEventNotifier::PostKeychainEvent(SecKeychainEvent whichEvent, const Keychain &keychain, const Item &kcItem)
{
	DLDbIdentifier dlDbIdentifier;
	PrimaryKey primaryKey;

	if (keychain)
		dlDbIdentifier = keychain->dLDbIdentifier();

    if (kcItem)
		primaryKey = kcItem->primaryKey();

	PostKeychainEvent(whichEvent, dlDbIdentifier, primaryKey);
}


void KCEventNotifier::PostKeychainEvent(SecKeychainEvent whichEvent,
										const DLDbIdentifier &dlDbIdentifier, 
										const PrimaryKey &primaryKey)
{
	CFRef<CFMutableDictionaryRef> mutableDict(::CFDictionaryCreateMutable(kCFAllocatorDefault,0,
			&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    KCThrowIfMemFail_(CFMutableDictionaryRef(mutableDict));

	SInt32 theEvent = SInt32(whichEvent);
    CFRef<CFNumberRef> theEventData(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &theEvent));
    KCThrowIfMemFail_(CFNumberRef(theEventData));
    CFDictionarySetValue(mutableDict, kSecEventTypeKey, theEventData);

	pid_t thePid = getpid();
    CFRef<CFNumberRef> thePidData(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &thePid));
    KCThrowIfMemFail_(CFNumberRef(thePidData));
    CFDictionarySetValue(mutableDict, kSecEventPidKey, thePidData);

	if (dlDbIdentifier)
	{
		CFRef<CFDictionaryRef> dict(DLDbListCFPref::dlDbIdentifierToCFDictionaryRef(dlDbIdentifier));
		KCThrowIfMemFail_(CFDictionaryRef(dict));
		CFDictionarySetValue(mutableDict, kSecEventKeychainKey, dict);
	}

    if (primaryKey)
    {
		CFRef<CFDataRef> data(CFDataCreate(kCFAllocatorDefault, primaryKey->Data, primaryKey->Length));
		KCThrowIfMemFail_(CFDataRef(data));
		CFDictionarySetValue(mutableDict, kSecEventItemKey, data);
    }


    // 'name' has to be globally unique (could be KCLockEvent, etc.)
    // 'object' is just information or a context that can be used.
    // 'userInfo' has info on event (i.e. which DL/DB(kc - see John's Dict), the event, 
    //								 item(cssmdbuniqueRec))
    CFNotificationCenterPostNotification(CFNotificationCenterGetDistributedCenter(), 
										 kSecEventNotificationName, NULL, mutableDict, false);
}

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

#include "ssclient.h"
#include "KCEventNotifier.h"
#include "KCExceptions.h"
#include "Keychains.h"

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
	NameValueDictionary nvd;

	pid_t thePid = getpid();
	nvd.Insert (new NameValuePair (PID_KEY, CssmData (reinterpret_cast<void*>(&thePid), sizeof (pid_t))));

	if (dlDbIdentifier)
	{
		NameValueDictionary::MakeNameValueDictionaryFromDLDbIdentifier (dlDbIdentifier, nvd);
	}

	CssmData* pKey = primaryKey;
	
    if (primaryKey)
    {
		nvd.Insert (new NameValuePair (ITEM_KEY, *pKey));
    }

	// flatten the dictionary
	CssmData data;
	nvd.Export (data);
	
	SecurityServer::ClientSession cs (CssmAllocator::standard(), CssmAllocator::standard());
	cs.postNotification (Listener::databaseNotifications, whichEvent, data);
	
    secdebug("kcnotify", "KCEventNotifier::PostKeychainEvent posted event %u", (unsigned int) whichEvent);

	free (data.data ());
}

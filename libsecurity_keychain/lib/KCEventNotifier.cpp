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
 *
 * KCEventNotifier.cpp -- OS X CF Notifier for Keychain Events
 */

#include <securityd_client/ssclient.h>
#include "KCEventNotifier.h"
#include "KCExceptions.h"
#include "Keychains.h"

using namespace KeychainCore;

void KCEventNotifier::PostKeychainEvent(SecKeychainEvent whichEvent, const Keychain &keychain, const Item &kcItem)
{
	DLDbIdentifier dlDbIdentifier;
	PrimaryKey primaryKey;

	if (keychain)
		dlDbIdentifier = keychain->dlDbIdentifier();

    if (kcItem)
		primaryKey = kcItem->primaryKey();

	PostKeychainEvent(whichEvent, dlDbIdentifier, primaryKey);
}


void KCEventNotifier::PostKeychainEvent(SecKeychainEvent whichEvent,
										const DLDbIdentifier &dlDbIdentifier, 
										const PrimaryKey &primaryKey)
{
	NameValueDictionary nvd;

	Endian<pid_t> thePid = getpid();
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
	
	SecurityServer::ClientSession cs (Allocator::standard(), Allocator::standard());
	cs.postNotification (SecurityServer::kNotificationDomainDatabase, whichEvent, data);

    secdebug("kcnotify", "KCEventNotifier::PostKeychainEvent posted event %u", (unsigned int) whichEvent);

	free (data.data ());
}

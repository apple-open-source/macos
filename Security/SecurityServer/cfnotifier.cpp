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


//
// cfnotifier - quick & dirty code to send keychain lock notification
//
#include "cfnotifier.h"
#include "notifications.h"
#include <Security/debugging.h>
#include "dictionary.h"

#include "session.h"

using namespace Security;
using namespace Security::MachPlusPlus;



//
// Main methods
//
void KeychainNotifier::lock(const DLDbIdentifier &db)
{ notify(db, Listener::lockedEvent); }

void KeychainNotifier::unlock(const DLDbIdentifier &db)
{ notify(db, Listener::unlockedEvent); }

void KeychainNotifier::passphraseChanged(const DLDbIdentifier &db)
{ notify(db, Listener::passphraseChangedEvent); }


//
// Lock and unlock notifications
//
void KeychainNotifier::notify(const DLDbIdentifier &db, int event)
{
	// export the dbID to a dictionary
	NameValueDictionary nvd;
	NameValueDictionary::MakeNameValueDictionaryFromDLDbIdentifier (db, nvd);
	
	// flatten the dictionary
	CssmData data;
	nvd.Export (data);
	
	Listener::notify (Listener::databaseNotifications, event, data);
	free (data.data ());
}


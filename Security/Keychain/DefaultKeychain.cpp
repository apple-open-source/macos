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
    File:		DefaultKeychain.cpp

    Contains:	User preference for default keychain

    Copyright:	2000 by Apple Computer, Inc., all rights reserved.

    To Do:
*/

#include "DefaultKeychain.h"

#include "CCallbackMgr.h"
#include "KCEventNotifier.h"
#include "Keychains.h"
#include "Globals.h"
#include "KCExceptions.h"

using namespace KeychainCore;
using namespace CssmClient;

DefaultKeychain::DefaultKeychain() : mPref(CFSTR("DefaultKeychain"))
{
}

// Set/Get via DLDbIdentifier
void DefaultKeychain::dLDbIdentifier(const DLDbIdentifier& keychainID)
{
    DLDbList& theList=mPref.list();
    if (theList.size()>0 && keychainID==theList[0])		// already the default keychain
        return;
    theList.clear();
    mPref.add(keychainID);			// destructor will save
    mPref.save();
    KCEventNotifier::PostKeychainEvent(kSecDefaultChangedEvent, keychainID);
    defaultID = keychainID;
}

// unset default
void DefaultKeychain::unset()
{
    DLDbList& theList=mPref.list();
 	theList.clear();
	mPref.clearDefaultKeychain();
	KCEventNotifier::PostKeychainEvent(kSecDefaultChangedEvent);
}

void DefaultKeychain::reload(bool force)
{
	if (!defaultID || mPref.revert(force))
    {
        DLDbList& theList=mPref.list();
        if (theList.size()==0)
            MacOSError::throwMe(errSecNoDefaultKeychain);
        defaultID = theList[0];
    }
}

DLDbIdentifier DefaultKeychain::dLDbIdentifier()
{
    reload();
    return defaultID;
}

// Set/Get via Keychain
void DefaultKeychain::keychain(const Keychain& keychain)
{
    DefaultKeychain::dLDbIdentifier(keychain->dLDbIdentifier());	// call the main "set" routine
}

Keychain DefaultKeychain::keychain()	// was: GetTimedDefaultKC
{
    return globals().storageManager.keychain(dLDbIdentifier());
}

bool DefaultKeychain::isSet()
{
    return mPref.list().size() != 0;
}

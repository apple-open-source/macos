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
    File:		DefaultKeychain.h

    Contains:	User preference for default keychain

    Written by:	John Hurley

    Copyright:	2000 by Apple Computer, Inc., all rights reserved.

    To Do:
*/

#ifndef _H_KEYCHAINCORE_DEFAULTKEYCHAIN__
#define _H_KEYCHAINCORE_DEFAULTKEYCHAIN__

#include <Security/DLDBListCFPref.h>
#include <Security/Keychains.h>

namespace Security
{

namespace KeychainCore
{

//---------------------------------------------------------------------------------
//	Default keychain
//
//	Note that this is strictly a user preference setting, indicating which keychain
//	should be used to add items to. No validity checking should be done on it in
//	this class, since it may not be available right now (e.g. on a network volume)
//----------------------------------------------------------------------------------

class Keychain;

class DefaultKeychain
{
public:
	DefaultKeychain();
	
	// Set/Get via DLDbIdentifier
	void dLDbIdentifier(const DLDbIdentifier& keychainID);
	DefaultKeychain &operator =(const DLDbIdentifier& keychainID)
	{ dLDbIdentifier(keychainID); return *this; }

    void reload(bool force = false);
	DLDbIdentifier dLDbIdentifier();
	operator DLDbIdentifier () { return dLDbIdentifier(); }

	// Remove if passed in DLDbIdentifier is currently the default
	void remove(const DLDbIdentifier& keychainID);

	// Set/Get via Keychain
	void keychain(const Keychain& keychain);
	DefaultKeychain &operator =(const Keychain& inKeychain) { keychain(inKeychain); return *this; }

	Keychain keychain();
	operator Keychain () { return keychain(); }

	void unset(); // Who needs a default keychain anyway.
    bool isSet();
private:
	DLDbListCFPref mPref;
    DLDbIdentifier defaultID;
};

}; // end namespace KeychainCore

} // end namespace Security

#endif /* _H_KEYCHAINCORE_DEFAULTKEYCHAIN__ */

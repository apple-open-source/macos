/*
 * Copyright (c) 2000-2004,2011,2013-2014 Apple Inc. All Rights Reserved.
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

//
// DefaultKeychain.h - Private "globals" and interfaces for KeychainCore
//
#ifndef _SECURITY_GLOBALS_H_
#define _SECURITY_GLOBALS_H_

#ifdef check
#undef check
#endif
#include <security_keychain/StorageManager.h>
#include <security_cdsa_client/aclclient.h>


namespace Security
{

namespace KeychainCore
{

class Globals
{
public:
    Globals();
	
	const AccessCredentials *keychainCredentials();
	const AccessCredentials *smartcardCredentials();
	const AccessCredentials *itemCredentials();
	const AccessCredentials *smartcardItemCredentials();

	void setUserInteractionAllowed(bool bUI) { mUI=bUI; }
	bool getUserInteractionAllowed() const { return mUI; }

	// Public globals
	StorageManager storageManager;

    bool integrityProtection() { return mIntegrityProtection; }

private:

	// Other "globals"
	bool mUI;
    bool mIntegrityProtection;
	CssmClient::AclFactory mACLFactory;
};

extern ModuleNexus<Globals> globals;
extern bool gServerMode;

} // end namespace KeychainCore

} // end namespace Security

extern "C" bool GetServerMode();

#endif // !_SECURITY_GLOBALS_H_

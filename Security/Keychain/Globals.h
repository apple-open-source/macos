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

//
// DefaultKeychain.h - Private "globals" and interfaces for KeychainCore
//
#ifndef _SECURITY_GLOBALS_H_
#define _SECURITY_GLOBALS_H_

#ifdef check
#undef check
#endif
#include <Security/StorageManager.h>
#include <Security/aclclient.h>


namespace Security
{

namespace KeychainCore
{

class Globals
{
public:
    Globals();
	const AccessCredentials *credentials();
	void setUserInteractionAllowed(bool bUI) { mUI=bUI; }
	bool getUserInteractionAllowed() const { return mUI; }

	// Public globals
	StorageManager storageManager;
	//DefaultKeychain defaultKeychain;
	Mutex apiLock;
private:

	// Other "globals"
	bool mUI;
	CssmClient::AclFactory mACLFactory;  
};

extern ModuleNexus<Globals> globals;

} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_GLOBALS_H_

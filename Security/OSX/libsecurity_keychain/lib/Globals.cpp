/*
 * Copyright (c) 2000-2002,2004,2011,2013-2014 Apple Inc. All Rights Reserved.
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
 * Globals.cpp -- Private "globals" and interfaces for KeychainCore
 */

#include "Globals.h"
#include "KCExceptions.h"


namespace Security {
namespace KeychainCore {

using namespace CssmClient;

ModuleNexus<Globals> globals;
bool gServerMode;

#pragma mark ÑÑÑÑ Constructor/Destructor ÑÑÑÑ

Globals::Globals() :
mUI(true)
{
}

const AccessCredentials * Globals::keychainCredentials() 
{
	return (mUI ? mACLFactory.unlockCred() : mACLFactory.cancelCred()); 
}

const AccessCredentials * Globals::smartcardCredentials() 
{
	return (mUI ? mACLFactory.promptedPINCred() : mACLFactory.cancelCred()); 
}

const AccessCredentials * Globals::itemCredentials() 
{
	return (mUI ? mACLFactory.promptCred() : mACLFactory.nullCred()); 
}

const AccessCredentials * Globals::smartcardItemCredentials() 
{
	return (mUI ? mACLFactory.promptedPINItemCred() : mACLFactory.cancelCred()); 
}
	
}	// namespace KeychainCore
}	// namespace Security



extern "C" bool GetServerMode()
{
	return Security::KeychainCore::gServerMode;
}

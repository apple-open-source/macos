/*
 * Copyright (c) 2000-2002,2004 Apple Computer, Inc. All Rights Reserved.
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


using namespace KeychainCore;
using namespace CssmClient;

ModuleNexus<Globals> KeychainCore::globals;

#pragma mark ÑÑÑÑ Constructor/Destructor ÑÑÑÑ

Globals::Globals() :
apiLock(Mutex::recursive),
mUI(true)
{
}

const AccessCredentials * Globals::credentials() 
{
	return (mUI ? mACLFactory.promptCred() : mACLFactory.nullCred()); 
}

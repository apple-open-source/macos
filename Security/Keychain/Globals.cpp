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
    File:		Globals.cpp

    Contains:	Private "globals" and interfaces for KeychainCore

    Copyright:	2000 by Apple Computer, Inc., all rights reserved.

    To Do:
	
	Notes:		Many functions moved from CKeychainMgr.h
*/

#include "Globals.h"
#include "KCExceptions.h"


using namespace KeychainCore;
using namespace CssmClient;

ModuleNexus<Globals> KeychainCore::globals;

#pragma mark ÑÑÑÑ Constructor/Destructor ÑÑÑÑ

Globals::Globals() :
mUI(true)
{
}

const AccessCredentials * Globals::credentials() 
{
	return (mUI ? mACLFactory.promptCred() : mACLFactory.nullCred()); 
}

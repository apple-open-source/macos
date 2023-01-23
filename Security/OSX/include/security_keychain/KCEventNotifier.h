/*
 * Copyright (c) 2000-2004,2011,2014 Apple Inc. All Rights Reserved.
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


/*
 *  KCEventNotifier.h -- OS X CF Notifier for Keychain Events
 */
#ifndef _SECURITY_KCEVENTNOTIFIER_H_
#define _SECURITY_KCEVENTNOTIFIER_H_

#include <CoreFoundation/CFNotificationCenter.h>
#include <CoreFoundation/CFString.h>
#include <security_keychain/Item.h>
#include <securityd_client/dictionary.h>
#include <list>

namespace Security
{

namespace KeychainCore
{

class Keychain;

class KCEventNotifier
{
public:
	static void PostKeychainEvent(SecKeychainEvent kcEvent, 
								  const Keychain& keychain, 
								  const Item &item = Item());
	static void PostKeychainEvent(SecKeychainEvent kcEvent, 
								  const DLDbIdentifier &dlDbIdentifier = DLDbIdentifier(), 
								  const PrimaryKey &primaryKey = PrimaryKey());
};

} // end namespace KeychainCore

} // end namespace Security

#endif /* _SECURITY_KCEVENTNOTIFIER_H_ */

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
	File:		KCEventNotifier.h

	Contains:	OS X CF Notifier for Keychain Events

	Written by:	Craig Mortensen

	Copyright:	2000 by Apple Computer, Inc., All rights reserved.

	Change History (most recent first):

	To Do:
*/

#ifndef _KCEVENTNOTIFIER_H_
#define _KCEVENTNOTIFIER_H_

#include <CoreFoundation/CFNotificationCenter.h>
#include <CoreFoundation/CFString.h>
#include <Security/Item.h>

namespace Security
{

namespace KeychainCore
{

#define kSecEventNotificationName CFSTR("com.apple.securitycore.kcevent")
#define kSecEventTypeKey CFSTR("type")
#define kSecEventKeychainKey CFSTR("keychain")
#define kSecEventItemKey CFSTR("item")

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

#endif /* _KCEVENTNOTIFIER_H_ */

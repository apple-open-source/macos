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
	File:		KCEventObserver.cpp

	Contains:	OS X CF Observer for Keychain Events

	Written by:	Craig Mortensen

	Copyright:	2000 by Apple Computer, Inc., All rights reserved.

	Change History (most recent first):

	To Do:
*/

#include "KCEventObserver.h"

using namespace Security;

Observer::Observer()
{
}
//
// Upon creation of this object, add this observer for this instance of KeychainCore
//
Observer::Observer( CFStringRef name, const void *object, 
                    CFNotificationSuspensionBehavior suspensionBehavior )
{
    add( name, object, suspensionBehavior );
}

//
// Upon destruction of this object, remove 'this' observer for this instance of KeychainCore
//
Observer::~Observer()
{
    ::CFNotificationCenterRemoveEveryObserver( CFNotificationCenterGetDistributedCenter(), this );
}

//
// 'callback' is passed in to CFNotificationCenterAddObserver() when this object
// is constructed when KeychainCore is created.  'callback' is called by CF whenever an event happens.
//
void Observer::callback(CFNotificationCenterRef 	center, 
                        void*					 	observer, 
                        CFStringRef 				name, 
                        const void* 				object, 
                        CFDictionaryRef 			userInfo)
{
    // 'Event' is where this KeychainCore notifies it's clients of the kc event that just happened.
    //
	try
	{
		reinterpret_cast<Observer *>(observer)->Event( center, name, object, userInfo );
	}
	catch(...)
	{
		// @@@ do a log to console();
	}
}

//
// Add 'this' observer to CF for this instance of KeychainCore
//
void Observer::add( CFStringRef 					 name, 
                    const void* 					 object, 
                    CFNotificationSuspensionBehavior suspensionBehavior )
{
    ::CFNotificationCenterAddObserver( CFNotificationCenterGetDistributedCenter(), 
                                       this, callback, name, object, suspensionBehavior );
}

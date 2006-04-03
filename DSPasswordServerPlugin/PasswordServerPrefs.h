/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#ifndef __PasswordServerPrefs__
#define __PasswordServerPrefs__

#include "PasswordServerPrefsDefs.h"
#import <Cocoa/Cocoa.h>

@interface PasswordServerPrefsObject : NSObject {
	NSMutableDictionary *mPrefsDict;
	NSMutableCharacterSet *mExternalToolIllegalChars;
	PasswordServerPrefs mPrefs;
	struct timespec mPrefsFileModDate;
}

// constructor/destructor
-(id)init;
-(void)dealloc;

// public methods
-(void)getPrefs:(PasswordServerPrefs *)outPrefs;
-(void)setPrefs:(PasswordServerPrefs *)inPrefs;

-(void)refreshIfNeeded;
-(int)loadPrefs;
-(int)savePrefs;

-(void)setRealm:(const char *)inRealm;
-(SASLPluginStatus)getSASLPluginStatus:(const char *)inSASLPluginName foundAtIndex:(int *)outIndex;
-(BOOL)methodExists:(const char *)method inArray:(NSArray *)inActivePluginArray;

// accessors
-(BOOL)passiveReplicationOnly;
-(BOOL)provideReplicationOnly;
-(unsigned long)badTrialDelay;
-(unsigned long)maxTimeSkewForSync;
-(unsigned long)syncInterval;
-(BOOL)localListenersOnly;
-(BOOL)testSpillBucket;
-(const char *)realm;
-(const char *)passwordToolPath;
-(unsigned long)getKerberosCacheLimit;
-(BOOL)syncSASLPluginList;

// protected methods
-(int)statPrefsFileAndGetModDate:(struct timespec *)outModDate;
-(int)loadXMLData;
-(int)saveXMLData;

@end

#endif

/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


#import "KNPersistentState.h"
#import <utilities/debugging.h>

@implementation KNPersistentState

-(NSURL*)urlForStorage
{
	return [NSURL URLWithString:@"Preferences/com.apple.security.KCN.plist" relativeToURL:[[NSFileManager defaultManager] URLForDirectory:NSLibraryDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:YES error:nil]];
}

+(instancetype)loadFromStorage
{
	KNPersistentState *state = [KNPersistentState new];
    if (!state) {
        return state;
    }
    
    id plist = @{@"lastWritten": [NSDate distantPast]};

    NSError *error = nil;
    NSData *stateData = [NSData dataWithContentsOfURL:[state urlForStorage] options:0 error:&error];
    if (!stateData) {
        secdebug("kcn", "Can't read state data (p=%@, err=%@)", [state urlForStorage], error);
    } else {
        NSPropertyListFormat format;
        plist = [NSPropertyListSerialization propertyListWithData:stateData options: NSPropertyListMutableContainersAndLeaves format:&format error:&error];
        
        if (plist == nil) {
            secdebug("kcn", "Can't deserialize %@, e=%@", stateData, error);
        }
    }
    
    state.lastCircleStatus 						= plist[@"lastCircleStatus"] ? [plist[@"lastCircleStatus"] intValue] : kSOSCCCircleAbsent;
    state.lastWritten							= plist[@"lastWritten"];
	state.pendingApplicationReminder			= plist[@"pendingApplicationReminder"] ?: [NSDate distantFuture];
	state.applicationDate						= plist[@"applicationDate"]            ?: [NSDate distantPast];
	state.debugLeftReason						= plist[@"debugLeftReason"];
	state.pendingApplicationReminderInterval	= plist[@"pendingApplicationReminderInterval"];
	state.absentCircleWithNoReason				= plist[@"absentCircleWithNoReason"] ? [plist[@"absentCircleWithNoReason"] intValue] : NO;

    if (!state.pendingApplicationReminderInterval || [state.pendingApplicationReminderInterval doubleValue] <= 0) {
        state.pendingApplicationReminderInterval = [NSNumber numberWithUnsignedInt: 24*60*60];
    }
    
    return state;
}

-(void)writeToStorage
{
    NSMutableDictionary *plist = [@{@"lastCircleStatus"					 : [NSNumber numberWithInt:self.lastCircleStatus],
									@"lastWritten"						 : [NSDate date],
									@"applicationDate"					 : self.applicationDate,
									@"pendingApplicationReminder"		 : self.pendingApplicationReminder,
									@"pendingApplicationReminderInterval": self.pendingApplicationReminderInterval,
									@"absentCircleWithNoReason" 		 : [NSNumber numberWithBool:self.absentCircleWithNoReason],
								   } mutableCopy];
	if (self.debugLeftReason)
		plist[@"debugLeftReason"] = self.debugLeftReason;
    secdebug("kcn", "writeToStorage plist=%@", plist);
	
    NSError *error = nil;
    NSData *stateData = [NSPropertyListSerialization dataWithPropertyList:plist format:NSPropertyListXMLFormat_v1_0 options:kCFPropertyListImmutable error:&error];
    if (!stateData) {
        secdebug("kcn", "Can't serialize %@: %@", plist, error);
        return;
    }
    if (![stateData writeToURL:[self urlForStorage] options:NSDataWritingAtomic error:&error]) {
        secdebug("kcn", "Can't write to %@, error=%@", [self urlForStorage], error);
    }
}


@end

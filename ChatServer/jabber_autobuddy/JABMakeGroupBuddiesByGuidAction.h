//
//  JABMakeGroupBuddiesByGuidAction.h
//  ChatServer/jabber_autobuddy
//
//  Copyright 2010 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "JABMakeAllBuddiesAction.h"
#import "JABDirectory.h"

@interface JABMakeGroupBuddiesByGuidAction : JABMakeAllBuddiesAction {

	NSString *_groupGuid; // GeneratedUID of OD group for JID membership
	JABDirectory *_jabDir;
}
@property(retain,readwrite) NSString *groupGuid;
@property(retain,readwrite) JABDirectory *jabDir;

- (id) initWithCommandOptions: (NSDictionary *) cmdOpts;
- (void) dealloc;

- (void) addRosterItemForOwner: (NSString *) ownerJid andBuddy: (NSString *) buddyJid;
- (BOOL) verifyGroupMembershipForOwner: (NSString *) ownerJid andBuddy: (NSString *) buddyJid;
- (BOOL) isGroupMemberJid: (NSString *) aJid;
@end

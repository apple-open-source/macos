//
//  JABMakeGroupBuddiesAction.h
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 7/18/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "JABMakeAllBuddiesAction.h"

@interface JABMakeGroupBuddiesAction : JABMakeAllBuddiesAction {

	NSString *_groupName; // name of OD group for JID membership

}
@property(retain,readwrite) NSString *groupName;

- (id) initWithCommandOptions: (NSDictionary *) cmdOpts;
- (void) dealloc;

- (void) addRosterItemForOwner: (NSString *) ownerJid andBuddy: (NSString *) buddyJid;
- (BOOL) verifyGroupMembershipForOwner: (NSString *) ownerJid andBuddy: (NSString *) buddyJid;
- (BOOL) isGroupMemberJid: (NSString *) aJid;
@end

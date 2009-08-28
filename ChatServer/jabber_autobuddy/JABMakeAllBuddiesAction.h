//
//  JABMakeAllBuddiesAction.h
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 7/18/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "JABDatabaseAction.h"

@class JABSelectAllActiveQuery;

@interface JABMakeAllBuddiesAction : JABDatabaseAction {

	// queries for accessing 'active' table items
	JABSelectAllActiveQuery *_activeQuery1;
	JABSelectAllActiveQuery *_activeQuery2;

}
@property(retain,readwrite) JABSelectAllActiveQuery *activeQuery1;
@property(retain,readwrite) JABSelectAllActiveQuery *activeQuery2;

- (id) initWithCommandOptions: (NSDictionary *) cmdOpts;
- (void) dealloc;

- (BOOL) requiresJid;

- (void) doDBAction;

- (BOOL) shouldModifyRosterItemForOwner: (NSString *) ownerJid andBuddy: (NSString *) buddyJid;
- (void) addRosterItemForOwner: (NSString *) ownerJid andBuddy: (NSString *) buddyJid;

@end


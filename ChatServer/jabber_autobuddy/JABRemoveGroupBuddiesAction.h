//
//  JABRemoveGroupBuddiesAction.h
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 9/26/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "JABDatabaseAction.h"

@class JABSelectAllActiveQuery;

@interface JABRemoveGroupBuddiesAction : JABDatabaseAction {

	NSString *_groupName; // name of OD group for JID membership

	// queries for accessing 'active' table items
	JABSelectAllActiveQuery *_activeQuery1;
	JABSelectAllActiveQuery *_activeQuery2;
}
@property(retain,readwrite) NSString *groupName;
@property(retain,readwrite) JABSelectAllActiveQuery *activeQuery1;
@property(retain,readwrite) JABSelectAllActiveQuery *activeQuery2;

- (id) initWithCommandOptions: (NSDictionary *) cmdOpts;
- (void) dealloc;

- (BOOL) requiresJid;

- (void) doDBAction;

@end

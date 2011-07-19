//
//  JABRemoveGroupBuddiesByGuidAction.h
//  ChatServer/jabber_autobuddy
//
//  Copyright 2010 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "JABDatabaseAction.h"
#import "JABDirectory.h"

@class JABSelectAllActiveQuery;

@interface JABRemoveGroupBuddiesByGuidAction : JABDatabaseAction {

	NSString *_groupGuid; // GeneneratedUID of OD group for JID membership

	// queries for accessing 'active' table items
	JABSelectAllActiveQuery *_activeQuery1;
	JABSelectAllActiveQuery *_activeQuery2;
	JABDirectory *_jabDir;
}
@property(retain,readwrite) NSString *groupGuid;
@property(retain,readwrite) JABSelectAllActiveQuery *activeQuery1;
@property(retain,readwrite) JABSelectAllActiveQuery *activeQuery2;
@property(retain,readwrite) JABDirectory *jabDir;

- (id) initWithCommandOptions: (NSDictionary *) cmdOpts;
- (void) dealloc;

- (BOOL) requiresJid;

- (void) doDBAction;

@end

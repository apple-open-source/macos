//
//  JABDeleteUserAction.h
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 7/18/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "JABDatabaseAction.h"

@class JABSelectAllActiveQuery;

@interface JABDeleteUserAction : JABDatabaseAction {
	
	NSArray *_deleteItems; // list of table+columns to delete
}
@property(retain,readwrite) NSArray *deleteItems;

- (id) initWithCommandOptions: (NSDictionary *) cmdOpts;
- (void) dealloc;

- (void) initDeleteItems;

- (void) doDBAction;

@end

@interface JABDeleteAllUsersAction : JABDeleteUserAction {
	
	NSString *_testPrefix;
	JABSelectAllActiveQuery *_activeQuery;
}
@property(retain,readwrite) NSString *testPrefix;
@property(retain,readwrite) JABSelectAllActiveQuery *activeQuery;

- (id) initWithCommandOptions: (NSDictionary *) cmdOpts;
- (void) dealloc;

- (BOOL) requiresJid;

- (void) doDBAction;

@end


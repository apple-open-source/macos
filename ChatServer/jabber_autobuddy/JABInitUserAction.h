//
//  JABInitUserAction.h
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 7/18/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "JABDatabaseAction.h"

@interface JABInitUserAction : JABDatabaseAction {
	
}

- (void) doDBAction;

@end

@interface JABInitTestUsersAction : JABDatabaseAction {
	
	NSInteger _testCount;
	NSString *_testPrefix;
}
@property(assign,readwrite) NSInteger testCount;
@property(retain,readwrite) NSString *testPrefix;

- (id) initWithCommandOptions: (NSDictionary *) cmdOpts;
- (void) dealloc;

- (BOOL) requiresJid;

- (void) doDBAction;

@end


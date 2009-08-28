//
//  JABAddRosterUserAction.h
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 7/18/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "JABDatabaseAction.h"

@class JABSelectAllActiveQuery;

@interface JABAddRosterUserAction : JABDatabaseAction {

	JABSelectAllActiveQuery *_activeQuery; // query for accessing 'active' table items
	
}
@property(retain,readwrite) JABSelectAllActiveQuery *activeQuery;

- (id) initWithCommandOptions: (NSDictionary *) cmdOpts;
- (void) dealloc;

- (void) doDBAction;

@end

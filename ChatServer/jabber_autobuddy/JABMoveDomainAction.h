//
//  JABMoveDomainAction.h
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 9/16/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import <Foundation/Foundation.h>

#import "JABDatabaseAction.h"

@class JABSelectAllActiveQuery;

//------------------------------------------------------------------------------
@interface JABMoveDomainAction : JABDatabaseAction {
	
	NSString *_sourceDomain; // domain users are moving FROM
	NSString *_destDomain; // domain users are moving TO
	
	NSArray *_ownerTables; // tables where "collection-owner" is replaced
	NSArray *_jidTables; // tables where "jid" is replaced
	
	JABSelectAllActiveQuery *_activeQuery;
}
@property(retain,readwrite) NSString *sourceDomain;
@property(retain,readwrite) NSString *destDomain;
@property(retain,readwrite) NSArray *ownerTables;
@property(retain,readwrite) NSArray *jidTables;
@property(retain,readwrite) JABSelectAllActiveQuery *activeQuery;

- (id) initWithCommandOptions: (NSDictionary *) cmdOpts;
- (void) dealloc;

- (BOOL) requiresJid;

- (void) doDBAction;

- (BOOL) updateTables: (NSArray *) tableList column: (NSString *) column
			 oldValue: (NSString *) oldVal newValue: (NSString *) newVal;

@end

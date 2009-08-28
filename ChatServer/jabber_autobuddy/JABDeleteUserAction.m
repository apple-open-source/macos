//
//  JABDeleteUserAction.m
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 7/18/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import "JABDeleteUserAction.h"

#import "JABDatabase.h"
#import "JABDatabaseQuery.h"

//------------------------------------------------------------------------------
// JABDeleteUserAction
//------------------------------------------------------------------------------
@implementation JABDeleteUserAction

@synthesize deleteItems = _deleteItems;

//------------------------------------------------------------------------------
- (id) initWithCommandOptions: (NSDictionary *) cmdOpts
{
	self = [super initWithCommandOptions: cmdOpts];
	
	[self initDeleteItems];
	
	return self;
}

//------------------------------------------------------------------------------
- (void) dealloc
{
	self.deleteItems = nil;
	
	[super dealloc];
}

//------------------------------------------------------------------------------
- (void) initDeleteItems
{
	self.deleteItems = 
	[NSArray arrayWithObjects: 
	 [NSArray arrayWithObjects: @"active", @"collection-owner", nil],
	 [NSArray arrayWithObjects: @"vcard", @"collection-owner", nil],
	 [NSArray arrayWithObjects: @"disco-items", @"collection-owner", nil],
	 [NSArray arrayWithObjects: @"privacy-default", @"collection-owner", nil],
	 [NSArray arrayWithObjects: @"privacy-items", @"collection-owner", nil],
	 [NSArray arrayWithObjects: @"private", @"collection-owner", nil],
	 [NSArray arrayWithObjects: @"queue", @"collection-owner", nil],
	 [NSArray arrayWithObjects: @"roster-groups", @"collection-owner", nil],
	 [NSArray arrayWithObjects: @"roster-groups", @"jid", nil],
	 [NSArray arrayWithObjects: @"roster-items", @"collection-owner", nil],
	 [NSArray arrayWithObjects: @"roster-items", @"jid", nil],
	 [NSArray arrayWithObjects: @"vacation-settings", @"collection-owner", nil],
	 [NSArray arrayWithObjects: @"status", @"collection-owner", nil],
	 [NSArray arrayWithObjects: @"motd-times", @"collection-owner", nil],
	 nil];
}

//------------------------------------------------------------------------------
- (void) doDBAction 
{
	// Execute the deletion list
	for (NSArray *anItem in _deleteItems) {
		[_database deleteRowFromTable: [anItem objectAtIndex: 0] 
						  whereColumn: [anItem objectAtIndex: 1]
						  equalsValue: _targetJid
							   source: __PRETTY_FUNCTION__
								 line: __LINE__];
		if (![self checkDatabaseStatus]) 
			break; // operation failed -- abort processing
	} // for..in
}

@end

//------------------------------------------------------------------------------
// JABDeleteAllUsersAction
//------------------------------------------------------------------------------
@implementation JABDeleteAllUsersAction

@synthesize testPrefix = _testPrefix;
@synthesize activeQuery  = _activeQuery;

//------------------------------------------------------------------------------
- (id) initWithCommandOptions: (NSDictionary *) cmdOpts
{
	self = [super initWithCommandOptions: cmdOpts];
	
 	self.testPrefix = [cmdOpts objectForKey: CMDOPT_KEY_TESTPREFIX];
	if ((nil == _testPrefix) || (1 > [_testPrefix length]) || 
		([_testPrefix isEqualToString: OPTINFO_PREFIX_USEDEFAULT])) {
		self.testPrefix = TESTUSER_DEFAULTPREFIX;
	}

	self.activeQuery = [JABSelectAllActiveQuery jabSelectAllActiveQueryForAction: self];
	
	return self;
}

//------------------------------------------------------------------------------
- (void) dealloc
{
	self.activeQuery = nil;
	self.testPrefix = nil;
	
	[super dealloc];
}

//------------------------------------------------------------------------------
- (BOOL) requiresJid
{
	return NO;
}

//------------------------------------------------------------------------------
- (void) doDBAction 
{
	// Remove all users matching the PREFIX criteria

	if ([_testPrefix isEqualToString: TESTUSER_DEFAULTPREFIX]) {
		// Execute the deletion list for ALL users
		for (NSArray *anItem in _deleteItems) {
			[_database deleteAllRowsFromTable: [anItem objectAtIndex: 0] 
									   source: __PRETTY_FUNCTION__
										 line: __LINE__];
			if (![self checkDatabaseStatus]) 
				break; // operation failed -- abort processing
		} // for..in
	}
	else {
		// Execute the deletion list for MATCHING users
		if (![_activeQuery startStatement])
			return; // query initialization failed -- abort
		NSString *ownerJid = nil;
		while (nil != (ownerJid = [_activeQuery getNextActiveJid])) {

			if (![ownerJid hasPrefix: _testPrefix])
				continue; // skip non-matching JID
			
			for (NSArray *anItem in _deleteItems) {
				[_database deleteRowFromTable: [anItem objectAtIndex: 0] 
								  whereColumn: [anItem objectAtIndex: 1]
								  equalsValue: ownerJid
									   source: __PRETTY_FUNCTION__
										 line: __LINE__];
				if (![self checkDatabaseStatus]) 
					break; // operation failed -- abort processing
			} // for..in
			
		}
		[_activeQuery finalizeStatement];
	}
	
}

@end

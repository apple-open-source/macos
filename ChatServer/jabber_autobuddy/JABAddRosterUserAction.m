//
//  JABAddRosterUserAction.m
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 7/18/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import "JABAddRosterUserAction.h"

#import "JABDatabaseQuery.h"

@implementation JABAddRosterUserAction

@synthesize activeQuery  = _activeQuery;

//------------------------------------------------------------------------------
- (id) initWithCommandOptions: (NSDictionary *) cmdOpts
{
	self = [super initWithCommandOptions: cmdOpts];
	
	self.activeQuery = [JABSelectAllActiveQuery jabSelectAllActiveQueryForAction: self];
	
	return self;
}

- (void) dealloc
{
	self.activeQuery = nil;
	
	[super dealloc];
}

//------------------------------------------------------------------------------
- (void) doDBAction 
{
	// Add the target JID to buddy lists of all other active users
	
	// Check that target JID actually represents an active user
	BOOL isActiveJid = [_database verifyActiveJid: _targetJid 
								   expectedResult: YES];
	if (![self checkDatabaseStatus]) 
		return; // operation failed -- abort processing
	if (!isActiveJid) return; // targetJid not found in active table

	// prepare a query for reading active JIDs
	if (![_activeQuery startStatement])
 		return; // query initialization failed -- abort

	// Create the insert list of bi-directional buddy pairings for each JID in 
	// active table where that pairing does not already exist in roster-items
	NSString *buddyJid = nil;
	while (nil != (buddyJid = [_activeQuery getNextActiveJid])) {
		
		if ([buddyJid isEqualToString: _targetJid])
			continue; // don't pair ownerJid with itself
		
		// perform the forward pairing
		BOOL bExists = [_database verifyRosterItemForOwner: _targetJid 
												  andBuddy: buddyJid];
		if (![self checkDatabaseStatus]) 
			break; // operation failed -- abort processing
		if (bExists) continue; // roster item already exists -- skip
		[_database insertRosterItemForOwner: _targetJid 
								   andBuddy: buddyJid
									 source: __PRETTY_FUNCTION__
									   line: __LINE__];
		if (![self checkDatabaseStatus]) 
			break; // operation failed -- abort processing
		
		// perform the reverse pairing
		bExists = [_database verifyRosterItemForOwner: buddyJid 
											 andBuddy: _targetJid];
		if (![self checkDatabaseStatus]) 
			break; // operation failed -- abort processing
		if (bExists) continue; // roster item already exists -- skip
		[_database insertRosterItemForOwner: buddyJid 
								   andBuddy: _targetJid
									 source: __PRETTY_FUNCTION__
									   line: __LINE__];
		if (![self checkDatabaseStatus]) 
			break; // operation failed -- abort processing
	} // for..in

	[_activeQuery finalizeStatement]; // clean up
}

@end


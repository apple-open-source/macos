//
//  JABRemoveGroupBuddiesAction.m
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 9/26/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import "JABRemoveGroupBuddiesAction.h"

#import "JABDatabaseQuery.h"

@implementation JABRemoveGroupBuddiesAction

@synthesize groupName = _groupName;
@synthesize activeQuery1 = _activeQuery1;
@synthesize activeQuery2 = _activeQuery2;

//------------------------------------------------------------------------------
- (id) initWithCommandOptions: (NSDictionary *) cmdOpts
{
	self = [super initWithCommandOptions: cmdOpts];
	
	self.groupName = [cmdOpts objectForKey: CMDOPT_KEY_GROUPNAME];

	self.activeQuery1 = [JABSelectAllActiveQuery jabSelectAllActiveQueryForAction: self];
	self.activeQuery2 = [JABSelectAllActiveQuery jabSelectAllActiveQueryForAction: self];
	
	return self;
}

- (void) dealloc
{
	self.activeQuery2 = nil;
	self.activeQuery1 = nil;
	self.groupName = nil;
	
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
	// Remove roster-groups items for all users & buddies belonging to a group
	
	// prepare a query for reading active JIDs
	if (![_activeQuery1 startStatement])
 		return; // query initialization failed -- abort
	
	// for each active JID, look for existing pairings with all other 
	// active JIDS in the roster-groups table and remove pairings
	// associated with the OD group.
	NSString *ownerJid = nil;
	while (nil != (ownerJid = [_activeQuery1 getNextActiveJid])) {
		
		// prepare a second query for reading active JIDs
		if (![_activeQuery2 startStatement])
			break; // query initialization failed -- abort
		
		NSString *buddyJid = nil;
		while (nil != (buddyJid = [_activeQuery2 getNextActiveJid])) {

			// verify roster-item prerequisites
			if ([buddyJid isEqualToString: ownerJid])
				continue; // skip self-pairing

			// check if the pairing exists in roster-group
			BOOL bExists = [_database verifyRosterGroup: _groupName 
											   forOwner: ownerJid 
											   andBuddy: buddyJid];
			if (![self checkDatabaseStatus]) 
				break; // operation failed -- abort processing
			if (bExists) {
				// Remove the roster-groups entry
				[_database deleteRosterGroup: _groupName
									forOwner: ownerJid
									andBuddy: buddyJid
									  source: __PRETTY_FUNCTION__
										line: __LINE__];
				if (![self checkDatabaseStatus]) 
					break; // operation failed -- abort processing
			}

			// check if the pairing exists in roster-items
			bExists = [_database verifyRosterItemForOwner: ownerJid andBuddy: buddyJid];
			if (![self checkDatabaseStatus]) 
				break; // operation failed -- abort processing
			if (bExists) {
				// Remove the roster-items entry
				[_database deleteRosterItemForOwner: ownerJid
									andBuddy: buddyJid
									  source: __PRETTY_FUNCTION__
										line: __LINE__];
				if (![self checkDatabaseStatus])
					break; // operation failed -- abort processing
			}
		} // while buddyJid

		[_activeQuery2 finalizeStatement];
		
		if (OPRESULT_OK != self.result) break; // query error -- abort

	} // while ownerJid

	[_activeQuery1 finalizeStatement];
}

@end

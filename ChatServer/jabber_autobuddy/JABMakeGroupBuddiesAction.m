//
//  JABMakeGroupBuddiesAction.m
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 7/18/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import "JABMakeGroupBuddiesAction.h"

#import "JABAddRosterUserAction.h"

#include <membershipPriv.h>
#include <membership.h>

enum {
	RosterGroupAction_None = 0,
	RosterGroupAction_Add,
	RosterGroupAction_Delete
};

@implementation JABMakeGroupBuddiesAction

@synthesize groupName = _groupName;

//------------------------------------------------------------------------------
- (id) initWithCommandOptions: (NSDictionary *) cmdOpts
{
	self = [super initWithCommandOptions: cmdOpts];
	
	self.groupName = [cmdOpts objectForKey: CMDOPT_KEY_GROUPNAME];

	return self;
}

- (void) dealloc
{
	self.groupName = nil;
	
	[super dealloc];
}

//------------------------------------------------------------------------------
- (void) addRosterItemForOwner: (NSString *) ownerJid andBuddy: (NSString *) buddyJid
{
	// Create a roster-group entry for the pair
	
	// set up an local autorelease pool to limit high-water memory usage
	NSAutoreleasePool *aPool = [[NSAutoreleasePool alloc] init];
	
	do { // not a loop
		
		// check group membership for pair
		BOOL bAreMembers = [self verifyGroupMembershipForOwner: ownerJid 
													  andBuddy: buddyJid];

		// check if the pairing exists in our special auto-add group
		BOOL bExists = [_database verifyRosterGroup: _groupName 
										   forOwner: ownerJid 
										   andBuddy: buddyJid];
		if (![self checkDatabaseStatus]) 
			break; // operation failed -- abort processing

		// Select the roster-groups/roster-items action to be taken
		NSInteger iRGAction = RosterGroupAction_None;
		if (bAreMembers && !bExists) // members that need to be added
			iRGAction = RosterGroupAction_Add;
		else if (!bAreMembers && bExists) // non-member that need to be removed
			iRGAction = RosterGroupAction_Delete;
		
		// Execute the action
		switch (iRGAction) {
			case RosterGroupAction_Add:
				// Use base-class to handle creation of roster-item pair
				[super addRosterItemForOwner: ownerJid andBuddy: buddyJid];
				if (OPRESULT_OK != self.result) break; // query failed -- abort
				// add the roster-group entry for the new roster-item
				// NOTE: it is not necessary to perform the reverse pairing here
				//       as it will will be performed when the buddyJid becomes
				//       the ownerJid in a later iteration.
				[_database insertRosterGroup: _groupName 
									forOwner: ownerJid 
									andBuddy: buddyJid
									  source: __PRETTY_FUNCTION__ 
										line: __LINE__];
				[self checkDatabaseStatus];
				break; // done
			case RosterGroupAction_Delete:
				// Pairings outside the group are automatically removed ONLY IF 
				// they also exist in the roster-groups table.  This extra check 
				// reduces the possibility of deleting user-added buddy pairings.
				[_database deleteRosterGroup: _groupName 
									forOwner: ownerJid 
									andBuddy: buddyJid
									  source: __PRETTY_FUNCTION__
										line: __LINE__];
				if (![self checkDatabaseStatus]) break; // operation failed -- abort processing
				// ...and remove the roster-item
				[_database deleteRosterItemForOwner: ownerJid
										   andBuddy: buddyJid
											 source: __PRETTY_FUNCTION__
											   line: __LINE__];
				[self checkDatabaseStatus];
				break; // done
				
			default: ; // no action required
		} // switch
		
	} while (0); // not a loop
	
	[aPool drain];
}

//------------------------------------------------------------------------------
- (BOOL) verifyGroupMembershipForOwner: (NSString *) ownerJid andBuddy: (NSString *) buddyJid 
{
	// verify group membership for both parties
	if (![self isGroupMemberJid: ownerJid])
		return NO;
	return [self isGroupMemberJid: buddyJid];
}

//------------------------------------------------------------------------------
- (BOOL) isGroupMemberJid: (NSString *) aJid
{
	// Isolate the user name from the jid and verify that it's a directory user
	NSString *userName = [[aJid componentsSeparatedByString: @"@"] objectAtIndex: 0];
	if (nil == userName) {
		[self logMalformedJidError: aJid];
		self.result = OPRESULT_FAILED;
		return NO;
	}
	
	// Figure if user (short name) is member of asked group (short name).
	
	if ([userName UTF8String] == NULL || [_groupName UTF8String] == NULL)
		return NO;
	
	uuid_t user_uuid;
	if (0 != mbr_user_name_to_uuid([userName UTF8String], user_uuid))
		return NO;
	
	uuid_t group_uuid;
	if (0 != mbr_group_name_to_uuid([_groupName UTF8String], group_uuid))
		return NO;
	
	int isMember;
	if (0 != mbr_check_membership(user_uuid, group_uuid, &isMember))
		return NO;
	
	return (0 != isMember);
}

@end

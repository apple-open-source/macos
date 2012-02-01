//
//  JABRemoveGroupBuddiesByGuidAction.m
//  ChatServer/jabber_autobuddy
//
//  Copyright 2010 Apple. All rights reserved.
//

#import "JABRemoveGroupBuddiesByGuidAction.h"
#import "JABDatabaseQuery.h"
#include <sys/param.h>

@implementation JABRemoveGroupBuddiesByGuidAction

@synthesize groupGuid = _groupGuid;
@synthesize activeQuery1 = _activeQuery1;
@synthesize activeQuery2 = _activeQuery2;
@synthesize jabDir = _jabDir;

//------------------------------------------------------------------------------
- (id) initWithCommandOptions: (NSDictionary *) cmdOpts
{
	self = [super initWithCommandOptions: cmdOpts];
	
	self.groupGuid = [cmdOpts objectForKey: CMDOPT_KEY_GROUPGUID];
	self.jabDir = [JABDirectory jabDirectoryWithScope: DIRECTORYSCOPE_SEARCH];

	self.activeQuery1 = [JABSelectAllActiveQuery jabSelectAllActiveQueryForAction: self];
	self.activeQuery2 = [JABSelectAllActiveQuery jabSelectAllActiveQueryForAction: self];
	
	return self;
}

- (void) dealloc
{
	self.activeQuery2 = nil;
	self.activeQuery1 = nil;
	self.groupGuid = nil;
	self.jabDir = nil;

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

	uuid_t group_uuid;
	uuid_parse([_groupGuid UTF8String], group_uuid);
	char *group_uuid_string = (char *)malloc(MAXLOGNAME);
	if (0 != mbr_uuid_to_string(group_uuid, group_uuid_string)) {
		[_logger logInfo: [NSString stringWithFormat: @"Failed to find group name for group UUID: %@", _groupGuid]];
		free(group_uuid_string);
		return;
	}
	NSString *groupName = [_jabDir findGroupNameForGeneratedUID:
				[NSString stringWithCString: group_uuid_string encoding: NSUTF8StringEncoding]];
	free(group_uuid_string);

	if (nil == groupName) {
		[_logger logInfo: [NSString stringWithFormat: @"Failed to find group name for group UUID: %@", _groupGuid]];
		return;
	}

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
			BOOL bExists = [_database verifyRosterGroup: groupName 
											   forOwner: ownerJid 
											   andBuddy: buddyJid];
			if (![self checkDatabaseStatus]) 
				break; // operation failed -- abort processing
			if (!bExists) continue; // nothing to remove
			
			// Remove the roster-groups entry
			[_database deleteRosterGroup: groupName
								forOwner: ownerJid
								andBuddy: buddyJid
								  source: __PRETTY_FUNCTION__
									line: __LINE__];
			if (![self checkDatabaseStatus]) 
				break; // operation failed -- abort processing

		} // while buddyJid

		[_activeQuery2 finalizeStatement];

		if (OPRESULT_OK != self.result) break; // query error -- abort

	} // while ownerJid

	[_activeQuery1 finalizeStatement];
}

@end

//
//  JABMakeAllBuddiesAction.m
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 7/18/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import "JABMakeAllBuddiesAction.h"

#import "JABDatabaseQuery.h"

@implementation JABMakeAllBuddiesAction

@synthesize activeQuery1 = _activeQuery1;
@synthesize activeQuery2 = _activeQuery2;
@synthesize userExceptionList = _userExceptionList;

//------------------------------------------------------------------------------
- (id) initWithCommandOptions: (NSDictionary *) cmdOpts
{
	self = [super initWithCommandOptions: cmdOpts];
	
	self.activeQuery1 = [JABSelectAllActiveQuery jabSelectAllActiveQueryForAction: self];
	self.activeQuery2 = [JABSelectAllActiveQuery jabSelectAllActiveQueryForAction: self];

	self.userExceptionList = [NSMutableArray arrayWithCapacity: 0];
	[_userExceptionList addObject: EXCLUDED_USER_NOTIFICATION];

	return self;
}

- (void) dealloc
{
	self.activeQuery2 = nil;
	self.activeQuery1 = nil;
	
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
	// Add JIDs for all users to buddy lists of all other users

	// prepare a query for reading active JIDs
	if (![_activeQuery1 startStatement])
 		return; // query initialization failed -- abort
	
	// for each active JID, pair with all other active JIDS
	// note: the outer loop establishes the base (owner) jid
	//       for each pairing, while the inner loop pairs
	//       the owner with all other entries in the active
	//       table -- excluding itself and duplicate entries
	NSString *ownerJid = nil;
	while (nil != (ownerJid = [_activeQuery1 getNextActiveJid])) {

		// prepare a second query for reading active JIDs
		if (![_activeQuery2 startStatement])
			break; // query initialization failed -- abort

		NSString *buddyJid = nil;
		while (nil != (buddyJid = [_activeQuery2 getNextActiveJid])) {
			// verify roster-item prerequisites
			if ([self shouldModifyRosterItemForOwner: ownerJid andBuddy: buddyJid]) {
				// add roster items (if needed)
				[self addRosterItemForOwner: ownerJid andBuddy: buddyJid];
				if (OPRESULT_OK != self.result) break; // query error -- abort
			}
		} // while
		[_activeQuery2 finalizeStatement];

		if (OPRESULT_OK != self.result) break; // query error -- abort
	}
	[_activeQuery1 finalizeStatement];
}

//------------------------------------------------------------------------------
- (BOOL) shouldModifyRosterItemForOwner: (NSString *) ownerJid andBuddy: (NSString *) buddyJid
{
	// check for illegal pairings
	NSArray *buddyJidComponents = [buddyJid componentsSeparatedByString:@"@"];
	NSArray *ownerJidComponents = [ownerJid componentsSeparatedByString:@"@"];
	
	for (NSString *username in _userExceptionList) {
		if ([username isEqualToString: [buddyJidComponents objectAtIndex: 0]] ||
					[username isEqualToString: [ownerJidComponents objectAtIndex: 0]]) {
			return NO;
		}
	}

	return (![buddyJid isEqualToString: ownerJid]);
}

//------------------------------------------------------------------------------
- (void) addRosterItemForOwner: (NSString *) ownerJid andBuddy: (NSString *) buddyJid
{
	// set up an local autorelease pool to limit high-water memory usage
	NSAutoreleasePool *aPool = [[NSAutoreleasePool alloc] init];
	
	do { // not a loop

		// check for redundant pairings
		BOOL bExists = [_database verifyRosterItemForOwner: ownerJid
												  andBuddy: buddyJid];
		if (![self checkDatabaseStatus]) 
			break; // operation failed -- abort processing
		if (bExists) break; // roster item already exists -- skip
		
		// perform the forward pairing
		// NOTE: it is not necessary to perform the reverse pairing here
		//       as it will will be performed when the buddyJid becomes
		//       the ownerJid in a later iteration.
		[_database insertRosterItemForOwner: ownerJid
								   andBuddy: buddyJid
									 source: __PRETTY_FUNCTION__
									   line: __LINE__];
		[self checkDatabaseStatus];

	} while (0); // not a loop

	[aPool drain];
}

@end


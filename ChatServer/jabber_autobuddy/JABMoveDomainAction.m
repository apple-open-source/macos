//
//  JABMoveDomainAction.m
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 9/16/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import "JABMoveDomainAction.h"

#import "JABLogger.h"
#import "JABDatabase.h"
#import "JABDatabaseQuery.h"

#import <signal.h>

//------------------------------------------------------------------------------
// JABMoveDomainAction methods
//------------------------------------------------------------------------------
@implementation JABMoveDomainAction

@synthesize sourceDomain = _sourceDomain;
@synthesize destDomain = _destDomain;
@synthesize activeQuery  = _activeQuery;
@synthesize ownerTables  = _ownerTables;
@synthesize jidTables  = _jidTables;

//------------------------------------------------------------------------------
- (id) initWithCommandOptions: (NSDictionary *) cmdOpts
{
	self = [super initWithCommandOptions: cmdOpts];

	self.sourceDomain = [cmdOpts objectForKey: CMDOPT_KEY_SRCDOMAIN];
	self.destDomain = [cmdOpts objectForKey: CMDOPT_KEY_DSTDOMAIN];

	self.activeQuery = [JABSelectAllActiveQuery jabSelectAllActiveQueryForAction: self];
	
	// Define the tables for which "collection-owner" must be replaced
	self.ownerTables = [NSArray arrayWithObjects: 
						@"logout", @"motd-times", @"privacy-items", 
						@"private", @"queue", @"roster-groups", 
						@"roster-items", @"vacation-settings",
						@"active", 
						nil];
	
	// Define the tables for which "jid" column must be replaced
	self.jidTables = [NSArray arrayWithObjects: 
					  @"roster-groups", @"roster-items",
					  nil];
	
	return self;
}

- (void) dealloc
{
	self.jidTables = nil;
	self.ownerTables = nil;
	self.activeQuery = nil;
	self.destDomain = nil;
	self.sourceDomain = nil;
	
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
	// Move all JIDs belonging to sourceDomain into destDomain

#ifndef DEBUG
	if ([self isJabberdRunning]) {
		[_logger logStdErrMessage: @"\nThis operation is not available while the iChat Service is running.\n\n"];
		return;
	}
#endif

	// get a count of all active users
	NSInteger totalUsers = 0;
 	if (![_activeQuery startStatement])
 		return; // query initialization failed -- abort
	NSString *ownerJid = nil;
	while (nil != (ownerJid = [_activeQuery getNextActiveJid])) {
		// Verify that the JID belongs to the target domain
		NSArray *jidParts = [ownerJid componentsSeparatedByString: @"@"];
		if (2 > [jidParts count]) continue;  // malformed JID?
		if (![_sourceDomain isEqualToString: [jidParts objectAtIndex: 1]])
			continue; // ignore active users not in targeted domain
		totalUsers++;
	}
	[_activeQuery finalizeStatement];

	if (1 > totalUsers) {
		[_logger logInfo: [NSString stringWithFormat: 
						   @"No active users found for domain \"%@\".", 
						   _sourceDomain]];
		[_logger logInfo: @"*** The existing jabberd database has not been modified."];
		return; // no action taken
	}
	
	// make a backup copy of the database before applying modifications
	[_logger logInfo: @"Creating backup for jabberd database."];
	if (![_database createBackup]) {
		[_logger logInfo: @"Operation canceled: database backup could not be performed."];
		return; // database backup failed -- abort
	}
	
	[_logger logInfo: [NSString stringWithFormat: 
					   @"Moving %ld accounts from \"%@\" to \"%@\".", 
					   totalUsers, _sourceDomain, _destDomain]];
	
	// prepare a query for reading active JIDs
	if (![_activeQuery startStatement])
 		return; // query initialization failed -- abort

	NSInteger iUser = 0;
	NSInteger movedUsers = 0;
	BOOL bOK = YES;
	while (nil != (ownerJid = [_activeQuery getNextActiveJid])) {

		// Verify that the JID belongs to the target domain
		NSArray *jidParts = [ownerJid componentsSeparatedByString: @"@"];
		if (2 > [jidParts count]) continue;  // malformed JID?
		if (![_sourceDomain isEqualToString: [jidParts objectAtIndex: 1]])
			continue; // ignore active users not in targeted domain

		// Create the new JID from the existing user name and new domain
		NSString *userName = [jidParts objectAtIndex: 0];
		NSString *newJid = [userName stringByAppendingFormat: @"@%@", _destDomain];

		[_logger logStdErrMessage: [NSString stringWithFormat: 
									@"[%ld/%ld]: %-20s", 
									++iUser, totalUsers, [userName UTF8String]]];
		
		// Update all tables where "jid" matches the old JID
		bOK = [self updateTables: _jidTables column: @"jid"
						oldValue: ownerJid newValue: newJid]; 
		if (!bOK) break; // error -- abort
		
		// Update all tables where "collection-owner" matches the old JID
		bOK = [self updateTables: _ownerTables column: @"collection-owner"
						oldValue: ownerJid newValue: newJid]; 
		if (!bOK) break; // error -- abort

		[_logger logStdErrMessage: @" [done]\n"];
		movedUsers++;
	}

	if (bOK) {
		[_logger logInfo: [NSString stringWithFormat: 
						   @"%ld/%ld accounts successfully moved.",
						   movedUsers, totalUsers]];
		[_logger logInfo: [NSString stringWithFormat: 
						   @"A copy of the original database was saved at %@",
						   [_database backupPath]]];
	}
	else {
		[_logger logInfo: @"\nOperation failed due to errors."];
		[_logger logInfo: @"*** The existing jabberd database has not been modified."];
	}
	
	[_activeQuery finalizeStatement];
}

//------------------------------------------------------------------------------
- (BOOL) updateTables: (NSArray *) tableList column: (NSString *) column
			 oldValue: (NSString *) oldVal newValue: (NSString *) newVal
{
	// Replace the selected column values for all tables in the list
	self.result = OPRESULT_OK;

	for (NSString *tableName in tableList) {
		[_database updateTable: tableName column: column 
					  oldValue: oldVal newValue: newVal 
						source: __PRETTY_FUNCTION__ line: __LINE__];
		if (SQLITE_OK != [_database dbResult]) {
			self.result = OPRESULT_FAILED;
			break;
		}
//		[_logger logStdErrMessage: @"."]; // indicate partial progress
	} // for..in

	return (OPRESULT_OK == _result);
}

@end

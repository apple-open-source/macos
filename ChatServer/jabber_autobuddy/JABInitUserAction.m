//
//  JABInitUserAction.m
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 7/18/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import "JABInitUserAction.h"

#import "JABActionInfo.h"

@implementation JABInitUserAction

//------------------------------------------------------------------------------
- (void) doDBAction 
{
	// See if user already exists
	BOOL isActiveJid = [_database verifyActiveJid: _targetJid 
								   expectedResult: NO];
	if (![self checkDatabaseStatus]) 
		return; // operation failed -- abort processing
	if (isActiveJid) return;
	
	// Execute the queries
	[_database insertActiveItemForOwner: _targetJid 
								 source: __PRETTY_FUNCTION__ 
								   line: __LINE__];
	if (![self checkDatabaseStatus]) 
		return; // operation failed -- abort processing

	[_database insertVcardItemForOwner: _targetJid
								source: __PRETTY_FUNCTION__
								  line: __LINE__];
	if (![self checkDatabaseStatus]) 
		return; // operation failed -- abort processing
}

@end

@implementation JABInitTestUsersAction

@synthesize testCount = _testCount;
@synthesize testPrefix = _testPrefix;

//------------------------------------------------------------------------------
- (id) initWithCommandOptions: (NSDictionary *) cmdOpts
{
	self = [super initWithCommandOptions: cmdOpts];
	
 	self.testCount = [[cmdOpts objectForKey: CMDOPT_KEY_TESTCOUNT] integerValue];

 	self.testPrefix = [cmdOpts objectForKey: CMDOPT_KEY_TESTPREFIX];
	if ((nil == _testPrefix) || (1 > [_testPrefix length]) || 
		([_testPrefix isEqualToString: OPTINFO_PREFIX_USEDEFAULT]))
		self.testPrefix = TESTUSER_DEFAULTPREFIX;
	
	return self;
}

//------------------------------------------------------------------------------
- (void) dealloc
{
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
	// Create a set of users based on auto-generated names 
	// in the server's domain (DEBUG ONLY)
	
	NSString *domain = nil; 
	
	// use the current hostname for the generated JIDs
	NSArray *hostNames = [[NSHost currentHost] names];
	if (1 < [hostNames count]) {
		for (NSString *aName in hostNames) {
			// avoid localhost or *.local if possible
			if ([aName isEqualToString: @"localhost"]) continue;
			if ([aName hasSuffix: @".local"]) continue;
			domain = aName; // use this
			break;
		}
		if (nil == domain) // perfered domain not available
			domain = [hostNames objectAtIndex: 0]; // use what is available
	}
	if (nil == domain) return; // no hostnames? -- abort

	NSInteger iUser = 0;
	for ( ; iUser < self.testCount; iUser++) {

		// format the test user's jid
		NSString *testJid = [NSString stringWithFormat: @"%@%ld@%@", 
							 _testPrefix, iUser, domain];
		
		// See if user already exists
		BOOL isActiveJid = [_database verifyActiveJid: testJid
									   expectedResult: NO];
		if (![self checkDatabaseStatus]) 
			break; // operation failed -- abort processing
		if (isActiveJid) continue; // ignore duplicate entries
		
		// Execute the queries
		[_database insertActiveItemForOwner: testJid
									 source: __PRETTY_FUNCTION__
									   line: __LINE__];
		if (![self checkDatabaseStatus]) 
			break; // operation failed -- abort processing

		[_database insertVcardItemForOwner: testJid
									source: __PRETTY_FUNCTION__
									  line: __LINE__];
		if (![self checkDatabaseStatus]) 
			break; // operation failed -- abort processing
		
	} // for
}
@end


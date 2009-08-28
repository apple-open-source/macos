//
//  JABDeleteRosterUserAction.m
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 7/18/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import "JABDeleteRosterUserAction.h"

@implementation JABDeleteRosterUserAction

//------------------------------------------------------------------------------
+ (NSArray *) jabGetDeleteRosterUserItems
{
	static NSArray *deleteItems = nil;
	
	if (nil != deleteItems) 
		return deleteItems;
	
	deleteItems = [NSArray arrayWithObjects: 
				   [NSArray arrayWithObjects: @"roster-groups", @"collection-owner", nil],
				   [NSArray arrayWithObjects: @"roster-groups", @"jid", nil],
				   [NSArray arrayWithObjects: @"roster-items", @"collection-owner", nil],
				   [NSArray arrayWithObjects: @"roster-items", @"jid", nil],
				   nil];
	return deleteItems;
}

//------------------------------------------------------------------------------
- (void) doDBAction 
{
	// Create the list of tables+fields to be deleted
	NSArray *deleteItems = [JABDeleteRosterUserAction jabGetDeleteRosterUserItems];
	
	// Execute the deletion list
	for (NSArray *anItem in deleteItems) {
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


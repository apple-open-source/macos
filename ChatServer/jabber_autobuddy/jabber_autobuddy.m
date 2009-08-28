// 
//	jabber_autobuddy.m
//  ChatServer/jabber_autobuddy
//
//  Created by Steve Peralta on 9/26/08.
//	Copyright 2006-2008 Apple Inc.  All Rights Reserved.
//
//  Description:
//  Tool for managing jabberd "buddies" and other Jabber database operations.  
//  NOTE: Only supports SQLite3 database backend. 
//

#import <Foundation/Foundation.h>

#include "JABActionInfo.h"

//------------------------------------------------------------------------------
// Main
//------------------------------------------------------------------------------
int main (int argc, const char * argv[]) 
{
    NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];

	// Assemble the list of command to be executed from the command line input
	JABActionInfo *actionInfo = [JABActionInfo jabActionInfoWithCommandInput: 
								 [[NSProcessInfo processInfo] arguments]];
	
	if (![actionInfo processCommandInput])
		return 0;
	
	// Prevent conflicting commands from being used together
	if ([actionInfo checkActionConflicts])
		 return 1;

	// Perform the requested actions
	[actionInfo performRequestedActions];
    
	[pool drain];

    return 0;
}

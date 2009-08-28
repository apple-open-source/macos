//
//  JABShowVersionAction.m
//  ChatServer2
//
//  Created by Steve Peralta on 8/8/08.
//  Copyright 2008 Apple. All rights reserved.
//

#import "JABShowVersionAction.h"

#import "JABLogger.h"

//------------------------------------------------------------------------------
@implementation JABShowVersionAction

- (void) doAction 
{
	// Display the version text from the pre-defined options metadata
	NSString *versTxt = [[self.actionInfo optionsInfo] objectForKey: OPTINFO_KEY_VERSIONTEXT];
	NSArray *outLines = [NSArray arrayWithObjects:
						 [NSString stringWithFormat: @"%@", versTxt],
#ifdef DEBUG
						 [NSString stringWithFormat: @"Built: %s  %s", __DATE__, __TIME__],
#endif
						 @" ",
						 nil];
	for (NSString *line in outLines)
		[_logger logStdErrMessage: [NSString stringWithFormat: @"%@\n", line]];
}
@end


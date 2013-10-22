//
//  NSSet+compactDescription.m
//  KeychainMigrator
//
//  Created by J Osborne on 3/21/13.
//
//

#import "NSSet+compactDescription.h"

@implementation NSSet (compactDescription)

-(NSString*)compactDescription
{
	NSMutableArray *results = [NSMutableArray new];
	for (id v in self) {
		if ([v respondsToSelector:@selector(compactDescription)]) {
			[results addObject:[v compactDescription]];
		} else {
			[results addObject:[v description]];
		}
	}
	return [NSString stringWithFormat:@"[%@]", [results componentsJoinedByString:@", "]];
}

@end

//
//  NSDictionary+compactDescription.m
//  KeychainMigrator
//
//  Created by J Osborne on 2/19/13.
//
//

#import "NSDictionary+compactDescription.h"
#import "NSString+compactDescription.h"

@implementation NSDictionary (compactDescription)

-(NSString*)compactDescription
{
	NSMutableArray *results = [NSMutableArray new];
	for (NSString *k in self) {
		id v = self[k];
		if ([v respondsToSelector:@selector(compactDescription)]) {
			v = [v compactDescription];
		} else {
			v = [v description];
		}
		
		[results addObject:[NSString stringWithFormat:@"%@=%@", [k compactDescription], v]];
	}
	return [NSString stringWithFormat:@"{%@}", [results componentsJoinedByString:@", "]];
}

-(NSString*)compactDescriptionWithoutItemData;
{
	NSMutableArray *results = [NSMutableArray new];
	for (NSString *k in self) {
		if ([k isEqualToString:kSecValueData]) {
			[results addObject:[NSString stringWithFormat:@"%@=<not-logged>", [k compactDescription]]];
			continue;
		}
		
		id v = self[k];
		if ([v respondsToSelector:@selector(compactDescription)]) {
			v = [v compactDescription];
		} else {
			v = [v description];
		}
		
		[results addObject:[NSString stringWithFormat:@"%@=%@", [k compactDescription], v]];
	}
	return [NSString stringWithFormat:@"{%@}", [results componentsJoinedByString:@", "]];

}

@end

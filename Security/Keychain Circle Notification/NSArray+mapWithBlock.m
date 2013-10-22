//
//  NSArray+mapWithBlock.m
//  Security
//
//  Created by J Osborne on 2/26/13.
//
//

#import "NSArray+mapWithBlock.h"

@implementation NSArray (mapWithBlock)

-(NSArray*)mapWithBlock:(mapBlock)block
{
	NSMutableArray *mapped = [[NSMutableArray alloc] initWithCapacity:self.count];
	
	for (id obj in self) {
		[mapped addObject:block(obj)];
	}
	
	return [mapped copy];
}

@end

//
//  NSString+compactDescription.m
//  KeychainMigrator
//
//  Created by J Osborne on 2/19/13.
//
//

#import "NSString+compactDescription.h"

@implementation NSString (compactDescription)

-(NSString*)compactDescription
{
	static NSCharacterSet *forceQuotes = nil;
	static dispatch_once_t setup;
	dispatch_once(&setup, ^{
		forceQuotes = [NSCharacterSet characterSetWithCharactersInString:@"\"' \t\n\r="];
	});
	
	if ([self rangeOfCharacterFromSet:forceQuotes].location != NSNotFound) {
		NSString *escaped = [self stringByReplacingOccurrencesOfString:@"\\" withString:@"\\\\"];
		escaped = [escaped stringByReplacingOccurrencesOfString:@"\"" withString:@"\\\""];
		return [NSString stringWithFormat:@"\"%@\"", escaped];
	} else {
		return self;
	}
}

@end

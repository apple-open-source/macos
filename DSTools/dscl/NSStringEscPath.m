/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header NSStringEscPath
 */


#import "NSStringEscPath.h"

@implementation NSString (NSStringEscPath)

+ (NSString *)escapablePathFromArray:(NSArray *)inArray
{
    NSString        *returnValue    = nil;
    
    if( [inArray count] )
    {
        NSEnumerator    *pathEnum       = [inArray objectEnumerator];
        NSString        *value          = nil;
        
        // preflight with first value
        returnValue = [[pathEnum nextObject] escapedString];
        
        while( value = [pathEnum nextObject] ) {
            returnValue = [returnValue stringByAppendingString: @"/"];
            returnValue = [returnValue stringByAppendingString: [value escapedString]];
        }
    }
    return returnValue;
}

- (NSArray *)unescapedPathComponents
{
    NSMutableArray		   *newComponents   = [NSMutableArray array];
    NSAutoreleasePool      *pool			= [[NSAutoreleasePool alloc] init];
    NSMutableString        *workingString   = [NSMutableString stringWithString: self];

    // unescape everything so we have good components that we can pass around without worrying
    [workingString replaceOccurrencesOfString: @"\\/" withString:@"%2f" options:NSLiteralSearch range:NSMakeRange(0,[workingString length])];
    [workingString replaceOccurrencesOfString: @"\\\\" withString:@"%5c" options:NSLiteralSearch range:NSMakeRange(0,[workingString length])];
    [workingString replaceOccurrencesOfString: @"\\" withString:@"" options:NSLiteralSearch range:NSMakeRange(0,[workingString length])];
    
    NSArray                *components      = [workingString pathComponents];
    NSEnumerator           *compEnum        = [components objectEnumerator];
    NSString               *component;
    
    while( component = [compEnum nextObject] )
    {
        [newComponents addObject: [component stringByReplacingPercentEscapesUsingEncoding: NSUTF8StringEncoding]];
    }

    [pool release];
    
    return newComponents;
}

- (NSString *)unescapedString
{
    NSMutableString *workingString = [[self mutableCopy] autorelease];
    
    [workingString replaceOccurrencesOfString: @"\\/" withString:@"%2f" options:NSLiteralSearch range:NSMakeRange(0,[workingString length])];
    [workingString replaceOccurrencesOfString: @"\\\\" withString:@"%5c" options:NSLiteralSearch range:NSMakeRange(0,[workingString length])];
    [workingString replaceOccurrencesOfString: @"\\" withString:@"" options:NSLiteralSearch range:NSMakeRange(0,[workingString length])];
    
    return [workingString stringByReplacingPercentEscapesUsingEncoding: NSUTF8StringEncoding];
}

- (NSString *)escapedString
{
    NSMutableString *workingString = [[self mutableCopy] autorelease];
    
    [workingString replaceOccurrencesOfString: @"\\" withString:@"\\\\" options:NSLiteralSearch range:NSMakeRange(0,[workingString length])];
    [workingString replaceOccurrencesOfString: @"/" withString: @"\\/" options:NSLiteralSearch range:NSMakeRange(0,[workingString length])];
    [workingString replaceOccurrencesOfString: @" " withString: @"\\ " options:NSLiteralSearch range:NSMakeRange(0,[workingString length])];
    
    return workingString;
}

- (NSString *)urlEncoded
{
	return [self stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding];
}

@end

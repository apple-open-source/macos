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
 * @header PathItem
 */


#import "PathItem.h"
#import "NSStringEscPath.h"

@implementation PathItem

// ----------------------------------------------------------------------------
// Initialization / teardown
#pragma mark ******** Initialization / teardown ********

- init
{
    [super init];
    return self;
}

- (NSString*)stripDSPrefixOffValue:(NSString*)inValue
{
    NSRange r = [inValue rangeOfString:@":" options:NSBackwardsSearch];
    if (!gRawMode && [inValue hasPrefix:@"ds"] && r.length > 0)
    {
        // inValue definitely (with high probability) has a prefix
        return [inValue substringFromIndex:r.location+1];
    }
    else
    { 
        // it doesn't look like a known prefix or we are
        // in raw mode, leave it alone
        return inValue;
    }
}

- (NSString*)description
{
    return [[super description] stringByAppendingFormat:@"(%@)",[self name]];
}

// ----------------------------------------------------------------------------
// PathItemProtocol implementations
// It is expected that these will all be implemented as needed
#pragma mark ******** PathItemProtocol implementations ********

- (tDirStatus) appendKey:(NSString*)inKey withValues:(NSArray*)inValues
{
    return eDSNoErr;
}

- (tDirStatus) authenticateName:(NSString*)inUsername withPassword:(NSString*)inPassword authOnly:(BOOL)inAuthOnly
{
    return eDSAuthFailed;
}

- (tDirStatus) authenticateName:(NSString*)inUsername withPassword:(NSString*)inPassword
{
    return [self authenticateName:inUsername withPassword:inPassword authOnly:NO];
}

- (tDirStatus) setPassword:(NSArray*)inPassword
{
	return eDSAuthFailed;
}

- (NSString*) name
{
    return nil;
}

- (PathItem*) cd:(NSString*)dest
{
    return nil;
}

- (tDirStatus) createKey:(NSString*)inKey withValues:(NSArray*)inValues
{
    return eDSNoErr;
}

- (tDirStatus) deleteItem
{
    return eDSNoErr;
}

- (tDirStatus) deleteKey:(NSString*)inKey withValues:(NSArray*)inValues
{
    return eDSNoErr;
}

- (tDirStatus) list:(NSString*)inPath key:(NSString*)inKey
{
    return eDSNoErr;
}

- (NSArray*) getList;
{
	return nil;
}

- (NSArray*) getPossibleCompletionsFor:(NSString*)inPrefix
{
	NSMutableArray* possibleCompletions = nil;
	NSArray* currentList = [self getList];
	if (currentList != nil)
	{
		inPrefix = [inPrefix lowercaseString];
		possibleCompletions = [NSMutableArray array];
		NSEnumerator* listEnum = [currentList objectEnumerator];
		NSString* currentItem = nil;
		
		while (currentItem = (NSString*)[listEnum nextObject])
		{
			if ([[currentItem lowercaseString] hasPrefix:inPrefix])
				[possibleCompletions addObject:currentItem];
		}
	}
	
	return possibleCompletions;
}

- (tDirStatus) mergeKey:(NSString*)inKey withValues:(NSArray*)inValues
{
    return eDSNoErr;
}

- (NSString*)nodeName
{
    return @"No Node";
}

- (tDirStatus) read:(NSArray*)inKeys
{
    return eDSNoErr;
}

- (tDirStatus) searchForKey:(NSString*)inKey withValue:(NSString*)inValue matchType:(NSString*)inType
{
	printf("You can only search a Node or Record type path.\n");
	return eDSNoErr;
}

- (tDirStatus) changeKey:(NSString*)inKey oldAndNewValues:(NSArray*)inValues
{
	return eDSNoErr;
}

- (tDirStatus) changeKey:(NSString*)inKey indexAndNewValue:(NSArray*)inValues
{
	return eDSNoErr;
}

@end

void printValue(NSString *inValue)
{
	if (gURLEncode)
		inValue = [inValue urlEncoded];
	printf(" %s", [inValue UTF8String]);
}
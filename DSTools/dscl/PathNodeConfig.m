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
 * @header PathNodeConfig
 */


#import "PathNodeConfig.h"
#import "PathRecordTypeConfig.h"

@implementation PathNodeConfig

- (PathItem*) cd:(NSString*)dest
{
	PathItem *nextItem = nil;

    // The following checks are in order of fastest check.

    // If the dest empty, abort.
    if (dest == nil || [dest length] == 0)
    {
        return nil;
    }
	
    // If the destination has a fully qualified record type, then use it;
    // else look for existing standard, then native types.
    else if ([dest hasPrefix:@"dsConfigType"])
    {
        nextItem = [[PathRecordTypeConfig alloc] initWithNode:_node recordType:dest];
    }

	// Try looking for a standard or native type by the name of the destination.
	else
	{
		NSArray *recordTypeList = [self getRecordList];
		NSString *stdDest = [NSString stringWithFormat:@"dsConfigType::%@",dest];

		if ([recordTypeList containsObject:stdDest])
			dest = stdDest;
		else
			dest = nil;

		if (dest != nil)
		{
			// The destination is either a fully qualified record type, or an existing type,
			// the next item is a record type node.
			nextItem = [[PathRecordTypeConfig alloc] initWithNode:_node recordType:dest];
		}
		else
		{
			return nil;
		}
	}

	return [nextItem autorelease];
}
@end

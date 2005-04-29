/*
 * Copyright (c) 2000 - 2003 Apple Computer, Inc. All rights reserved.
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
 * @header DSException
 */


#import "DSException.h"

@implementation DSException

+ (DSException*) name:(NSString*)inName reason:(NSString*)inReason status:(tDirStatus)inStatus
{
    DSException    *selfInstance	= nil;
    NSDictionary   *statusDict		= [NSDictionary dictionaryWithObject: [NSNumber numberWithInt:inStatus] forKey:@"status"];
	
    selfInstance = [[self alloc] initWithName:inName reason:inReason userInfo:statusDict];
	
    return [selfInstance autorelease];
}

- (id)initWithName:(NSString *)inname reason:(NSString *)inreason userInfo:(NSDictionary *)inuserInfo
{
    _dsStat = [[DSStatus sharedInstance] retain];
	
    return [super initWithName:inname reason:inreason userInfo:inuserInfo];
}

- (void) dealloc
{
    [_dsStat release];
    [super dealloc];
}	 

- (tDirStatus) status
{
    if ([self userInfo] != nil)
	{
        NSNumber *statusNumber = [[self userInfo] objectForKey:@"status"];
        if (statusNumber != nil)
            return [statusNumber intValue];
    }
    return eDSNoErr;
}

- (char*) statusCString
{
    return [_dsStat cStringForStatus:[self status]];
}

@end

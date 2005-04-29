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
 * @header DSoException
 */


#import "DSoException.h"


@implementation DSoException

+ (DSoException*) name:(NSString*)inName reason:(NSString*)inReason status:(tDirStatus)inStatus
{
    DSoException *selfInstance = nil;
    NSDictionary *statusDict = [NSDictionary dictionaryWithObject: [NSNumber numberWithInt:inStatus] forKey:kDSoExceptionStatusKey];
    selfInstance = [[self alloc] initWithName:inName reason:inReason userInfo:statusDict];
    return [selfInstance autorelease];
}

+ (void) raiseWithStatus:(tDirStatus)inStatus;
{
    [[DSoException name:nil reason:nil status:inStatus] raise];
}

- (id)initWithName:(NSString *)inname reason:(NSString *)inreason userInfo:(NSDictionary *)inuserInfo
{
    _dsStat = [[DSoStatus sharedInstance] retain];
    return [super initWithName:inname reason:inreason userInfo:inuserInfo];
}

- (void) dealloc
{
    [_dsStat release];
    [super dealloc];
}

- (void) finalize
{
    [super finalize];
}	 

- (tDirStatus) status
{
    if ([self userInfo] != nil) {
        NSNumber *statusNumber = [[self userInfo] objectForKey:kDSoExceptionStatusKey];
        if (statusNumber != nil)
            return [statusNumber intValue];
    }
    return eDSNoErr;
}

- (NSString*) statusString
{
    return [_dsStat stringForStatus:[self status]];
}

- (const char*) statusCString
{
    NSString *s = [self statusString];
    return s == nil ? nil : [s cString];
}

@end

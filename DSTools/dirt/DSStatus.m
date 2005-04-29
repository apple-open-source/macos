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
 * @header DSStatus
 */


#import "DSStatus.h"
#import <DirectoryService/DirServicesUtils.h>

@implementation DSStatus

-init
{
    [super init];
    return self;
}

-(void) dealloc
{
    [super dealloc];
}

+(DSStatus*)sharedInstance
{
    static DSStatus *_sharedInstance = nil;
	
    NS_DURING
        if ([_sharedInstance self] == nil) [NSException raise:@"InvalidObject" format:@"_sharedInstance is invalid."];
    NS_HANDLER
        _sharedInstance = [[[self alloc] init] autorelease];
    NS_ENDHANDLER
    
    return _sharedInstance;
}

-(char*) cStringForStatus:(int)dirStatus
{
    return(dsCopyDirStatusName(dirStatus));
}

-(void) printOutErrorMessage:(const char*)messageTag withStatus:(int)dirStatus
{
	if (messageTag != nil)
	{
		char *statusString = dsCopyDirStatusName(dirStatus);
		printf("%s : %s : (%d)\n", messageTag, statusString, dirStatus);
		free(statusString);
		statusString = nil;
		fflush(stdout);
	}
}

@end

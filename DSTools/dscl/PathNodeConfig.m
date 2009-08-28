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

#import <DirectoryService/DirectoryService.h>
#import <DSObjCWrappers/DSObjCWrappers.h>
#import <DirectoryServiceCore/CSharedData.h>

#import "PathNodeConfig.h"
#import "PathRecordTypeConfig.h"

@interface PathNodeConfig (Private)

- (void) _destroyRights;

@end


@implementation PathNodeConfig (Private)

- (void)_destroyRights
{
	// free _authExternalForm
	if (_haveRights) {
		[_node customCall:eDSCustomCallConfigureDestroyAuthRef
			  withAuthorization:&_authExternalForm];
	}
}

@end

@implementation PathNodeConfig

- (id)init
{
    [super init];
    bzero(&_authExternalForm,sizeof(_authExternalForm));
    return self;
}

- (id)initWithDir:(DSoDirectory*)inDir path:(NSString*)inPath
{
    [super initWithDir:inDir path:inPath];
    bzero(&_authExternalForm,sizeof(_authExternalForm));
    return self;
}

- (id)initWithNode:(DSoNode*)inNode path:(NSString*)inPath
{
    [super initWithNode:inNode path:inPath];
    bzero(&_authExternalForm,sizeof(_authExternalForm));
    return self;
}

- (void)dealloc
{
	[self _destroyRights];
    [super dealloc];
}

- (tDirStatus) authenticateName:(NSString*)inUsername withPassword:(NSString*)inPassword authOnly:(BOOL)inAuthOnly
{
	NSAutoreleasePool* pool = [NSAutoreleasePool new];
	DSoSearchNode* searchNode = [[_node directory] searchNode];
	DSoUser* user = nil;
	tDirStatus status = eDSAuthFailed;
	NSMutableData* outputData = nil;
	
	NS_DURING
	user = [searchNode findUser:inUsername];
	if (user != nil) {
		status = [[user node] authenticateName:inUsername withPassword:inPassword authOnly:YES];
		if (status == eDSNoErr && inAuthOnly == NO) {
			outputData = [NSMutableData dataWithLength:sizeof(AuthorizationExternalForm)];
			status = [_node customCall:eDSCustomCallConfigureGetAuthRef
							 sendItems:[NSArray arrayWithObjects:inUsername,inPassword,nil]
							outputData:outputData];
			if (status == eDSNoErr && [outputData length] >= sizeof( AuthorizationExternalForm ) )
			{
				[outputData getBytes:&_authExternalForm length:sizeof( AuthorizationExternalForm )];
				_haveRights = YES;
			}
		}
	}
	NS_HANDLER
	NS_ENDHANDLER
	
	[pool release];
	
    return status;
}

- (PathItem*) cd:(NSString*)dest
{
	PathRecordTypeConfig *nextItem = nil;

    // The following checks are in order of fastest check.

    // If the dest empty, abort.
    if (dest == nil || [dest length] == 0)
    {
        return nil;
    }
	
    // If the destination has a fully qualified record type, then use it;
    // else look for existing standard, then native types.
    else if ([dest hasPrefix:@"dsConfigType"] || [dest hasPrefix:@kDSStdRecordTypePrefix])
    {
        nextItem = [[PathRecordTypeConfig alloc] initWithNode:_node recordType:dest];
    }

	// Try looking for a standard or native type by the name of the destination.
	else
	{
		NSArray *recordTypeList = [self getRecordList];
		NSString *cfgDest = [NSString stringWithFormat:@"dsConfigType::%@",dest];
        NSString *stdDest = [@kDSStdRecordTypePrefix stringByAppendingString:dest];		

		if ([recordTypeList containsObject:stdDest])
			dest = stdDest;
		else if ([recordTypeList containsObject:cfgDest])
			dest = cfgDest;
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
	
	[nextItem setAuthExternalForm:&_authExternalForm];

	return [nextItem autorelease];
}

@end

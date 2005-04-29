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

#import "PathNodeSearch.h"

@interface PathNodeSearch (Private)

- (void) _destroyRights;

@end


@implementation PathNodeSearch (Private)

- (void)_destroyRights
{
	// free _authExternalForm
	if (_haveRights) {
		if (_configNode == nil)
			_configNode = [[[_node directory] findNodeViaEnum:eDSConfigNodeName] retain];
		[_configNode customCall:eDSCustomCallConfigureDestroyAuthRef
			  withAuthorization:&_authExternalForm];
	}
}

@end

@implementation PathNodeSearch

- init
{
    [super init];
    bzero(&_authExternalForm,sizeof(_authExternalForm));
    return self;
}

- initWithDir:(DSoDirectory*)inDir path:(NSString*)inPath
{
    [super initWithDir:inDir path:inPath];
    bzero(&_authExternalForm,sizeof(_authExternalForm));
    return self;
}

- initWithNode:(DSoNode*)inNode path:(NSString*)inPath
{
    [super initWithNode:inNode path:inPath];
    bzero(&_authExternalForm,sizeof(_authExternalForm));
    return self;
}

- (void)dealloc
{
	[self _destroyRights];
	[_configNode release];
    [super dealloc];
}

- (tDirStatus) authenticateName:(NSString*)inUsername withPassword:(NSString*)inPassword authOnly:(BOOL)inAuthOnly
{
	NSAutoreleasePool* pool = [NSAutoreleasePool new];
	DSoUser* user = nil;
	tDirStatus status = eDSAuthFailed;
	NSMutableData* outputData = nil;
	
	NS_DURING
	user = [_node findUser:inUsername];
	if (user != nil) {
		status = [[user node] authenticateName:inUsername withPassword:inPassword authOnly:YES];
		if (status == eDSNoErr && inAuthOnly == NO) {
			if (_configNode == nil) {
				_configNode = [[[_node directory] findNodeViaEnum:eDSConfigNodeName] retain];
			}
			outputData = [NSMutableData dataWithLength:sizeof(AuthorizationExternalForm)];
			status = [_configNode customCall:eDSCustomCallConfigureGetAuthRef
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

- (tDirStatus) createKey:(NSString*)inKey withValues:(NSArray*)inValues
{
    return [self modify:ATTR_CREATE withKey:inKey withValues:inValues];
}

- (tDirStatus) deleteItem
{
    return eDSPermissionError;
}

- (tDirStatus) deleteKey:(NSString*)inKey withValues:(NSArray*)inValues
{
    return [self modify:ATTR_DELETE withKey:inKey withValues:inValues];
}

- (tDirStatus) appendKey:(NSString*)inKey withValues:(NSArray*)inValues
{
    return [self modify:ATTR_APPEND withKey:inKey withValues:inValues];
}

- (tDirStatus) mergeKey:(NSString*)inKey withValues:(NSArray*)inValues
{
    return [self modify:ATTR_MERGE withKey:inKey withValues:inValues];
}

- (tDirStatus) changeKey:(NSString*)inKey oldAndNewValues:(NSArray*)inValues
{
	return [self modify:ATTR_CHANGE withKey:inKey withValues:inValues];
}

- (tDirStatus) changeKey:(NSString*)inKey indexAndNewValue:(NSArray*)inValues
{
	return [self modify:ATTR_CHANGE_INDEX withKey:inKey withValues:inValues];
}

// ----------------------------------------------------------------------------
// Utility methods
#pragma mark ******** Utility methods ********

- (tDirStatus) modify:(tAttrCAM)inAction withKey:(NSString*)inKey withValues:(NSArray*)inValues
{
    tDirStatus		nError				= eDSPermissionError;
	tDirStatus		firstError			= eDSNoErr;
	unsigned int	index				= NSNotFound;
	NSEnumerator	*objEnum			= nil;
	NSString		*oldValue			= nil;
	NSString		*newValue			= nil;
	NSArray			*oldValues			= nil;
	NSMutableArray	*newValues			= nil;
	BOOL			changeSearchPath;

	if ([inKey isEqualToString:@kDS1AttrSearchPolicy]
		|| [[@kDSStdAttrTypePrefix stringByAppendingString:inKey] isEqualToString:@kDS1AttrSearchPolicy])
	{
		changeSearchPath = NO;
	} 
	else if ([inKey isEqualToString:@kDSNAttrCSPSearchPath]
		|| [[@kDSStdAttrTypePrefix stringByAppendingString:inKey] isEqualToString:@kDSNAttrCSPSearchPath])
	{
		changeSearchPath = YES;
	}
	else
	{
		return eDSPermissionError;
	}

    NS_DURING
        switch (inAction)
        {
            case ATTR_CREATE:
				if (!changeSearchPath && [inValues count] == 1) {
					nError = [self setSearchPolicy:[inValues lastObject]];
				} else if (changeSearchPath) {
					if ([inValues count] >= 1 
						&& [[_node getAttributeFirstValue:kDSNAttrCSPSearchPath] 
							isEqual:[inValues objectAtIndex:0]]) {
						objEnum = [inValues objectEnumerator];
						while (newValue = [objEnum nextObject]) {
							if (![self nodeNameIsValid:newValue]) {
								nError = eDSNodeNotFound;
								break;
							}
						}
						if (nError != eDSNodeNotFound) {
							nError = [self setCustomSearchPath:inValues];
						}
					}
				}
                break;
            case ATTR_APPEND:
            case ATTR_MERGE:
				if (changeSearchPath) {
					objEnum = [inValues objectEnumerator];
					newValues = [[[_node getAttribute:kDSNAttrCSPSearchPath] mutableCopy] autorelease];
					while (newValue = [objEnum nextObject]) {
						if (![newValues containsObject:newValue]) {
							if ([self nodeNameIsValid:newValue]) {
								[newValues addObject:newValue];
							} else {
								nError = eDSNodeNotFound;
								break;
							}
						}
					}
					if (nError != eDSNodeNotFound) {
						nError = [self setCustomSearchPath:newValues];
					}
				}
                break;
            case ATTR_DELETE:
				if (changeSearchPath && [inValues count] > 0) {
					oldValues = [_node getAttribute:kDSNAttrCSPSearchPath];
					newValues = [[oldValues mutableCopy] autorelease];
					[newValues removeObjectsInArray:inValues];
					if ([newValues count] >= 1 && [oldValues count] >= 1
						&& [[newValues objectAtIndex:0] isEqual:[oldValues objectAtIndex:0]]) {
						nError = [self setCustomSearchPath:newValues];
					}
				}
                break;
			case ATTR_CHANGE:
				if (!changeSearchPath) {
					oldValue = [_node getAttributeFirstValue:kDS1AttrSearchPolicy];
					if ([[inValues objectAtIndex:0] isEqual:oldValue]
						|| [[@kDSStdAttrTypePrefix stringByAppendingString:[inValues objectAtIndex:0]] isEqual:oldValue]) {
						nError = [self setSearchPolicy:[inValues objectAtIndex:1]];
					} else {
						nError = eDSAttributeValueNotFound;
					}
				} else {
					oldValues = [_node getAttribute:kDSNAttrCSPSearchPath];
					newValues = [[oldValues mutableCopy] autorelease];
					index = [oldValues indexOfObject:[inValues objectAtIndex:0]];
					newValue = [inValues objectAtIndex:1];
					if (index == 0) {
						nError = eDSPermissionError;
					} else if (index == NSNotFound) {
						nError = eDSAttributeValueNotFound;
					} else if ([self nodeNameIsValid:newValue]) {
						[newValues replaceObjectAtIndex:index
							withObject:newValue];
						nError = [self setCustomSearchPath:newValues];						
					} else {
						nError = eDSNodeNotFound;
					}
				}
				break;
			case ATTR_CHANGE_INDEX:
				index = [[inValues objectAtIndex:0] intValue];
				if (!changeSearchPath && index == 0) {
					nError = [self setSearchPolicy:[inValues objectAtIndex:1]];
				} else if (changeSearchPath && index != 0) {
					oldValues = [_node getAttribute:kDSNAttrCSPSearchPath];
					newValues = [[oldValues mutableCopy] autorelease];
					newValue = [inValues objectAtIndex:1];
					if (index >= [newValues count]) {
						nError = eDSIndexOutOfRange;
					} else if ([self nodeNameIsValid:newValue]) {
						[newValues replaceObjectAtIndex:index withObject:newValue];
						nError = [self setCustomSearchPath:newValues];
					} else {
						nError = eDSNodeNotFound;
					}
				}
				break;
        }
        NS_HANDLER
            if ([localException isKindOfClass:[DSoException class]])
            {
                nError = [(DSoException*)localException status];
				firstError = nError;
                //printf("standard createKey status was: %d\n", nError);
            }
            else
                [localException raise];
        NS_ENDHANDLER
		
	return nError;
}

- (tDirStatus)setSearchPolicy:(NSString*)newPolicy
{
	tDirStatus status = eDSPermissionError;
	int aCommand = 0;
	
	if (![newPolicy hasPrefix:@kDSStdAttrTypePrefix]) {
		newPolicy = [@kDSStdAttrTypePrefix stringByAppendingString:newPolicy];
	}
	
	if ([newPolicy isEqualToString:@kDSNAttrNSPSearchPath])
	{
		aCommand = eDSCustomCallSearchSetPolicyAutomatic;
	}
	else if ([newPolicy isEqualToString:@kDSNAttrLSPSearchPath])
	{
		aCommand = eDSCustomCallSearchSetPolicyLocalOnly;
	}
	else if ([newPolicy isEqualToString:@kDSNAttrCSPSearchPath])
	{
		aCommand = eDSCustomCallSearchSetPolicyCustom;
	}
	if (aCommand != 0)
	{
		status = [_node customCall:aCommand withAuthorization:&_authExternalForm];
	}
	return status;
}

- (tDirStatus)setCustomSearchPath:(NSArray*)nodeList
{
	tDirStatus status = eDSPermissionError;
	NSMutableArray* newNodeList = [[nodeList mutableCopy] autorelease];
	
	if ([newNodeList count] >= 1) {
		[newNodeList removeObjectAtIndex:0];
		status = [_node customCall:eDSCustomCallSearchSetCustomNodeList 
			sendPropertyList:newNodeList withAuthorization:&_authExternalForm];
	}
	
	return status;
}

- (BOOL)nodeNameIsValid:(NSString*)nodeName
{
	NSAutoreleasePool* pool = [NSAutoreleasePool new];
	BOOL result = NO;
	
	NS_DURING
	DSoNode* node = [[_node directory] findNode:nodeName];
	if (node != nil)
		result = YES;
	NS_HANDLER
	NS_ENDHANDLER
	
	[pool release];
	return result;
}

@end

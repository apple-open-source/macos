/*
 * Copyright (c) 2003-2009 Apple Inc. All rights reserved.
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
#import <opendirectory/odutils.h>
#import <DirectoryService/DirServicesConstPriv.h>

#import "PathNodeSearch.h"

@interface PathNodeSearch (Private)

- (DSoNodeConfig*)configNode;
- (void) _destroyRights;

@end


@implementation PathNodeSearch (Private)

- (DSoNodeConfig*)configNode
{
	if (_configNode == nil) {
		_configNode = [[[_node directory] findNodeViaEnum:eDSConfigNodeName] retain];
	}
	return _configNode;
}

- (void)_destroyRights
{
	// free _authExternalForm
	if (_haveRights) {
		[[self configNode] customCall:eODCustomCallConfigureDestroyAuthRef
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
	if ( [inPath isEqualTo: @"/Search"] == YES ) {
		_type = eDSSearchNodeName;
	}
    bzero(&_authExternalForm,sizeof(_authExternalForm));
    return self;
}

- initWithNode:(DSoNode*)inNode path:(NSString*)inPath
{
    [super initWithNode:inNode path:inPath];
	
	if ( [[inNode getName] isEqualTo: @"/Search"] == YES || [inPath isEqualTo: @"/Search"] == YES ) {
		_type = eDSSearchNodeName;
	}
    bzero(&_authExternalForm,sizeof(_authExternalForm));
    return self;
}

- initWithNode:(DSoNode*)inNode path:(NSString*)inPath type:(tDirPatternMatch)val
{
    [super initWithNode:inNode path:inPath];
	if ( [[inNode getName] isEqualTo: @"/Search"] == YES || [inPath isEqualTo: @"/Search"] ) {
		_type = eDSSearchNodeName;
	}
	else {
		_type = val;
	}
    bzero(&_authExternalForm,sizeof(_authExternalForm));
    return self;
}

- (void)dealloc
{
	[self _destroyRights];
	[_configNode release];
	_configNode = nil;
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
			outputData = [NSMutableData dataWithLength:sizeof(AuthorizationExternalForm)];
			status = [[self configNode] customCall:eODCustomCallConfigureGetAuthRef
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
    return eDSReadOnly;
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
    tDirStatus		nError				= eDSReadOnly;
	tDirStatus		firstError			= eDSNoErr;
	NSUInteger		index				= NSNotFound;
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
		return eDSReadOnly;
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
					
					nError = [self setCustomSearchPath:newValues];
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
						nError = eDSReadOnly;
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
	tDirStatus status = eDSReadOnly;
	int aCommand = 0;
	
	if (![newPolicy hasPrefix:@kDSStdAttrTypePrefix]) {
		newPolicy = [@kDSStdAttrTypePrefix stringByAppendingString:newPolicy];
	}
	
	if ([newPolicy isEqualToString:@kDSNAttrNSPSearchPath])
	{
		aCommand = eODCustomCallSearchSetPolicyAutomatic;
	}
	else if ([newPolicy isEqualToString:@kDSNAttrLSPSearchPath])
	{
		aCommand = eODCustomCallSearchSetPolicyLocalOnly;
	}
	else if ([newPolicy isEqualToString:@kDSNAttrCSPSearchPath])
	{
		aCommand = eODCustomCallSearchSetPolicyCustom;
	}
	if (aCommand != 0)
	{
		status = [_node customCall:aCommand withAuthorization:&_authExternalForm];
	}
	return status;
}

- (tDirStatus)setCustomSearchPath:(NSArray*)nodeList
{
	NSMutableArray* newNodeList = [[nodeList mutableCopy] autorelease];
	
	// operating system was added in 10.6 and that's all we are looking for
	id sysVersion = [[self configNode] getAttribute: kDS1AttrOperatingSystemVersion];
	if ( [sysVersion count] == 0 || _type == eDSSearchNodeName ) {
		for ( id path in [_node getAttribute:kDSNAttrLSPSearchPath] ) {
			[newNodeList removeObject: path];
		}
	}
	
	return [_node customCall: eODCustomCallSearchSetCustomNodeList 
			sendPropertyList: newNodeList
		   withAuthorization: &_authExternalForm];
}

- (BOOL)nodeNameIsValid:(NSString*)nodeName
{
	UInt32		ulCount		= 0;
	BOOL			result		= NO;
    tDirStatus	nError		= eDSReadOnly;
	DSoBuffer	   *bufNodeList	= nil;
	DSoDataList *dlPattern		= nil;
	
	NS_DURING
		//do we even care if this node is registered?
		//we certainly do not care if it is not reachable at this time since search node takes care of
		//node reachability ie. achieving and maintaining reachability
		bufNodeList	= [[DSoBuffer alloc] initWithDir:[_node directory] bufferSize:strlen([nodeName UTF8String]) + 128];
		dlPattern		= [[DSoDataList alloc] initWithDir:[_node directory] separator:'/' pattern:nodeName];
		do {
			nError = dsFindDirNodes([[_node directory] dsDirRef], [bufNodeList dsDataBuffer], [dlPattern dsDataList], eDSExact, &ulCount, NULL) ;
			if (nError == eDSBufferTooSmall) {
				[bufNodeList grow: [bufNodeList getBufferSize] *2];
			}
		} while (nError == eDSBufferTooSmall);
	
		[dlPattern release];
		[bufNodeList release];
		if ( ( nError == eDSNoErr ) && ( ulCount > 0 ) )
			result = YES;
	NS_HANDLER
	NS_ENDHANDLER
	
	return result;
}

@end

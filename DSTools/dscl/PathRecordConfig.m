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
 * @header PathRecordConfig
 */

#import "PathRecordConfig.h"
#import "DSoDirectory.h"
#import "DSoNode.h"
#import "DSoNodeConfig.h"
#import "DSoRecord.h"
#import "DSoUser.h"
#import "DSoException.h"

#import <DirectoryService/DirServicesConst.h>
#import <opendirectory/odutils.h>

@interface PathRecordConfig (Private)

- (void) _destroyRights;

@end


@implementation PathRecordConfig (Private)

- (void)_destroyRights
{
	// free _authExternalForm
	if (_haveRights) {
		[[_record node] customCall:eODCustomCallConfigureDestroyAuthRef
				 withAuthorization:&_authExternalForm];
		_haveRights = NO;
	}
}

@end

@implementation PathRecordConfig

// ----------------------------------------------------------------------------
// Initialization / teardown
#pragma mark ******** Initialization / teardown ********

- init
{
    [super init];
    bzero(&_authExternalForm,sizeof(_authExternalForm));
    return self;
}

- initWithRecord:(DSoRecord*)inRec
{
    [super initWithRecord:inRec];
    bzero(&_authExternalForm,sizeof(_authExternalForm));
    return self;
}

- (void)dealloc
{
	[self _destroyRights];
    [super dealloc];
}

- (void)setAuthExternalForm:(AuthorizationExternalForm*)externalForm {
	// don't set _haveRights here since a node above us owns the auth right
	memcpy(&_authExternalForm, externalForm, sizeof(AuthorizationExternalForm));
}

// ----------------------------------------------------------------------------
// PathItemProtocol implementations
#pragma mark ******** PathItemProtocol implementations ********

- (tDirStatus) authenticateName:(NSString*)inUsername withPassword:(NSString*)inPassword authOnly:(BOOL)inAuthOnly
{
	NSAutoreleasePool* pool = [NSAutoreleasePool new];
	DSoSearchNode* searchNode = [[[_record node] directory] searchNode];
	DSoUser* user = nil;
	tDirStatus status = eDSAuthFailed;
	NSMutableData* outputData = nil;
	
	NS_DURING
	user = [searchNode findUser:inUsername];
	if (user != nil) {
		status = [[user node] authenticateName:inUsername withPassword:inPassword authOnly:YES];
		if (status == eDSNoErr && inAuthOnly == NO) {
			outputData = [NSMutableData dataWithLength:sizeof(AuthorizationExternalForm)];
			status = [[_record node] customCall:eODCustomCallConfigureGetAuthRef
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
    tDirStatus nError = eDSNoErr;
    
    NS_DURING
	[_record removeRecord];
    NS_HANDLER
	if ([localException isKindOfClass:[DSoException class]])
		nError = [(DSoException*)localException status];
	else
		[localException raise];
    NS_ENDHANDLER
	
    return nError;
}

- (NSDictionary*)getDictionary:(NSArray*)inKeys
{
    NSAutoreleasePool      *pool	= [[NSAutoreleasePool alloc] init];
	NSArray				   *records	= nil;
    NSDictionary		   *attribs = nil;
    
    NS_DURING
    
	if ([[NSString stringWithUTF8String:[_record getType]] hasPrefix:@kDSStdRecordTypePrefix])
		records = [[_record node] findRecordNames:@kDSRecordsAll
									andAttributes:[NSArray arrayWithObject:@kDSAttributesAll]
											ofType:[_record getType]
										 matchType:eDSExact];
	else
		records = [[_record node] findRecordNames:@kDSRecordsAll
									andAttributes:[NSArray arrayWithObject:@kDSAttributesAll]
											ofType:[_record getType]
										 matchType:eDSExact];
	for (NSDictionary *recordDict in records) {
		NSArray *values = [recordDict objectForKey:@kDSNAttrRecordName];
		if ([values count] > 0 && [[values objectAtIndex:0] isEqual: [_record getName]]) {
			attribs = recordDict;
			break;
		}
	}
    if ([inKeys count] > 0)
    {
        //NSArray* niceKeys    = prefixedAttributeKeysWithNode([_record node], inKeys);
		NSMutableDictionary* attribsFiltered = [NSMutableDictionary dictionary];
		for (NSString *key in inKeys) {
			NSString *niceKey = nil;
			if ([key hasPrefix:@kDSStdAttrTypePrefix] || [key hasPrefix:@kDSNativeAttrTypePrefix])
			{
				niceKey = key;
			}
			else
			{
				niceKey = [@kDSStdAttrTypePrefix stringByAppendingString:key];
				if ([attribs objectForKey: niceKey] == nil && ![[[[_record node] directory] standardAttributeTypes] containsObject:niceKey])
					niceKey = [@kDSNativeAttrTypePrefix stringByAppendingString:key];
			}
			id value = [attribs objectForKey:niceKey];
			if (value != nil) {
				[attribsFiltered setObject:value forKey:niceKey];
			}
		}
        attribs = [[attribsFiltered copy] autorelease];
    }
    
    NS_HANDLER
    [localException retain];
    [pool release];
    [[localException autorelease] raise];
    NS_ENDHANDLER
    
    [attribs retain];
    [pool release];
	
    return [attribs autorelease];
}

// ----------------------------------------------------------------------------
// Utility methods
#pragma mark ******** Utility methods ********

- (tDirStatus) modify:(tAttrCAM)inAction withKey:(NSString*)inKey withValues:(NSArray*)inValues
{
    tDirStatus		nError				= eDSReadOnly;
	tDirStatus		firstError			= eDSNoErr;
	NSUInteger		index				= NSNotFound;
	NSDictionary	*valuesDict			= nil;
	NSArray			*oldValues			= nil;
	
	if (![[NSString stringWithUTF8String:[_record getType]] hasSuffix:@":Plugins"]) {
		return eDSReadOnly;
	}
	
	if (![inKey isEqualToString:@kDS1AttrFunctionalState]
		&& ![[@kDSStdAttrTypePrefix stringByAppendingString:inKey] isEqualToString:@kDS1AttrFunctionalState])
	{
		return eDSReadOnly;
	}
	
    NS_DURING
	switch (inAction)
	{
		case ATTR_CREATE:
			if ([inValues count] == 1) {
				nError = [self setPluginEnabled:[inValues lastObject]];
			}
			break;
		case ATTR_APPEND:
		case ATTR_MERGE:
		case ATTR_DELETE:
			// these are not supported
			break;
		case ATTR_CHANGE:
			valuesDict = [self getDictionary: [NSArray arrayWithObject:@kDS1AttrFunctionalState]];
			oldValues = (NSArray*)[valuesDict objectForKey:@kDS1AttrFunctionalState];
			if ([oldValues count] == 1 && [[inValues objectAtIndex:0] isEqual:[oldValues objectAtIndex:0]]) {
				nError = [self setPluginEnabled:[inValues objectAtIndex:1]];
			} else {
				nError = eDSAttributeValueNotFound;
			}
			break;
		case ATTR_CHANGE_INDEX:
			index = [[inValues objectAtIndex:0] intValue];
			if (index == 0) {
				nError = [self setPluginEnabled:[inValues objectAtIndex:1]];
			}
			break;
	}
	NS_HANDLER
	if ([localException isKindOfClass:[DSoException class]])
	{
		nError = [(DSoException*)localException status];
		firstError = nError;
	}
	else
		[localException raise];
	NS_ENDHANDLER
	
	return nError;
}

- (tDirStatus)setPluginEnabled:(NSString*)newState
{
	BOOL newStateBool;
	
	if ([newState isEqualToString:@"Active"])
	{
		newStateBool = YES;
	}
	else if ([newState isEqualToString:@"Inactive"]) {
		newStateBool = NO;
	}
	else
	{
		return eDSReadOnly;
	}
	
	[(DSoNodeConfig*)[_record node] setPlugin:[_record getName] enabled:newStateBool withAuthorization:&_authExternalForm];
	
	return eDSNoErr;
}

@end

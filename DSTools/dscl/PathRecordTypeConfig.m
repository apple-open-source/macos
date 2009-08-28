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
 * @header PathRecordTypeConfig
 */


#import "PathRecordTypeConfig.h"
#import "PathRecordConfig.h"
#import "DSoException.h"
#import "DSoDirectory.h"
#import "DSoNode.h"
#import "DSoRecord.h"
#import "DSoRecordPriv.h"

#import <DirectoryServiceCore/CSharedData.h>

extern BOOL gHACK;

@interface PathRecordTypeConfig (Private)

- (void) _destroyRights;

@end


@implementation PathRecordTypeConfig (Private)

- (void)_destroyRights
{
	// free _authExternalForm
	if (_haveRights) {
		[_node customCall:eDSCustomCallConfigureDestroyAuthRef
				 withAuthorization:&_authExternalForm];
	}
}

@end

@implementation PathRecordTypeConfig

// ----------------------------------------------------------------------------
// Initialization / teardown
#pragma mark ******** Initialization / teardown ********

- init
{
    [super init];
    bzero(&_authExternalForm,sizeof(_authExternalForm));
    return self;
}

- initWithNode:(DSoNode*)inNode recordType:(NSString*)inType
{
    [super initWithNode:inNode recordType:inType];
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

- (NSArray*) getList:(NSString*)inKey
{
    NSArray *list = nil;

    NS_DURING
        if ([_recordType hasPrefix:@kDSStdRecordTypePrefix])
            list = [[_node findRecordNames:@kDSRecordsAll
                            ofType:[_recordType UTF8String]
                            matchType:eDSExact]
                        sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
        else
            list = [[_node findRecordNames:@"dsConfigType::GetAllRecords"
                            ofType:[_recordType UTF8String]
                            matchType:eDSExact]
                        sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
	NS_HANDLER
		if (!DS_EXCEPTION_STATUS_IS(eDSRecordNotFound) &&
	        !DS_EXCEPTION_STATUS_IS(eDSInvalidRecordType))
		{
			[localException raise];
		}
	NS_ENDHANDLER

	return list;
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

- (tDirStatus) createKey:(NSString*)inKey withValues:(NSArray*)inValues
{
	if (inValues == nil && [self cd:inKey] != nil)
	{
		return eDSNoErr;
	}
	else
	{
		return eDSPermissionError;
	}
}

- (PathItem*) cd:(NSString*)dest
{
    DSoRecord			*rec = nil;
    PathRecordConfig	*p   = nil;
    
    if (dest != nil && [dest length] > 0)
    {
		NS_DURING
		rec = [_node findRecord:dest ofType:[_recordType UTF8String]];
		NS_HANDLER
		if (gHACK && DS_EXCEPTION_STATUS_IS(eDSRecordNotFound)) // Hack to force read-only entry into a record which doesn't implement dsOpenRecord()
			rec = [[[DSoRecord alloc] initInNode:_node
											type:[_recordType UTF8String] name:dest create:NO] autorelease];
		else
			[localException raise];
		NS_ENDHANDLER
		
        if (rec != nil)
            p = [[PathRecordConfig alloc] initWithRecord:rec];
		
		[p setAuthExternalForm:&_authExternalForm];

        return [p autorelease];
    }
    else
        return nil;
}

- (tDirStatus) read:(NSString*)inPath keys:(NSArray*)inKeys
{
    NSAutoreleasePool      *pool	= [[NSAutoreleasePool alloc] init];
    tDirStatus status = eDSRecordNotFound;
    NSArray* foundRecords = nil;
	NSMutableDictionary* foundRecord = nil;
    NSUInteger recordIndex = 0;
    NSUInteger recordCount = 0;
	
    NS_DURING
	if ([_recordType hasPrefix:@kDSStdRecordTypePrefix])
		foundRecords = [_node findRecordNames:@kDSRecordsAll andAttributes:[NSArray arrayWithObject:@kDSAttributesAll]
									   ofType:[_recordType UTF8String] matchType:eDSExact];
	else
		foundRecords = [_node findRecordNames:@"dsConfigType::GetAllRecords" andAttributes:[NSArray arrayWithObject:@kDSAttributesAll] 
									   ofType:[_recordType UTF8String] matchType:eDSExact];
    NS_HANDLER
	[localException retain];
	[pool release];
	[[localException autorelease] raise];
    NS_ENDHANDLER
    
    recordCount = [foundRecords count];
    for (recordIndex = 0; recordIndex < recordCount; recordIndex++)
    {
        NSDictionary *record = [foundRecords objectAtIndex:recordIndex];
		NSArray *values = [record objectForKey:@kDSNAttrRecordName];
		if ([values count] > 0 && [values containsObject:inPath])
		{
			foundRecord = [[record mutableCopy] autorelease];
			status = eDSNoErr;
			break;
		}
    }
	if (foundRecord != nil)
	{
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
				else if ([key isEqualToString:@kDSAttributesStandardAll])
				{
					for (NSString *aKey in [foundRecord allKeys])
					{
						if ([aKey hasPrefix:@kDSStdAttrTypePrefix]) {
							[attribsFiltered setObject:[foundRecord objectForKey:aKey] forKey:aKey];
						}
					}
					continue;
				}
				else if ([key isEqualToString:@kDSAttributesNativeAll])
				{
					for (NSString *aKey in [foundRecord allKeys])
					{
						if ([aKey hasPrefix:@kDSNativeAttrTypePrefix]) {
							[attribsFiltered setObject:[foundRecord objectForKey:aKey] forKey:aKey];
						}
					}
					continue;
				}
				else
				{
					niceKey = [@kDSStdAttrTypePrefix stringByAppendingString:key];
					if ([foundRecord objectForKey: niceKey] == nil && ![[[_node directory] standardAttributeTypes] containsObject:niceKey])
						niceKey = [@kDSNativeAttrTypePrefix stringByAppendingString:key];
				}
				id value = [foundRecord objectForKey:niceKey];
				if (value != nil) {
					[attribsFiltered setObject:value forKey:niceKey];
				}
			}
			foundRecord = [[attribsFiltered copy] autorelease];
		}
		[self printDictionary:foundRecord withRequestedKeys:inKeys];
	}
    
    [pool release];
    return status;
}

@end

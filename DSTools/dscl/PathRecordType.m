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
 * @header PathRecordType
 */


#import "PathRecordType.h"
#import "DSoDirectory.h"
#import "DSoNode.h"
#import "PathRecord.h"
#import <DirectoryService/DirServicesConst.h>
#import "DSoException.h"
#import "DSoRecord.h"
#import "DSoRecordPriv.h"

extern BOOL gHACK;

@interface PathRecordType (PathRecordTypePrivate)
- (void)printSearch:(NSString*)inKey Results:(NSArray*)inResults;
@end

NSInteger compareRecordDicts(id leftDict, id rightDict, void * context)
{
    return [[[(NSDictionary*)leftDict objectForKey:@kDSNAttrRecordName] 
        objectAtIndex:0]
        caseInsensitiveCompare: [[(NSDictionary*)rightDict 
            objectForKey:@kDSNAttrRecordName] objectAtIndex:0]];
}


@implementation PathRecordType

// ----------------------------------------------------------------------------
// Initialization / teardown
#pragma mark ******** Initialization / teardown ********

- init
{
    [super init];
    _node = nil;
    _recordType = nil;
    return self;
}

- initWithNode:(DSoNode*)inNode recordType:(NSString*)inType
{
    [self init];
    _node = [inNode retain];
    _recordType = [inType retain];
    return self;
}

- (void)dealloc
{
    [_recordType release];
    [_node release];
    [super dealloc];
}

// ----------------------------------------------------------------------------
// PathItemProtocol implementations
#pragma mark ******** PathItemProtocol implementations ********

- (tDirStatus) authenticateName:(NSString*)inUsername withPassword:(NSString*)inPassword authOnly:(BOOL)inAuthOnly
{
    return [_node authenticateName:inUsername withPassword:inPassword authOnly:inAuthOnly];
}

- (NSString*) name
{
    return [self stripDSPrefixOffValue:_recordType];
}

- (NSArray*) getList:(NSString*)inKey
{
    NSArray *list = nil;

    NS_DURING
        if (inKey == nil)
        {
            list = [[_node findRecordNames:@kDSRecordsAll
                            ofType:[_recordType UTF8String]
                            matchType:eDSExact]
                        sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
        }
        else
        {
            list = [[_node findRecordNames:@kDSRecordsAll
                            andAttributes:[NSArray arrayWithObject:inKey]
                            ofType:[_recordType UTF8String] 
                            matchType:eDSExact]
                        sortedArrayUsingFunction:compareRecordDicts context:nil];
        }
	NS_HANDLER
		if (!DS_EXCEPTION_STATUS_IS(eDSRecordNotFound) &&
	        !DS_EXCEPTION_STATUS_IS(eDSInvalidRecordType))
		{
			[localException raise];
		}
	NS_ENDHANDLER

	return list;
}

- (NSArray*) getList
{
	return [self getList:nil];
}

- (NSArray*) getListWithKeys:(NSArray*)inKeys
{
    NSArray            *list        = nil;    
    NSArray            *niceKeys    = nil;

    NS_DURING
        if ( [inKeys count] == 0  )
        {
            list = [[_node  findRecordNames:@kDSRecordsAll
                            andAttributes:[NSArray arrayWithObject:@kDSAttributesAll]
                            ofType:[_recordType UTF8String]
                            matchType:eDSExact]
                            sortedArrayUsingFunction:compareRecordDicts context:nil];
        }
        else
        {
            niceKeys = prefixedAttributeKeysWithNode(_node, inKeys);
            list = [[_node findRecordNames:@kDSRecordsAll
                            andAttributes:niceKeys
                            ofType:[_recordType UTF8String] 
                            matchType:eDSiExact]
                        sortedArrayUsingFunction:compareRecordDicts context:nil];
        }
	NS_HANDLER
		if (!DS_EXCEPTION_STATUS_IS(eDSRecordNotFound) &&
	        !DS_EXCEPTION_STATUS_IS(eDSInvalidRecordType))
		{
			[localException raise];
		}
	NS_ENDHANDLER

	return list;
}

- (tDirStatus) list:(NSString*)inPath key:(NSString*)inKey
{
    NSArray		   *list;
    NSString	   *key		= inKey;
    unsigned long   i		= 0;
	unsigned long   count   = 0;

    if (inKey != nil 
        && ![key hasPrefix:@kDSStdAttrTypePrefix] 
        && ![key hasPrefix:@kDSNativeAttrTypePrefix])
    {
        key = [@kDSStdAttrTypePrefix stringByAppendingString:inKey];
        if (![[[_node directory] standardAttributeTypes] containsObject:key])
            key = [@kDSNativeAttrTypePrefix stringByAppendingString:inKey];
    }
    list = [self getList:key];
    if (list != nil && [list count] > 0)
    {
        count = [list count];
        if (inKey == nil)
        {
            for (i = 0; i < count; i++)
            {
                printf("%s\n",[[list objectAtIndex:i] UTF8String]);
            }
        }
        else
        {
            int maxLength = 0;
            int currentLength = 0;
            
            for (i = 0; i < count; i++)
            {
                NSDictionary* record = (NSDictionary*)[list objectAtIndex:i];
                NSArray* recordNames = [record objectForKey:@kDSNAttrRecordName];
                NSArray* keyValues = [record objectForKey:key];
                
                if ([keyValues count] == 0)
                    continue;
                
                if ([recordNames count] > 0)
                {
                    currentLength = strlen( [[recordNames objectAtIndex:0] UTF8String]);
                    if (currentLength > maxLength)
                    {
                        maxLength = currentLength;
                    }
                }
            }

            for (i = 0; i < count; i++)
            {
                NSDictionary* record = (NSDictionary*)[list objectAtIndex:i];
                NSArray* recordNames = [record objectForKey:@kDSNAttrRecordName];
                NSArray* keyValues = [record objectForKey:key];
                NSString* value = nil;
                NSEnumerator* valueEnum = [keyValues objectEnumerator];
                
                if ([keyValues count] == 0)
                    continue;
                
                if ([recordNames count] > 0)
                {
                    printf("%s",[[recordNames objectAtIndex:0] UTF8String]);
                }
                currentLength = maxLength 
                    - strlen([[recordNames objectAtIndex:0] UTF8String]) + 2;
                while (currentLength > 0)
                {
                    printf(" ");
                    currentLength--;
                }
                while ((value = (NSString*)[valueEnum nextObject]) != nil)
                {
                    printValue(value, NO);
                }
                printf("\n");
            }
        }
	}

    return eDSNoErr;
}

- (NSArray*) getPossibleCompletionsFor:(NSString*)inPrefix
{
	NSMutableArray* possibleCompletions = [NSMutableArray array];
	NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSArray *searchResults = nil;
	NSArray *recordTypes = [NSArray arrayWithObject:_recordType];
	NSString *key = [NSString stringWithUTF8String:kDSNAttrRecordName];
	NSMutableArray *attribList = [NSMutableArray arrayWithObjects:
		key, nil];
	tDirPatternMatch type = eDSiStartsWith;

	NS_DURING
		searchResults = [_node findRecordsOfTypes:recordTypes withAttribute:[key UTF8String]
								value:inPrefix matchType:type retrieveAttributes:attribList];
	if (searchResults != nil)
	{
		NSEnumerator* listEnum = [searchResults objectEnumerator];
		NSDictionary* currentItem = nil;
		
		while (currentItem = (NSDictionary*)[listEnum nextObject])
		{
			id recordName = [currentItem objectForKey:key];
			if ([recordName isKindOfClass:[NSArray class]]
				&& [recordName count] > 0) {
				recordName = [recordName objectAtIndex:0];
			}
			if ([recordName isKindOfClass:[NSString class]]
				&& [[recordName lowercaseString] hasPrefix:inPrefix]
				&& ![possibleCompletions containsObject:recordName])
				[possibleCompletions addObject:recordName];
		}
	}
	NS_HANDLER
		if (!DS_EXCEPTION_STATUS_IS(eNotYetImplemented) &&
	        !DS_EXCEPTION_STATUS_IS(eNotHandledByThisNode))
		{
			[localException retain];
			[pool release];
			[[localException autorelease] raise];
		} 
		else 
		{
			possibleCompletions = nil;
		}
	NS_ENDHANDLER
	
	[pool release];
	if (possibleCompletions == nil)
		possibleCompletions = (NSMutableArray*)[super getPossibleCompletionsFor:inPrefix];

	return possibleCompletions;
}

- (PathItem*) cd:(NSString*)dest
{
    DSoRecord      *rec = nil;
    PathRecord     *p   = nil;
    
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
            p = [[PathRecord alloc] initWithRecord:rec];

        return [p autorelease];
    }
    else
        return nil;
}

- (tDirStatus) createKey:(NSString*)inKey withValues:(NSArray*)inValues
{
    tDirStatus status = eDSNoErr;
	
    NS_DURING
        [_node newRecord:inKey ofType:[_recordType UTF8String]];
    NS_HANDLER
        if ([localException isKindOfClass:[DSoException class]])
            status = [(DSoException*)localException status];
        else
            [localException raise];
    NS_ENDHANDLER
            
    return status;
}

- (tDirStatus) deleteItem
{
    NSArray				   *list;
    DSoRecord			   *rec;
    unsigned long			i		= 0;
	unsigned long			count   = 0;
    const char			   *recType = NULL;
    tDirStatus				status  = eDSNoErr;
    NSAutoreleasePool      *pool	= [[NSAutoreleasePool alloc] init];

    recType = [_recordType UTF8String];
    
    NS_DURING
        list = [_node findRecordNames:@kDSRecordsAll
                                ofType:recType
                             matchType:eDSAnyMatch];
        count = [list count];

        for (i = 0; i < count; i++)
        {
            rec = [_node findRecord:[list objectAtIndex:i] ofType:recType];
            [rec removeRecord];
        }
    NS_HANDLER
        if ([localException isKindOfClass:[DSoException class]])
        {
            status = [(DSoException*)localException status];
        }
        else
        {
            [localException retain];
            [pool release];
            [[localException autorelease] raise];
        }
    NS_ENDHANDLER

    [pool release];
    return status;
}

- (NSString*)nodeName
{
    return [_node getName];
}

- (tDirStatus) read:(NSArray*)inKeys
{
    printf("name: %s\n", [_recordType UTF8String]);
    return eDSNoErr;
}

- (tDirStatus) read:(NSString*)inPath keys:(NSArray*)inKeys
{
    NSAutoreleasePool      *pool	= [[NSAutoreleasePool alloc] init];
    tDirStatus status = eDSRecordNotFound;
    NSArray* niceKeys = [inKeys count] > 0 ? prefixedAttributeKeysWithNode(_node, inKeys) : [NSArray arrayWithObject:@kDSAttributesAll];
    NSArray* foundRecords = nil;
    NSUInteger recordIndex = 0;
    NSUInteger recordCount = 0;

    NS_DURING
        foundRecords = [_node findRecordNames:inPath andAttributes:niceKeys 
            ofType:[_recordType UTF8String] matchType:eDSExact];
    NS_HANDLER
        [localException retain];
        [pool release];
        [[localException autorelease] raise];
    NS_ENDHANDLER
    
    recordCount = [foundRecords count];
    for (recordIndex = 0; recordIndex < recordCount; recordIndex++)
    {
        NSMutableDictionary* foundRecord = [[[foundRecords objectAtIndex:recordIndex] mutableCopy] autorelease];
        if ([inKeys count] > 0 && ![niceKeys containsObject:@kDSNAttrRecordName])
        {
            [foundRecord removeObjectForKey:@kDSNAttrRecordName];
        }
        [self printDictionary:foundRecord withRequestedKeys:inKeys];
        status = eDSNoErr;
    }
    
    [pool release];
    return status;
}

- (tDirStatus) searchForKey:(NSString*)inKey withValue:(NSString*)inValue matchType:(NSString*)inType
{
	NSAutoreleasePool  *pool			= [[NSAutoreleasePool alloc] init];
	NSArray			   *searchResults   = nil;
	NSArray			   *recordTypes		= [NSArray arrayWithObject:_recordType];
	NSMutableArray     *attribList		= [NSMutableArray arrayWithObject:@kDSNAttrRecordName];
	NSString		   *key				= nil;
	tDirPatternMatch	type			= eDSExact;

	NS_DURING
		if ([inKey hasPrefix:@kDSStdAttrTypePrefix] || [inKey hasPrefix:@kDSNativeAttrTypePrefix])
		{
			key = inKey;
		}
		else
		{
			key = [@kDSStdAttrTypePrefix stringByAppendingString:inKey];
			if (![[[_node directory] standardAttributeTypes] containsObject:key])
				key = [@kDSNativeAttrTypePrefix stringByAppendingString:inKey];
		}
        [attribList addObject:key];
        searchResults = [_node findRecordsOfTypes:recordTypes withAttribute:[key UTF8String]
                            value:inValue matchType:type retrieveAttributes:attribList];
        [self printSearch:key Results:searchResults];
		
		NS_HANDLER
			[localException retain];
			[pool release];
			[[localException autorelease] raise];
		NS_ENDHANDLER

		[pool release];
		
		return eDSNoErr;
}

-(DSoNode*) node
{
	// ATM - needed for PlugInManager
	return _node;
}

-(NSString*) recordType
{
	// ATM - needed for PlugInManager
	return _recordType;
}

@end

// ----------------------------------------------------------------------------
// Private methods
#pragma mark ******** Private methods ********

@implementation PathRecordType (PathRecordTypePrivate)
- (void)printSearch:(NSString*)inKey Results:(NSArray*)inResults
{
	NSEnumerator	   *resultEnumerator	= [inResults objectEnumerator];
	id					d					= nil;
	
	while(d = [resultEnumerator nextObject])
	{
		printf("%s\t\t%s = %s\n", [[[d objectForKey:@kDSNAttrRecordName] objectAtIndex:0] UTF8String],
		 [[self stripDSPrefixOffValue:inKey] UTF8String], [[[d objectForKey:inKey] description] UTF8String]);
	}
}

@end

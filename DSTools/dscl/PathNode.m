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
 * @header PathNode
 */


#import "PathNode.h"
#import "PathRecordType.h"
#import "DSoDirectory.h"
#import "DSoNode.h"
#import "DSoException.h"
#import <DirectoryService/DirServicesConst.h>

static NSString *kNSStdRecordTypePrefix		= @"dsRecTypeStandard:";
static NSString *kNSNativeRecordTypePrefix  = @"dsRecTypeNative:";
static NSString *kNSStdAttrTypePrefix		= @"dsAttrTypeStandard:";
static NSString *kNSNativeAttrTypePrefix	= @"dsAttrTypeNative:";

@interface PathNode (PathNodePrivate)
// Print the search results from the searchForKey:withValue:matchType: routine.
- (void)printSearch:(NSString*)inKey Results:(NSArray*)inResults;
@end


@implementation PathNode

// ----------------------------------------------------------------------------
// Initialization / teardown
#pragma mark ******** Initialization / teardown ********


- init
{
    [super init];
    _pathName = nil;
    _dir = nil;
    _node = nil;
    _enableSubNodes = YES;
    return self;
}

- initWithDir:(DSoDirectory*)inDir path:(NSString*)inPath
{
    [self init];
    _pathName = inPath;
    [_pathName retain];
    _dir = inDir;
    return self;
}

- initWithNode:(DSoNode*)inNode path:(NSString*)inPath
{
    [self init];
    _pathName = inPath;
    [_pathName retain];
    _node = [inNode retain];
    _dir = [_node directory];
    return self;
}

- (void)dealloc
{
    [_pathName release];
    [_node release];
    [super dealloc];
}

// ----------------------------------------------------------------------------
// PathItemProtocol implementations
#pragma mark ******** PathItemProtocol implementations ********

- (NSString*)name
{
    NSAutoreleasePool      *pool;
    NSString			   *name	= nil;
    
    if ([_pathName isEqualToString:@"/NetInfo/root"])
        return @"NetInfo/root";
    else
    {
        pool = [[NSAutoreleasePool alloc] init];
        name = [[[_pathName componentsSeparatedByString:@"/"] lastObject] retain];
        [pool release];
        return [name autorelease];
    }
}

- (NSArray*)getList
{
	NSArray    *recordList  = [self getRecordList];
	NSArray    *subnodeList = nil;
	
	if (_enableSubNodes)
		subnodeList = [self getSubnodeList];

	if (recordList != nil && !gRawMode && [recordList count] > 0)
	{
		NSMutableArray *newList = [NSMutableArray arrayWithCapacity:[recordList count]];
		unsigned int	i			= 0;
		unsigned int	cntLimit	= [recordList count];
		for (i = 0; i < cntLimit; i++)
			[newList addObject:[self stripDSPrefixOffValue:[recordList objectAtIndex:i]]];
		recordList = newList;
	}
	if (recordList != nil && subnodeList != nil
		&& [recordList count] > 0 && [subnodeList count] > 0)
		return [subnodeList arrayByAddingObjectsFromArray:recordList];
	else if (recordList != nil && [recordList count] > 0)
		return recordList;
	else if (subnodeList != nil && [subnodeList count] > 0)
		return subnodeList;
	else
		return nil;
}

- (tDirStatus)list:(NSString*)inPath key:(NSString*)inKey
{
    NSAutoreleasePool      *pool		= [[NSAutoreleasePool alloc] init];
    NSArray				   *recordList  = [self getRecordList];
    NSString			   *recType		= nil;
    unsigned long			i			= 0;
	unsigned long			sCount		= 0;
	unsigned long			rCount		= 0;
    
    [super list:inPath key:(NSString*)inKey];
    
    if (_enableSubNodes)
    {
        NSArray		*subnodeList = [self getSubnodeList];
        sCount = [subnodeList count];
        for (i = 0; i < sCount; i++)
        {
            printf("%s\n", [[subnodeList objectAtIndex:i] UTF8String]);
        }
    }
    
    rCount = [recordList count];
    if (sCount && rCount)
        printf ("\n");

    for (i = 0; i < rCount ; i++)
    {
        recType = [self stripDSPrefixOffValue:[recordList objectAtIndex:i]];
        printf("%s\n", [recType UTF8String]);
    }
        
    [pool release];
	
    return eDSNoErr;
}

- (PathItem*) cd:(NSString*)dest
{
    PathItem *nextItem = nil;

    // The following checks are in order of fastest check.
    
    // If the dest empty, abort.
    if (dest == nil || [dest length] == 0)
    {
        return nil;
    }
	
    // If the destination hase a fully qualified record type, then use it;
    // else look for existing standard, then native types.
    else if ([dest hasPrefix:kNSStdRecordTypePrefix] || [dest hasPrefix:kNSNativeRecordTypePrefix])
    {
        nextItem = [[PathRecordType alloc] initWithNode:_node recordType:dest];
    }
	
    // try using it as the name of a child of this node, but only if
    // configured to do so.
    else if (_enableSubNodes)
    {
        NSString *fullPathName = [NSString stringWithFormat:@"%@/%@",_pathName,dest];
        DSoNode *n;
        NS_DURING
            n = [_dir findNode:fullPathName matchType:eDSExact useFirst:NO]; // more efficient for non-existant names
            nextItem = [[PathNode alloc] initWithNode:n path:fullPathName];
        NS_HANDLER
            if (!DS_EXCEPTION_STATUS_IS(eDSUnknownNodeName) &&
                !DS_EXCEPTION_STATUS_IS(eDSNodeNotFound))
            {	// Some unexpected exception
                [localException raise];
            }
        NS_ENDHANDLER
    }
        
    // Try looking for a standard or native type by the name of the destination.
    if (nextItem == nil) // I would have used else here, but the Exception handlers were causing problems.
    {
        NSString *stdDest = [kNSStdRecordTypePrefix stringByAppendingString:dest];
        NSString *nativeDest = [kNSNativeRecordTypePrefix stringByAppendingString:dest];
        
        NS_DURING
            if ([_node hasRecordsOfType:[stdDest UTF8String]])
                nextItem = [[PathRecordType alloc] initWithNode:_node recordType:stdDest];
            else if ([_node hasRecordsOfType:[nativeDest UTF8String]])
                nextItem = [[PathRecordType alloc] initWithNode:_node recordType:nativeDest];
        NS_HANDLER
        NS_ENDHANDLER
    }
    
    // if all else fails try to open the name as is
    if (nextItem == nil && _enableSubNodes)
    {
        NSString *fullPathName = [NSString stringWithFormat:@"%@/%@",_pathName,dest];
        DSoNode *n;
        
        NS_DURING
            n = [_dir findNode:fullPathName matchType:eDSExact useFirst:YES]; // just open the name as provided
            nextItem = [[PathNode alloc] initWithNode:n path:fullPathName];
        NS_HANDLER
            if (!DS_EXCEPTION_STATUS_IS(eDSUnknownNodeName) &&
                !DS_EXCEPTION_STATUS_IS(eDSNodeNotFound))
            {	// Some unexpected exception
                [localException raise];
            }
        NS_ENDHANDLER
    }
        
    return [nextItem autorelease];
}

- (NSDictionary *) getDictionary:(NSArray *)inKeys
{
    NSAutoreleasePool      *pool	= [[NSAutoreleasePool alloc] init];
    NSDictionary		   *attribs = nil;
    id						key     = nil;
    NSString               *attrib  = nil;
    unsigned long			i		= 0;
    
    NS_DURING
        
        if (inKeys == nil || [inKeys count] == 0)
        {
            attribs = [_node getAllAttributes];
        }
        else
        {
            NSMutableDictionary* mutableAttribs = [NSMutableDictionary dictionary];
            unsigned long   cntLimit	= 0;
            
            attribs = [_node getAllAttributes];
            
            cntLimit = [inKeys count];
            for (i = 0; i < cntLimit; i++)
            {
                key = [inKeys objectAtIndex:i];
                if ([key hasPrefix:@kDSStdAttrTypePrefix] || [key hasPrefix:@kDSNativeAttrTypePrefix])
                {
                    attrib = key;
                    if([attribs objectForKey:attrib] != nil)
                        [mutableAttribs setObject:[attribs objectForKey:attrib] forKey:attrib];
                }
                else
                {
                    attrib = [@kDSStdAttrTypePrefix stringByAppendingString:key];
                    if([attribs objectForKey:attrib] != nil)
                        [mutableAttribs setObject:[attribs objectForKey:attrib] forKey:attrib];
                    
                    attrib = [@kDSNativeAttrTypePrefix stringByAppendingString:key];
                    if([attribs objectForKey:attrib] != nil)
                        [mutableAttribs setObject:[attribs objectForKey:attrib] forKey:attrib];
                }
            }
            attribs = (NSDictionary *)mutableAttribs;
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

- (tDirStatus) searchForKey:(NSString*)inKey withValue:(NSString*)inValue matchType:(NSString*)inType
{
	NSAutoreleasePool      *pool			= [[NSAutoreleasePool alloc] init];
	NSArray				   *searchResults   = nil;
	NSArray				   *recordTypes		= [_node findRecordTypes];
	NSMutableArray		   *attribList		= [NSMutableArray arrayWithObjects:
												@kDSNAttrRecordName,
												kDSOAttrRecordType, nil];
	NSString			   *key				= nil;
	tDirPatternMatch		type			= eDSExact;

	NS_DURING
		if ([inKey hasPrefix:kNSStdAttrTypePrefix] || [inKey hasPrefix:kNSNativeAttrTypePrefix])
		{
			key = inKey;
		}
		else
		{
			key = [kNSStdAttrTypePrefix stringByAppendingString:inKey];
			if (![[[_node directory] standardAttributeTypes] containsObject:key])
				key = [kNSNativeAttrTypePrefix stringByAppendingString:inKey];
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

// ----------------------------------------------------------------------------
// Utility functions
#pragma mark ******** Utility functions ********

- (PathItem*)cdNode:(NSString*)dest
{
    DSoNode    *n   = [_dir findNode:dest];
    PathNode   *p   = nil;
    
    if (n == nil)
    {
        //We just have a node prefix.
        p = [[PathNode alloc] initWithDir:_dir path:dest];
    }
    else
    {
        p = [[PathNode alloc] initWithNode:n path:dest];
    }
    
    return [p autorelease];
}

- (PathItem*)cdRecordType:(NSString*)destType
{
    PathRecordType *p = [[PathRecordType alloc] initWithNode:_node recordType:destType];
	
    return [p autorelease];
}


- (NSArray*)getSubnodeList
{
    NSAutoreleasePool      *pool					= [[NSAutoreleasePool alloc] init];
    NSArray				   *findResults				= nil;
    NSArray				   *findResults2			= nil;
	NSArray				   *nameComponents			= nil;
    NSArray				   *list					= nil;
	NSMutableSet		   *set						= [NSMutableSet set];
    NSString			   *name					= nil;
    unsigned long			i						= 0;
	unsigned long			count					= 0;
    int						currentComponentCount   = 0;
	int						iCompCount				= 0;
    
    NS_DURING
        findResults = [_dir findNodeNames:_pathName matchType:eDSStartsWith];
        
    NS_HANDLER
        if (!DS_EXCEPTION_STATUS_IS(eDSUnknownNodeName)
			&& !DS_EXCEPTION_STATUS_IS(eDSNodeNotFound))
        {
            // Clean up memory from the autorelease pool & the pool itself
            // before sending the exception on up.
            // This also means we have to transfer the localException from our local
            // pool to the containing pool, else it is lost.
            [localException retain];
            [pool release];
            [[localException autorelease] raise];
        }
    NS_ENDHANDLER
	
	NS_DURING
		if (_node != nil)
		{
			findResults2 = [_node getAttribute:kDSNAttrSubNodes];
			if (findResults2 != nil)
				findResults = [findResults arrayByAddingObjectsFromArray:findResults2];
		}
        
    NS_HANDLER
        // ignore exceptions here
    NS_ENDHANDLER
    
	count = [findResults count];
	
	// examine results to find only immediate child nodes.
	// We do this by comparing the number of components in the node names
	// using "/" as the component divider.
	currentComponentCount = [[_pathName componentsSeparatedByString:@"/"] count];
	for (i = 0; i < count; i++) 
	{
		name = [findResults objectAtIndex:i];
		nameComponents = [name componentsSeparatedByString:@"/"];
		iCompCount = [nameComponents count];
		if (iCompCount == currentComponentCount + 1)
			[set addObject:[nameComponents lastObject]];
	}
	list = [[set allObjects] retain];
	
	[pool release];
	
    return [list autorelease];
}

- (NSArray*)getRecordList
{
    if (_node != nil)
        return [_node findRecordTypes];
    else
        return [NSArray array];
}

// ----------------------------------------------------------------------------
// Accessor functions
#pragma mark ******** Accessor functions ********

- (BOOL)enableSubNodes
{
    return _enableSubNodes;
}

- (void)setEnableSubNodes:(BOOL)value
{
    _enableSubNodes = value;
}

- (NSString*)nodeName
{
    return [_node getName];
}

- (tDirStatus) authenticateName:(NSString*)inUsername withPassword:(NSString*)inPassword authOnly:(BOOL)inAuthOnly
{
    return [_node authenticateName:inUsername withPassword:inPassword authOnly:inAuthOnly];
}

-(DSoNode*) node
{
	// ATM - PlugInManager needs access to node instance
	return _node;
}

@end

// ----------------------------------------------------------------------------
// Private functions
#pragma mark ******** Private functions ********

@implementation PathNode (PathNodePrivate)
/* Used by searchForKey:withValue:matchType: */
- (void)printSearch:(NSString*)inKey Results:(NSArray*)inResults
{
	NSEnumerator	   *resultEnumerator	= [inResults objectEnumerator];
	NSString		   *kNSAttrRecordName   = @kDSNAttrRecordName;
	NSDictionary	   *d					= nil;
	
	while(d = [resultEnumerator nextObject])
	{
		printf("%s/%s\t\t%s = %s\n",[[self stripDSPrefixOffValue:[d objectForKey:kDSOAttrRecordType]] UTF8String],
		 [[[d objectForKey:kNSAttrRecordName] objectAtIndex:0] UTF8String],[[self stripDSPrefixOffValue:inKey] UTF8String],[[[d objectForKey:inKey] description] UTF8String]);
	}
}

@end
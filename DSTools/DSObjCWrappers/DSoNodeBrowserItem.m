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
 * @header DSoNodeBrowserItem
 */


#import "DSoNodeBrowserItem.h"

#import <DSObjCWrappers/DSObjCWrappers.h>

#ifndef kDSNAttrSubNodes
#define kDSNAttrSubNodes "dsAttrTypeStandard:SubNodes"
#endif

@implementation DSoNodeBrowserItem

- (DSoNodeBrowserItem*)initWithName:(NSString*)name directory:(DSoDirectory*)dir
{
	self = [super init];
	
	_path = [[@"/" stringByAppendingString:name] retain];
	_dir = [dir retain];
	useNode = NO;
	
	return self;
}

- (DSoNodeBrowserItem*)initWithPath:(NSString*)path directory:(DSoDirectory*)dir
{
	self = [super init];
	
	_path = [path copy];
	_dir = [dir retain];
	useNode = YES;
	
	return self;
}

- (void)dealloc
{
	[_path release];
	[_node release];
	[_dir release];
	[_children release];
	[super dealloc];
}

- (void)finalize
{
	[super finalize];
}

- (NSString*)name
{
	return [_path lastPathComponent];
}

- (NSString*)path
{
	return _path;
}

- (DSoNode*)node
{
    @try
    {
        if (_node == nil && useNode)
        {
            _node = [[_dir findNode:[self path]] retain];
        }
    } @catch( NSException *exception ) {
        // ignore any exceptions here
    }
    // give up after one failure
    if (_node == nil)
        useNode = NO;

	return _node;
}

- (BOOL)loadedChildren
{
	return _children != nil;
}

- (BOOL)hasChildren
{
	if (_children != nil) {
		return [_children count] > 0;
	} else {
		return [[self registeredChildrenPaths] count] > 0;
	}
}

- (NSArray*)registeredChildrenPaths
{
	NSAutoreleasePool      *pool					= [NSAutoreleasePool new];
	NSArray				   *findResults				= nil;
	NSArray				   *nameComponents			= nil;
	NSString			   *name					= nil;
	unsigned long			i						= 0;
	unsigned long			count					= 0;
	int						currentComponentCount   = 0;
	int						iCompCount				= 0;
	NSMutableSet		   *childSet				= [NSMutableSet set];
	NSString			   *path					= [self path];
	NSArray				   *subNodes				= nil;
		
	@try
    {
        findResults = [_dir findNodeNames:path matchType:eDSStartsWith];
        count = [findResults count];
        
        // examine results to find only immediate child nodes.
        // We do this by comparing the number of components in the node names
        // using "/" as the component divider.
        currentComponentCount = [[path pathComponents] count];
        for (i = 0; i < count; i++) 
        {
            name = [findResults objectAtIndex:i];
            nameComponents = [name pathComponents];
            iCompCount = [nameComponents count];
            if (iCompCount == currentComponentCount + 1)
            {
                [childSet addObject:name];
            }
            else if (iCompCount > currentComponentCount + 1)
            {
                [childSet addObject:[NSString pathWithComponents:[nameComponents 
				subarrayWithRange:NSMakeRange(0,currentComponentCount+1)]]];
            }
        }        
    } @catch( NSException *exception ) {
        // ignore exceptions here
    }
	
	subNodes = [[childSet allObjects] retain];
	[pool drain];
	
	return [subNodes autorelease];
}


- (NSArray*)children
{
	// children could come from either dsFindDirNodes/dsGetDirNodeList,
	// or dsGetDirNodeInfo on the current node
	if (_children == nil)
	{
		NSAutoreleasePool      *pool					= [NSAutoreleasePool new];
		NSArray				   *findResults				= nil;
		NSString			   *name					= nil;
		NSMutableSet		   *childSet				= [NSMutableSet set];
		NSEnumerator		   *childEnum				= nil;
		NSArray				   *subNodes				= nil;
		
		_children = [NSMutableArray new];
    	
		@try
        {
            findResults = [self registeredChildrenPaths];
            [childSet addObjectsFromArray:findResults];
        } @catch( NSException *exception ) {
            // ignore exceptions here
        }
		
		@try
        {
            subNodes = [[self node] getAttribute:kDSNAttrSubNodes];
            [childSet addObjectsFromArray:subNodes];
        } @catch( NSException *exception ) {
            // ignore exceptions here
        }
		
		childEnum = [childSet objectEnumerator];
		while ((name = (NSString*)[childEnum nextObject]) != nil)
		{
			DSoNodeBrowserItem* item = [[DSoNodeBrowserItem alloc] initWithPath:name
				directory:_dir];
			[_children addObject:item];
			[item release];
		}
    
		[pool drain];
	}
	
	return _children;
}

- (DSoNodeBrowserItem*)childWithName:(NSString*)name
{
	NSArray* children = [self children];
	NSEnumerator* childEnum = [children objectEnumerator];
	DSoNodeBrowserItem* child = nil;
	
	while ((child = [childEnum nextObject]) != nil)
	{
		if ([[child name] isEqualToString:name])
			return child;
	}
	
	return nil;
}

- (int)compareNames:(DSoNodeBrowserItem*)item
{
	return [[self name] caseInsensitiveCompare:[item name]];
}

@end

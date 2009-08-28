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
 * @header PathDirService
 */


#import <DirectoryService/DirServicesTypes.h>
#import "PathDirService.h"
#import "DSoDirectory.h"
#import "DSoNode.h"
#import "PathNode.h"
#import "PathNodeConfig.h"
#import "PathNodeSearch.h"
#import "DSoRecord.h"
#import "DSoNodeConfig.h"
#import "DSoException.h"

#import "DSoDirectoryPriv.h"

#define kHostnameKey	@"hostname"
#define kUserKey		@"user"

static NSDictionary *gSearchPaths;


@implementation PathDirService

+ (void)initialize
{
    gSearchPaths = [[NSDictionary alloc] initWithObjectsAndKeys:[NSNumber numberWithLong:eDSSearchNodeName],
							@"Search",
							[NSNumber numberWithLong:eDSContactsSearchNodeName],
							@"Contact",
							nil];
}
// ----------------------------------------------------------------------------
// Initialization / teardown
#pragma mark ******** Initialization / teardown ********

- init
{
    [super init];
    _attribs = [[NSMutableDictionary alloc] init];
    return self;
}

- initWithLocal
{
    [self init];
    _dir = [[DSoDirectory alloc] initWithLocal];
    [_attribs setObject:@"localhost" forKey:kHostnameKey];
    return self;
}

- initWithLocalPath:(NSString*)filePath
{
    [self init];
    _dir = [[DSoDirectory alloc] initWithLocalPath:filePath];
    [_attribs setObject:@"localonly" forKey:kHostnameKey];
    return self;
}

- initWithHost:(NSString*)hostName user:(NSString*)user password:(NSString*)password
{
    [self init];
    _dir = [[DSoDirectory alloc] initWithHost:hostName user:user password:password];
    [_attribs setObject:hostName forKey:kHostnameKey];
    [_attribs setObject:user forKey:kUserKey];
    return self;
}

- (void)dealloc
{
    [_dir release];
    [_attribs release];
    [super dealloc];
}

-(DSoDirectory*) directory
{
	// ATM - PlugInManager needs access to directory instance
	return _dir;
}

// ----------------------------------------------------------------------------
// PathItemProtocol implementations
#pragma mark ******** PathItemProtocol implementations ********

- (NSString*)name
{
    return @"";
}

- (tDirStatus) read:(NSArray*)inKeys
{
    NSAutoreleasePool      *pool	= [[NSAutoreleasePool alloc] init];
    NSString			   *key		= nil;
	NSString			   *value   = nil;
    NSEnumerator		   *keyEnum;
    
    if (inKeys != nil && [inKeys count] > 0)
    {
        keyEnum = [inKeys objectEnumerator];
    }
    else
    {
        keyEnum = [_attribs keyEnumerator];
    }
    
    while (key = [keyEnum nextObject])
    {
        value = [_attribs objectForKey:key];
        printf("%s: %s\n", [key UTF8String], [value UTF8String]);
    }
    [pool release];
	
    return eDSNoErr;
}

- (NSArray*) getPluginList
{
	NSArray* nodeNames = [_dir findNodeNames];
	NSMutableSet* firstComponents = [NSMutableSet set];
	NSEnumerator* nodeNameEnum = [nodeNames objectEnumerator];
	NSString* nodeName = nil;
	
	while ((nodeName = (NSString*)[nodeNameEnum nextObject]) != nil)
	{
		NSArray* pathComponents = [nodeName pathComponents];
		if ([pathComponents count] > 1)
		{
			[firstComponents addObject:[pathComponents objectAtIndex:1]];
		}
	}
	
	return [firstComponents allObjects]; // Already autoreleased.
}

- (NSArray*) getList
{
	return [[self getPluginList] arrayByAddingObjectsFromArray:[gSearchPaths allKeys]];
}

- (tDirStatus) list:(NSString*)inPath key:(NSString*)inKey
{
    NSAutoreleasePool      *p			= [[NSAutoreleasePool alloc] init];
    DSoNodeConfig		   *configNode  = [_dir configNode];
    id						keyEnum;
	id						key;
    NSArray				   *plugins		= [[self getPluginList] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
    NSString			   *pluginName  = nil;
    int						i			= 0;
	int						cntLimit	= 0;
    
	cntLimit = [plugins count];
    for (i = 0; i < cntLimit; i++)
    {
        pluginName = [plugins objectAtIndex:i];
        if ([configNode pluginEnabled:pluginName])
            printf("%s\n", [pluginName UTF8String]);
    }
    printf("\n");
    keyEnum = [gSearchPaths keyEnumerator];
    while(key = [keyEnum nextObject])
    {
        printf("%s\n", [key UTF8String]);
    }

    [p release];
	
    return eDSNoErr;
}

- (PathItem*) cd:(NSString*)dest
{
    PathItem	   *p		= nil;
    NSString	   *s		= nil;
    DSoNode		   *n		= nil;
    id				pathVal = nil;
    
    if (pathVal = [gSearchPaths objectForKey:dest])
    {
        // They typed a search path name.
        long val = [pathVal longValue];
        if (val == eDSSearchNodeName)
        {
            n = [_dir searchNode];
        }
        else
        {
            n = [_dir findNode:nil matchType:val];
        }
        if (n != nil)
        {
			s = [[NSString alloc] initWithFormat:@"/%@",dest];
			p = [[PathNodeSearch alloc] initWithNode:n path:s type:val];
			[(PathNode *)p setEnableSubNodes:NO];
			[s release];
        }
    }
    else if ([dest caseInsensitiveCompare:@"config"] == NSOrderedSame)
    {
        p = [[PathNodeConfig alloc] initWithNode:[_dir configNode] path:@"/Config"];
        [(PathNodeConfig*)p setEnableSubNodes:NO];
    }
    else if ([dest caseInsensitiveCompare:@"configure"] == NSOrderedSame)
    {
        p = [[PathNodeConfig alloc] initWithNode:[_dir configNode] path:@"/Configure"];
        [(PathNodeConfig*)p setEnableSubNodes:NO];
    }
    else if ([dest caseInsensitiveCompare:@"cache"] == NSOrderedSame)
    {
		NS_DURING
			n = [_dir findNode:@"/Cache"];
		NS_HANDLER
			if (!DS_EXCEPTION_STATUS_IS(eNotHandledByThisNode))
				[localException raise];
		NS_ENDHANDLER
		p = [[PathNode alloc] initWithNode:n path:@"/Cache"];
    }
    else if ([[_dir configNode] pluginEnabled:dest])
    {
        // If they are traversing to the NetInfo, append "/root" to its name.
        if ([dest isEqualToString:@"NetInfo"])
        {
            s = [[NSString alloc] initWithFormat:@"/%@/root",dest];
        }
        else
        {
            s = [[NSString alloc] initWithFormat:@"/%@",dest];
        }
        NS_DURING
            n = [_dir findNode:s];
        NS_HANDLER
            if (!DS_EXCEPTION_STATUS_IS(eNotHandledByThisNode))
                [localException raise];
        NS_ENDHANDLER
        
        if (n == nil)
        {
            //We just have a node prefix.
            p = [[PathNode alloc] initWithDir:_dir path:s];
        }
        else
        {
            p = [[PathNode alloc] initWithNode:n path:s];
        }
        [s release];
    }
    else
    {
        // Invalid destination
        p = nil;
    }
	
    return [p autorelease];
}

@end

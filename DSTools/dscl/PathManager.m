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
 * @header PathManager
 */


#import "PathManager.h"
#import "PathDirService.h"
#import "PathNode.h"
#import "PathNodeConfig.h"
#import "PathNodeSearch.h"
#import "DSoDirectory.h"
#import "DSoNode.h"
#import "DSoException.h"
#import "NSStringEscPath.h"

BOOL gRawMode = NO;

@implementation PathManager

// ----------------------------------------------------------------------------
// Initialization / teardown
#pragma mark ******** Initialization / teardown ********

- init
{
    [super init];
    _stack = [[NSMutableArray alloc] init];
	_pushdPopdStack = [[NSMutableArray alloc] init];
    _stackBackup = nil;
    return self;
}

// Open a connection to the local machine.
- initWithLocal
{
    id dirBase;
    
    [self init];
    dirBase = [[PathDirService alloc] initWithLocal];
    [_stack addObject:dirBase];
    [dirBase release];
    
    return self;
}

// Open a conection to a slimmed version of DS daemon that likely ONLY has a local node in it.
- initWithLocalPath:(NSString*)filePath
{
    id dirBase;
    
    [self init];
    dirBase = [[PathDirService alloc] initWithLocalPath:filePath];
    [_stack addObject:dirBase];
    [dirBase release];
    
    return self;
}

// Open a conection to a remote machine using DS Proxy.
- initWithHost:(NSString*)hostName user:(NSString*)user password:(NSString*)password
{
    id dirBase;
    
    [self init];
    dirBase = [[PathDirService alloc] initWithHost:hostName user:user password:password];
    [_stack addObject:dirBase];
    [dirBase release];
    
    return self;
}

- initWithNodeEnum:(int)inNodeEnum
{
    DSoDirectory	*dir		= nil;
    DSoNode			*node		= nil;
    PathNode		*dirBase	= nil;
    
    [self init];
    
    NS_DURING
        dir		= [[DSoDirectory alloc] initWithLocal];
        node	= [dir findNodeViaEnum:inNodeEnum];
        dirBase = [[PathNode alloc] initWithNode:node path:@"/"];
        [dir release];
    NS_HANDLER
        [dir release];
        [self release];
        if ([localException isKindOfClass:[DSoException class]])
        {
            [dirBase release];
            dirBase = nil;
        }
        else
            [localException raise];
    NS_ENDHANDLER
    
    if (dirBase)
    {
        [dirBase setEnableSubNodes:NO];
        [_stack addObject:dirBase];
        [dirBase release];
        return self;
    }
    else
        return nil;
}

- initWithNodeName:(NSString*)inNodeName
{
    DSoDirectory	*dir		= nil;
    DSoNode			*node		= nil;
    PathNode		*dirBase	= nil;
    
    [self init];
    
    NS_DURING
        dir		= [[DSoDirectory alloc] initWithLocal];
        node	= [dir findNode:inNodeName];
        if ([inNodeName isEqualToString:@"/Search"]
            || [inNodeName isEqualToString:@"/Search/Contacts"]) {
            dirBase = [[PathNodeSearch alloc] initWithNode:node path:@"/"];
        } else if ([inNodeName isEqualToString:@"/Configure"]) {
            dirBase = [[PathNodeConfig alloc] initWithNode:node path:@"/"];		
        } else {
            dirBase = [[PathNode alloc] initWithNode:node path:@"/"];
        }
        [dir release];
    NS_HANDLER
        [dir release];
        [self release];
        if ([localException isKindOfClass:[DSoException class]])
        {
            [dirBase release];
            dirBase = nil;
        }
        else
            [localException raise];
    NS_ENDHANDLER
    
    if (dirBase)
    {
        [dirBase setEnableSubNodes:NO];
        [_stack addObject:dirBase];
        [dirBase release];
        return self;
    }
    else
        return nil;
}

- initWithNodeName:(NSString*)inNodeName user:(NSString*)inUsername password:(NSString*)inPassword
{
    PathNode	   *node	= nil;
    tDirStatus		status  = eDSNoErr;

    if ([self initWithNodeName:inNodeName] == nil)
        return nil;
    
    node = [_stack objectAtIndex:0];
    status = [node authenticateName:inUsername withPassword:inPassword];
    if (status != eDSNoErr)
    {
        [_stack release];
        [_stackBackup release];
        return nil;
    }
    return self;
}

- initWithLocalNode
{
    DSoDirectory	*dir		= nil;
    DSoNode			*localNode  = nil;
    PathNode		*dirBase	= nil;
    [self init];
    
    dir = [[DSoDirectory alloc] initWithLocal];
    localNode = [dir localNode];
    dirBase = [[PathNode alloc] initWithNode:localNode path:@"/"];

    [dirBase setEnableSubNodes:NO];
    [_stack addObject:dirBase];
    
    [dirBase release];
    [dir release];
    
    return self;
}

- initWithLocalNodeAuthUser:(NSString*)inUsername password:(NSString*)inPassword
{
    PathNode	   *node	= nil;
    tDirStatus		status  = eDSNoErr;

    [self initWithLocalNode];
    node = [_stack objectAtIndex:0];
    status = [node authenticateName:inUsername withPassword:inPassword];
    if (status != eDSNoErr)
    {
        [_stack release];
        [_stackBackup release];
        return nil;
    }
    return self;
}


- (void)dealloc
{
    [_stack release];
	[_pushdPopdStack release];
    [_stackBackup release]; // just in case.
    [super dealloc];
}

// ----------------------------------------------------------------------------
// Command actions
#pragma mark ******** Command Actions ********

- (tDirStatus)authenticateUser:(NSString*)inUsername password:(NSString*)inPassword authOnly:(BOOL)inAuthOnly
{
    tDirStatus authStatus = eDSNoErr;

    authStatus = [(PathNode*)[_stack lastObject] authenticateName:inUsername withPassword:inPassword authOnly:inAuthOnly];

    if (authStatus != eDSNoErr)
    {
        printf("Authentication for node %s failed. (%d, %s)\n", [[[_stack lastObject] nodeName] UTF8String],
              authStatus, [[DSoStatus sharedInstance] cStringForStatus:authStatus]);
    }
    return authStatus;
}

- (void)list:(NSString*)inPath key:(NSString*)inKey
{
    if (inPath == nil)
    {
        [(PathItem*)[_stack lastObject] list:inPath key:inKey];
    }
    else 
    {
        [self backupStack];
        [self cd:inPath];
        [(PathItem*)[_stack lastObject] list:inPath key:inKey];
        [self restoreStack];
    }
}

- (tDirStatus)createRecord:(NSString*)inRecordPath key:(NSString*)inKey values:(NSArray*)inValues
{
    tDirStatus status = eDSNoErr;

    if (inRecordPath == nil || [inRecordPath isEqualToString:@"."] )
    {
        status = [[_stack lastObject] createKey:inKey withValues:inValues];
    }
    else 
    {
        [self backupStack];
        status = [self createAndCd:inRecordPath];
        if (status == eDSNoErr && inKey != nil)
            status = [[_stack lastObject] createKey:inKey withValues:inValues];
            
        [self restoreStack];
    }
    return status;
}

- (tDirStatus)appendToRecord:(NSString*)inRecordPath key:(NSString*)inKey values:(NSArray*)inValues
{
    tDirStatus status = eDSNoErr;

    if (inRecordPath == nil || [inRecordPath isEqualToString:@"."] )
    {
        status = [[_stack lastObject] appendKey:inKey withValues:inValues];
    }
    else
    {
        [self backupStack];
        [self cd:inRecordPath];
        status = [[_stack lastObject] appendKey:inKey withValues:inValues];
        [self restoreStack];
    }
    return status;
}

- (tDirStatus)deleteInRecord:(NSString*)inRecordPath key:(NSString*)inKey values:(NSArray*)inValues
{
    tDirStatus status = eDSNoErr;

    if (inRecordPath == nil || [inRecordPath isEqualToString:@"."] )
    {
        status = [[_stack lastObject] deleteKey:inKey withValues:inValues];
    }
    else
    {
        [self backupStack];
        [self cd:inRecordPath];
        status = [[_stack lastObject] deleteKey:inKey withValues:inValues];
        [self restoreStack];
    }
    return status;
}

- (tDirStatus)deleteRecord:(NSString*)inRecordPath
{
    tDirStatus status = eDSNoErr;

    if (inRecordPath == nil || [inRecordPath isEqualToString:@"."] )
    {
        status = [[_stack lastObject] deleteItem];
        if (status == eDSNoErr)
        {
            [_stack removeLastObject];
        }
    }
    else
    {
        [self backupStack];
        [self cd:inRecordPath];
        status = [[_stack lastObject] deleteItem];
        [self restoreStack];
    }
    return status;

}

- (tDirStatus)mergeToRecord:(NSString*)inRecordPath key:(NSString*)inKey values:(NSArray*)inValues
{
    tDirStatus status = eDSNoErr;

    if (inRecordPath == nil || [inRecordPath isEqualToString:@"."] )
    {
        status = [[_stack lastObject] mergeKey:inKey withValues:inValues];
    }
    else
    {
        [self backupStack];
        [self cd:inRecordPath];
        status = [[_stack lastObject] mergeKey:inKey withValues:inValues];
        [self restoreStack];
    }
    return status;
}

- (tDirStatus)changeInRecord:(NSString*)inRecordPath key:(NSString*)inKey values:(NSArray*)inValues
{
	tDirStatus status = eDSNoErr;

    if (inRecordPath == nil || [inRecordPath isEqualToString:@"."] )
    {
        status = [[_stack lastObject] changeKey:inKey oldAndNewValues:inValues];
    }
    else
    {
        [self backupStack];
        [self cd:inRecordPath];
        status = [[_stack lastObject] changeKey:inKey oldAndNewValues:inValues];
        [self restoreStack];
    }
    return status;
}

- (tDirStatus)changeInRecordByIndex:(NSString*)inRecordPath key:(NSString*)inKey values:(NSArray*)inValues
{
	tDirStatus status = eDSNoErr;

    if (inRecordPath == nil || [inRecordPath isEqualToString:@"."] )
    {
        status = [[_stack lastObject] changeKey:inKey indexAndNewValue:inValues];
    }
    else
    {
        [self backupStack];
        [self cd:inRecordPath];
        status = [[_stack lastObject] changeKey:inKey indexAndNewValue:inValues];
        [self restoreStack];
    }
    return status;
}

- (tDirStatus)read:(NSString*)inPath keys:(NSArray*)inKeys
{
    tDirStatus status = eDSNoErr;
    
    if (inPath == nil)
    {
        status = (tDirStatus)[[_stack lastObject] read:inKeys];
    }
    else 
    {
		[self backupStack];

		// Mount records can look like a path, but need to be treated as a
		// single entity.  Mount records look like:  "machine:/path/to/somewhere".
		// Need to handle both: "machine:/path/to/somewhere" and
		// "/LDAPv3/127.0.0.1/machine:/path/to/somewhere".
		NSRange colon = [inPath rangeOfString:@":"];
		if (colon.location == NSNotFound)
		{
			// Regular path (not a mount record).
			[self cd:[inPath stringByDeletingLastPathComponent]];
			status = (tDirStatus)[[_stack lastObject] read:[inPath lastPathComponent] keys:inKeys];
		}
		else
		{
			// Mount record.  Split the DS path from the mount record itself.

			NSString* mountRecString;

			// Find the first slash that preceeds the colon - that will be the
			// end of the DS path.
			colon.length = colon.location;
			colon.location = 0;
			NSRange dsPathEnd = [inPath rangeOfString:@"/" options:NSBackwardsSearch range:colon];
			if (dsPathEnd.location == NSNotFound)
			{
				// No DS path preceeding the mount record.
				mountRecString = inPath;
			}
			else
			{
				// Split the DS path from the mount record.
				NSString* dsPath = [inPath substringToIndex:dsPathEnd.location];
				mountRecString   = [inPath substringFromIndex:dsPathEnd.location + 1];

				[self cd:dsPath];
			}

			status = (tDirStatus)[[_stack lastObject] read:mountRecString keys:inKeys];
		}

		[self restoreStack];
    }
    
    return status;
}

- (tDirStatus)readAll:(NSString*)inPath keys:(NSArray*)inKeys
{
    tDirStatus status = eDSNoErr;
    if (inPath == nil)
    {
        status = [[_stack lastObject] readAll:inKeys];
    }
    else 
    {
        [self backupStack];
        [self cd:inPath];
        status = [[_stack lastObject] readAll:inKeys];
        [self restoreStack];
    }
    
    return status;
}

- (tDirStatus)read:(NSString*)inPath key:(NSString*)inKey plistPath:(NSString*)inPlistPath
{
    tDirStatus status = eDSNoErr;
    
    if (inPath != nil)
    {
        [self backupStack];
        [self cd:inPath];
        status = (tDirStatus)[[_stack lastObject] read:inKey plistPath:inPlistPath];
        [self restoreStack];
    }
    
    return status;
}

- (tDirStatus)read:(NSString*)inPath key:(NSString*)inKey atIndex:(int)index plistPath:(NSString*)inPlistPath
{
    tDirStatus status = eDSNoErr;
    
    if (inPath != nil)
    {
        [self backupStack];
        [self cd:inPath];
        status = (tDirStatus)[[_stack lastObject] read:inKey atIndex:index plistPath:inPlistPath];
        [self restoreStack];
    }
    
    return status;
}

- (tDirStatus)create:(NSString*)inPath key:(NSString*)inKey plistPath:(NSString*)inPlistPath values:(NSArray*)inValues
{
    tDirStatus status = eDSNoErr;
    
    if (inPath != nil)
    {
        [self backupStack];
        [self cd:inPath];
        status = (tDirStatus)[[_stack lastObject] create:inKey plistPath:inPlistPath values:inValues];
        [self restoreStack];
    }
    
    return status;
}

- (tDirStatus)create:(NSString*)inPath key:(NSString*)inKey atIndex:(int)index plistPath:(NSString*)inPlistPath values:(NSArray*)inValues
{
    tDirStatus status = eDSNoErr;
    
    if (inPath != nil)
    {
        [self backupStack];
        [self cd:inPath];
        status = (tDirStatus)[[_stack lastObject] create:inKey atIndex:index plistPath:inPlistPath values:inValues];
        [self restoreStack];
    }
    
    return status;
}

- (tDirStatus)delete:(NSString*)inPath key:(NSString*)inKey plistPath:(NSString*)inPlistPath values:(NSArray*)inValues
{
    tDirStatus status = eDSNoErr;
    
    if(inPath != nil)
    {
        [self backupStack];
        [self cd:inPath];
        status = (tDirStatus)[[_stack lastObject] delete:inKey plistPath:inPlistPath values:inValues];
        [self restoreStack];
    }
    
    return status;
}

- (tDirStatus)delete:(NSString*)inPath key:(NSString*)inKey atIndex:(int)index plistPath:(NSString*)inPlistPath values:(NSArray*)inValues
{
    tDirStatus status = eDSNoErr;
    
    if(inPath != nil)
    {
        [self backupStack];
        [self cd:inPath];
        status = (tDirStatus)[[_stack lastObject] delete:inKey atIndex:index plistPath:inPlistPath values:inValues];
        [self restoreStack];
    }
    
    return status;
}

- (void)diff:(NSString*)inPath1 otherPath:(NSString*)inPath2 keys:(NSArray*)inKeys
{
    NSDictionary* dict1 = nil;
    NSDictionary* dict2 = nil;
    if (inPath1 != nil && inPath2 != nil)
    {
        // should always be true but just in case
        [self backupStack];
        [self cd:inPath1];
        dict1 = [[_stack lastObject] getDictionary:inKeys];
        [self restoreStack];
        [self backupStack];
        [self cd:inPath2];
        dict2 = [[_stack lastObject] getDictionary:inKeys];
        [self restoreStack];
        
        // Get the union of the keys from both dictionaries and then sort them in to an array
        NSArray *keys = [dict1 allKeys];
        NSMutableSet *keysSet = [[NSMutableSet alloc] initWithArray:keys];
        keys = [dict2 allKeys];
        [keysSet addObjectsFromArray:keys];
        keys = [keysSet allObjects];
        [keys sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
        
        NSEnumerator *enumerator = [keys objectEnumerator];
        id key;
        
        while(key = [enumerator nextObject])
        {
            id object1;
            id object2;
            object1 = [dict1 objectForKey:key];
            object2 = [dict2 objectForKey:key];
            if(![object2 isEqual:object1])
            {
                if(object1 != nil)
                {
                    printAttribute(key, object1, @"- ");
                }
                if(object2 != nil)
                {
                    printAttribute(key, object2, @"+ ");
                }
            }
        }
        
        [keysSet release];
    }
}

- (tDirStatus) setPasswordForUser:(NSString*)inRecordPath withParams:(NSArray*)inParams
{
	tDirStatus status = eDSNoErr;
	
	if ([inRecordPath isEqualToString:@"."])
	{
		status = [[_stack lastObject] setPassword:inParams];
	}
	else
    {
        [self backupStack];
        [self cd:inRecordPath];
        status = [[_stack lastObject] setPassword:inParams];
        [self restoreStack];
    }
	return status;
}

- (tDirStatus)createAndCd:(NSString*)inPath
{
    NSArray		   *pathComponents  = [inPath unescapedPathComponents];
    NSString	   *pathComp		= nil;
    unsigned int	i				= 0;
    tDirStatus		status			= eDSNoErr;
	int				cntLimit		= 0;

    if ([[pathComponents objectAtIndex:0] isEqualToString:@"/"])
        i = 1;
    
	cntLimit = [pathComponents count];
    for(; i < cntLimit; i++)
    {                
        pathComp = [pathComponents objectAtIndex:i];
        status = [[_stack lastObject] createKey:pathComp withValues:nil];
        if (status == eDSRecordAlreadyExists)
            status = eDSNoErr;
        else if (status != eDSNoErr)
            break;
        [self cd: [pathComp escapedString]];
    }
    return status;
}

- (void)cd:(NSString*)dest
{
    NSAutoreleasePool      *pool			= [[NSAutoreleasePool alloc] init];
    PathItem			   *p				= nil;
    NSArray				   *pathElements	= nil;
    NSMutableArray		   *newPath			= [[NSMutableArray alloc] initWithArray:_stack];
    NSString			   *s				= nil;
    int						i				= 0;
	int						start			= 0;
    BOOL					failure			= NO;
    
    pathElements = [dest unescapedPathComponents];
    if ( [pathElements count] )
    {
        // If the first element is empty, then we have specified an absolute path.
        // So strip the new stack down to the base element.
        if ([[pathElements objectAtIndex:0] isEqualToString:@"/"])
        {
            if ( [_stack count] > 1)
            {
                NSRange r = NSMakeRange(1,[_stack count] - 1);
                [newPath removeObjectsInRange:r];
            }
            start = 1;
        }
        else
        {
            start = 0;
        }
        
        // Now iterate through the elements and successively 'cd' into each one.
        NS_DURING
			int cntLimit = [pathElements count];
            for (i = start; i < cntLimit && !failure; i++)
            {
                s = [pathElements objectAtIndex:i];
                
                // If it is "..", then go up a path by removing the item from the stack.
                if ([s isEqualToString:@".."])
                {
                    // If they have already reached the top, then don't go any farther.
                    if ([newPath count] > 1)
                        [newPath removeLastObject];
                }
                // If it is empty or they have specifed the extraneous "root" after "/NetInfo", then just skip this element.
                else if ([s length] == 0 || [s isEqualToString:@"."] ||
                            ([s isEqualToString:@"root"] && [[[newPath lastObject] name] isEqualToString:@"NetInfo/root"]) )
                {
                    continue;
                }
                // Otherwise, add the next item down.
                else
                {
                    // don't do anything to s, PathItem takes unescaped strings only
                    p = [(PathItem*)[newPath lastObject] cd:s];
					if (p != nil)
						[newPath addObject:p];
					else
						failure = YES;
                }
            }
        NS_HANDLER
            // If there was an exception (such as too many "cd .." causing the newPath
            // Array to empty, then fail the whole command.
            failure = YES;
        NS_ENDHANDLER
        
        if (!failure)
        {
            // Success!  replace the _stack variable with our new, updated copy.
            [_stack release];
            _stack = newPath;
        }
        else
        {
            // Failure, release the copy, pool, and raise error.
            [newPath release];
            [pool release];
            [NSException raise:@"DSCL" format:@"Invalid Path"];
        }
    }
    [pool release];
}

- (void)pushd:(NSString*)dest
{
	if (dest == nil)
	{
		NSMutableArray *swapStack = nil;
		if ([_pushdPopdStack count] == 0)
			[NSException raise:@"DSCL" format:@"no other directory"];
		// No new destination was specified.  Swap the current path and the top path:
		swapStack = [[_pushdPopdStack lastObject] retain];
		[_pushdPopdStack removeLastObject];
		[_pushdPopdStack addObject:_stack];
		[_stack release];
		_stack = swapStack;
	}
	else
	{
		[_pushdPopdStack addObject:_stack];
		NS_DURING
			[self cd:dest];
		NS_HANDLER
			[_pushdPopdStack removeLastObject];
			[localException raise];
		NS_ENDHANDLER
	}
	[self printPushdPopdStack];
}

- (void)popd
{
	if ([_pushdPopdStack count] == 0)
		[NSException raise:@"DSCL" format:@"Directory stack empty."];

	[_stack release];
	_stack = [[_pushdPopdStack lastObject] retain];
	[_pushdPopdStack removeLastObject];
	[self printPushdPopdStack];
}

- (NSString*)cwd
{
    NSString           *outCwd		= nil;
    NSEnumerator       *stackEnum   = [_stack objectEnumerator];
    NSMutableArray     *pathArray   = [NSMutableArray array];
    PathItem           *pathItem;
    
    // loop over all of them and add names
    while( pathItem = [stackEnum nextObject] )
    {
        [pathArray addObject:[pathItem name]];
    }
    
    if( [pathArray count] == 0 ) {
        outCwd = @"/";
    } else {
        outCwd = [NSString escapablePathFromArray: pathArray];
    }
	
    return outCwd;
}

- (NSArray*)getCurrentList:(NSString*)inPath
{
	NSArray    *retValue			= nil;
	NSArray    *inPathComponents	= [inPath unescapedPathComponents];

	// First drop the last item on the inPath...
	// (Later we may pick it back up and pass it to the getList methods)
	if ([inPathComponents count] > 1)
		inPath = [NSString escapablePathFromArray: [inPathComponents subarrayWithRange: NSMakeRange(0, [inPathComponents count] - 1)]];
	else
		inPath = nil;
	
    if (inPath == nil)
    {
        retValue = [[_stack lastObject] getList];
    }
    else
    {
        [self backupStack];
        [self cd:[inPath escapedString]];
        retValue = [[_stack lastObject] getList];
        [self restoreStack];
    }

	return retValue;
}


- (NSArray*)getPossibleCompletionsFor:(NSString*)inPathAndPrefix
{
	NSArray         *retValue           = nil;
	NSArray         *pathComponents     = [inPathAndPrefix unescapedPathComponents];
	NSString        *prefix             = [[pathComponents lastObject] lowercaseString];
    unsigned int    pathCount           = [pathComponents count];

	if (pathCount > 1)
    {
        NSArray *tempArray = [pathComponents subarrayWithRange: NSMakeRange(0, pathCount - 1)];
        
        [self backupStack];
        [self cd: [NSString escapablePathFromArray: tempArray]];
        retValue = [[_stack lastObject] getPossibleCompletionsFor: prefix];
        [self restoreStack];
    }
	else if( prefix )
    {
        retValue = [[_stack lastObject] getPossibleCompletionsFor: prefix];
    }
	
	return retValue;
}


- (void)searchInPath:(NSString*)inPath forKey:(NSString*)inKey withValue:(NSString*)inValue matchType:(NSString*)inType
{
	BOOL isCurrentDir = [inPath isEqualToString:@"."];
	if (!isCurrentDir)
	{
		[self backupStack];
		[self cd:inPath];
	}

	[[_stack lastObject] searchForKey:inKey withValue:inValue matchType:inType];

	if (!isCurrentDir)
		[self restoreStack];
}

// ----------------------------------------------------------------------------
// Utility methods
#pragma mark ******** Utility methods ********

- (void)backupStack
{
    _stackBackup = [[NSMutableArray alloc] initWithArray:_stack];
}

- (void)restoreStack
{
    if (_stackBackup != nil)
    {
        [_stack release];
        _stack = _stackBackup;
        _stackBackup = nil;
    }
}

- (void)printPushdPopdStack
{
	int		i   = 0;
	int		j   = 0;
	int		cnt = 0;
	
	printf("%s ", [[[self cwd] unescapedString] UTF8String]);
	cnt = [_pushdPopdStack count];
	for (i=cnt-1; i >= 0 ; i--)
	{
		NSArray *pathStack = [_pushdPopdStack objectAtIndex:i];
		int cntLimit = [pathStack count];
		if (cntLimit == 1)
			printf("/");
		else
		{
			for (j=1; j < cntLimit; j++)
			{
				NSString *pathItemName = [[pathStack objectAtIndex:j] name];
				printf("/%s",[pathItemName UTF8String]);
			}
		}
		printf(" ");
	}
	printf("\n");
}

-(NSArray*) stack
{
	// ATM - give PlugInManager access to the stack
	return _stack;
}

@end

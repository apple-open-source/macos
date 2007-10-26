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
 * @header PathRecord
 */


#import <unistd.h>
#import <DirectoryService/DirServicesConst.h>
#import "PathRecord.h"
#import "DSoDirectory.h"
#import "DSoNode.h"
#import "DSoRecord.h"
#import "DSoUser.h"
#import "DSoException.h"

@implementation PathRecord

// ----------------------------------------------------------------------------
// Initialization / teardown
#pragma mark ******** Initialization / teardown ********

- init
{
    [super init];
    _record = nil;
    return self;
}

- initWithRecord:(DSoRecord*)inRec
{
    [self init];
    _record = [inRec retain];
    return self;
}

- (void)dealloc
{
    [_record release];
    [super dealloc];
}

// ----------------------------------------------------------------------------
// PathItemProtocol implementations
#pragma mark ******** PathItemProtocol implementations ********

- (tDirStatus) appendKey:(NSString*)inKey withValues:(NSArray*)inValues
{
    return [self modify:ATTR_APPEND withKey:inKey withValues:inValues];
}

- (tDirStatus) authenticateName:(NSString*)inUsername withPassword:(NSString*)inPassword authOnly:(BOOL)inAuthOnly
{
    tDirStatus status = eDSNoErr;

    status = [[_record node] authenticateName:inUsername withPassword:inPassword authOnly:inAuthOnly];
    if (status == eDSNoErr && inAuthOnly == NO)
    {
        // Since we are now authenticated, we need to re-open this record.
        DSoRecord *newRec =	nil;

        newRec = [[_record node] findRecord:[_record getName] ofType:[_record getType]];
        [_record release];
        _record = [newRec retain];
    }
    return status;
}

- (NSString*) name
{
    return [_record getName];
}

- (tDirStatus) list:(NSString*)inPath key:(NSString*)inKey
{
    return eDSNoErr;
}

- (PathItem*) cd:(NSString*)dest
{
    return nil;
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

- (tDirStatus) delete:(NSString*)inKey atIndex:(int)index plistPath:(NSString*)inPlistPath values:(NSArray*)inValues
{
    NSAutoreleasePool      *pool	= [[NSAutoreleasePool alloc] init];
    NSDictionary		   *attribs = nil;
    NSMutableArray         *values = nil;
    NSString               *value = nil;
    NSMutableDictionary    *plist = nil;
    NSArray                *pathElements = nil;
    NSEnumerator           *pathEnum = nil;
    NSString               *currentPathElement = nil;
    NSString               *previousPathElement = nil;
    id                      currentElement = nil;
    id                      previousElement = nil;
    NSPropertyListFormat	format = NSPropertyListXMLFormat_v1_0;
    BOOL                    changed = NO;
    tDirStatus              status = eDSNoErr;
    NS_DURING
        
    attribs = [self getDictionary:[NSArray arrayWithObject:inKey]];
    if([attribs count] == 0)
    {
        printf("Invalid key.\n");
        NS_VALUERETURN(eDSEmptyAttribute,tDirStatus);
    }
    NSString *attrib;
    if (!([inKey hasPrefix:@kDSStdAttrTypePrefix] || [inKey hasPrefix:@kDSNativeAttrTypePrefix]))
    {
        attrib = [@kDSStdAttrTypePrefix stringByAppendingString:inKey];
        if([attribs objectForKey:attrib] != nil)
            inKey = attrib;
        
        attrib = [@kDSNativeAttrTypePrefix stringByAppendingString:inKey];
        if([attribs objectForKey:attrib] != nil)
            inKey = attrib;
    }
    values = [[[attribs objectForKey:inKey] mutableCopy] autorelease];
    if([values count] < 1)
    {
        printf("There is no value for attribute %s\n", [inKey UTF8String]);
        NS_VALUERETURN(eDSEmptyAttribute,tDirStatus);
    }
    else if(index >= [values count])
    {
        printf("Value index out of range\n");
        NS_VALUERETURN(eDSIndexOutOfRange,tDirStatus);
    }
    else
    {
        value = [values objectAtIndex:index];
    }
    plist = [NSPropertyListSerialization propertyListFromData:[value dataUsingEncoding:NSUTF8StringEncoding] 
                                             mutabilityOption:NSPropertyListMutableContainersAndLeaves format:&format errorDescription:nil];
    pathElements = [inPlistPath componentsSeparatedByString:@":"];
    pathEnum = [pathElements objectEnumerator];
    currentElement = plist;
    
    while (currentElement != nil && ((currentPathElement = (NSString*)[pathEnum nextObject]) != nil))
    {
        previousPathElement = currentPathElement;
        previousElement = currentElement;
        if ([currentElement isKindOfClass:[NSDictionary class]])
        {
            currentElement = [currentElement objectForKey:currentPathElement];
        }
        else if([currentElement isKindOfClass:[NSArray class]])
        {
            NSString* intString = [[NSString alloc] initWithFormat:@"%d",[currentPathElement intValue]];
            
            if([currentPathElement intValue] >= [currentElement count] || ![currentPathElement isEqualToString:intString])
            {
                break; // index out of range
            }
            else
            {
                currentElement = [currentElement objectAtIndex:[currentPathElement intValue]];
            }
            [intString release];
        }
        else
        {
            break; // not a valid path
        }
    }
    
    inValues = [inValues sortedArrayUsingSelector:@selector(compare:)];
    
    int c;
    for(c = [inValues count] - 1; c >= 0; c--)
    {
        if (currentPathElement == nil && currentElement != nil)
        {
            // found something
            if([currentElement isKindOfClass:[NSDictionary class]])
            {
                [currentElement removeObjectForKey:[inValues objectAtIndex:c]];
                changed = YES;
            }
            else if([currentElement isKindOfClass:[NSArray class]])
            {
                NSString* intString = [[NSString alloc] initWithFormat:@"%d",[[inValues objectAtIndex:c] intValue]];
                if(![[inValues objectAtIndex:c] isEqualToString:intString])
                {
                    printf("Invalid index %s\n", [[inValues objectAtIndex:c] UTF8String]);
                }
                else if([[inValues objectAtIndex:c] intValue] >= [currentElement count])
                {
                    printf("Index out of range\n");
                }
                else
                {
                    [currentElement removeObjectAtIndex:[[inValues objectAtIndex:c] intValue]];
                    changed = YES;
                }
                [intString release];
            }
        }
        else
        {
            // bogus path
            printf("No such plist path: %s\n", [inPlistPath UTF8String]);
            NS_VALUERETURN(eDSUnknownMatchType, tDirStatus);
        }
    }
    if([inValues count] == 0)
    {
        if(previousPathElement && previousElement)
        {
            if([previousElement isKindOfClass:[NSDictionary class]])
            {
                [previousElement removeObjectForKey:previousPathElement];
                changed = YES;
            }
            else if([previousElement isKindOfClass:[NSArray class]])
            {
                NSString* intString = [[NSString alloc] initWithFormat:@"%d",[previousPathElement intValue]];
                if(![previousPathElement isEqualToString:intString])
                {
                    printf("Invalid index %s\n", [previousPathElement UTF8String]);
                }
                else if([previousPathElement intValue] >= [previousElement count])
                {
                    printf("Index out of range\n");
                }
                else
                {
                    [previousElement removeObjectAtIndex:[previousPathElement intValue]];
                    changed = YES;
                }
            }
        }
    }
    if(changed)
    {
        NSData *data = [NSPropertyListSerialization dataFromPropertyList:plist format:format errorDescription:nil];
        NSString *dataString = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        [values replaceObjectAtIndex:index withObject:dataString];
        [dataString release];
        status = [self modify:ATTR_CREATE withKey:inKey withValues:values];
    }
    
    NS_HANDLER
        [localException retain];
        [pool release];
        [[localException autorelease] raise];
    NS_ENDHANDLER
    
    [pool release];
    
    return status;
}

- (tDirStatus) delete:(NSString*)inKey plistPath:(NSString*)inPlistPath values:(NSArray*)inValues
{
    return [self delete:inKey atIndex:0 plistPath:inPlistPath values:inValues];
}

- (tDirStatus) create:(NSString*)inKey atIndex:(int)index plistPath:(NSString*)inPlistPath values:(NSArray*)inValues
{
    NSAutoreleasePool      *pool	= [[NSAutoreleasePool alloc] init];
    NSDictionary		   *attribs = nil;
    NSMutableArray         *values = nil;
    NSString               *value = nil;
    NSMutableDictionary    *plist = nil;
    NSArray                *pathElements = nil;
    NSEnumerator           *pathEnum = nil;
    NSString               *currentPathElement = nil;
    NSString               *previousPathElement = nil;
    id                      currentElement = nil;
    id                      previousElement = nil;
    NSPropertyListFormat	format = NSPropertyListXMLFormat_v1_0;
    BOOL                    changed = NO;
    tDirStatus              status = eDSNoErr;
    NS_DURING
        
    attribs = [self getDictionary:[NSArray arrayWithObject:inKey]];
    if([attribs count] == 0)
    {
        printf("Invalid key.\n");
    }
    NSString *attrib;
    if (!([inKey hasPrefix:@kDSStdAttrTypePrefix] || [inKey hasPrefix:@kDSNativeAttrTypePrefix]))
    {
        attrib = [@kDSStdAttrTypePrefix stringByAppendingString:inKey];
        if([attribs objectForKey:attrib] != nil)
            inKey = attrib;
        
        attrib = [@kDSNativeAttrTypePrefix stringByAppendingString:inKey];
        if([attribs objectForKey:attrib] != nil)
            inKey = attrib;
    }
    values = [[[attribs objectForKey:inKey] mutableCopy] autorelease];
    if([values count] < 1)
    {
        printf("There is no value for attribute %s\n", [inKey UTF8String]);
        NS_VALUERETURN(eDSEmptyAttribute,tDirStatus);
    }
    else if(index >= [values count])
    {
        printf("Value index out of range\n");
        NS_VALUERETURN(eDSIndexOutOfRange,tDirStatus);
    }
    else
    {
        value = [values objectAtIndex:index];
    }
    plist = [NSPropertyListSerialization propertyListFromData:[value dataUsingEncoding:NSUTF8StringEncoding] 
                                             mutabilityOption:NSPropertyListMutableContainersAndLeaves format:&format errorDescription:nil];
    pathElements = [inPlistPath componentsSeparatedByString:@":"];
    pathEnum = [pathElements objectEnumerator];
    currentElement = plist;
    
    while (currentElement != nil && ((currentPathElement = (NSString*)[pathEnum nextObject]) != nil))
    {
        previousPathElement = currentPathElement;
        previousElement = currentElement;
        if ([currentElement isKindOfClass:[NSDictionary class]])
        {
            currentElement = [currentElement objectForKey:currentPathElement];
        }
        else if([currentElement isKindOfClass:[NSArray class]])
        {
            NSString* intString = [[NSString alloc] initWithFormat:@"%d",[currentPathElement intValue]];
            
            if([currentPathElement intValue] > [currentElement count] || ![currentPathElement isEqualToString:intString])
            {
                currentPathElement = nil;
                currentElement = nil;
                printf("Index out of range\n");
                break; // index out of range
            }
            else if([currentPathElement intValue] == [currentElement count])
            {
                currentElement = nil;
                break;
            }
            else
            {
                currentElement = [currentElement objectAtIndex:[currentPathElement intValue]];
            }
            [intString release];
        }
        else
        {
            currentPathElement = [pathEnum nextObject];
            if (currentPathElement != nil)
            {
                break; // not a valid path
            }
        }
    }
    
    id container = nil;
    id key = nil;
    
    if((currentPathElement == nil && currentElement != nil) || 
       (currentPathElement != nil && previousElement != nil && [pathEnum nextObject] == nil))
    {
        container = previousElement;
        key = previousPathElement;
    }
    else
    {
        container = nil;
        key = nil;
        printf("No such plist path: %s\n", [inPlistPath UTF8String]);
        NS_VALUERETURN(eDSUnknownMatchType, tDirStatus);
    }
    
    if([container isKindOfClass:[NSArray class]])
    {
        // Adding to an array
        printf("Changing an array\n");
        if([inValues count] == 1)
        {
            //[container removeAllObjects];
            //[container addObject:[inValues objectAtIndex:0]];
            if([key intValue] < [container count])
                [container replaceObjectAtIndex:[key intValue] withObject:[inValues objectAtIndex:0]];
            else
                [container addObject:[inValues objectAtIndex:0]];
        }
        else
        {
            if([key intValue] < [container count])
                [container replaceObjectAtIndex:[key intValue] withObject:inValues];
            else
                [container addObject:inValues];
        }
        changed = YES;
    }
    else if([container isKindOfClass:[NSDictionary class]])
    {
        // Adding to a dictionary
        printf("Changing a dictionary\n");
        if([inValues count] == 1)
        {
            [container setObject:[inValues objectAtIndex:0] forKey:key];
        }
        else
        {
            [container setObject:inValues forKey:key];
        }
        changed = YES;
    }
    else
    {
        printf("No such plist path: %s\n", [inPlistPath UTF8String]);
        NS_VALUERETURN(eDSUnknownMatchType, tDirStatus);
    }
    if(changed)
    {
        NSData *data = [NSPropertyListSerialization dataFromPropertyList:plist format:format errorDescription:nil];
        NSString *dataString = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
        [values replaceObjectAtIndex:index withObject:dataString];
        [dataString release];
        status = [self modify:ATTR_CREATE withKey:inKey withValues:values];
    }
    
    NS_HANDLER
        [localException retain];
        [pool release];
        [[localException autorelease] raise];
    NS_ENDHANDLER
    
    [pool release];
    
    return status;
}

- (tDirStatus) create:(NSString*)inKey plistPath:(NSString*)inPlistPath values:(NSArray*)inValues
{
    return [self create:inKey atIndex:0 plistPath:inPlistPath values:inValues];
}

- (tDirStatus) deleteKey:(NSString*)inKey withValues:(NSArray*)inValues
{
    return [self modify:ATTR_DELETE withKey:inKey withValues:inValues];
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

- (NSString*)nodeName
{
    return [[_record node] getName];
}

- (NSDictionary*)getDictionary:(NSArray*)inKeys
{
    NSAutoreleasePool      *pool	= [[NSAutoreleasePool alloc] init];
    NSDictionary		   *attribs = nil;
    
    NS_DURING
    
    if (inKeys == nil || [inKeys count] == 0)
    {
        attribs = [_record getAllAttributesAndValues];
    }
    else
    {
        NSArray* niceKeys    = prefixedAttributeKeysWithNode([_record node], inKeys);
        attribs = [_record getAttributes:niceKeys];
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

- (tDirStatus) setPassword:(NSArray*)inParams
{
	if (!strcmp([_record getType], kDSStdRecordTypeUsers))
	{
		// This is a user record, proceed to set the password
		if ([inParams count] == 2)
			[(DSoUser*)_record changePassword:[inParams objectAtIndex:0]
								toNewPassword:[inParams objectAtIndex:1]];
		else
		{
			NS_DURING
				[(DSoUser*)_record setPassword:[inParams objectAtIndex:0]];
			NS_HANDLER
				if (DS_EXCEPTION_STATUS_IS(eDSPermissionError))
				{
					NSString* oldPassword = [NSString stringWithUTF8String:getpass("Permission denied. Please enter user's old password:")];
					[(DSoUser*)_record changePassword:oldPassword toNewPassword:[inParams objectAtIndex:0]];
				}
				else
					[localException raise];
			NS_ENDHANDLER
		}
	}
	else
	{
		// This is some other record type, we can't set a password
		[DSoException raiseWithStatus:eDSInvalidRecordType];
	}
	return eDSNoErr;
}

// ----------------------------------------------------------------------------
// Utility methods
#pragma mark ******** Utility methods ********

- (tDirStatus) modify:(tAttrCAM)inAction withKey:(NSString*)inKey withValues:(NSArray*)inValues
{
    NSString	   *attrib				= nil;
    tDirStatus		nError				= eDSNoErr;
	tDirStatus		firstError			= eDSNoErr;

    if ([inKey hasPrefix:@kDSStdAttrTypePrefix] || [inKey hasPrefix:@kDSNativeAttrTypePrefix])
    {
        attrib = inKey;
    }
    else
    {
        attrib = [@kDSStdAttrTypePrefix stringByAppendingString:inKey];
        if (![[[[_record node] directory] standardAttributeTypes] containsObject:attrib])
            attrib = [@kDSNativeAttrTypePrefix stringByAppendingString:inKey];
    }

    NS_DURING
        switch (inAction)
        {
            case ATTR_CREATE:
                [_record setAttribute:[attrib UTF8String] values:inValues];
                break;
            case ATTR_APPEND:
                [_record addAttribute:[attrib UTF8String] values:inValues mergeValues:NO];
                break;
            case ATTR_MERGE:
                [_record addAttribute:[attrib UTF8String] values:inValues mergeValues:YES];
                break;
            case ATTR_DELETE:
                if (inValues == nil || [inValues count] == 0)
                    [_record removeAttribute:[attrib UTF8String]];
                else
                    [_record removeAttribute:[attrib UTF8String] values:inValues];
                break;
			case ATTR_CHANGE:
				[_record changeAttribute:[attrib UTF8String] oldValue:[inValues objectAtIndex:0]
								   newValue:[inValues objectAtIndex:1]];
				break;
			case ATTR_CHANGE_INDEX:
				[_record changeAttribute:[attrib UTF8String] index:[[inValues objectAtIndex:0] intValue]
								   newValue:[inValues objectAtIndex:1]];
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

-(DSoRecord*) record
{
	// ATM - give PlugInManager access to record instance
	return _record;
}

@end

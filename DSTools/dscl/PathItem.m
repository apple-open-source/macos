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
 * @header PathItem
 */


#import "DSoDirectory.h"
#import "PathItem.h"
#import "NSStringEscPath.h"

BOOL gPlistMode = NO;

@implementation PathItem

// ----------------------------------------------------------------------------
// Initialization / teardown
#pragma mark ******** Initialization / teardown ********

- init
{
    [super init];
    return self;
}

- (NSString*)stripDSPrefixOffValue:(NSString*)inValue
{
    NSRange r = [inValue rangeOfString:@":" options:NSBackwardsSearch];
    if (!gRawMode && r.length > 0 && [inValue hasPrefix:@"ds"]
         && ![inValue hasPrefix:@kDSNativeAttrTypePrefix]
         && ![inValue hasPrefix:@kDSNativeRecordTypePrefix])
    {
        // inValue definitely (with high probability) has a prefix
        return [inValue substringFromIndex:r.location+1];
    }
    else
    { 
        // it doesn't look like a known prefix or we are
        // in raw mode, leave it alone
        return inValue;
    }
}

- (NSString*)description
{
    return [[super description] stringByAppendingFormat:@"(%@)",[self name]];
}

// ----------------------------------------------------------------------------
// PathItemProtocol implementations
// It is expected that these will all be implemented as needed
#pragma mark ******** PathItemProtocol implementations ********

- (tDirStatus) appendKey:(NSString*)inKey withValues:(NSArray*)inValues
{
    return eDSNoErr;
}

- (tDirStatus) authenticateName:(NSString*)inUsername withPassword:(NSString*)inPassword authOnly:(BOOL)inAuthOnly
{
    return eDSAuthFailed;
}

- (tDirStatus) authenticateName:(NSString*)inUsername withPassword:(NSString*)inPassword
{
    return [self authenticateName:inUsername withPassword:inPassword authOnly:NO];
}

- (tDirStatus) setPassword:(NSArray*)inPassword
{
	return eDSAuthFailed;
}

- (NSString*) name
{
    return nil;
}

- (PathItem*) cd:(NSString*)dest
{
    return nil;
}

- (tDirStatus) createKey:(NSString*)inKey withValues:(NSArray*)inValues
{
    return eDSNoErr;
}

- (tDirStatus) deleteItem
{
    return eDSNoErr;
}

- (tDirStatus) deleteKey:(NSString*)inKey withValues:(NSArray*)inValues
{
    return eDSNoErr;
}

- (tDirStatus) list:(NSString*)inPath key:(NSString*)inKey
{
    return eDSNoErr;
}

- (NSDictionary*) getDictionary:(NSArray*)inKeys
{
    return nil;
}

- (NSArray*) getList
{
	return nil;
}

- (NSArray*) getListWithKeys:(NSArray*)inKeys
{
    return nil;
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

- (tDirStatus) mergeKey:(NSString*)inKey withValues:(NSArray*)inValues
{
    return eDSNoErr;
}

- (NSString*)nodeName
{
    return @"No Node";
}

- (void) printDictionary:(NSDictionary*)inDict withRequestedKeys:(NSArray*)inKeys
{
    NSAutoreleasePool      *pool	= [[NSAutoreleasePool alloc] init];
    id						key     = nil;
    id                      keys    = nil;
	id						values  = nil;
    unsigned long           cnt     = 0;
    unsigned long			i		= 0;
    
    if (gPlistMode)
    {
        NSData* data = (NSData*)CFPropertyListCreateXMLData(NULL, inDict);
        char* plistStr = NULL;
        if (data != nil)
        {
            plistStr = (char*)calloc(sizeof(char), [data length] + 1);
        }
        if (plistStr != NULL)
        {
            strlcpy(plistStr, (char*)[data bytes], [data length]);
            printf("%s\n", plistStr);
            free(plistStr);
            plistStr = NULL;
        }
        [data release];
        data = nil;
    }
    else
    {
        keys = [[inDict allKeys] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
        cnt = [keys count];
        for(i = 0; i < cnt; i++)
        {
            key = [keys objectAtIndex:i];
            values = [inDict objectForKey:key];
            printAttribute(key, values, nil);
        }
        if ([inKeys count] > 0)
        {
            NSEnumerator    *keyEnum    = nil;
            NSString        *key        = nil;
            NSString		*attrib		= nil;
            NSString		*attrib2	= nil;
            unsigned long	cntLimit	= 0;
            unsigned long	i			= 0;
            NSMutableSet    *missingAttributes = [NSMutableSet set];
            
            cntLimit = [inKeys count];
            for (i = 0; i < cntLimit; i++)
            {
                key = [inKeys objectAtIndex:i];
                if (!([key hasPrefix:@kDSStdAttrTypePrefix] || [key hasPrefix:@kDSNativeAttrTypePrefix]))
                {
                    attrib = [@kDSStdAttrTypePrefix stringByAppendingString:key];
                    attrib2 = [@kDSNativeAttrTypePrefix stringByAppendingString:key];
                    if([inDict objectForKey:attrib] == nil && [inDict objectForKey:attrib2] == nil)
                        [missingAttributes addObject:key];
                }
                else
                {
                    if([inDict objectForKey:key] == nil)
                        [missingAttributes addObject:key];
                }
            }
            keyEnum = [missingAttributes objectEnumerator];
            while ((key = (NSString*)[keyEnum nextObject]) != nil)
            {
                printf("No such key: %s\n", [key UTF8String]);
            }
        }
    }
    
    [pool release];
}

- (tDirStatus) read:(NSArray*)inKeys
{
    NSAutoreleasePool      *pool	= [[NSAutoreleasePool alloc] init];
    NSDictionary		   *attribs = nil;
    
    NS_DURING
    
    attribs = [self getDictionary:inKeys];
    [self printDictionary:attribs withRequestedKeys:inKeys];
    
    NS_HANDLER
    [localException retain];
    [pool release];
    [[localException autorelease] raise];
    NS_ENDHANDLER
    
    [pool release];

    return eDSNoErr;
}

- (tDirStatus) readAll:(NSArray*)inKeys
{
    NSAutoreleasePool      *pool	= [[NSAutoreleasePool alloc] init];
    NSArray                *list    = [self getListWithKeys:inKeys];
    NSEnumerator           *listEnum = [list objectEnumerator];
    NSDictionary		   *attribs;
    
    NS_DURING
    
    if (gPlistMode)
    {
        [self printDictionary:(NSDictionary*)list withRequestedKeys:nil];
    }
    else
    {
        attribs = (NSDictionary*)[listEnum nextObject];
        
        while (attribs != nil)
        {
            [self printDictionary:attribs withRequestedKeys:nil];
            attribs = (NSDictionary*)[listEnum nextObject];
            if (attribs != nil)
            {
                printf("-\n");
            }
        }
    }
    
    NS_HANDLER
    [localException retain];
    [pool release];
    [[localException autorelease] raise];
    NS_ENDHANDLER
    
    [pool release];
    
    return eDSNoErr;
}

- (tDirStatus) read:(NSString*)inPath keys:(NSArray*)inKeys
{
    tDirStatus status = eDSNoErr;
    
    if ( ([inPath isEqualToString:@"."]) ||([inPath isEqualToString:@"/"])  )
    {
        status = [self read:inKeys];
    }
    else
    {
        PathItem* destPathItem = [self cd:inPath];
        status = (tDirStatus)[destPathItem read:inKeys];
    }
    
    return status;
}

- (tDirStatus) read:(NSString*)inKey atIndex:(int)index plistPath:(NSString*)inPlistPath
{
    NSAutoreleasePool      *pool	= [[NSAutoreleasePool alloc] init];
    NSDictionary		   *attribs = nil;
    NSArray                *values = nil;
    NSString               *value = nil;
    NSDictionary           *plist = nil;
    NSArray                *pathElements = nil;
    NSEnumerator           *pathEnum = nil;
    NSString               *currentPathElement = nil;
    id                      currentElement = nil;
    NSPropertyListFormat	format = NSPropertyListXMLFormat_v1_0;
    
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
    values = [attribs objectForKey:inKey];
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
                                             mutabilityOption:NSPropertyListImmutable format:&format errorDescription:nil];
    pathElements = [inPlistPath componentsSeparatedByString:@":"];
    pathEnum = [pathElements objectEnumerator];
    currentElement = plist;
    while (currentElement != nil && ((currentPathElement = (NSString*)[pathEnum nextObject]) != nil))
    {
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
    
    if (currentPathElement == nil && currentElement != nil)
    {
        // found something
        if ([currentElement isKindOfClass:[NSString class]])
        {
            printAttribute(inPlistPath, [NSArray arrayWithObject:currentElement], nil);
        }
        else if ([currentElement isKindOfClass:[NSArray class]] || [currentElement isKindOfClass:[NSDictionary class]])
        {
            printPlist(inPlistPath, currentElement);
        }
        else if([currentElement isKindOfClass:[NSNumber class]])
        {
            printf("%s: %s\n", [inPlistPath UTF8String], [[currentElement stringValue] UTF8String]);
        }
        else if([currentElement isKindOfClass:[NSDate class]])
        {
            printf("%s: %s\n", [inPlistPath UTF8String], [[currentElement description] UTF8String]);
        }
        else if([currentElement isKindOfClass:[NSData class]])
        {
            NSString *dataString = [[NSString alloc] initWithData:currentElement encoding:NSUTF8StringEncoding];
            printf("%s: %s", [inPlistPath UTF8String], [dataString UTF8String]);
            [dataString release];
        }
        else
        {
            printf("Currently, the class %s is not supported.\n", [[currentElement className] UTF8String]);
        }
    }
    else
    {
        // bogus path
        printf("No such plist path: %s\n", [inPlistPath UTF8String]);
        NS_VALUERETURN(eDSUnknownMatchType, tDirStatus);
    }
    
    NS_HANDLER
        [localException retain];
        [pool release];
        [[localException autorelease] raise];
    NS_ENDHANDLER
    
    [pool release];
    
    return eDSNoErr;
}

- (tDirStatus) read:(NSString*)inKey plistPath:(NSString*)inPlistPath
{
    return [self read:inKey atIndex:0 plistPath:inPlistPath];
}

- (tDirStatus) create:(NSString*)inKey plistPath:(NSString*)inPlistPath values:(NSArray*)inValues
{
    printf("You can only use createpl on a record.\n");
    return eDSNoErr;
}

- (tDirStatus) delete:(NSString*)inKey plistPath:(NSString*)inPlistPath values:(NSArray*)inValues
{
    printf("You can only use deletepl on a record.\n");
    return eDSNoErr;
}

- (tDirStatus) create:(NSString*)inKey atIndex:(int)index plistPath:(NSString*)inPlistPath values:(NSArray*)inValues
{
    printf("You can only use createpli on a record.\n");
    return eDSNoErr;
}

- (tDirStatus) delete:(NSString*)inKey atIndex:(int)index plistPath:(NSString*)inPlistPath values:(NSArray*)inValues
{
    printf("You can only use deletepli on a record.\n");
    return eDSNoErr;
}

- (tDirStatus) searchForKey:(NSString*)inKey withValue:(NSString*)inValue matchType:(NSString*)inType
{
	printf("You can only search a node or record type path.\n");
	return eDSNoErr;
}

- (tDirStatus) changeKey:(NSString*)inKey oldAndNewValues:(NSArray*)inValues
{
	return eDSNoErr;
}

- (tDirStatus) changeKey:(NSString*)inKey indexAndNewValue:(NSArray*)inValues
{
	return eDSNoErr;
}

@end

NSArray* prefixedAttributeKeysWithNode(DSoNode* inNode, NSArray* inKeys)
{
    NSString           *attrib      = nil;
    unsigned long       cntLimit    = 0;
    id					key         = nil;
    NSMutableArray     *niceKeys    = nil;
    unsigned long       i           = 0;

    if ( inKeys == nil )
    {
        return nil;
    }

    //here we need to make the keys "nice"
    cntLimit = [inKeys count];
    niceKeys = [NSMutableArray arrayWithCapacity:cntLimit];
    for (i = 0; i < cntLimit; i++)
    {
        key = [inKeys objectAtIndex:i];
        if ([key hasPrefix:@kDSStdAttrTypePrefix] || [key hasPrefix:@kDSNativeAttrTypePrefix])
        {
            attrib = key;
        }
        else
        {
            attrib = [@kDSStdAttrTypePrefix stringByAppendingString:key];
            if (![[[inNode directory] standardAttributeTypes] containsObject:attrib])
                attrib = [@kDSNativeAttrTypePrefix stringByAppendingString:key];
        }
        [niceKeys addObject:attrib];
    }
    
    return niceKeys;
}

void printValue(NSString *inValue, BOOL hasSpaces)
{
	if (gURLEncode)
		inValue = [inValue urlEncoded];
    if (!hasSpaces)
        printf(" %s", [inValue UTF8String]);
    else
        printf("\n %s", [inValue UTF8String]);
}

NSString* stripDSPrefixOffValue(NSString* inValue)
{
    NSRange r = [inValue rangeOfString:@":" options:NSBackwardsSearch];
    if (!gRawMode && r.length > 0 && [inValue hasPrefix:@"ds"]
        && ![inValue hasPrefix:@kDSNativeAttrTypePrefix]
        && ![inValue hasPrefix:@kDSNativeRecordTypePrefix])
    {
        // inValue definitely (with high probability) has a prefix
        return [inValue substringFromIndex:r.location+1];
    }
    else
    { 
        // it doesn't look like a known prefix or we are
        // in raw mode, leave it alone
        return inValue;
    }
}

void printAttribute(NSString *key, NSArray *values, NSString *prefixString)
{
    BOOL hasSpaces;
    int j;
    NSString *value = nil;
    key = stripDSPrefixOffValue(key);
    if(prefixString)
        printf("%s", [prefixString UTF8String]);
    printf("%s:", [key UTF8String]);
    unsigned long cntLimit = [values count];
    hasSpaces = NO;
    for(j = 0; !hasSpaces && j < cntLimit; j++)
    {
        hasSpaces = [[values objectAtIndex:j] rangeOfString:@" "].location != NSNotFound;
    }
    for (j = 0; j < cntLimit; j++)
    {
        value = [values objectAtIndex:j];
        printValue(value, hasSpaces);
    }
    printf("\n");
}

void printPlist(NSString *plistPath, id currentElement)
{
    printf("%s: \n", [plistPath UTF8String]);
    NSData *data = [NSPropertyListSerialization dataFromPropertyList:currentElement format:NSPropertyListXMLFormat_v1_0 errorDescription:nil];
    NSString *dataString = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    printf("%s", [dataString UTF8String]);
    [dataString release];
}

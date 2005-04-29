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

- (tDirStatus) read:(NSArray*)inKeys
{
    NSAutoreleasePool      *pool	= [[NSAutoreleasePool alloc] init];
    NSDictionary		   *attribs;
    id						key;
    NSString               *attrib;
	id						values;
	id						value;
    unsigned long			i		= 0;
	unsigned long			j		= 0;
    
    NS_DURING
    
    if (inKeys == nil || [inKeys count] == 0)
    {
        id keys;
        attribs = [_record getAllAttributesAndValues];
        keys = [[attribs allKeys] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
		unsigned long cnt = [keys count];
        for(i = 0; i < cnt; i++)
        {
            key = [keys objectAtIndex:i];
            values = [attribs objectForKey:key];
            key = [self stripDSPrefixOffValue:key];
            printf("%s:", [key UTF8String]);
			unsigned long cntLimit = [values count];
            for (j = 0; j < cntLimit; j++)
            {
                value = [values objectAtIndex:j];
				printValue(value);
            }
            printf("\n");
        }
    }
    else
    {
        BOOL			success		= NO;
        unsigned long   valueCount  = 0;
		unsigned long   cntLimit	= 0;
        
		cntLimit = [inKeys count];
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
                if (![[[[_record node] directory] standardAttributeTypes] containsObject:attrib])
                    attrib = [@kDSNativeAttrTypePrefix stringByAppendingString:key];
            }
            NS_DURING
                valueCount = [_record getAttributeValueCount:[attrib UTF8String]];
                if (gRawMode)
                    printf("%s:", [attrib UTF8String]);
                else
                    printf("%s:", [key UTF8String]);
                for (j = 1; j <= valueCount; j++)
                {
                    value = [_record getAttribute:[attrib UTF8String] index:j];
                    printValue(value);
                }
                printf("\n");
                success = YES;
            NS_HANDLER
                if (!DS_EXCEPTION_STATUS_IS(eDSAttributeNotFound) &&
                    !DS_EXCEPTION_STATUS_IS(eDSInvalidAttributeType))
                {
                    [localException raise];
                }
            NS_ENDHANDLER    
            if (!success)
                printf("No such key: %s\n", [key UTF8String]);
            else
                success = NO;
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

@end

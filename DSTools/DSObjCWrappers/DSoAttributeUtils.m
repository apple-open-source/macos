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
 * @header DSoAttributeUtils
 */


#import "DSoAttributeUtils.h"

#import "DSoDirectory.h"
#import "DSoNode.h"
#import "DSoBuffer.h"
#import "DSoException.h"

@interface DSoAttributeUtils(PrivateMethods)

+ (id) _getAttributesInNode:(DSoNode*)inNode
                 fromBuffer:(DSoBuffer*)inBuf
              listReference:(tAttributeListRef)inListRef
                      count:(unsigned long)inCount
              includeValues:(BOOL)inIncludeVals
                allowBinary:(BOOL)inAllowBinary;
@end


@implementation DSoAttributeUtils

+ (NSDictionary*)getAttributesAndValuesInNode:(DSoNode*)inNode
                                   fromBuffer:(DSoBuffer*)inBuf
                                listReference:(tAttributeListRef)inListRef
                                        count:(unsigned long)inCount
{
    return [DSoAttributeUtils _getAttributesInNode: inNode
                                        fromBuffer: inBuf 
                                     listReference: inListRef
                                             count: inCount
                                     includeValues: YES
                                       allowBinary: NO];    
}

+ (NSDictionary*)getAttributesAndValuesInNode:(DSoNode*)inNode
                                   fromBuffer:(DSoBuffer*)inBuf
                                listReference:(tAttributeListRef)inListRef
                                        count:(unsigned long)inCount
                                  allowBinary:(BOOL)inAllowBinary
{
    return [DSoAttributeUtils _getAttributesInNode: inNode
                                        fromBuffer: inBuf
                                     listReference: inListRef
                                             count: inCount
                                     includeValues: YES
                                       allowBinary: inAllowBinary];
}

+ (NSArray*)getAttributesInNode:(DSoNode*)inNode
                     fromBuffer:(DSoBuffer*)inBuf
                  listReference:(tAttributeListRef)inListRef
                          count:(unsigned long)inCount
{
    return [DSoAttributeUtils _getAttributesInNode: inNode
                                        fromBuffer: inBuf 
                                     listReference: inListRef
                                             count: inCount
                                     includeValues: NO
                                       allowBinary: NO];
}

+ (NSArray*)getAttributesInNode:(DSoNode*)inNode
                     fromBuffer:(DSoBuffer*)inBuf
                  listReference:(tAttributeListRef)inListRef
                          count:(unsigned long)inCount
                    allowBinary:(BOOL)inAllowBinary
{
    return [DSoAttributeUtils _getAttributesInNode: inNode
                                        fromBuffer: inBuf
                                     listReference: inListRef
                                             count: inCount
                                     includeValues: NO
                                       allowBinary: inAllowBinary];
}

+ (BOOL) _isBinaryAttribute:(char *)inBuffer length:(unsigned long)inLength
{
    BOOL    returnValue = NO;
    
    if( strlen(inBuffer) != inLength ) {
        returnValue = YES;
    }
    
    // we'll add more here if necessary...
    
    return returnValue;
}

+ (id) getAttributeFromBuffer:(tDataBufferPtr)inBufferPtr allowBinary:(BOOL)inAllowBinary
{
    id  returnValue = nil;
    
    if( inBufferPtr->fBufferLength )
    {
        @try
        {
            returnValue = [NSString stringWithUTF8String: inBufferPtr->fBufferData];
            
            // if the lengths don't match, we should it treat as binary
            if( returnValue && strlen([returnValue UTF8String]) != inBufferPtr->fBufferLength ) {
                returnValue = nil;
            }
        } @catch ( NSException *exception ) {
            // must not have been a UTF8 string...
            returnValue = nil;
        }
            
        if( returnValue == nil || [DSoAttributeUtils _isBinaryAttribute: inBufferPtr->fBufferData 
                                                                 length: inBufferPtr->fBufferLength] )
        {
            returnValue = [NSData dataWithBytes: inBufferPtr->fBufferData length: inBufferPtr->fBufferLength];
            if( !inAllowBinary ) {
                NSString    *description = [returnValue description];
                
                // make a string that contains just the hex, not including the '<>'
                returnValue = [description substringWithRange: NSMakeRange(1,[description length]-2)];
            }
        }
    } else {
        returnValue = [NSString string];
    }
    
    return returnValue;
}

+ (id) _getAttributesInNode:(DSoNode*)inNode
                 fromBuffer:(DSoBuffer*)inBuf
              listReference:(tAttributeListRef)inListRef
                      count:(unsigned long)inCount
              includeValues:(BOOL)inIncludeVals
                allowBinary:(BOOL)inAllowBinary
{
    tAttributeValueListRef 	valueRef		= 0;
    tAttributeEntryPtr 		pAttrEntry		= nil;
    tAttributeValueEntryPtr	pValueEntry		= nil;
    tDirStatus 				err				= eDSNoErr;
    char				   *attributeName   = nil;
    NSMutableArray		   *attributeValues = nil;
    id						attributes		= nil;
    unsigned long			i				= 0;
	unsigned long			j				= 0;

    // This is a multi-purpose routine, that might return an NSArray of the attribute names,
    // or an NSDictionary of the attribute names and their values.
    if (inIncludeVals)
        attributes = [[NSMutableDictionary alloc] init];
    else
        attributes = [[NSMutableArray alloc] init];


    for (i = 1; i <= inCount && !err; i++)
    {
        err = dsGetAttributeEntry([inNode dsNodeReference], [inBuf dsDataBuffer],inListRef,
                                  i, &valueRef, &pAttrEntry);
        if (!err)
        {
            attributeName = pAttrEntry->fAttributeSignature.fBufferData;

            // If we should also include the values, then retrieve those and place them into the NSDictionary
            if (inIncludeVals)
            {
                attributeValues = [[NSMutableArray alloc] initWithCapacity:pAttrEntry->fAttributeValueCount];
                for (j =1; j <= pAttrEntry->fAttributeValueCount; j++)
                {
                    err = dsGetAttributeValue([inNode dsNodeReference],[inBuf dsDataBuffer], j, valueRef, &pValueEntry );

                    if (err == eDSNoErr)
                    {
                        id value = [DSoAttributeUtils getAttributeFromBuffer: &pValueEntry->fAttributeValueData allowBinary: inAllowBinary];
                        
                        [attributeValues addObject: value];
                        
                        err = dsDeallocAttributeValueEntry([[inNode directory] dsDirRef], pValueEntry);
                        pValueEntry = nil;
                    }
                    else
                    {
                        break; // we'll release below
                    }
                }
                if (err == eDSNoErr)
                {
                    [attributes setObject: attributeValues forKey:[NSString stringWithUTF8String:attributeName]];
                }
                [attributeValues release];
                attributeValues = nil;
            }
            else // we aren't including the values, we are returning an NSArray of just the attribute names.
            {
                [attributes addObject:[NSString stringWithUTF8String:attributeName]];
            }
            dsCloseAttributeValueList(valueRef);
            dsDeallocAttributeEntry([[inNode directory] dsDirRef], pAttrEntry);
        }			// end if (!err)
    }				// end for (...)

    if (err)
    {
        [attributes release];
        attributes = nil;
        [DSoException raiseWithStatus:err];
    }

    return [attributes autorelease];
}

@end

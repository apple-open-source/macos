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


#import <Foundation/Foundation.h>
#import <DirectoryService/DirectoryService.h>

@class DSoNode, DSoBuffer;

@interface DSoAttributeUtils : NSObject {

}

/*!
    @method     getAttributeFromBuffer:allowBinary:
    @abstract   this method takes a tDataBufferPtr and puts it into NSData or NSString
    @discussion this method will take an existing tDataBufferPtr and determine
                if it is a binary value or an UTF8String.  Depending on the value
                it will return a NSData or an NSString.  If allowBinary is set
                to NO, then it will return Hex encoded value instead.
*/
+ (id) getAttributeFromBuffer:(tDataBufferPtr)inBufferPtr 
                  allowBinary:(BOOL)inAllowBinary DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

+ (NSDictionary*)getAttributesAndValuesInNode:(DSoNode*)inNode
                                   fromBuffer:(DSoBuffer*)inBuf
                                listReference:(tAttributeListRef)inListRef
                                        count:(unsigned long)inCount DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

+ (NSDictionary*)getAttributesAndValuesInNode:(DSoNode*)inNode
                                   fromBuffer:(DSoBuffer*)inBuf
                                listReference:(tAttributeListRef)inListRef
                                        count:(unsigned long)inCount
                                  allowBinary:(BOOL)inAllowBinary DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

+ (NSArray*)getAttributesInNode:(DSoNode*)inNode
                     fromBuffer:(DSoBuffer*)inBuf
                  listReference:(tAttributeListRef)inListRef
                          count:(unsigned long)inCount DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

+ (NSArray*)getAttributesInNode:(DSoNode*)inNode
                     fromBuffer:(DSoBuffer*)inBuf
                  listReference:(tAttributeListRef)inListRef
                          count:(unsigned long)inCount
                    allowBinary:(BOOL)inAllowBinary DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

@end

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
 * @header DSoDataNode
 */


#import <Foundation/Foundation.h>
#import <DirectoryService/DirectoryService.h>

#import "DSoDirectory.h"

/*!
 * @class DSoDataNode 
 * 		This class is a wrapper object for a tDataNode data type
 *		in the Directory Services framework.
 *
 *		While it is perfectly OK to use this class, it is primarily
 *		intended to be only used by the other classes of this framework.
 *		It is the intention that the classes for high-level data objects
 *		(Nodes, users, records, etc.)
 *		provide a sufficient Objective-C style interface to the necessary data
 *		and functionality, that the lower-level data types such as this one.
 *		can be avoided in favor of such types as NSArrays, NSStrings, and NSDictionarys.
 */

@interface DSoDataNode : NSObject {
    DSoDirectory	   *mDir;
    tDataNodePtr		mNode;
}

/*!
 * @method initWithDir:bufferSize:dataLength:data:
 * @abstract Initialize a data node with raw data.
 * @discussion This will initialize the data node with a character
 *		array of data and specified buffer size and length.
 * @param inDir The DS process object to initialize with.
 * @param inBufSize The size to use for the data node's buffer.
 * @param inDataLength The length of the data to be stored in the data node.
 * @param inData A pointer to an array of characters containing the data to be stored.
 */
- (DSoDataNode*)initWithDir:(DSoDirectory*)inDir bufferSize:(unsigned long)inBufSize dataLength:(unsigned long)inDataLength data:(const void*)inData  DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method initWithDir:value:
 * @abstract Initialize a data node with a value.
 * @discussion This will initialize the data node with the
 *		contents of the given NSString or NSData, with both the data length
 *		and buffer size set to the length of that object.
 * @param inDir The DS process object to initialize with.
 * @param inValue The NSString or NSData style value to use for the data.
 */
- (DSoDataNode*)initWithDir:(DSoDirectory*)inDir value:(id)inValue DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method initWithDir:string:
 * @abstract Initialize a data node with a string.
 * @discussion This will initialize the data node with the
 *		contents of the given NSString, with both the data length
 *		and buffer size set to the length of that string.
 * @param inDir The DS process object to initialize with.
 * @param inString The NSString style string to use for the data.
 */
- (DSoDataNode*)initWithDir:(DSoDirectory*)inDir string:(NSString*)inString DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method initWithDir:cString:
 * @abstract Initialize a data node with a C-string.
 * @discussion This will initialize the data node with the
 *		contents of a standard C-style, null-terminated string
 *		(character array), with both the data length
 *		and buffer size set to the length of that string.
 * @param inDir The DS process object to initialize with.
 * @param inString The null terminated C-style string to use for the data.
 */
- (DSoDataNode*)initWithDir:(DSoDirectory*)inDir cString:(const char*)inString DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method initWithDir:dsDataNode:
 * @abstract Initialize with a tDataNode object.
 * @discussion This will initialize the object with an existing
 *		tDataNode variable.  It stores the pointer to the variable
 *		and the contents of the variable is freed when this object is deallocated.
 * @param inDir The DS process object to initialize with.
 * @param inNode A pointer to the tDataNode variable.
 */
- (DSoDataNode*)initWithDir:(DSoDirectory*)inDir dsDataNode:(tDataNode*)inNode DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method initWithDir:copyOfDsDataNode:
 * @abstract  Initialize with a copy of a tDataNode object.
 * @discussion This will initialize the object with a copy of an existing
 *		tDataNode variable. Only the copied variable is freed
 *		when this object is deallocated.
 * @param inDir The DS process object to initialize with.
 * @param inNode A pointer to the tDataNode variable.
*/
- (DSoDataNode*)initWithDir:(DSoDirectory*)inDir copyOfDsDataNode:(tDataNode*)inNode DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

//  accessors.

/*!
 * @method getBufferSize
 * @abstract Get the size of the data node's buffer.
 */
- (unsigned long) getBufferSize DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method getDataLength
 * @abstract Get the length of valid data in the data node's buffer.
 */
- (unsigned long) getDataLength DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method setDataLength:
 * @abstract Sets the length of valid data in the data node's buffer.
 * @param inLength The new length to use for the data node's buffer.
 */
- (void) setDataLength:(unsigned long)inLength DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method dsDataNode
 * @abstract Method for accessing the low-level data type.
 * @result A pointer to the internally wrapped tDataNode variable.
 */
- (tDataNodePtr) dsDataNode DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

@end

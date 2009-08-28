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
 * @header DSoBuffer
 */


#import <Foundation/Foundation.h>
#import <DirectoryService/DirectoryService.h>

@class DSoDirectory;

enum { kDefaultBufferSize = 128 } ;

/*!
 * @class DSoBuffer 
 * 		This class is a wrapper object for a tDataBuffer data type
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

@interface DSoBuffer : NSObject {
	DSoDirectory	   *mDir ;
	tDataBufferPtr		mBuffer ;
}

/*!
 * @method initWithDir:
 * @abstract Initialize with respect to a certain DS process reference.
 * @param inDir The DS object in which to initialize this buffer.
 */
- (DSoBuffer*)initWithDir:(DSoDirectory*)inDir DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method initWithDir:
 * @abstract Initialize with respect toa  certain DS process reference,
 * @discussion This method allows an initial buffer size to be specified.
 *		This is the most commonly used initialization method.
 * @param inDir The DS object in which to initialize this buffer.
 */
- (DSoBuffer*)initWithDir:(DSoDirectory*)inDir bufferSize:(unsigned long)inBufferSize DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method getBufferSize
 * @abstract Get the maximum size, in bytes, of the buffer.
 */
- (unsigned long) getBufferSize DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method getDataLength
 * @abstract Get the actual length of data used, in bytes.
 */
- (unsigned long) getDataLength DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method setData:length:
 * @abstract Set the data contents.
 * @discussion Sets the data content when provided with
 *		a character array of data and the length of this data.
 * @param inData A pointer to a character array of data.
 * @param inLength The number of bytes to read from the data pointer.
 */ 
- (void)setData:(const void*)inData length:(unsigned long)inLength DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method appendData:length:
 * @abstract Append to the data contents.
 * @discussion Appends to the data content when provided with
 *		a character array of data and the length of this data.
 * @param inData A pointer to a character array of data.
 * @param inLength The number of bytes to read from the data pointer.
 */ 
- (void)appendData:(const void*)inData length:(unsigned long)inLength DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method setDataLength:
 * @abstract Set an arbitrary length for the data length.
 * @param inLength The new length to be considered as the length of data.
 */
- (void)setDataLength:(unsigned long)inLength DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method dsDataBuffer
 * @abstract Method for accessing the low-level data type.
 * @result A pointer to the internally wrapped tDataBuffer variable.
 */
- (tDataBufferPtr)dsDataBuffer DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method grow:
 * @abstract Increase the buffer size.
 * @discussion Causes the buffer size to grow to the specified
 *		size, preserving existing buffer data and length.
 * @param inNewSize The new size in bytes for the buffer.
 * @result Returns a pointer to the internal buffer struct
 *		simply as a convenience for checking that the operation
 *		succeeded.  It should generally be ignored.
 */
- (tDataBufferPtr) grow:(unsigned long)inNewSize DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;


@end

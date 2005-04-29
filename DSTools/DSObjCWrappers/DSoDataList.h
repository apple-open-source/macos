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
 * @header DSoDataList
 */


#import <Foundation/Foundation.h>
#import <DirectoryService/DirectoryService.h>

#import "DSoDirectory.h"

@class DSoDataNode;

/*!
 * @class DSoDataList 
 * 		This class is a wrapper object for a tDataList data type
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

@interface DSoDataList : NSObject {
    DSoDirectory	   *mDir;
    tDataList          mList;
}

/*!
* @method initWithDir:value:
 * @abstract Initialize a data list with a single value.
 * @discussion This will initialize the data list with one item: the
 *		contents of the given NSString or NSData.
 * @param inDir The DS process object to initialize with.
 * @param inValue The NSString or NSData to use for the single data item.
 */
- (DSoDataList*)initWithDir:(DSoDirectory*)inDir value:(id)inValue;

/*!
 * @method initWithDir:string:
 * @abstract Initialize a data list with a single string.
 * @discussion This will initialize the data list with one item: the
 *		contents of the given NSString.
 * @param inDir The DS process object to initialize with.
 * @param inString The NSString style string to use for the single data item.
 */
- (DSoDataList*)initWithDir:(DSoDirectory*)inDir string:(NSString*)inString;

/*!
 * @method      initWithDir:values:
 * @abstract    Initialize a data list with an array of values.
 * @discussion  This will initialize the data list with a list of strings
                or data in the specified array.
 * @param inDir The DS process object to initialize with.
 * @param inStrings The NSArray of NSString or NSData values to use for the list of data items.
 */
- (DSoDataList*)initWithDir:(DSoDirectory*)inDir values:(NSArray*)inValues;

/*!
 * @method initWithDir:strings:
 * @abstract Initialize a data list with an array of strings.
 * @discussion This will initialize the data list with a list of strings
 		in the specified array.
 * @param inDir The DS process object to initialize with.
 * @param inStrings The NSArray of NSString style strings to use for the list of data items.
 */
- (DSoDataList*)initWithDir:(DSoDirectory*)inDir strings:(NSArray*)inStrings;

/*!
 * @method initWithDir:cString:
 * @abstract Initialize a data list with a single c-string.
 * @discussion This will initialize the data list with one item: the
 *		contents of the given null terminated, c-style string.
 * @param inDir The DS process object to initialize with.
 * @param inString The null terminated C-style string to use for the single data item.
 */
- (DSoDataList*)initWithDir:(DSoDirectory*)inDir cString:(const char*)inString;

/*!
 * @method initWithDir:cStrings:...
 * @abstract Initialize a data list with a list of c-strings.
 * @discussion This will initialize the data list with the
 *		contents of the given list of null terminated, c-style strings.
 * @param inDir The DS process object to initialize with.
 * @param inString The list of strings to use for the data items, terminating with a nil.
 */
- (DSoDataList*)initWithDir:(DSoDirectory*)inDir cStrings:(const char	*)inString, ...;

/*!
 * @method initWithDir:separator:pattern:
 * @abstract Initialize a data list by separating a string.
 * @discussion This will build the data list from the pieces of a string
 *		separated by a specified character.
 * @param inDir The DS process object to initialize with.
 * @param inSep The character on which to separate the string.
 * @param inPattern The string to be separated into components.
 */
- (DSoDataList*)initWithDir:(DSoDirectory*)inDir separator:(char)inSep pattern:(NSString*)inPattern;

/*!
 * @method initWithDataList:
 * @abstract Initialize with a copy of another data list.
 * @discussion This method will initialize the list with copies
 *		 of the contents of the given data list.
 * @param inOrg The original data list to copy.
 */
- (DSoDataList*)initWithDataList:(DSoDataList*)inOrg;

/*!
 * @method initWithDir:dsDataList:
 * @abstract Initialize with an existing tDataList object.
 * @discussion This method will initialize the object by wrapping
 *		an existing tDataList variable.  A pointer to that variable is maintained,
 *		and the contents of the variable is freed when this object is deallocated.
 * @param inDir The DS process object to initialize with.
 * @param inList A pointer to the tDataList variable to use.
 */
- (DSoDataList*)initWithDir:(DSoDirectory*)inDir dsDataList:(tDataListPtr)inList;

/*!
 * @method getcount
 * @abstract Get the number of items in the list.
 */
- (unsigned long)getCount;

/*!
 * @method getDataLength
 * @abstract Get the total length of data used in the data list.
 */
- (unsigned long)getDataLength;

/*!
 * @method objectAtIndex:
 * @abstract Get a specific data node from the list by index.
 * @discussion This method returns a Data Node object wrapping the tDataNode
 *		data that is the inIndex'th item in this data list.
 * @param inIndex The index of the data node to retrieve from the list.  The first item has an index of '1'
 * @result A DSoDataNode object wrapping the specified tDataNode item in the data list.
 */
- (DSoDataNode*)objectAtIndex:(unsigned long)inIndex;

/*!
 * @method append
 * @abstract Append a value to the list.
 * @discussion A convenience method for appending a data node
 *		to the list whose contents are that of the specified NSData or NSString.
 * @param inString A NSString or NSData containing the data to be appended to the list.
 */
- (void)append:(id)inValue;

/*!
 * @method dsDataList
 * @abstract Method for accessing the low-level data type.
 * @result A pointer to the internally wrapped tDataList variable.
 */
- (tDataListPtr)dsDataList;

@end

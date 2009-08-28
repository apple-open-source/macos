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
 * @header DSoRecord
 */


#import <Foundation/Foundation.h>
#import <DirectoryService/DirectoryService.h>

@class DSoDirectory, DSoNode, DSoDataNode, DSoBuffer;

/*!
 * @class DSoRecord
 * @abstract Class representing a Directory Services node record.
 * @discussion This class provides a wrapper around the DS record reference.
 *		It provides useful functions for retrieving information about the record.
 */

@interface DSoRecord : NSObject {
	DSoNode			   *mParent ;
	tRecordReference	mRecRef ;
	NSString		   *mName ;
    NSString		   *mType;
    // Grab a pointer to the node's associated Directory object for convenience,
    // but it won't be  retained; we retain the node, which is already retaining the directory
    DSoDirectory	   *mDirectory;
}
	/**** Instance methods. ****/

/*!
 * @method initInNode:recordRef:type:
 * @abstract Initialize object with an existing record reference and a type string.
 * @discussion This should never be invoked directly.  Record objects should
 *		be retrieved with the methods for doing so that DSoNode provides.
 * @param inParent The Node object that contains this record.
 * @param inRecRef The DS record reference number referring to this record.
 * @param inType The DS string constant for type of this record.
 */
- (DSoRecord*)initInNode:(DSoNode*)inParent recordRef:(tRecordReference)inRecRef type:(const char*)inType DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method initInNode:type:name:
 * @abstract Initialize object with a type string and record name.
 * @discussion This should never be invoked directly.  Record objects should
 *		be created with the methods for doing so that DSoNode provides.
 * 		This method will create a brand new object to be inserted
 *		into the specified node as the specified type with the specified name.
 * @param inParent The Node object that will contain this record.
 * @param inType The DS string constant for the type of this record.
 * @param inName The record name for this record.
 */
- (DSoRecord*)initInNode:(DSoNode*)inParent type:(const char*)inType name:(NSString*)inName DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

	// simple accessors.
/*!
 * @method getName
 * @abstract Retrieve the record's name.
 */
- (NSString*)getName DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method getType
 * @abstract Retrieve the record's DS type string constant.
 */
- (const char*)getType DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method node
 * @abstract Retrieve a pointer to the Record's container node.
 */
- (DSoNode*)node DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

	// Calculation methods.

/*!
 * @method attributeCount
 * @abstract Retrieve the number of attribute types this record has.
 */
- (unsigned long)attributeCount DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;
//- (NSArray*)getAttributeTypes;

/*!
 * @method getAllAttributes
 * @abstract Retrieve a list of the attribute types this record has.
 * @result An array of NSString objects whose values are the names of the attribute types.
 */
- (NSArray*)getAllAttributes DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method getAllAttributesAndValues
 * @abstract Retrieve a dictionary of all the attributes and values for this record.
 * @result An NSDictionary whose keys are the attribute type names
 *		and whose values are NSArray objects containing a list of the values
 *		for that attribute type.
 */
- (NSDictionary*)getAllAttributesAndValues DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method getAttributes:
 * @abstract Retrieve a dictionary of the specified attributes and values for this record.
 * @result An NSDictionary whose keys are the attribute type names
 *		and whose values are NSArray objects containing a list of the values
 *		for that attribute type.
 */
- (NSDictionary*)getAttributes:(NSArray*)inAttributes DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method getAttribute:
 * @abstract Get the first value of a specific attribute type.
 * @discussion Record attributes, or attribute types, can contain 
 *		multiple values.
 *		This method will return the first value of the specified attribute.
 * @result An NSString containing the first value of the given attribute type.
 */
- (NSString*)getAttribute:(const char*)inAttributeType DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

- (id)getAttribute:(const char*)inAttributeType allowBinary:(BOOL)inAllowBinary DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method getAttribute:index:
 * @abstract Get a particular value of a specific attribute type.
 * @discussion Record attributes, or attribute types, can contain 
 *		multiple values.
 *		This method will return a specific value as identified
 *		by its (1-based) index in the list of values.
 * @param inAttributeType The DS constant for a known attribute type or the string for an unknown type.
 * @param inIndex The index of the value desired.  The first value has an index of '1'.
 * @result An NSString containing the first value of the given attribute type.
 */
- (NSString*)getAttribute:(const char*)inAttributeType index:(unsigned long)inIndex DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

- (id)getAttribute:(const char*)inAttributeType index:(unsigned long)inIndex allowBinary:(BOOL)inAllowBinary DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method getAttribute:range:
 * @abstract Get a particular value of a specific attribute type.
 * @discussion Record attributes, or attribute types, can contain 
 *		multiple values.
 *		This method will return a list of values in the specified range.
 * @param inAttributeType The DS constant for a known attribute type or the string for an unknown type.
 * @param inRange An NSRange values specifying the desired range. The first value has a location of 1. 
 * @result An array of NSString objects containing the values of the desired attribute.
 */
- (NSArray*)getAttribute:(const char*)inAttributeType range:(NSRange)inRange DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

- (NSArray*)getAttribute:(const char*)inAttributeType range:(NSRange)inRange allowBinary:(BOOL)inAllowBinary DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
* @method getAttributeValueCount:
* @abstract Get the number of values for an attribute type.
* @discussion Returns the number of values in the record for the
*		specified attribute type.
* @param inAttributeType The DS constant for a known attribute type or the string for an unknown type.
* @result The number of values.
*/
- (unsigned long)getAttributeValueCount:(const char*)inAttributeType DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method setAttribute:value:
 * @abstract Create an attribute and give it a single value.
 * @discussion Invokes setAttribute:values: with values set to an
 *		single valued array containing inAttributeValue.
 * @param inAttributeType The DS constant for a known attribute type or the string for an unknown type.
 * @param inAttributeValue An NSString or NSData object containing the value of the new attribute.
 */
- (void)setAttribute:(const char*)inAttributeType value:(id)inAttributeValue DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method setAttribute:values:
 * @abstract Create an attribute and set its values.
 * @discussion This method will create an attribute of the given
 *		attribute type with values of those contained in the specified array.
 *
 *		If this attribute already exists, its values get overwritten by the new values.
 * @param inAttributeType The DS constant for a known attribute type or the string for an unknown type.
 * @param inAttributeValues An array of NSString or NSData objects containing the values of the new attribute.
 */
- (void)setAttribute:(const char*)inAttributeType values:(NSArray*)inAttributeValues DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method addAttribute:values:
 * @abstract Add values to an existing attribute, merging values.
 * @discussion Invokes addAttribute:values:mergeValues: with mergeValues set to YES.
 */
- (void)addAttribute:(const char*)inAttributeType values:(NSArray*)inAttributeValues DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method addAttribute:values:mergeValues:
 * @abstract Add values to an existing attribute, merging values.
 * @discussion This will add a values to an existing attribute type.  If the
 *		provided attribute and value already exist, then this method
 *		will not add it again if mergVals is set to YES, otherwise it
 *		will add the value a second time if mergVals is set to NO.
 * @param inAttributeType The DS constant for a known attribute type.
 * @param inAttributeValues An array of NSString objects containing the values to add.
 * @param mergVals NO or YES depending on if you want to add the value again or not.
 */
- (void)addAttribute:(const char*)inAttributeType values:(NSArray*)inAttributeValues mergeValues:(BOOL)mergVals DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method changeAttribute:oldValue:newValue:
 * @abstract Change an existing attribute value to a new value.
 * @discussion This method will look for an existing attribute value
 *		and replace that value with a new value.
 * @param inAttributeType The DS constant for a known attribute type.
 * @param inAttrValue The existing old value to be changed.
 * @param inNewAttrValue The new value to replace the old value.
 */
- (void)changeAttribute:(const char*)inAttributeType oldValue:(NSString*)inAttrValue newValue:(id)inNewAttrValue DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method changeAttribute:index:newValue:
 * @abstract Change an existing attribute value to a new value.
 * @discussion This method will replace an existing attribute value
 *		as referenced by its 1-based index, and replace it with a new value.
 * @param inAttributeType The DS constant for a known attribute type.
 * @param inIndex The index of the old value to change The first value has an index of '1'
 * @param inNewAttrValue The new value to replace the old value.
 */
- (void)changeAttribute:(const char*)inAttributeType index:(unsigned int)inIndex newValue:(id)inNewAttrValue DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method attributeExists:withValue:
 * @abstract Determines whether an attribute value exists.
 * @discussion Searches the record for the given record type and
 *		determines whether it contains the given value.
 * @param inAttributeType The DS constant for a known attribute type.
 * @param inAttributeValue The value to search for.
 * @result YES if the value exists for this attribute, NO otherwise.
 */
- (BOOL)attributeExists:(const char*)inAttributeType withValue:(id)inAttributeValue DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method removeAttribute:
 * @abstract Remove an attribute type and its values.
 * @discussion Removes an attribute (type) and all its values.
 * @param inAttributetype The DS constant for a known attribute type in the record.
 */
- (void)removeAttribute:(const char*)inAttributeType DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method removeAttribute:value:
 * @abstract Remove a value from an attribute type by value.
 * @discussion Removes an existing value of the specified
 *		attribute type.
 * @param inAttributetype The DS constant for a known attribute type in the record.
 * @param inAttributeValue The value to remove NSData or NSString.
 */
- (void)removeAttribute:(const char*)inAttributeType value:(id)inAttributeValue DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method removeAttribute:values:
 * @abstract Remove a list of values from an attribute type by value names.
 * @discussion Removes existing values of the specified
 *		attribute type as identified by the values themselves.
 * @param inAttributetype The DS constant for a known attribute type in the record.
 * @param inAttributeValues Array of NSStrings or NSDatas with the values to remove.
 */
- (void)removeAttribute:(const char*)inAttributeType values:(NSArray*)inAttributeValues DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method removeAttribute:index:
 * @abstract Remove a value from an attribute type by index.
 * @discussion Removes an existing value as identified by its index
 *		of the specified attribute type.
 * @param inAttributetype The DS constant for a known attribute type in the record.
 * @param inIndex The index of the value to remove.
 */
- (void)removeAttribute:(const char*)inAttributeType index:(unsigned int)inIndex DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method removeRecord
 * @abstract Remove this record from the node.
 * @discussion This method will immediately remove the record this object
 *		represents from the Directory Services node that it belongs to.
 *		After calling this method, this object should be sent a "release" message
 *		since any further operations on the object will result in Exceptions
 *		being raised, or unpredictable results.
 */
- (void)removeRecord DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

    // Other methods.
/*!
 * @method dsRecordReference
 * @abstract Method for accessing the low-level data type.
 * @result The DS record reference number for this record.
 */
- (tRecordReference)dsRecordReference DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

@end

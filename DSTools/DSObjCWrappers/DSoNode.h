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
 * @header DSoNode
 */


#import <Foundation/Foundation.h>
#import <DirectoryService/DirectoryService.h>

#define kDSOAttrRecordType @"dsoAttrTypeWrapper:RecordType"

@class DSoDirectory, DSoRecord, DSoGroup, DSoUser;

/*!
 * @class DSoNode
 * @abstract Class representing a Directory Services Node. (tDirNodeReference)
 * @discussion This class provides a representation of the Directory Services node.
 * 		It provides useful operations required of a node as well
 * 		as providing attribute information about the node.
 *
 *		Note: In methods where a Record Type is required, the type is a C String
 *		as defined by constants in &lt;DirectoryServices/DirServicesConst.h&gt;
 */
 
@interface DSoNode : NSObject {
    DSoDirectory	   *mDirectory;
	NSString*			mNodeName ;	// canonical, from DS.
	BOOL				mIsAuthenticated;
	BOOL				mSupportsSetAttributeValues;
    tDirNodeReference   mNodeRef;
    DSoGroup		   *mAdminGroup;  // legacy code cached this group, so we will for now as well
    NSLock			   *mCacheLock;
    NSArray			   *mTypeList;
}

/**** Instance methods. ****/

/*!
 * @method getAttributeFirstValue:
 * @abstract Retrieve the first value of an attribute type.
 * @discussion Convenince method for retrieving the first (and usually only)
 *       value of the requested attribute type.
 *
 *       Invokes getAttribute: and grabs the first value of the array it returns.
 * @param inAttributeType The DS attribute type constant to get values for.
 */
- (NSString*)			getAttributeFirstValue:(const char*)inAttributeType DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method      getAttributeFirstValue:allowBinary:
 * @abstract    Retrieve the first value of an attribute type, allowing binary or only text
 * @discussion  Convenience method to retrieve the first (and usually only) value of
                the requested attribute type.  If allowBinary is set, it will return an
                NSData, otherwise an NSString is returned. If no value is available it will
                return nil.
 */

- (id) getAttributeFirstValue:(const char *)inAttributeType allowBinary:(BOOL)inAllowBinary DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method getAttribute:
 * @abstract Retrieve a list of the attribute values of the given attribute type.
 * @param inAttributeType The DS attribute type constant to get values for.
 * @result An array of NSStrings with the values of the requested attribute type.
 */
- (NSArray*)			getAttribute:(const char*)inAttributeType DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method      getAttribute:allowBinary:
 * @abstract    Retrieve a list of the attribute values of the given attribute type.
 * @param       inAttributeType The DS attribute type constant to get values for.
                inAllowBinary Whether or not to return binary NSData or just NSStrings
 * @result      An array of NSStrings or NSData with the values of the requested attribute type.
 */
- (NSArray*)            getAttribute:(const char*)inAttributeType allowBinary:(BOOL)inAllowBinary DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*! 
 * @method getAllAttributes
 * @abstract Retrieve the attribute types and values for this node.
 * @discussion The keys of the dictionary are NSStrings matching the 
        DS attribute type constants and the values of the dictionary
        are NSStrings of the corresponding values of that attribute type.
 */
- (NSDictionary*)		getAllAttributes DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method      getAllAttributesAllowingBinary:
 * @abstract    Retrieve the attribute types and values for this node,
                allowing binary data.
 * @discussion  The keys of the dictionary are NSStrings matching the 
                DS attribute type constants and the values of the dictionary
                are NSStrings or NSData (if data is binary) of the corresponding
                values of that attribute type.
 */

- (NSDictionary*)		getAllAttributesAllowingBinary:(BOOL)allowBinary DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method getName
 * @abstract The Directory Services name of this node.
 */
- (NSString*)		getName DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method findRecordTypes
 * @abstract Retrieve a list of the record types this node contains.
 * @result An array of NSStrings whose values are equal to the C string, DS Record type constants.
 */
- (NSArray*)		findRecordTypes DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method hasRecordsOfType:
 * @abstract Determine if the node contains any records of the specified type.
 * @param inType The type of record to check for.
 * @result YES if there exists at least 1 record of the specified type.
 */
- (BOOL)	hasRecordsOfType:(const char*)inType DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method findRecordNames:andAttributes:ofType:matchType:
 * @abstract Retrieve a matching list of record names.
 * @discussion Searches the node for records whose names match the specified parameters
 	and returns a list of the matching record names (not full records).
 	Returns an empty array if no matching results are found.
 *
 *		All parameters except for inAttributes must be non-null.
 * @param inName The name (or name substring) to search for.
 * @param inAttributes List of attribute types to return the values for.
 * @param inType The type of record to search for.
 * @param inMatchType The type of patterm matching to use (tDirPatternMatch).
 * @result An NSArray of NSDictionarys whose keys are the attribute type name and whose values
 *		are a an NSArray of attribute values.  It will always include the attribute kDSNAttrRecordName.
 */
- (NSArray*)		findRecordNames:(NSString*)inName andAttributes:(NSArray*)inAttributes ofType:(const char*)inType matchType:(tDirPatternMatch)inMatchType DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method findRecordNames:ofType:matchType:
 * @abstract Retrieve a matching list of record names.
 * @discussion Searches the node for records whose names match the specified parameters
 *      and returns a list of the matching record names (not full records).
 * 		Returns an empty array if no matching results are found.
 *		All parameters must be non-null. This method just calls
 *		-findRecordNames:andAttributes:ofType:matchType:
 *		with inAttributes set to nil.
 * @param inName The name (or name substring) to search for.
 * @param inType The type of record to search for.
 * @param inMatchType The type of patterm matching to use (tDirPatternMatch).
 * @result An NSArray of NSStrings whose values are the names of the matching records.
 */
- (NSArray*)		findRecordNames:(NSString*)inName ofType:(const char*)inType matchType:(tDirPatternMatch)inMatchType DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method findRecordNamesOfTypes:withAttribute:value:matchType:
 * @abstract Retrieve a list of record names with matching attribute values.
 * @discussion Searches the node for records of the specified record types
 *		that have the specified attribute with the specified value.
 * @param inTypes Array of NSStrings of the record types to search for.
 * @param inAttrib The attribute type constant to search on.
 * @param inValue The attribute value NSData or NSString to search for.
 * @param inMatchType The type of pattern matching to use for the search.
 * @result An NSArray of NSStrings whose values are the names of the matching records.
 */
- (NSArray*)		findRecordNamesOfTypes:(NSArray*)inTypes withAttribute:(const char*)inAttrib value:(id)inValue matchType:(tDirPatternMatch)inMatchType DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method findRecordsOfTypes:withAttribute:value:matchType:
 * @abstract Retrieve a list of records with matching attribute values.
 * @discussion Searches the node for records of the specified record types
 *		that have the specified attribute with the specified value.
 * @param inTypes Array of NSStrings of the record types to search for.
 * @param inAttrib The attribute type constant to search on.
 * @param inValue The attribute value NSData or NSString to search for.
 * @param inMatchType The type of pattern matching to use for the search.
 * @result An Array of NSDictionarys whose keys are the attribute types of the records
 *			and the values are NSArrays of the attribute values.
 */
- (NSArray*)		findRecordsOfTypes:(NSArray*)inTypes withAttribute:(const char*)inAttrib value:(id)inValue matchType:(tDirPatternMatch)inMatchType DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method findRecordsOfTypes:withAttribute:value:matchType:allowBinary:
 * @abstract Retrieve a list of records with matching attribute values.
 * @discussion Searches the node for records of the specified record types
 *		that have the specified attribute with the specified value.
 * @param inTypes Array of NSStrings of the record types to search for.
 * @param inAttrib The attribute type constant to search on.
 * @param inValue The attribute value NSData or NSString to search for.
 * @param inMatchType The type of pattern matching to use for the search.
 * @param inAllowBinary Boolean determines if call will return NSData (YES) or NSString (NO) 
 * @result An Array of NSDictionarys whose keys are the attribute types of the records
 *			and the values are NSArrays of the attribute values.
 */
- (NSArray*)		findRecordsOfTypes:(NSArray*)inTypes withAttribute:(const char*)inAttrib value:(id)inValue matchType:(tDirPatternMatch)inMatchType allowBinary:(BOOL)inAllowBinary DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method findRecordsOfTypes:withAttribute:value:matchType:retrieveAttributes:
 * @abstract Retrieve a list of records with matching attribute values.
 * @discussion Searches the node for records of the specified record types
 *		that have the specified attribute with the specified value.
 *		Only retrieve the specified list of attribute types.
 * @param inTypes Array of NSStrings of the record types to search for.
 * @param inAttrib The attribute type constant to search on.
 * @param inValue The attribute value of type NSData or NSString to search for.
 * @param inMatchType The type of pattern matching to use for the search.
 * @param inAttribsToRetrieve Array of NSStrings of the attribute types to return in the result.
 * @result An Array of NSDictionarys whose keys are the requested attribute types
 *			of the records and the values are NSArrays of the attribute values.
 */
- (NSArray*)		findRecordsOfTypes:(NSArray*)inTypes withAttribute:(const char*)inAttrib value:(id)inValue matchType:(tDirPatternMatch)inMatchType retrieveAttributes:(NSArray*)inAttribsToRetrieve DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method findRecordsOfTypes:withAttribute:value:matchType:retrieveAttributes:allowBinary:
 * @abstract Retrieve a list of records with matching attribute values.
 * @discussion Searches the node for records of the specified record types
 *		that have the specified attribute with the specified value.
 *		Only retrieve the specified list of attribute types.
 * @param inTypes Array of NSStrings of the record types to search for.
 * @param inAttrib The attribute type constant to search on.
 * @param inValue The attribute value of type NSData or NSString to search for.
 * @param inMatchType The type of pattern matching to use for the search.
 * @param inAttribsToRetrieve Array of NSStrings of the attribute types to return in the result.
 * @param inAllowBinary Boolean determines if call will return NSData (YES) or NSString (NO)
 * @result An Array of NSDictionarys whose keys are the requested attribute types
 *			of the records and the values are NSArrays of the attribute values.
 */
- (NSArray*)		findRecordsOfTypes:(NSArray*)inTypes withAttribute:(const char*)inAttrib value:(id)inValue matchType:(tDirPatternMatch)inMatchType retrieveAttributes:(NSArray*)inAttribsToRetrieve allowBinary:(BOOL)inAllowBinary DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method findRecord:ofType:
 * @abstract Find a record in the node.
 * @discussion Retrieves a record object for a record in the node
 * 		matching the specified name and type.
 * @param inName The name of the record.
 * @param inType The type of the record.
 */
- (DSoRecord*)		findRecord:(NSString*)inName ofType:(const char*)inType DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method findUser:
 * @abstract Find a user in the node.
 * @discussion Invokes findRecord:ofType: with ofType set to kDSStdRecordTypeUsers.
 * @param inName The record name of the user.
 */
- (DSoUser*)		findUser:(NSString*)inName DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;
/*!
 * @method findGroup:
 * @abstract Find a group in the node.
 * @discussion Invokes findRecord:ofType: with ofType set to kDSStdRecordTypeGroups.
 * @param inName The record name of the group.
 */
- (DSoGroup*)		findGroup:(NSString*)inName DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method newRecord:ofType:
 * @abstract Create a new record in the node.
 * @param inName The name of the new record.
 * @param inType The type of the record.
 */
- (DSoRecord*)		newRecord:(NSString*)inName ofType:(const char*)inType DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method authenticateName:withPassword:
 * @abstract Check a user's password for validity.
 * @discussion Invokes authenticateName:withPassword:authOnly:
 *		with authOnly set to YES.
 */
- (tDirStatus)		authenticateName:(NSString*)inName
                      withPassword: (NSString*)inPasswd DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method authenticateName:withPassword:authOnly:
 * @abstract Authenticates a user in this node.
 * @discussion Will attempt to authenticate a user with this node.
        It optionally will persistently authenticate the user to this node
        for future operations.
		Simply invokes authenticateWithBufferItems:authType:authOnly:
 		with the name and password as the buffer items, and kDSStdAuthNodeNativeClearTextOK
 		kDSStdAuthNodeNativeClearTextOK as the auth type.
 * @param inName The username.
 * @param inPasswd The password to try.
 * @param inAuthOnly Specify whether the password should just be checked (YES) or
        whether the node should be authenticated to.
 */
- (tDirStatus)		authenticateName:(NSString*)inName
                      withPassword: (NSString*)inPasswd
                      authOnly: (BOOL)inAuthOnly DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method authenticateWithBufferItems:authType:authOnly:
 * @abstract Authenticate a user to the node
 * @discussion Generic method to authenticate a user to the node.  The item
 *		in buffer will vary according to the authentication type that is
 *		passed in.  Basically this is slightly higher layer of dsDoDirNodeAuth().
 * @param inBufferItems An array of items to pack into the buffer for dsDoDirNodeAuth().
 * @param inAuthType The DS authentication type to be used (must be supported by the node).
 * @param inAuthOnly Specify whether the password should just be checked (YES) or
 *		whether the node should be authenticated too.
 */
- (tDirStatus) authenticateWithBufferItems: (NSArray*)inBufferItems
								  authType: (const char*)inAuthType
								  authOnly: (BOOL)inAuthOnly DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method authenticateWithBufferItems:authType:authOnly:responseBufferItems:
 * @abstract Authenticate a user to the node
 * @discussion Generic method to authenticate a user to the node.  The item
 *		in buffer will vary according to the authentication type that is
 *		passed in.  Basically this is slightly higher layer of dsDoDirNodeAuth().
 * @param inBufferItems An array of items to pack into the buffer for dsDoDirNodeAuth().
 * @param inAuthType The DS authentication type to be used (must be supported by the node).
 * @param inAuthOnly Specify whether the password should just be checked (YES) or
 *		whether the node should be authenticated too.
 * @param outBufferItems An array of items returned from the buffer for dsDoDirNodeAuth().
 */
- (tDirStatus) authenticateWithBufferItems: (NSArray*)inBufferItems
                                  authType: (const char*)inAuthType
                                  authOnly: (BOOL)inAuthOnly
					   responseBufferItems: (NSArray**)outBufferItems DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method customCall:inputData:outputData:
 * @abstract Use a custom call on the node
 * @discussion Generic method to access custom calls on the node.  The input
 *		and output data will vary based on which custom call is used.
 *		Basically this is slightly higher layer of dsDoPlugInCustomCall().
 * @param number Number identifying which custom call to perform (usually plug-in specific).
 * @param inputData Data to pass as input to the custom call.
 * @param outputData Data returned from the custom call.
 */
- (tDirStatus)customCall:(int)number inputData:(NSData*)inputData
                                    outputData:(NSMutableData*)outputData DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;
									
- (tDirStatus)customCall:(int)number
	sendPropertyList:(id)propList 
	withAuthorization:(void*)authExternalForm DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

- (tDirStatus)customCall:(int)number
	sendData:(NSData*)data 
	withAuthorization:(void*)authExternalForm DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;
	
/*!
 * @method customCall:sendItems:outputData:
 * @abstract Use a custom call on the node
 * @discussion Generic method to access custom calls on the node.  The input
 *		and output data will vary based on which custom call is used.
 *		Basically this is slightly higher layer of dsDoPlugInCustomCall().
 * @param number Number identifying which custom call to perform (usually plug-in specific).
 * @param items Array of items to pack into the input buffer.
 * @param outputData Data returned from the custom call.
 */
- (tDirStatus)customCall:(int)number sendItems:(NSArray*)items 
									outputData:(NSMutableData*)outputData DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

- (tDirStatus)customCall:(int)number
	withAuthorization:(void*)authExternalForm DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

- (tDirStatus)customCall:(int)number
	receiveData:(NSMutableData*)outputData 
	withAuthorization:(void*)authExternalForm
	sizeCall:(int)sizeNumber DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method adminGroup
 * @abstract Convenience method to get the admin group from this node.
 */
- (DSoGroup*)		adminGroup DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method directory
 * @abstract Gets the Directory object that contains this node.
 */
- (DSoDirectory*)	directory DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method dsNodeReference
 * @abstract Method for accessing the low-level data type.
 * @result The Directory Services node reference value for this node.
 */
- (tDirNodeReference)dsNodeReference DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

// "protected" methods

// This is for internal use only, and is only a "hand pass off" object,
// thus we do not need to worry abour retain counts on the object contained here-in.
typedef struct {
    DSoNode *node;
    tRecordReference recordRef;
    
} RecID;

/*!
 * @method initWithDir:nodeRef:nodeName:
 * @abstract Initializes the receiver with the necessary information.
 * @discussion Initializes the receiver with a pointer to inDir, which it sends
 *       a retain message to; a copy of the inNodeRef; and a pointer to inNodeName.
 *       which it also retains.
 *
 *		This should never be called directly.  Instead, instances should
 *		be created with DSoDirectory's findNode: methods.
 */
- (id)initWithDir:(DSoDirectory*)inDir nodeRef:(tDirNodeReference)inNodeRef 
                  nodeName:(NSString*)inNodeName DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

- (RecID) _findRecord: (NSString*)inName
            ofType: (const char*)inType DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

- (BOOL)usesMultiThreaded DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;
- (void)setUsesMultiThreaded:(BOOL)inValue DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;
- (BOOL)supportsSetAttributeValues DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;
- (void)setSupportsSetAttributeValues:(BOOL)inValue DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

@end

// The Search node is a subclass of DSoNode because it must override _FindRecord
// to find the first match's home node.
@interface DSoSearchNode: DSoNode {

}
@end


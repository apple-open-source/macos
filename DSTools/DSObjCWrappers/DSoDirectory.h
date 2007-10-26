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
 * @header DSoDirectory
 */


#import <Foundation/Foundation.h>
#import <DirectoryService/DirectoryService.h>

@class DSoNode, DSoSearchNode;
typedef tDirReference DSRef ;

/*!
 * @class DSoDirectory
 * @abstract Class for communication with a Directory Services process.
 * @discussion This class provides connectivity to a Directory Services process,
 *		either on the localhost, or on a remote host using DS-Proxy.
 * 		In addition to providing connectivity, the class provides methods
 *		for retrieving and opening nodes.  These methods should be used
 *		to open nodes, rather than instantiating DSoNode objects manually.
 */

@interface DSoDirectory : NSObject {
    @protected
        NSRecursiveLock		   *mDirRefLock;
        DSRef					mDirRef;
        DSoNode 			   *mLocalNode;
        DSoSearchNode		   *mSearchNode;
        NSString			   *proxyHostname;
        NSString			   *proxyUsername;
        NSString			   *proxyPassword;
        NSArray 			   *mRecordTypes;
        NSArray 			   *mAttributeTypes;
}

/*!
 * @method initWithLocal
 * @abstract Open a connection to the local machine's DS process.
 */
- (id)initWithLocal;

/*!
 * @method initWithLocalPath
 * @abstract Open a connection to the local machine's Local Only DS process.
 */
- (id)initWithLocalPath:(NSString*)filePath;

/*!
 * @method initWithHost:user:password:
 * @abstract Open a connection to a remote machine's DS process.
 * @discussion With an admin's username and password, one can connect
 * 		to a remote DS process, rather than the local machine's process.
 * @param hostName The FQDN or IP address of the remote host.
 * @param inUser An administrator's username.
 * @param inPassword The administrator's password.
 */
- (id)initWithHost:(NSString*)hostName user:(NSString*)inUser password:(NSString*)inPassword;

/*!
 * @method nodeCount
 * @abstract Finds the number of nodes registered by DS.
 */
- (unsigned long)	nodeCount;

/*!
 * @method findNodeNames:matchType:
 * @abstract Finds the list of node names registered by DS.
 *			Returns an empty array if no matching results are found.
 * @result An array of NSString objects containing the node names.
 */
- (NSArray*) findNodeNames:(NSString*)inPattern matchType:(tDirPatternMatch)inType;

/*!
 * @method findNode:
 * @abstract Find and open a DS Node.
 * @discussion Invokes findNode:matchType: with matchType an actual pattern to match.
 */
- (DSoNode*) findNodeViaEnum: (int)inPattern;

/*!
 * @method findNode:
 * @abstract Find and open a DS Node.
 * @discussion Invokes findNode:matchType: with matchType set to eDSExact.
 */
- (DSoNode*) findNode: (NSString*)inPattern;

/*!
 * @method findNode:matchType:
 * @abstract Find and open a DS Node.
 * @discussion Invokes findNode:matchType:useFirst: with useFirst set to YES.
 */
- (DSoNode*) findNode: (NSString*)inPattern 
                     matchType: (tDirPatternMatch)inType;
                     
/*!
 * @method findNode:matchType:useFirst:
 * @abstract Find and open a DS Node.
 * @discussion Finds a DS node whose name matches the given parameters.
 * 		The node is then opened and wrapped in a DSoNode object.
 * @param inPattern The name pattern of the node to find.
 * @param inType The type of pattern matching to perform.
 * @param useFirst Documenation forthcoming.
 * @result A DSoNode object representing the found and opened node.
 */
- (DSoNode*) findNode: (NSString*)inPattern 
                     matchType: (tDirPatternMatch)inType
                     useFirst: (BOOL)inUseFirst;

/*!
 * @method findNodeNames
 * @abstract Finds the list of all node names registered by DS.
 * @result An array of NSString objects containing the node names.
 */
- (NSArray*) findNodeNames;

/*!
 * @method nodeBrowserItems
 * @abstract Finds the all top level items for the node browser.
 * @result An array of DSoNodeBrowserItem objects.
 */
- (NSArray*) nodeBrowserItems;

/*!
 * @method standardRecordTypes
 * @abstract Retrieves all defined standard record types from the config node.
 * @result An array of strings of the record types.
 */
- (NSArray*) standardRecordTypes;

/*!
 * @method standardAttributeTypes
 * @abstract Retrieves all defined standard attribute types from the config node.
 * @result An array of strings of the attribute types.
 */
- (NSArray*) standardAttributeTypes;

/*!
 * @method localNode
 * @abstract Convenience method to find and open the local node.
 */
- (DSoNode*) localNode;

/*!
 * @method searchNode
 * @abstract Convenience method to find and open
 *		the (authentication) search node.
 */
- (DSoSearchNode*) searchNode;

/*!
 * @method verifiedDirRef
 * @abstract Method for accessing the low-level data type.
 * @discussion This method differs from dsDirRef in that it 
 *		first verifies the reference for validity.  If it fails
 *		the check, then the connection is re-opened.
 * @result The Directory Services reference value for this connection.
 */
- (DSRef)verifiedDirRef;

/*!
 * @method dsDirRef
 * @abstract Method for accessing the low-level data type.
 * @result The Directory Services reference value for this connection.
 */
- (DSRef)dsDirRef;

@end

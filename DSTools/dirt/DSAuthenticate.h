/*
 * Copyright (c) 2000 - 2003 Apple Computer, Inc. All rights reserved.
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
 * @header DSAuthenticate
 *  DSAuthenticate provides access to authentication functionality
 *  in Directory Services.
 *  It provides routines for authenticating a user against the 
 *  local node, search path, or a specific named node.
 *  It will also search the authentication search policy to find
 *  a user without authenticating that user.
 */


#import <Foundation/Foundation.h>
#import <DirectoryService/DirectoryService.h>
#import "DSException.h"
#import "DSStatus.h"

#define kBufferBlockSize 512

@interface DSAuthenticate : NSObject {
    tDirReference		_DirRef;
    tDirNodeReference   _SearchNodeRef;
    tDirPatternMatch	_SearchNodeNameToUse;
    char			   *_SearchObjectType; // Is either user or group.
    NSMutableArray     *_MasterUserList;
    DSStatus		   *_dsStat;
    
@private
    
}

- (void) dealloc; 
- (void) reset;

// ============================================================================
// Configuring the object
//
- (void)useAuthenticationSearchPath; // Default
- (void)useContactSearchPath;

- (void)searchForUsers; //Default
- (void)searchForGroups;

// ============================================================================
// Wrapper methods for Directory Service Utility functions
//
- (void)allocateDataBuffer:(tDataBufferPtr*)buffer
        withNumberOfBlocks:(unsigned short) numberOfBlocks 
        shouldReallocate:(BOOL) reAllocate;
- (tDirStatus)deallocateDataBuffer:(tDataBufferPtr)buffer;
- (tDirStatus)deallocateDataList:(tDataListPtr)list;
- (tDirStatus)deallocateDataNode:(tDataNodePtr)node;

// ============================================================================
// Private utility methods for accessing, obtaining and opening
// directory nodes.
//
- (tDataBufferPtr) retrieveLocalNode;
- (tDataBufferPtr) retrieveSearchPathNode;
- (tDataListPtr) getNodeNameForBuffer:(tDataBufferPtr)buffer;
- (tDirNodeReference) openDirNodeWithName:(tDataListPtr)nodeName;

// ============================================================================
// Private methods for performing search functionality in 
// Directory Services.
//
- (tDirStatus)fetchRecordListOnSearchNodeMatchingName:(NSString*)inUsername;
- (tDirStatus)fetchRecordListOnNode:(tDirNodeReference)nodeRefToSearch matchingName:(NSString*)inUsername;

- (NSString*)nodePathStrForSearchNodeRecordNumber:(unsigned long)recordNumber;

// ============================================================================
// Public methods for accessing the Directory Service functions
// that this class provides.
//
- (tDirStatus)authOnNodePath:(NSString*)nodePathStr username:(NSString*)inUsername password:(NSString*)inPassword;
- (tDirStatus)authenticateInNodePathStr:(NSString*)nodePathStr username:(NSString*)inUsername
                password:(NSString*)inPassword;
- (tDirStatus)authenticateInNode:(tDirNodeReference)userNode username:(NSString*)inUsername
                password:(NSString*)inPassword;
- (tDirStatus)authUserOnLocalNode:(NSString*)inUsername password:(NSString*)inPassword;
- (tDirStatus)authUserOnSearchPath:(NSString*)inUsername password:(NSString*)inPassword;
- (NSArray*)getListOfNodesWithUser:(NSString*)inUsername;

@end

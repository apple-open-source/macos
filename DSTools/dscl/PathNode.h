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
 * @header PathNode
 */


#import <Foundation/Foundation.h>
#import <DirectoryService/DirServicesTypes.h>
#import "PathItem.h"

@class DSoDirectory,DSoNode;

@interface PathNode : PathItem
{
    NSString	   *_pathName;
    DSoDirectory   *_dir;
    DSoNode		   *_node;
    BOOL			_enableSubNodes;
}

// Initialize with a Directory object, and a pathname of the node (or node prefix)
- initWithDir:(DSoDirectory*)inDir path:(NSString*)inPath;

// Initialize with a Node object and its intended pathname.
- initWithNode:(DSoNode*)inNode path:(NSString*)inPath;

// Get a list of the sub-node, i.e. at list of nodes
// whose names begin with the name of this node.
- (NSArray*)getSubnodeList;

// Get a list of Record types that this node contains.
- (NSArray*)getRecordList;

// Create a new sub-node path object derived from this node object.
- (PathItem*)cdNode:(NSString*)dest;

// Create a new record type path object derived from this node object.
- (PathItem*)cdRecordType:(NSString*)dest;

- (BOOL)enableSubNodes;
- (void)setEnableSubNodes:(BOOL)value;

// ATM - PlugInManager needs access to node
-(DSoNode*) node;
@end

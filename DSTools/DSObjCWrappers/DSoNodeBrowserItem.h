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
 * @header DSoNodeBrowserItem
 */


#import <Foundation/Foundation.h>

@class DSoDirectory, DSoNode;

@interface DSoNodeBrowserItem : NSObject {
	DSoDirectory* _dir;
	NSString* _path;
	BOOL useNode;
	DSoNode* _node;
	NSMutableArray* _children;
}

- (DSoNodeBrowserItem*)initWithName:(NSString*)name directory:(DSoDirectory*)dir DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;
- (DSoNodeBrowserItem*)initWithPath:(NSString*)path directory:(DSoDirectory*)dir DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

- (NSString*)name DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;
- (NSString*)path DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;
- (DSoNode*)node DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;
- (BOOL)loadedChildren DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;
- (BOOL)hasChildren DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;
- (NSArray*)children DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;
- (NSArray*)registeredChildrenPaths DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;
- (DSoNodeBrowserItem*)childWithName:(NSString*)name DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

- (int)compareNames:(DSoNodeBrowserItem*)item DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

@end

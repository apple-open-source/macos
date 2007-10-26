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
 * @header PathItemProtocol
 */

#import <DirectoryService/DirectoryService.h>

@class PathItem;

@protocol PathItemProtocol

// Return the name (for interactive display purposes)
// of this path location.
- (NSString*) name;

- (tDirStatus) appendKey:(NSString*)inKey withValues:(NSArray*)inValues;

// verify or authenticate a user to the node for or containing the current path item.
- (tDirStatus) authenticateName:(NSString*)inUsername withPassword:(NSString*)inPassword authOnly:(BOOL)inAuthOnly;

// Authenticate a user to the node for or containing the current path item.
- (tDirStatus) authenticateName:(NSString*)inUsername withPassword:(NSString*)inPassword;

- (tDirStatus) setPassword:(NSArray*)inPassword;

// Change directory into path derived from this path.
// ie. a sub-node, or a record type within a node
// or a record within a record type.
- (PathItem*) cd:(NSString*)dest;

- (tDirStatus) createKey:(NSString*)inKey withValues:(NSArray*)inValues;
- (tDirStatus) create:(NSString*)inKey plistPath:(NSString*)inPlistPath values:(NSArray*)inValues;
- (tDirStatus) create:(NSString*)inKey atIndex:(int)index plistPath:(NSString*)inPlistPath values:(NSArray*)inValues;

// Delete the record held by this PathItem.
- (tDirStatus) deleteItem;
- (tDirStatus) deleteKey:(NSString*)inKey withValues:(NSArray*)inValues;
- (tDirStatus) delete:(NSString*)inKey plistPath:(NSString*)inPlistPath values:(NSArray*)inValues;
- (tDirStatus) delete:(NSString*)inKey atIndex:(int)index plistPath:(NSString*)inPlistPath values:(NSArray*)inValues;

// List the contents of the current path
- (NSDictionary*) getDictionary:(NSArray*)inKeys;
- (NSArray*) getList;
- (NSArray*) getListWithKeys:(NSArray*)inKeys;
- (tDirStatus) list:(NSString*)inPath key:(NSString*)inKey;
- (NSArray*) getPossibleCompletionsFor:(NSString*)inPrefix;

- (tDirStatus) mergeKey:(NSString*)inKey withValues:(NSArray*)inValues;

- (tDirStatus) changeKey:(NSString*)inKey oldAndNewValues:(NSArray*)inValues;
- (tDirStatus) changeKey:(NSString*)inKey indexAndNewValue:(NSArray*)inValues;

// Return the name of the node for or containing the current PathItem.
- (NSString*)nodeName;

// Perform a read operation on the current path location.
// ie. for a record, this will display all its attributes.
- (tDirStatus) read:(NSArray*)inKeys;

// Perform a read all operation on the current path location.
// ie. for a record type, this will display all the attributes for all records of that type.
- (tDirStatus) readAll:(NSArray*)inKeys;

- (tDirStatus) read:(NSString*)inPath keys:(NSArray*)inKeys;

- (tDirStatus) read:(NSString*)inKey plistPath:(NSString*)inPlistPath;

- (tDirStatus) read:(NSString*)inKey atIndex:(int)index plistPath:(NSString*)inPlistPath;


// Search for records with a matching key & value pair.
- (tDirStatus) searchForKey:(NSString*)inKey withValue:(NSString*)inValue matchType:(NSString*)inType;

- (void) printDictionary:(NSDictionary*)inDict withRequestedKeys:(NSArray*)inKeys;

@end

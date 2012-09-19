/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1998-2003 Apple Computer, Inc.  All Rights Reserved.
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


#import <Foundation/Foundation.h>


@interface OutlineViewNode : NSObject {
    NSString *          _name;
    NSString *          _value;
    NSMutableArray *    _children;
    NSString *          _tempName;
    NSString *          _tempValue;
}

- init;
- initWithName:(NSString *)name value:(NSString *)value;
- (void)dealloc;


    // Accessor methods for the strings
- (NSString *)name;
- (NSString *)value;
- (void)setName:(NSString *)aString;
- (void)setValue:(NSString *)aString;

    // Accessors for the children
- (void)addChild:(OutlineViewNode *)aNode;
- (int)childrenCount;
- (NSArray *)children;
- (OutlineViewNode *)childAtIndex:(int)i;
- (void)removeAllChildren;

- (void)addNode:(OutlineViewNode *)aNode atDepth:(int)depth;
- (void)addNodeWithName:(char *)name value:(char *)value atDepth:(int)depth;

    // Other properties
- (BOOL)isExpandable;

- (OutlineViewNode *)deepestChild;

- (NSString*)stringRepresentation:(NSString*)name:(int)startingLevel;
- (NSString *)stringRepresentation:(int)startingLevel;
- (NSString *)stringRepresentationOfValues:(int)startingLevel;
- (NSMutableDictionary *)dictionaryVersionOfMe;

@end

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
 * @header PathItem
 */


#import <Foundation/Foundation.h>
#import <DSObjCWrappers/DSoNode.h>
#import "PathItemProtocol.h"

#define DS_EXCEPTION_STATUS_IS(S) ([localException isKindOfClass:[DSoException class]] && [(DSoException*)localException status] == S)

extern BOOL gRawMode;
extern BOOL gURLEncode;

/* printValue()
 * Print an attribute value, but url encoded if appropriate
 */
void printValue(NSString *inValue, BOOL hasSpaces);
NSString* stripDSPrefixOffValue(NSString* inValue);
void printAttribute(NSString *key, NSArray *values, NSString *prefixString);
void printPlist(NSString *plistPath, id currentElement);
NSArray* prefixedAttributeKeysWithNode(DSoNode* inNode, NSArray* inKeys);

@interface PathItem : NSObject <PathItemProtocol>
{
}

/*
 * Strip the Prefix off of a DS constant.
 * Many DS contants have a prefix followed by a colon followed
 * by a unique string for that particular constant.  This is a simple
 * utility function grab just the post-colon string.
 */
- (NSString*)stripDSPrefixOffValue:(NSString*)inValue;

@end

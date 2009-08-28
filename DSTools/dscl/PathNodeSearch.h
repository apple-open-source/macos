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
 * @header PathNodeConfig
 */


#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import "PathNode.h"
#import "PathRecord.h"

@class DSoNodeConfig;

@interface PathNodeSearch : PathNode
{
	AuthorizationExternalForm _authExternalForm;
	BOOL _haveRights;
	DSoNodeConfig* _configNode;
	tDirPatternMatch _type;
}

- initWithNode:(DSoNode*)inNode path:(NSString*)inPath type:(tDirPatternMatch)val;

- (tDirStatus) modify:(tAttrCAM)inAction withKey:(NSString*)inKey withValues:(NSArray*)inValues;

- (tDirStatus)setSearchPolicy:(NSString*)newPolicy;
- (tDirStatus)setCustomSearchPath:(NSArray*)nodeList;
- (BOOL)nodeNameIsValid:(NSString*)nodeName;

@end

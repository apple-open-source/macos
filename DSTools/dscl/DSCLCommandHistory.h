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
 * @header DSCLCommandHistory
 */


#import <Foundation/Foundation.h>

#define HISTORY_CAPACITY 50

@interface DSCLCommandHistory : NSObject
{
	NSString		   *mCommandList[HISTORY_CAPACITY];
	unsigned int		mLastItem;
	unsigned int		mCurrentItem;
	BOOL				mIsClean;
}

- (void)addCommand:(NSString*)inCommand;
- (void)addTemporaryCommand:(NSString*)inCommand;

- (NSString*)previousCommand;
- (NSString*)nextCommand;

- (void)resetToLast;
- (BOOL)isClean;

@end

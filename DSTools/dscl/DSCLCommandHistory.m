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


#import "DSCLCommandHistory.h"

@interface DSCLCommandHistory (DSCLCommandHistoryPrivate)
- (unsigned int)increment:(unsigned int*)i;
- (unsigned int)decrement:(unsigned int*)i;
- (unsigned int)incrementCurrent;
- (unsigned int)decrementCurrent;
@end

@implementation DSCLCommandHistory

- init
{
	unsigned int i;
	[super init];
	mCurrentItem = 0;
	mLastItem = 0;
	mIsClean = YES;
	for (i=0; i < HISTORY_CAPACITY; i++)
		mCommandList[i] = nil;
	return self;
}

- (void) dealloc
{
	unsigned int i;
	for (i=0; i < HISTORY_CAPACITY; i++)
		[mCommandList[i] release];  // It's ok to send a superfluous message to a nil pointer
	[super dealloc];
}

- (void)addCommand:(NSString*)inCommand
{
	[mCommandList[mLastItem] release];
	mCommandList[mLastItem] = [inCommand retain];
	[self increment:&mLastItem];
	mCurrentItem = mLastItem;
}

- (void)addTemporaryCommand:(NSString*)inCommand
{
	[mCommandList[mLastItem] release];
	mCommandList[mLastItem] = [inCommand retain];
}

- (NSString*)previousCommand
{
	[self decrementCurrent];
	return mCommandList[mCurrentItem];
}

- (NSString*)nextCommand
{
	[self incrementCurrent];
	return mCommandList[mCurrentItem];
}

- (void)resetToLast
{
	mCurrentItem = mLastItem;
}

- (BOOL)isClean
{
	return 	mCurrentItem == mLastItem;
}

@end

@implementation DSCLCommandHistory (DSCLCommandHistoryPrivate)
- (unsigned int)increment:(unsigned int*)i
{
	unsigned int oldValue = *i;
	if (*i == HISTORY_CAPACITY-1)
		*i = 0;
	else
		(*i)++;
	return oldValue;
}

- (unsigned int)decrement:(unsigned int*)i
{
	unsigned int oldValue = *i;
	if (*i == 0)
		*i = HISTORY_CAPACITY - 1;
	else
		(*i)--;
	return oldValue;
}

- (unsigned int)incrementCurrent
{
	if (mCurrentItem != mLastItem)
		return [self increment:&mCurrentItem];
	else
		return mCurrentItem;
}

- (unsigned int)decrementCurrent
{
	unsigned int oldValue = [self decrement:&mCurrentItem];
	if (mLastItem == mCurrentItem || mCommandList[mCurrentItem] == nil)
		[self increment:&mCurrentItem];
	return oldValue;
}
		

@end
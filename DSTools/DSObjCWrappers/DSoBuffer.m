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
 * @header DSoBuffer
 */


#import "DSoBuffer.h"

#import "DSoException.h"
#import "DSoDirectory.h"

@implementation DSoBuffer

#pragma mark Constructors & Desctructors
// ----------------------------------------------------------------------------
// Constructors & Desctructors

- (DSoBuffer*)init
{
    [super init];
    mBuffer = calloc(1, sizeof(tDataBuffer));
    return self;
}

- (DSoBuffer*)initWithDir:(DSoDirectory*)inDir
{
    return [self initWithDir:inDir bufferSize:kDefaultBufferSize];
}

- (DSoBuffer*)initWithDir:(DSoDirectory*)inDir bufferSize:(unsigned long)inBufferSize
{
    [self init];
    
    mDir = [inDir retain];
	if (mBuffer)
		free(mBuffer);
    mBuffer = dsDataBufferAllocate([mDir dsDirRef], inBufferSize);
	if (!mBuffer)
	{
        [self release];
        [DSoException raiseWithStatus:eDSAllocationFailed];
	}
	
    return self;
}

- (void)dealloc
{
    if (mBuffer)
        dsDataBufferDeAllocate ([mDir dsDirRef], mBuffer) ;
    [mDir release];
    [super dealloc];
}

- (void)finalize
{
    if (mBuffer)
        dsDataBufferDeAllocate ([mDir dsDirRef], mBuffer) ;
    [super finalize];
}

#pragma mark Public Accessors
// ----------------------------------------------------------------------------
//  Public Accessors

- (unsigned long) getBufferSize
{
    return mBuffer->fBufferSize ;
}

- (unsigned long) getDataLength
{
    return mBuffer->fBufferLength ;
}

- (void)setData:(const void*)inData length:(unsigned long)inLength
{
    mBuffer->fBufferLength = 0 ;
    [self grow:inLength];
    memcpy (mBuffer->fBufferData, inData, inLength) ;
    mBuffer->fBufferLength = inLength ; 
}

- (void)appendData:(const void*)inData length:(unsigned long)inLength
{
    char *cpBuf;
    
    if ((mBuffer->fBufferLength + inLength) > mBuffer->fBufferSize)
        [self grow:(mBuffer->fBufferLength + inLength)] ;
    cpBuf = mBuffer->fBufferData + mBuffer->fBufferLength ;
    memcpy (cpBuf, inData, inLength) ;
    mBuffer->fBufferLength += inLength ; 
}

- (void)setDataLength:(unsigned long)inLength
{
    if (inLength > mBuffer->fBufferSize)
        [DSoException raiseWithStatus:eDSBufferTooSmall];
    mBuffer->fBufferLength = inLength ; 
}

- (tDataBufferPtr)dsDataBuffer
{
    return mBuffer;
}

- (tDataBufferPtr) grow:(unsigned long)inNewSize
{
	UInt32			ulTemp  = 16;
	tDataBufferPtr	bufNew  = nil;
	DSRef			dirRef  = [mDir dsDirRef];

	if (!inNewSize)
		inNewSize = kDefaultBufferSize ;
	if (mBuffer && (inNewSize <= mBuffer->fBufferSize))
		return mBuffer ;

	if (inNewSize == kDefaultBufferSize)
		ulTemp = inNewSize ;
	else
		for ( ; ulTemp < inNewSize ; ulTemp <<= 1) ;

	bufNew = dsDataBufferAllocate (dirRef, ulTemp) ;
	if (!bufNew)
        [DSoException raiseWithStatus:eDSAllocationFailed];

	if (mBuffer && (ulTemp = mBuffer->fBufferLength))
		memcpy (bufNew->fBufferData, mBuffer->fBufferData, ulTemp) ;
	else
		ulTemp = 0 ;
        
	bufNew->fBufferLength = ulTemp ;
	dsDataBufferDeAllocate (dirRef, mBuffer) ;
    
	return (mBuffer = bufNew) ;
}

- (NSString*)description
{
	int					i				= 0;
	NSMutableString    *bufferString	= [[NSMutableString alloc] init];
	NSString		   *retValue		= nil;
	
	for (i = 0; i < mBuffer->fBufferLength; i++)
	{
		if (mBuffer->fBufferData[i] > 126 || mBuffer->fBufferData[i] < 32)
			[bufferString appendFormat:@"<%d>", mBuffer->fBufferData[i]];
		else
			[bufferString appendFormat:@"%c", mBuffer->fBufferData[i]];
	}
    retValue = [NSString stringWithFormat:@"%@ {\nBuffer Size:%ld\nBuffer Length:%ld\n%@\n}\n",
		[super description], mBuffer->fBufferSize, mBuffer->fBufferLength, bufferString];
	[bufferString release];
	return retValue;
}

@end

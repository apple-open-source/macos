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
 * @header DSoDataNode
 */


#import "DSoDataNode.h"

#import "DSoException.h"

@implementation DSoDataNode

- (DSoDataNode*)initWithDir:(DSoDirectory*)inDir bufferSize:(unsigned long)inBufSize
						dataLength:(unsigned long)inDataLength data:(const void*)inData
{
    [self init];
    mNode = dsDataNodeAllocateBlock ([inDir dsDirRef], inBufSize, inDataLength, (void*)inData);
    if (!mNode) {
        [self release];
        [DSoException raiseWithStatus:eDSAllocationFailed];
    }
    mDir = [inDir retain];
    return self;
}

- (DSoDataNode*)initWithDir:(DSoDirectory*)inDir value:(id)inValue
{
    if( self = [self init] )
    {
        if( [inValue isKindOfClass:[NSString class]] )
        {
            mNode = dsDataNodeAllocateString( [inDir dsDirRef], [inValue UTF8String] );
        }
        else if( [inValue isKindOfClass:[NSData class]] )
        {
            mNode = dsDataNodeAllocateBlock( [inDir dsDirRef], [inValue length], [inValue length], (tBuffer) [inValue bytes] );
        }
        else if ( inValue != nil )
        {
            [self release];
            @throw [NSException exceptionWithName: NSInvalidArgumentException reason: @"[DSoDataNode initWithDir:value:] value is not a valid NSString nor NSData" userInfo: nil];
        }
        
        if( mNode == nil )
        {
            [self release];
            return nil;
        }
    }
    return self;
}

- (DSoDataNode*)initWithDir:(DSoDirectory*)inDir string:(NSString*)inString;
{
    return [self initWithDir:inDir cString:[inString UTF8String]];
}

- (DSoDataNode*)initWithDir:(DSoDirectory*)inDir cString:(const char*)inString;
{
    [self init];
    mDir = [inDir retain];
    mNode = dsDataNodeAllocateString ([inDir dsDirRef], inString);
    if (!mNode) {
        [self release];
        [DSoException raiseWithStatus:eDSAllocationFailed];
    }
    return self;
}

- (DSoDataNode*)initWithDir:(DSoDirectory*)inDir dsDataNode:(tDataNode*)inNode;
{
    [self init];
    mDir = [inDir retain];
    mNode = inNode;
    if (!mNode) {
        [self release];
        [DSoException raiseWithStatus:eDSAllocationFailed];
    }
    return self;
}

- (DSoDataNode*)initWithDir:(DSoDirectory*)inDir copyOfDsDataNode:(tDataNode*)inNode;
{
    [self init];
    mDir = [inDir retain];
    mNode = dsDataNodeAllocateBlock ([inDir dsDirRef], inNode->fBufferSize, 
                                        inNode->fBufferLength, (void*)inNode->fBufferData);
    if (!mNode) {
        [self release];
        [DSoException raiseWithStatus:eDSAllocationFailed];
    }
    return self;
}

- (void)dealloc
{
    if (mNode)
        dsDataNodeDeAllocate ([mDir dsDirRef], mNode) ;
    [mDir release];
    [super dealloc];
}

- (void)finalize
{
    if (mNode)
        dsDataNodeDeAllocate ([mDir dsDirRef], mNode) ;
    [super finalize];
}

	// Inline accessors.
- (unsigned long) getBufferSize
{
    return dsDataNodeGetSize (mNode) ;
}

- (unsigned long) getDataLength
{ 
    return dsDataNodeGetLength (mNode) ; 
}

- (void) setDataLength:(unsigned long)inLength
{
    tDirStatus	nError = dsDataNodeSetLength (mNode, inLength) ;
	if (nError)
        [DSoException raiseWithStatus:nError];
}

- (tDataNodePtr) dsDataNode;
{
    return mNode;
}

- (NSString*)description
{
	int i;
	NSMutableString *bufferString = [[NSMutableString alloc] init];
	NSString *retValue;

	for (i = 0; i < mNode->fBufferLength; i++)
	{
		if (mNode->fBufferData[i] > 126 || mNode->fBufferData[i] < 32)
			[bufferString appendFormat:@"<%d>", mNode->fBufferData[i]];
		else
			[bufferString appendFormat:@"%c", mNode->fBufferData[i]];
	}
    retValue = [NSString stringWithFormat:@"%@ {\nNode Size:%ld\nBuffer Length:%ld\n%@\n}\n",
		[super description], mNode->fBufferSize, mNode->fBufferLength, bufferString];
	[bufferString release];
	return retValue;
}
@end

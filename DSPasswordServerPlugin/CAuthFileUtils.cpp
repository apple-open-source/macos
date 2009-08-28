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

#include "CAuthFileUtils.h"

#define kFixedDESChunk			8

CAuthFileUtils::CAuthFileUtils()
{
	int32_t desKeyArray[] = {	3785, 1706062950, 57253, 290150789, 20358, -1895669319, 31632, -1897719547, 
								14472, -356383814, 45160, 1045764877, 42031, 340922616, 17982, 893499693, 
								26821, 1697433225, 49601, 1067086915, 42371, -1280392416, 46874, -1550815422, 
								47922, -788758000, 7254, -590256566, 22097, 1547481608, 22125, -1323694915 };
	
	memcpy( mDESKeyArray, desKeyArray, sizeof(KeysArray) );
}


CAuthFileUtils::~CAuthFileUtils()
{
}


//----------------------------------------------------------------------------------------------------
//	getGMTime
//
//	Returns: a time struct based on GMT
//----------------------------------------------------------------------------------------------------

void
CAuthFileUtils::getGMTime(struct tm *outGMT)
{
    time_t theTime;
    struct tm gmt;
    
    ::time(&theTime);
    ::gmtime_r(&theTime, &gmt);
    memcpy( outGMT, &gmt, sizeof(struct tm) );
}

void
CAuthFileUtils::getGMTime(BSDTimeStructCopy *outGMT)
{
    struct tm bsdTimeStruct;
    getGMTime( &bsdTimeStruct );
    StructTM2BSDTimeStructCopy( &bsdTimeStruct, outGMT );
}
    


//----------------------------------------------------------------------------------------------------
//	slotToOffset
//
//	Returns: the position in the database file (from SEEK_SET) for the slot
//----------------------------------------------------------------------------------------------------

long
CAuthFileUtils::slotToOffset(long slot)
{
   	return sizeof(PWFileHeader) + (slot - 1) * sizeof(PWFileEntry);
}


/*----------------------------------------------------------------------------------*/

#pragma mark -
#pragma mark ¥DES ACCESSORS¥
#pragma mark -


void
CAuthFileUtils::DESEncode(void *data, unsigned long inDataLen)
{
    char *tptr = (char *)data;
    
    while ( inDataLen > 0 )
    {
        Encode(mDESKeyArray, kFixedDESChunk, tptr);
        tptr += kFixedDESChunk;
        inDataLen -= kFixedDESChunk;
    }
}


void
CAuthFileUtils::DESDecode(void *data, unsigned long inDataLen)
{
    char *tptr = (char *)data;
    
    while ( inDataLen > 0 )
    {
        Decode(mDESKeyArray, kFixedDESChunk, tptr);
        tptr += kFixedDESChunk;
        inDataLen -= kFixedDESChunk;
    }
}


void
CAuthFileUtils::DESAutoDecode( void *data )
{
	PWFileEntry anEntry;
	unsigned long offset;
	unsigned char *dataPtr;
	
	// decrypt each block of 8
	// because decryption is expensive, look for the 0-terminator in the decrypted data
	// to determine the stopping point.
	
	for ( offset = 0; offset <= sizeof(anEntry.passwordStr) - kFixedDESChunk; offset += kFixedDESChunk )
	{
		dataPtr = (unsigned char *)data + offset;
		DESDecode( dataPtr, kFixedDESChunk );
		
		if ( dataPtr[0] == 0 || dataPtr[1] == 0 || dataPtr[2] == 0 ||
			 dataPtr[3] == 0 || dataPtr[4] == 0 || dataPtr[5] == 0 ||
			 dataPtr[6] == 0 || dataPtr[7] == 0 )
			break;
	}
}






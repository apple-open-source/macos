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
CAuthFileUtils::getGMTime(struct tm *inOutGMT)
{
    time_t theTime;
    struct tm gmt;
    
    ::time(&theTime);
    ::gmtime_r(&theTime, &gmt);
    memcpy( inOutGMT, &gmt, sizeof(struct tm) );
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


//----------------------------------------------------------------------------------------------------
//	passwordRecRefToString
//
//----------------------------------------------------------------------------------------------------

void
CAuthFileUtils::passwordRecRefToString(PWFileEntry *inPasswordRec, char *outRefStr)
{
    sprintf( outRefStr, "0x%.8lx%.8lx%.8lx%.8lx",
                inPasswordRec->time,
                inPasswordRec->rnd,
                inPasswordRec->sequenceNumber,
                inPasswordRec->slot );
}


//----------------------------------------------------------------------------------------------------
//	stringToPasswordRecRef
//
//	Returns: Boolean (1==valid ref, 0==fail)
//----------------------------------------------------------------------------------------------------

int
CAuthFileUtils::stringToPasswordRecRef(const char *inRefStr, PWFileEntry *outPasswordRec)
{
    char tempStr[9];
    const char *sptr;
    int result = false;
    
    // invalid slot value
    outPasswordRec->slot = 0;
    
    if ( strncmp( inRefStr, "0x", 2 ) == 0 && strlen(inRefStr) == 2+8*4 )
    {
        sptr = inRefStr + 2;
        
        memcpy( tempStr, sptr, 8 );
        tempStr[8] = 0;
        sscanf( tempStr, "%lx", &outPasswordRec->time );
        sptr += 8;
        
        memcpy( tempStr, sptr, 8 );
        tempStr[8] = 0;
        sscanf( tempStr, "%lx", &outPasswordRec->rnd );
        sptr += 8;
        
        memcpy( tempStr, sptr, 8 );
        tempStr[8] = 0;
        sscanf( tempStr, "%lx", &outPasswordRec->sequenceNumber );
        sptr += 8;
        
        memcpy( tempStr, sptr, 8 );
        tempStr[8] = 0;
        sscanf( tempStr, "%lx", &outPasswordRec->slot );
        //sptr += 8;
        
        result = true;
    }
    
    return result;
}


/*----------------------------------------------------------------------------------*/

#pragma mark -
#pragma mark ¥DES ACCESSORS¥
#pragma mark -


void
CAuthFileUtils::DESEncode(const void *key, void *data, unsigned long inDataLen)
{
    KeysArray theKeyArray;
    char *tptr = (char *)data;
    
    KeySched((const EncryptBlk *)key, theKeyArray, kDESVersion2);
    
    while ( inDataLen > 0 )
    {
        Encode(theKeyArray, kFixedDESChunk, tptr);
        tptr += kFixedDESChunk;
        inDataLen -= kFixedDESChunk;
    }
}


void
CAuthFileUtils::DESDecode(const void *key, void *data, unsigned long inDataLen)
{
    KeysArray theKeyArray;
    char *tptr = (char *)data;
    
    KeySched((const EncryptBlk *)key, theKeyArray, kDESVersion2);
    while ( inDataLen > 0 )
    {
        Decode(theKeyArray, kFixedDESChunk, tptr);
        tptr += kFixedDESChunk;
        inDataLen -= kFixedDESChunk;
    }
}


void
CAuthFileUtils::DESAutoDecode(const void *key, void *data)
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
		DESDecode( key, dataPtr, kFixedDESChunk );
		
		if ( dataPtr[0] == 0 || dataPtr[1] == 0 || dataPtr[2] == 0 ||
			 dataPtr[3] == 0 || dataPtr[4] == 0 || dataPtr[5] == 0 ||
			 dataPtr[6] == 0 || dataPtr[7] == 0 )
			break;
	}
}






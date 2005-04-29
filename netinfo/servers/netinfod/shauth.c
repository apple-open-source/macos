/*
 *  shauth.c
 *
 *  Created by kaw on Wed Apr 16 2003.
 *  Copyright (c) 2003 Apple Computer. All rights reserved.
 *
 */

#include <stdio.h>
#include <string.h>		//used for strcpy, etc.
#include <stdlib.h>		//used for malloc
#include <openssl/sha.h>
#include <openssl/md4.h>
#include <ppc/endian.h>

#include "shauth.h"
#include "psauth.h"

#define kShadowHashDirPath				"/var/db/shadow/hash/"
#define kShadowHashOldDirPath			"/var/db/samba/hash/"
#define kShadowHashStateFileSuffix		".state"
#define kShadowHashRecordName			"shadowhash"

// --------------------------------------------------------------------------------
//	Hash Length Constants
#define		kHashShadowChallengeLength			8
#define		kHashShadowKeyLength				21
#define		kHashShadowResponseLength			24
#define		kHashShadowOneLength				16
#define		kHashShadowBothLength				32
#define		kHashSecureLength					20
#define		kHashCramLength						32
#define		kHashSaltedSHA1Length				24
#define		kHashRecoverableLength				512

#define		kHashTotalLength					(kHashShadowBothLength + kHashSecureLength + \
												 kHashCramLength + kHashSaltedSHA1Length + \
												 kHashRecoverableLength)
#define		kHashShadowBothHexLength			64
#define		kHashOldHexLength					104
#define		kHashTotalHexLength					(kHashTotalLength * 2)


u_int16_t ByteSwapInt16(u_int16_t value)
{
	u_int16_t mask = value;
	mask <<= 8;
	value >>= 8;
	value |= mask;
	return value;
}


void CStringToUnicode(char *cstr, u_int16_t *unicode)
{
	int i;
	u_int16_t val;
 	int len;
 	 
 	len = strlen(cstr);
 	
	for(i = 0; i < len; i++) 
	{
		val = *cstr;
		if (BYTE_ORDER == BIG_ENDIAN)
			*unicode = ByteSwapInt16(val);
		else
			*unicode = val;			
		unicode++;
		cstr++;
		if (val == 0) break;
	}
}

void MD4Encode(unsigned char *output, const unsigned char *input, unsigned int len)
{

  MD4_CTX context = {};

  MD4_Init (&context);
  MD4_Update (&context, (unsigned char *)input, len);
  MD4_Final (output, &context);
}

void CalculateSMBNTHash(const char *utf8Password, unsigned char outHash[16])
{
	u_int16_t unicodeLen = 0;
	u_int16_t unicodepwd[258] = {0};
	char *password[128] = {0};
	int passLen = 0;
	//unsigned char P21[21] = {0};
	
	if (utf8Password == NULL || outHash == NULL) return;
	
	if (strlen(utf8Password) < 128)
		passLen = strlen(utf8Password);
	else
		passLen = 128;
		
	memmove(password, utf8Password, passLen);
	unicodeLen = strlen((char *)password) * sizeof(u_int16_t);
		
	CStringToUnicode((char *)password, unicodepwd);
	MD4Encode(outHash, (unsigned char *)unicodepwd, unicodeLen);	
}

//--------------------------------------------------------------------------------------------------
// * ReadShadowHash ()
//--------------------------------------------------------------------------------------------------

int ReadShadowHash( const char *inUserName, char *inGUIDString, unsigned char outHashes[kHashTotalLength], unsigned long *outLen )
{
	int				siResult							= kAuthOtherError;
	char			*path								= NULL;
	char			hexHashes[kHashTotalHexLength + 1]	= { 0 };
	long			readBytes							= 0;
	unsigned long	pathSize							= 0;
	FILE			*hashFile							= NULL;
	
	if (inGUIDString != NULL)
	{
		pathSize = sizeof(kShadowHashDirPath) + strlen(inGUIDString) + 1;
	}
	else
	{
		pathSize = sizeof(kShadowHashDirPath) + strlen(inUserName) + 1;
	}
	
	// get path
	path = (char *) calloc( pathSize, 1 );
	if ( path != NULL )
	{
		if ( inGUIDString != NULL )
		{
			strcpy( path, kShadowHashDirPath );
			strcat( path, inGUIDString );
		}
		else
		{
			strcpy( path, kShadowHashDirPath );
			strcat( path, inUserName );
		}
		
		// read file
		hashFile = fopen( path, "r" );
		if ( hashFile != NULL )
		{
			bzero( hexHashes, sizeof(hexHashes) );
			readBytes = fread( hexHashes, 1, kHashTotalHexLength, hashFile );
			fclose( hashFile );
			hashFile = NULL;
			
			// check that enough bytes are present
			if ( readBytes >= kHashShadowBothHexLength )
			{
				ConvertHexToBinary( hexHashes, outHashes, outLen );
				siResult = kAuthNoError;
			}
			
			bzero( hexHashes, kHashTotalHexLength );
		}
		
		free( path );
		path = NULL;
	}
	
	return( siResult );
} // ReadShadowHash


//--------------------------------------------------------------------------------------------------
// * GenerateShadowHashes
//--------------------------------------------------------------------------------------------------

void GenerateShadowHashes(
	const char *inPassword,
	long inPasswordLen,
	const unsigned char *inSHA1Salt,
	unsigned char *outHashes,
	unsigned long *outHashTotalLength )
{
	SHA_CTX				sha_context							= {};
	unsigned char		digestData[kHashSecureLength]		= {0};
	long				pos									= 0;
	unsigned long		salt;
	
	/* start clean */
	bzero( outHashes, kHashTotalLength );
	
	/* NT */
	CalculateSMBNTHash( inPassword, outHashes );
	
	/* Skip LM (if present, NT is present) */
	pos = kHashShadowBothLength;
	
	/* skip SHA1 - Deprecated */
	pos += kHashSecureLength;
	
	/* skip CRAM-MD5 (not on desktop) */
	pos += kHashCramLength;
	
	/* 4-byte Salted SHA1 */
	if ( inSHA1Salt != NULL )
	{
		memcpy( &salt, inSHA1Salt, 4 );
		memcpy( outHashes + pos, inSHA1Salt, 4 );

		pos += 4;
		SHA1_Init( &sha_context );
		SHA1_Update( &sha_context, (unsigned char *)&salt, 4 );
		SHA1_Update( &sha_context, (unsigned char *)inPassword, inPasswordLen );
		SHA1_Final( digestData, &sha_context );
		memmove( outHashes + pos, digestData, kHashSecureLength );
	}
	pos += kHashSecureLength;
	
	/* skip recoverable (not on desktop) */
	
	/* TODO: add "Security Team Favorite" hash when we find out what it is */
	
	*outHashTotalLength = kHashTotalLength;
}


//----------------------------------------------------------------------------------------------------
//  HashesEqual
//
//  Returns: BOOL
//
// ================================================================================
//	Hash File Matrix (Tiger)
// ---------------------------------------------------------------------
//	Hash Type						 Desktop		 Server		Priority
// ---------------------------------------------------------------------
//		NT								X				X			3
//		LM							   Opt.				X			4
//	   SHA1							  Erase			  Erase			-
//	 CRAM-MD5											X			5
//	Salted SHA1						   Opt.			   Opt.			2
//	RECOVERABLE										   Opt.			6
//	Security Team Favorite			  Only			  Only			1
//	
// ================================================================================
//----------------------------------------------------------------------------------------------------

int HashesEqual( const unsigned char *inUserHashes, const unsigned char *inGeneratedHashes )
{
	int idx;
	int start, len;
	int result = 0;
	unsigned char zeros[kHashRecoverableLength];
	
	static int sPriorityMap[  ][ 2 ] =
	{
		//	start, len
						// security team favorite goes here //
		{ kHashShadowBothLength + kHashSecureLength + kHashCramLength, kHashSaltedSHA1Length },	// Salted SHA1
		{ 0, kHashShadowOneLength },															// NT
		{ 0, 0 }																				// END
	};
	
	bzero( zeros, sizeof(zeros) );
	
	for ( idx = 0;; idx++ )
	{
		start = sPriorityMap[idx][0];
		len = sPriorityMap[idx][1];
		
		if ( start == 0 && len == 0 )
			break;
		
		// verify with the highest priority hash that exists
		if ( memcmp( inUserHashes + start, zeros, len ) != 0 )
		{
			if ( memcmp( inUserHashes + start, inGeneratedHashes + start, len ) == 0 )
				result = 1;
			
			// stop here - do not fallback to lower priority hashes
			break;
		}
	}
	
	return result;
}


int DoSHAuth(char* inUserName, char* inPassword, char* inGUID)
{
	int				result								= kAuthOtherError;
	unsigned char	calculatedHashes[kHashTotalLength]	= {0};
	unsigned long	calculatedHashesLen					= 0;
	unsigned char	storedHashes[kHashTotalLength]		= {0};
	unsigned long	storedHashesLen						= 0;
	unsigned char	*saltPtr							= NULL;
	
	if ( (inUserName == NULL) || (inPassword == NULL) )
	{
		return result;
	}

	if ( ReadShadowHash( inUserName, inGUID, storedHashes, &storedHashesLen ) == kAuthNoError )
	{
		if ( storedHashesLen >= kHashShadowBothLength + kHashSecureLength + kHashCramLength + kHashSaltedSHA1Length )
			saltPtr = storedHashes + kHashShadowBothLength + kHashSecureLength + kHashCramLength;
		
		GenerateShadowHashes( inPassword, strlen(inPassword), saltPtr, calculatedHashes, &calculatedHashesLen );
		if ( HashesEqual( storedHashes, calculatedHashes ) == 1 )
			result = kAuthNoError;
	}
	
	return result;
}

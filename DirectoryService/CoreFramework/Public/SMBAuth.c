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
 
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/des.h>
#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <syslog.h>

#include "DSEndian.h"

/*!
 * @header SMBAuth
 */

#include <stdio.h>
#include <sys/types.h>
#include "SMBAuth.h"

enum {
    NTLM_NONCE_LENGTH		= 8,
    NTLM_HASH_LENGTH		= 21,
    NTLM_RESP_LENGTH		= 24,
    NTLM_SESSKEY_LENGTH		= 16,
};

/* utility functions prototypes */
#ifdef __cplusplus
extern "C" {
#endif

u_int16_t ByteSwapInt16(u_int16_t value);
void CStringToUnicode(char *cstr, u_int16_t *unicode);
void strnupper(char *str, int maxlen);

#ifdef __cplusplus
}
#endif

#define kDESVersion1	1

/*
 * Pads used in key derivation
 */

 unsigned char SHSpad1[40] =
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

 unsigned char SHSpad2[40] =
   {0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
    0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
    0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
    0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2};

/*
 * "Magic" constants used in key derivations
 */
 
 unsigned char Magic1[27] =
   {0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74,
    0x68, 0x65, 0x20, 0x4d, 0x50, 0x50, 0x45, 0x20, 0x4d,
    0x61, 0x73, 0x74, 0x65, 0x72, 0x20, 0x4b, 0x65, 0x79};

unsigned char Magic2[84] =
   {0x4f, 0x6e, 0x20, 0x74, 0x68, 0x65, 0x20, 0x63, 0x6c, 0x69,
    0x65, 0x6e, 0x74, 0x20, 0x73, 0x69, 0x64, 0x65, 0x2c, 0x20,
    0x74, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68,
    0x65, 0x20, 0x73, 0x65, 0x6e, 0x64, 0x20, 0x6b, 0x65, 0x79,
    0x3b, 0x20, 0x6f, 0x6e, 0x20, 0x74, 0x68, 0x65, 0x20, 0x73,
    0x65, 0x72, 0x76, 0x65, 0x72, 0x20, 0x73, 0x69, 0x64, 0x65,
    0x2c, 0x20, 0x69, 0x74, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68,
    0x65, 0x20, 0x72, 0x65, 0x63, 0x65, 0x69, 0x76, 0x65, 0x20,
    0x6b, 0x65, 0x79, 0x2e};

unsigned char Magic3[84] =
   {0x4f, 0x6e, 0x20, 0x74, 0x68, 0x65, 0x20, 0x63, 0x6c, 0x69,
    0x65, 0x6e, 0x74, 0x20, 0x73, 0x69, 0x64, 0x65, 0x2c, 0x20,
    0x74, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68,
    0x65, 0x20, 0x72, 0x65, 0x63, 0x65, 0x69, 0x76, 0x65, 0x20,
    0x6b, 0x65, 0x79, 0x3b, 0x20, 0x6f, 0x6e, 0x20, 0x74, 0x68,
    0x65, 0x20, 0x73, 0x65, 0x72, 0x76, 0x65, 0x72, 0x20, 0x73,
    0x69, 0x64, 0x65, 0x2c, 0x20, 0x69, 0x74, 0x20, 0x69, 0x73,

    0x20, 0x74, 0x68, 0x65, 0x20, 0x73, 0x65, 0x6e, 0x64, 0x20,
    0x6b, 0x65, 0x79, 0x2e};
	
/* Utility functions */


void CalculateP24(unsigned char *P21, unsigned char *C8, unsigned char *P24)
{
	// setup P24
	memcpy(P24, C8, 8);
	memcpy(P24+8, C8, 8);
	memcpy(P24+16, C8, 8);

	DESEncode(P21, P24);
	DESEncode(P21+7, P24+8);
	DESEncode(P21+14, P24+16);
}

void CalculateSMBNTHash(const char *utf8Password, unsigned char outHash[16])
{
	u_int16_t unicodeLen = 0;
	u_int16_t unicodepwd[258] = {0};
	char *password[128] = {0};
	int passLen = 0;
	//unsigned char P21[21] = {0};
	
	if (utf8Password == NULL || outHash == NULL) return;
	
	passLen = strlen(utf8Password);
	if (passLen > 128)
		passLen = 128;
	
	memmove(password, utf8Password, passLen);
	unicodeLen = strlen((char *)password) * sizeof(u_int16_t);
		
	CStringToUnicode((char *)password, unicodepwd);
	MD4Encode(outHash, (unsigned char *)unicodepwd, unicodeLen);	
}

void CalculateSMBLANManagerHash(const char *password, unsigned char outHash[16])
{
	unsigned char S8[8] = {0x4B, 0x47, 0x53, 0x21, 0x40, 0x23, 0x24, 0x25};
	int passLen = 0;
	unsigned char P21[21] = {0};
	unsigned char P14[14] = {0};
	unsigned char *P16 = P21;

	passLen = strlen(password);
	if ( passLen > 14 )
		passLen = 14;

	// setup P14
	memmove(P14, password, passLen);
	strnupper((char *)P14, 14);

	// setup P16
	memmove(P16, S8, 8);
	memmove(P16+8, S8, 8);

	DESEncode(P14, P16);
	DESEncode(P14+7, P16+8);

	memmove(outHash, P16, 16);
}


int32_t LittleEndianCharsToInt32( const char *inCharPtr )
{
	int32_t anInt;
	
	memcpy( &anInt, inCharPtr, 4 );
	anInt = EndianU32_BtoL( anInt );
	
	return anInt;
}


void CalculateWorkstationCredentialSessKey( const unsigned char inNTHash[16], const char serverChallenge[8], const char clientChallenge[8], unsigned char outWCSK[8] )
{
	int32_t schal, cchal;
	int32_t add1, add2;
	
	schal = LittleEndianCharsToInt32( serverChallenge );
	cchal = LittleEndianCharsToInt32( clientChallenge );
	add1 = schal + cchal;
	
	schal = LittleEndianCharsToInt32( serverChallenge + 4 );
	cchal = LittleEndianCharsToInt32( clientChallenge + 4 );
	add2 = schal + cchal;
	
	add1 = EndianU32_BtoL( add1 );
	add2 = EndianU32_BtoL( add2 );
		
	memcpy( outWCSK, &add1, 4 );
	memcpy( outWCSK + 4, &add2, 4 );
	
	DESEncode( inNTHash, outWCSK );
	DESEncode( inNTHash + 9, outWCSK );	
}


void CalculatePPTPSessionKeys( const unsigned char inNTHash[16], const unsigned char inNTResponse[24], int inSessionKeyLen, unsigned char *outSendKey, unsigned char *outReceiveKey )
{
	unsigned char hashHashBuff[MD4_DIGEST_LENGTH + 1];
    unsigned char masterKey[16];
	
	MD4Encode( hashHashBuff, inNTHash, MD4_DIGEST_LENGTH );
	GetMasterKey( hashHashBuff, inNTResponse, masterKey );
	GetAsymetricStartKey( masterKey, outSendKey, inSessionKeyLen, TRUE, TRUE );
	GetAsymetricStartKey( masterKey, outReceiveKey, inSessionKeyLen, FALSE, TRUE );
}


void GetMasterKey( const unsigned char inNTHashHash[16], const unsigned char inNTResponse[24], unsigned char outMasterKey[16] )
{
	unsigned char Digest[20];
	SHA_CTX Context;
	
	bzero( Digest, sizeof(Digest) );

	SHA1_Init( &Context );
	SHA1_Update( &Context, inNTHashHash, 16 );
	SHA1_Update( &Context, inNTResponse, 24 );
	SHA1_Update( &Context, Magic1, 27 );
	SHA1_Final( Digest, &Context );

	memmove( outMasterKey, Digest, 16 );
}


void GetAsymetricStartKey( unsigned char inMasterKey[16], unsigned char *outSessionKey, int inSessionKeyLen, bool inIsSendKey, bool inIsServer )
{
	unsigned char Digest[20];
	unsigned char *magicPtr;
	SHA_CTX Context;
	
	if ( inSessionKeyLen > (int)sizeof(Digest) )
		return;
	
	bzero( Digest, sizeof(Digest) );
	
	if ( inIsSendKey )
	{
		if (inIsServer) {
			magicPtr = Magic3;
		}
		else {
			magicPtr = Magic2;
		}
	}
	else
	{
		if (inIsServer) {
			magicPtr = Magic2;
		} else {
			magicPtr = Magic3;
		}
	}
	
	SHA1_Init( &Context );
	SHA1_Update( &Context, inMasterKey, 16 );
	SHA1_Update( &Context, SHSpad1, 40 );
	SHA1_Update( &Context, magicPtr, 84 );
	SHA1_Update( &Context, SHSpad2, 40 );
	SHA1_Final( Digest, &Context );
	
	memmove( outSessionKey, Digest, inSessionKeyLen );
}

#pragma mark -
#pragma mark NTLM
#pragma mark -

/*
	Note: <V2> must be at least EVP_MAX_MD_SIZE
*/

int NTLMv2(unsigned char *V2, unsigned char *inNTHash,
		 const char *authid, const char *target,
		 const unsigned char *challenge,
		 const unsigned char *blob, unsigned bloblen)
{
    HMAC_CTX ctx;
    char *upper;
    int len;
	char *buf;
	unsigned buflen;
	
    /* Allocate enough space for the unicode target */
    len = strlen(authid) + strlen(target);
	buflen = 2 * len + 1;
	buf = (char *) malloc( buflen );
	if ( buf == NULL )
		return -1;
	
	/* NTLMv2hash = HMAC-MD5(NTLMhash, unicode(ucase(authid + domain))) */
	
	/* Use the tail end of the buffer for ucase() conversion */
	upper = buf + len;
	strcpy(upper, authid);
	if (target)
		strcat(upper, target);
	strnupper(upper, len);
	CStringToUnicode(upper, (u_int16_t *)buf);
	
	HMAC(EVP_md5(), inNTHash, MD4_DIGEST_LENGTH, buf, 2 * len, inNTHash, &len);
	
	/* V2 = HMAC-MD5(NTLMv2hash, challenge + blob) + blob */
	HMAC_Init(&ctx, inNTHash, len, EVP_md5());
	HMAC_Update(&ctx, challenge, NTLM_NONCE_LENGTH);
	HMAC_Update(&ctx, blob, bloblen);
	HMAC_Final(&ctx, V2, &len);
	HMAC_cleanup(&ctx);
	
	/* the blob is concatenated outside of this function */
	
	free( buf );
    return 0;
}


void CalculateNTLMv2SessionKey(
	const unsigned char *inServerChallenge,
	const unsigned char *inClientChallenge,
	const unsigned char *inNTLMHash,
	unsigned char *outSessionKey )
{
	MD5_CTX ctx;
    unsigned char md5ResultOnly8BytesUsed[MD5_DIGEST_LENGTH];
	unsigned char paddedHash[21];
	
	MD5_Init( &ctx );
	MD5_Update( &ctx, inServerChallenge, 8 );
	MD5_Update( &ctx, inClientChallenge, 8 );
	MD5_Final( md5ResultOnly8BytesUsed, &ctx );
	
	memcpy( paddedHash, inNTLMHash, 16 );
	bzero(paddedHash + 16, 5);
	
	CalculateP24( paddedHash, md5ResultOnly8BytesUsed, outSessionKey );
}


#pragma mark -
#pragma mark Utilities
#pragma mark -

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


void LittleEndianUnicodeToUnicode(const u_int16_t *unistr, int unistrLen, u_int16_t *unicode)
{
	int i;
	u_int16_t val;
 	 
	for(i = 0; i < unistrLen; i++)
	{
		val = *unistr;
		if (BYTE_ORDER == BIG_ENDIAN)
			*unicode = ByteSwapInt16(val);
		else
			*unicode = val;			
		unicode++;
		unistr++;
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

void strnupper(char *str, int maxlen)
{
	char *s = str;

	while (*s && maxlen)
	{
		if (islower(*s))
			*s = toupper(*s);
		s++;
		maxlen--;
	}
}

void DESEncode(const void *str, void *data)
{
	des_key_schedule theKeyArray;
	unsigned char	key[8]	= {};
	des_cblock output;

	str_to_key((unsigned char *)str, key);
	des_set_key_unchecked((const_des_cblock *)key, theKeyArray);
	des_ecb_encrypt((const_des_cblock *)data,  &output, theKeyArray, kDESVersion1);
	memcpy(data,output,8);
}

void str_to_key(const unsigned char *str, unsigned char key[8])
{
	int i;
	key[0] = str[0]>>1;
	key[1] = ((str[0]&0x01)<<6) | (str[1]>>2);
	key[2] = ((str[1]&0x03)<<5) | (str[2]>>3);
	key[3] = ((str[2]&0x07)<<4) | (str[3]>>4);
	key[4] = ((str[3]&0x0F)<<3) | (str[4]>>5);
	key[5] = ((str[4]&0x1F)<<2) | (str[5]>>6);
	key[6] = ((str[5]&0x3F)<<1) | (str[6]>>7);
	key[7] = str[6]&0x7F;
	for (i=0;i<8;i++) {
		key[i] = (key[i]<<1);
	}
	des_set_odd_parity((des_cblock *)&key); //des_cblock *key // typedef unsigned char des_cblock[8];
}


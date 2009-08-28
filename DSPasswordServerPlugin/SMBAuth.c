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
#include <syslog.h>
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonHMAC.h>

#include <CoreServices/CoreServices.h>

/*!
 * @header SMBAuth
 */

#include "SMBAuth.h"
#include <stdio.h>
#include <sys/types.h>

#define kUnicodeStrMaxLen		258
#define kDESVersion1			1

static u_int16_t ByteSwapInt16(u_int16_t value);
static void str_to_key(const unsigned char *str, des_cblock key);

/*
 * Pads used in key derivation
 */

 unsigned char pwsf_SHSpad1[40] =
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

 unsigned char pwsf_SHSpad2[40] =
   {0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
    0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
    0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2,
    0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2, 0xf2};

/*
 * "Magic" constants used in key derivations
 */
 
unsigned char pwsf_Magic1[27] =
   {0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74,
    0x68, 0x65, 0x20, 0x4d, 0x50, 0x50, 0x45, 0x20, 0x4d,
    0x61, 0x73, 0x74, 0x65, 0x72, 0x20, 0x4b, 0x65, 0x79};

unsigned char pwsf_Magic2[84] =
   {0x4f, 0x6e, 0x20, 0x74, 0x68, 0x65, 0x20, 0x63, 0x6c, 0x69,
    0x65, 0x6e, 0x74, 0x20, 0x73, 0x69, 0x64, 0x65, 0x2c, 0x20,
    0x74, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68,
    0x65, 0x20, 0x73, 0x65, 0x6e, 0x64, 0x20, 0x6b, 0x65, 0x79,
    0x3b, 0x20, 0x6f, 0x6e, 0x20, 0x74, 0x68, 0x65, 0x20, 0x73,
    0x65, 0x72, 0x76, 0x65, 0x72, 0x20, 0x73, 0x69, 0x64, 0x65,
    0x2c, 0x20, 0x69, 0x74, 0x20, 0x69, 0x73, 0x20, 0x74, 0x68,
    0x65, 0x20, 0x72, 0x65, 0x63, 0x65, 0x69, 0x76, 0x65, 0x20,
    0x6b, 0x65, 0x79, 0x2e};

unsigned char pwsf_Magic3[84] =
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


static int unicode_strlen( const uint16_t *s )
{
	int len = 0;
	while (*s++)
		len++;
	
	return len;
}

void pwsf_CalculateP24(unsigned char *P21, unsigned char *C8, unsigned char *P24)
{
	// setup P24
	memcpy(P24, C8, 8);
	memcpy(P24+8, C8, 8);
	memcpy(P24+16, C8, 8);

	pwsf_DESEncodeV1(P21, P24);
	pwsf_DESEncodeV1(P21+7, P24+8);
	pwsf_DESEncodeV1(P21+14, P24+16);
}

void pwsf_CalculateSMBNTHash(const char *utf8Password, unsigned char outHash[16])
{
	u_int16_t unicodeLen = 0;
	u_int16_t unicodepwd[kUnicodeStrMaxLen] = {0};
	char password[128] = {0};
	size_t passLen = 0;
	
	if (utf8Password == NULL || outHash == NULL) return;
	
	passLen = strlen(utf8Password);
	if (passLen > 128)
		passLen = 128;
	
	memmove(password, utf8Password, passLen);
	pwsf_CStringToUnicode((char *)password, unicodepwd);
	unicodeLen = unicode_strlen(unicodepwd);
	pwsf_MD4Encode(outHash, (unsigned char *)unicodepwd, unicodeLen * 2);
	bzero(unicodepwd, unicodeLen);
}

void pwsf_CalculateSMBLANManagerHash(const char *password, unsigned char outHash[16])
{
	unsigned char S8[8] = {0x4B, 0x47, 0x53, 0x21, 0x40, 0x23, 0x24, 0x25};
	size_t passLen = 0;
	unsigned char P21[21] = {0};
	unsigned char P14[14] = {0};
	unsigned char *P16 = P21;
	
	passLen = strlen(password);
	if ( passLen > 14 )
		passLen = 14;
	
	// setup P14
	memmove(P14, password, passLen);
	pwsf_strnupper((char *)P14, 14);
	
	// setup P16
	memmove(P16, S8, 8);
	memmove(P16+8, S8, 8);

	pwsf_DESEncodeV1(P14, P16);
	pwsf_DESEncodeV1(P14+7, P16+8);

	memmove(outHash, P16, 16);
}

void pwsf_CalculateWorkstationCredentialStrongSessKey( const unsigned char inNTHash[16], const char serverChallenge[8], const char clientChallenge[8], unsigned char outWCSK[16] )
{
	uint32_t zero = 0;
	CCHmacContext ctx;
	CC_MD5_CTX md5;
	unsigned char tmp[16];

	CCHmacInit(&ctx, kCCHmacAlgMD5, inNTHash, CC_MD5_DIGEST_LENGTH);

	CC_MD5_Init(&md5);
	CC_MD5_Update(&md5, &zero, 4);
	CC_MD5_Update(&md5, clientChallenge, 8);
	CC_MD5_Update(&md5, serverChallenge, 8);
	CC_MD5_Final(tmp, &md5);
	
	CCHmacUpdate(&ctx, tmp, 16);
	CCHmacFinal(&ctx, outWCSK);
	memset(tmp, 0, sizeof(tmp));
	memset(&ctx, 0, sizeof(ctx));
	return;
}

void pwsf_CalculateWorkstationCredentialSessKey( const unsigned char inNTHash[16], const char serverChallenge[8], const char clientChallenge[8], unsigned char outWCSK[8] )
{
	int32_t schal, cchal;
	int32_t add1, add2;
	
	schal = pwsf_LittleEndianCharsToInt32( serverChallenge );
	cchal = pwsf_LittleEndianCharsToInt32( clientChallenge );
	add1 = schal + cchal;
	
	schal = pwsf_LittleEndianCharsToInt32( serverChallenge + 4 );
	cchal = pwsf_LittleEndianCharsToInt32( clientChallenge + 4 );
	add2 = schal + cchal;
	
	add1 = EndianU32_NtoL( add1 );
	add2 = EndianU32_NtoL( add2 );
		
	memcpy( outWCSK, &add1, 4 );
	memcpy( outWCSK + 4, &add2, 4 );
	
	pwsf_DESEncodeV1( inNTHash, outWCSK );
	pwsf_DESEncodeV1( inNTHash + 9, outWCSK );	
}


void pwsf_CalculatePPTPSessionKeys( const unsigned char inNTHash[16], const unsigned char inNTResponse[24], int inSessionKeyLen, unsigned char *outSendKey, unsigned char *outReceiveKey )
{
	unsigned char hashHashBuff[MD4_DIGEST_LENGTH + 1];
    unsigned char masterKey[16];
	
	pwsf_MD4Encode( hashHashBuff, inNTHash, MD4_DIGEST_LENGTH );
	pwsf_GetMasterKey( hashHashBuff, inNTResponse, masterKey );
	pwsf_GetAsymetricStartKey( masterKey, outSendKey, inSessionKeyLen, TRUE, TRUE );
	pwsf_GetAsymetricStartKey( masterKey, outReceiveKey, inSessionKeyLen, FALSE, TRUE );
}


void pwsf_GetMasterKey( const unsigned char inNTHashHash[16], const unsigned char inNTResponse[24], unsigned char outMasterKey[16] )
{
	unsigned char Digest[20];
	SHA_CTX Context;
	
	bzero( Digest, sizeof(Digest) );

	SHA1_Init( &Context );
	SHA1_Update( &Context, inNTHashHash, 16 );
	SHA1_Update( &Context, inNTResponse, 24 );
	SHA1_Update( &Context, pwsf_Magic1, 27 );
	SHA1_Final( Digest, &Context );

	memmove( outMasterKey, Digest, 16 );
}


void pwsf_GetAsymetricStartKey( unsigned char inMasterKey[16], unsigned char *outSessionKey, int inSessionKeyLen, bool inIsSendKey, bool inIsServer )
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
			magicPtr = pwsf_Magic3;
		}
		else {
			magicPtr = pwsf_Magic2;
		}
	}
	else
	{
		if (inIsServer) {
			magicPtr = pwsf_Magic2;
		} else {
			magicPtr = pwsf_Magic3;
		}
	}
	
	SHA1_Init( &Context );
	SHA1_Update( &Context, inMasterKey, 16 );
	SHA1_Update( &Context, pwsf_SHSpad1, 40 );
	SHA1_Update( &Context, magicPtr, 84 );
	SHA1_Update( &Context, pwsf_SHSpad2, 40 );
	SHA1_Final( Digest, &Context );
	
	memmove( outSessionKey, Digest, inSessionKeyLen );
}


#pragma mark -
#pragma mark Utilities
#pragma mark -

int32_t pwsf_LittleEndianCharsToInt32( const char *inCharPtr )
{
	int32_t anInt;
	
	memcpy( &anInt, inCharPtr, 4 );
	anInt = EndianU32_NtoL( anInt );
	
	return anInt;
}


u_int16_t ByteSwapInt16(u_int16_t value)
{
	u_int16_t mask = value;
	mask <<= 8;
	value >>= 8;
	value |= mask;
	return value;
}


void pwsf_CStringToUnicode(char *cstr, u_int16_t *unicode)
{
	CFStringRef convertString = CFStringCreateWithCString( NULL, cstr, kCFStringEncodingUTF8 );
	if ( convertString != NULL ) {
		 CFStringGetCString( convertString, (char *)unicode, kUnicodeStrMaxLen, kCFStringEncodingUTF16LE );
		 CFRelease( convertString );
	}
}


void pwsf_LittleEndianUnicodeToUnicode(const u_int16_t *unistr, int unistrLen, u_int16_t *unicode)
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


void pwsf_MD4Encode(unsigned char *output, const unsigned char *input, unsigned int len)
{

  MD4_CTX context = {};

  MD4_Init (&context);
  MD4_Update (&context, (unsigned char *)input, len);
  MD4_Final (output, &context);
}

void pwsf_strnupper(char *str, int maxlen)
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

void pwsf_DESEncodeV1(const void *str, void *data)
{
	unsigned char key[8] = {};

#if USE_LIBCRYPTO
	des_key_schedule theKeyArray;
	des_cblock output;

	str_to_key((unsigned char *)str, key);
	des_set_key_unchecked((const_des_cblock *)key, theKeyArray);
	des_ecb_encrypt((const_des_cblock *)data,  &output, theKeyArray, DES_ENCRYPT);
	memcpy(data,output,8);
#else
	CCCryptorStatus status = kCCSuccess;
	size_t dataMoved = 0;
	char output[8] = {0};
	
	str_to_key((unsigned char *)str, key);
	status = CCCrypt( kCCEncrypt, kCCAlgorithmDES, 0,
						key, sizeof(key),
						NULL,
						data, 8,
						output, 8, &dataMoved );
	if ( status == kCCSuccess )
		memcpy( data, output, 8 );
	else
		bzero( output, 8 );
#endif
}


void str_to_key(const unsigned char *str, des_cblock key)
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
	des_set_odd_parity((des_cblock*)&key);
}

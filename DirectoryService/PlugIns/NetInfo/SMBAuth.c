/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header SMBAuth
 */
#include "TimConditional.h"
#ifdef TIM_CLIENT_PRESENT

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "md4.h"

#include "SMBAuth.h"

#include <TimClient/Buffer.h>

typedef long KeysArray[32];

typedef struct EncryptBlk
{
	unsigned long 	keyHi;
	unsigned long 	keyLo;

} EncryptBlk;

// utility functions prototypes
#ifdef __cplusplus
extern "C" {
#endif

u_int16_t ByteSwapInt16(u_int16_t value);
void CStringToUnicode(char *cstr, u_int16_t *unicode);
void MD4Encode(unsigned char *output, unsigned char *input, unsigned int len);
void strnupper(char *str, int maxlen);
void DESEncode(void *str, void *data);
void des_set_odd_parity(unsigned char *key);
void str_to_key(unsigned char *str, unsigned char *key);

#ifdef __cplusplus
}
#endif

extern void desKeySched(EncryptBlk *, long *, short);
extern void desEncode(long *, char *);
extern void desDecode (long *, char *);

#define kDESVersion1	1

unsigned char odd_parity[256]={
	1,  1,  2,  2,  4,  4,  7,  7,  8,  8, 11, 11, 13, 13, 14, 14,
	16, 16, 19, 19, 21, 21, 22, 22, 25, 25, 26, 26, 28, 28, 31, 31,
	32, 32, 35, 35, 37, 37, 38, 38, 41, 41, 42, 42, 44, 44, 47, 47,
	49, 49, 50, 50, 52, 52, 55, 55, 56, 56, 59, 59, 61, 61, 62, 62,
	64, 64, 67, 67, 69, 69, 70, 70, 73, 73, 74, 74, 76, 76, 79, 79,
	81, 81, 82, 82, 84, 84, 87, 87, 88, 88, 91, 91, 93, 93, 94, 94,
	97, 97, 98, 98, 100, 100, 103, 103, 104, 104, 107, 107, 109, 109, 110, 110,
	112, 112, 115, 115, 117, 117, 118, 118, 121, 121, 122, 122, 124, 124, 127, 127,
	128, 128, 131, 131, 133, 133, 134, 134, 137, 137, 138, 138, 140, 140, 143, 143,
	145, 145, 146, 146, 148, 148, 151, 151, 152, 152, 155, 155, 157, 157, 158, 158,
	161, 161, 162, 162, 164, 164, 167, 167, 168, 168, 171, 171, 173, 173, 174, 174,
	176, 176, 179, 179, 181, 181, 182, 182, 185, 185, 186, 186, 188, 188, 191, 191,
	193, 193, 194, 194, 196, 196, 199, 199, 200, 200, 203, 203, 205, 205, 206, 206,
	208, 208, 211, 211, 213, 213, 214, 214, 217, 217, 218, 218, 220, 220, 223, 223,
	224, 224, 227, 227, 229, 229, 230, 230, 233, 233, 234, 234, 236, 236, 239, 239,
	241, 241, 242, 242, 244, 244, 247, 247, 248, 248, 251, 251, 253, 253, 254, 254};

// Utility functions

void BinaryFromHexString(char * sourceString, unsigned int maxLen, unsigned char* outBinary)
{
	//char* resultString = NULL;
	Buffer* sourceBuff = bufferFromDataNoCopy(sourceString,maxLen);
	Buffer* resultBuff = bufferFromHexBuffer(sourceBuff);
	//resultString = bufferToString(resultBuff);
	if (resultBuff != NULL)
	{
		memmove(outBinary,resultBuff->data,resultBuff->length);
	}
	bufferRelease(sourceBuff);
	bufferRelease(resultBuff);

	//return resultString;
}

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
	
	if (strlen(utf8Password) < 128)
		passLen = strlen(utf8Password);
	else
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

	if (strlen(password) < 14)
		passLen = strlen(password);
	else
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

void MD4Encode(unsigned char *output, unsigned char *input, unsigned int len)
{

  MD4_CTX context = {};

  MD4Init (&context);
  MD4Update (&context, (unsigned char *)input, len);
  MD4Final (output, &context);
}

char* HexStringFromBinary(unsigned char * sourceString, unsigned int len)
{
	char* resultString = NULL;
	Buffer* sourceBuff = bufferFromDataNoCopy(sourceString,len);
	Buffer* resultBuff = bufferToHexBuffer(sourceBuff);
	resultString = bufferToString(resultBuff);
	bufferRelease(sourceBuff);
	bufferRelease(resultBuff);
	
	return resultString;
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

void DESEncode(void *str, void *data)
{
	KeysArray theKeyArray = {};
	unsigned char	key[8] = {};

	str_to_key((unsigned char *)str, key);
	desKeySched((EncryptBlk *)key, theKeyArray, kDESVersion1);
	desEncode(theKeyArray, (char *)data);
}

void des_set_odd_parity(unsigned char *key)
{
	int i;

	for (i=0; i<8; i++)
		key[i]=odd_parity[key[i]];
}

void str_to_key(unsigned char *str, unsigned char *key)
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
	des_set_odd_parity(key);
}
#endif

/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*!
 * @header SMBAuth
 */

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <openssl/md4.h>
#include <openssl/des.h>
#include "SMBAuth.h"

// utility functions prototypes
#ifdef __cplusplus
extern "C" {
#endif

u_int16_t ByteSwapInt16(u_int16_t value);
void CStringToUnicode(char *cstr, u_int16_t *unicode);
void MD4Encode(unsigned char *output, unsigned char *input, unsigned int len);
void strnupper(char *str, int maxlen);
void DESEncode(void *str, void *data);
void str_to_key(unsigned char *str, unsigned char key[8]);

#ifdef __cplusplus
}
#endif

#define kDESVersion1	1

// Utility functions

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

void DESEncode(void *str, void *data)
{
	des_key_schedule theKeyArray;
	unsigned char	key[8]	= {};
	des_cblock output;

	str_to_key((unsigned char *)str, key);
	des_set_key_unchecked((const_des_cblock *)key, theKeyArray);
	des_ecb_encrypt((const_des_cblock *)data,  &output, theKeyArray, kDESVersion1);
	memcpy(data,output,8);
}

void str_to_key(unsigned char *str, unsigned char key[8])
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


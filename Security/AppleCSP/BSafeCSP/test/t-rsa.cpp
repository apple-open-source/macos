/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


#include <bsafe.h>
#include <aglobal.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


B_ALGORITHM_METHOD *chooser[] = {
  &AM_SHA_RANDOM,
  &AM_RSA_KEY_GEN,
  &AM_RSA_STRONG_KEY_GEN,
  &AM_RSA_ENCRYPT,
  &AM_RSA_DECRYPT,
  &AM_RSA_CRT_ENCRYPT,
  &AM_RSA_CRT_DECRYPT,
  NULL
};


char in[] = "something wicked this way comes, "
	"and it's a private key!";
unsigned char crypt[1024];
unsigned char out[1024], out2[1024];

unsigned char seed[] = { 17, 22, 99, 205, 3 };

unsigned char exponent[] = { 1, 0, 1 };

#define check(expr) \
  if (status = (expr)) { printf("error %d at %d\n", status, __LINE__); abort(); } else /* ok */

int main(int argc, char *argv[])
{
	int status;

	int keySize = argv[1] ? atoi(argv[1]) : 512;
	printf("Key size = %d bits\n", keySize);

	B_KEY_OBJ pubKey = NULL; check(B_CreateKeyObject(&pubKey));
	B_KEY_OBJ privKey = NULL; check(B_CreateKeyObject(&privKey));

	B_ALGORITHM_OBJ random = NULL; check(B_CreateAlgorithmObject(&random));
	check(B_SetAlgorithmInfo(random, AI_X962Random_V0, NULL));
	check(B_RandomInit(random, chooser, NULL));
	check(B_RandomUpdate(random, seed, sizeof(seed), NULL));
	for (int n = 0; n < 5; n++) {
		unsigned char buf[4];
		check(B_GenerateRandomBytes(random,
			POINTER(buf), sizeof(buf), NULL));
		printf("Randoms = ");
		for (int n = 0; n < sizeof(buf); n++)
			printf("%2.2x", buf[n]);
		printf("\n");
	}

	B_ALGORITHM_OBJ gen = NULL; check(B_CreateAlgorithmObject(&gen));
	A_RSA_KEY_GEN_PARAMS args;
	args.modulusBits = keySize;
	args.publicExponent.data = exponent;
	args.publicExponent.len = sizeof(exponent);
	check(B_SetAlgorithmInfo(gen, AI_RSAStrongKeyGen, POINTER(&args)));
	check(B_GenerateInit(gen, chooser, NULL));
	check(B_GenerateKeypair(gen, pubKey, privKey, random, NULL));

	B_ALGORITHM_OBJ enc = NULL; check(B_CreateAlgorithmObject(&enc));
	check(B_SetAlgorithmInfo(enc, AI_PKCS_RSAPublic, NULL));
	check(B_EncryptInit(enc, pubKey, chooser, NULL));
	unsigned int inLen;
	check(B_EncryptUpdate(enc, crypt, &inLen, sizeof(crypt),
		POINTER(in), sizeof(in), random, NULL));
	printf("EncryptUpdate output = %u\n", inLen);
	check(B_EncryptFinal(enc, crypt, &inLen, sizeof(crypt), random, NULL));
	printf("EncryptFinal output=%u\n", inLen);

	B_ALGORITHM_OBJ dec = NULL; check(B_CreateAlgorithmObject(&dec));
	check(B_SetAlgorithmInfo(dec, AI_PKCS_RSAPrivate, NULL));
	check(B_DecryptInit(dec, privKey, chooser, NULL));
	unsigned int outLen, outLen2;
	check(B_DecryptUpdate(dec, out, &outLen, sizeof(out),
		crypt, inLen, random, NULL));
	printf("DecryptUpdate output = %u\n", outLen);
	check(B_DecryptFinal(dec, out2, &outLen2, sizeof(out2), random, NULL));
	printf("DecryptFinal output=%u %s\n", outLen2, (char*)out2);
	

	B_DestroyKeyObject(&pubKey);
	B_DestroyKeyObject(&privKey);
	B_DestroyAlgorithmObject(&random);
	exit(0);
}

void T_free(POINTER p)
{ free(p); }

POINTER T_malloc(unsigned int size)
{ return (POINTER)malloc(size); }

POINTER T_realloc(POINTER p, unsigned int size)
{ return (POINTER)realloc(p, size); }

int T_memcmp(POINTER p1, POINTER p2, unsigned int size)
{ return memcmp(p1, p2, size); }
void T_memcpy(POINTER p1, POINTER p2, unsigned int size)
{ memcpy(p1, p2, size); }
void T_memmove(POINTER p1, POINTER p2, unsigned int size)
{ memmove(p1, p2, size); }
void T_memset(POINTER p1, int size, unsigned int val)
{ memset(p1, size, val); }
extern "C" int T_GetDynamicList()
{ printf("GetDynamicList!\n"); abort(); }

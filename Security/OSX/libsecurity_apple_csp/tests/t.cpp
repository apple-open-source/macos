/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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
  &AM_SHA,
  &AM_SHA_RANDOM,
  NULL
};

void dumpItem(ITEM &item, const char *name);

unsigned char seed[] = { 17, 205, 99, 13, 6, 199 };
char data[] = "These are the times that try men's souls.";


#define check(expr) \
  if (status = (expr)) { printf("error %d at %d\n", status, __LINE__); abort(); } else /* ok */

int main(int argc, char *argv[])
{
	int status;

	ITEM key;
	key.data = (unsigned char *)"Walla Walla Washington! Yeah, yeah, yeah!";
	key.len = strlen((const char *)key.data);
	B_KEY_OBJ bsKey = NULL;
	check(B_CreateKeyObject(&bsKey));
	check(B_SetKeyInfo(bsKey, KI_Item, POINTER(&key)));

	B_ALGORITHM_OBJ macAlg = NULL;
	check(B_CreateAlgorithmObject(&macAlg));
	B_DIGEST_SPECIFIER macSpec;
	macSpec.digestInfoType = AI_SHA1;
	macSpec.digestInfoParams = NULL_PTR;
	check(B_SetAlgorithmInfo(macAlg, AI_HMAC, POINTER(&macSpec)));

	check(B_DigestInit(macAlg, bsKey, chooser, NULL));
	check(B_DigestUpdate(macAlg,
		POINTER(data), sizeof(data), NULL));
	char mac[128];
	unsigned int length;
	check(B_DigestFinal(macAlg, POINTER(mac), &length, sizeof(mac),
		NULL));
	ITEM macItem; macItem.data = POINTER(mac); macItem.len = length;
	dumpItem(macItem, "MAC");

	check(B_DigestUpdate(macAlg, POINTER(data), 10, NULL));
	check(B_DigestUpdate(macAlg,
		POINTER(data+10), sizeof(data)-10, NULL));
	check(B_DigestFinal(macAlg, POINTER(mac), &length, sizeof(mac),
		NULL));
	macItem.data = POINTER(mac); macItem.len = length;
	dumpItem(macItem, "MAC");

	printf("Done.\n");

	exit(0);
}

void dumpItem(ITEM &item, const char *name)
{
  printf("%s [%d] ", name, item.len);
  for (unsigned char *p = item.data; p < item.data + item.len; p++)
    printf("%2.2x", *p);
  printf("\n");
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

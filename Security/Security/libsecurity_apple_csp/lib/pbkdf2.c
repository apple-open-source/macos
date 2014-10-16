/*
 * Copyright (c) 2000-2001,2011,2013-2014 Apple Inc. All Rights Reserved.
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


/*
 	File:		pbkdf2.c
 	Contains:	Apple Data Security Services PKCS #5 PBKDF2 function definition.
 	Copyright (c) 1999,2011,2013-2014 Apple Inc. All Rights Reserved.
*/
#include "pbkdf2.h"
#include <ConditionalMacros.h>
#include <string.h>
/* Will write hLen bytes into dataPtr according to PKCS #5 2.0 spec.
   See: http://www.rsa.com/rsalabs/pubs/PKCS/html/pkcs-5.html for details. 
   tempBuffer is a pointer to at least MAX (hLen, saltLen + 4) + hLen bytes. */
static void 
F (PRF prf, uint32 hLen,
   const void *passwordPtr, uint32 passwordLen,
   const void *saltPtr, uint32 saltLen,
   uint32 iterationCount,
   uint32 blockNumber,
   void *dataPtr,
   void *tempBuffer)
{
	uint8 *inBlock, *outBlock, *resultBlockPtr;
	uint32 iteration;
	outBlock = (uint8*)tempBuffer;
	inBlock = outBlock + hLen;
	/* Set up inBlock to contain Salt || INT (blockNumber). */
	memcpy (inBlock, saltPtr, saltLen);

	inBlock[saltLen + 0] = (uint8)(blockNumber >> 24);
	inBlock[saltLen + 1] = (uint8)(blockNumber >> 16);
	inBlock[saltLen + 2] = (uint8)(blockNumber >> 8);
	inBlock[saltLen + 3] = (uint8)(blockNumber);

	/* Caculate U1 (result goes to outBlock) and copy it to resultBlockPtr. */
	resultBlockPtr = (uint8*)dataPtr;
	prf (passwordPtr, passwordLen, inBlock, saltLen + 4, outBlock);
	memcpy (resultBlockPtr, outBlock, hLen);
	/* Calculate U2 though UiterationCount. */
	for (iteration = 2; iteration <= iterationCount; iteration++)
	{
		uint8 *tempBlock;
		uint32 byte;
		/* Swap inBlock and outBlock pointers. */
		tempBlock = inBlock;
		inBlock = outBlock;
		outBlock = tempBlock;
		/* Now inBlock conatins Uiteration-1.  Calclulate Uiteration into outBlock. */
		prf (passwordPtr, passwordLen, inBlock, hLen, outBlock);
		/* Xor data in dataPtr (U1 \xor U2 \xor ... \xor Uiteration-1) with
		   outBlock (Uiteration). */
		for (byte = 0; byte < hLen; byte++)
			resultBlockPtr[byte] ^= outBlock[byte];
	}
}
void pbkdf2 (PRF prf, uint32 hLen,
			 const void *passwordPtr, uint32 passwordLen,
			 const void *saltPtr, uint32 saltLen,
			 uint32 iterationCount,
			 void *dkPtr, uint32 dkLen,
			 void *tempBuffer)
{
	uint32 completeBlocks = dkLen / hLen;
	uint32 partialBlockSize = dkLen % hLen;
	uint32 blockNumber;
	uint8 *dataPtr = (uint8*)dkPtr;
	uint8 *blkBuffer = (uint8*)tempBuffer;
	/* First calculate all the complete hLen sized blocks required. */
	for (blockNumber = 1; blockNumber <= completeBlocks; blockNumber++)
	{
		F (prf, hLen, passwordPtr, passwordLen, saltPtr, saltLen,
		   iterationCount, blockNumber, dataPtr, blkBuffer + hLen);
		dataPtr += hLen;
	}
	/* Finally if the requested output size was not an even multiple of hLen, calculate
	   the final block and copy the first partialBlockSize bytes of it to the output. */
	if (partialBlockSize > 0)
	{
		F (prf, hLen, passwordPtr, passwordLen, saltPtr, saltLen,
		   iterationCount, blockNumber, blkBuffer, blkBuffer + hLen);
		memcpy (dataPtr, blkBuffer, partialBlockSize);
	}
}

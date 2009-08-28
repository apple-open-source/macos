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

#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>
#include <sys/_endian.h>
#include "DES.h"

#define kVersion1 1
#define kVersion2 2 

void DES (const int32_t *keysArrayPtr, int32_t Count, char * encryptData,short mode);


#define klonghiBit 	0x80000000
#define kwordhiBit 	0x8000
#define klongloBit 	0x00000001
#define kbit0 		0x00000000
#define klowWord 	0x0000FFFF
#define khiBit 		0x00000020

#define kwordSize 	16
#define kkeyArraySize 	128 /* bytes for 16 keys */
#define klowKeySize  	4   /* bytes in half the key size */
#define knumKeys	16  /* # keys in key array */
#define kkeySize	8   /* bytes per key */
#define kDecrypt	1
#define kEncrypt	0


typedef struct doubleLong
{
	uint16_t 	bits49to64;			
	uint16_t 	bits17to32;			
	uint16_t 	bits33to48;			
	uint16_t 	bits1to16;			
} doubleLong;

typedef char byte;

/* Internal function prototypes */
#if defined(__cplusplus)
	extern "C" {
#endif

void Permute(EncryptBlk *aBlkPTr, int32_t* aKeyPtr);
int32_t RotateExtended(uint16_t *theWord, uint32_t resultLo);
void Extract(doubleLong *ExtractData, uint32_t *resultLow, uint32_t *resultHigh);
void InitialPermutation(EncryptBlk *sourceBlkPTr,EncryptBlk *resultBlkPTr );
int32_t FRK(uint32_t theData,uint32_t keyHi,uint32_t keyLo);
#if defined(__cplusplus)
}
#endif


/* -------------------------------------------------------------------- */
/*	Macros  							*/
/* -------------------------------------------------------------------- */

/* Most of these macros emulate the 68K instruction set used in the original
   assembly language version of this code.*/

#define GetBit(val,Reg) gTestVal = val; gTestVal = gTestVal % khiBit; gTemp = klongloBit; if (gTestVal != 0) gTemp = gTemp << gTestVal;
#define BitClear(val,Reg) GetBit(val,Reg); Reg = Reg & (~gTemp);
#define BitSet(bit,Reg) GetBit(bit,Reg) Reg = Reg | gTemp;
#define BitTest(val, Reg) GetBit(val,Reg);  gTestVal = gTemp & Reg;
#define BNE(label) if (gTestVal != 0) goto label;
#define BEQ(label) if (gTestVal == 0) goto label;
#define BRA(label) goto label;
#define Compare(val,reg) gTestVal = val - reg;
#define EXchange(reg1,reg2) gTemp = reg1;reg1 = reg2; reg2 = gTemp;
#define LSLword(Reg,amount)	gTestVal = kwordhiBit & Reg; ;Reg = Reg <<  amount;
#define LSRword(Reg,amount)	BitTest(1,Reg); Reg = Reg >> amount;
#define LSL(Reg,amount)	gTestVal = klonghiBit & Reg; Reg = Reg <<  amount;
#define LSR(Reg,amount)	gTestVal = klongloBit & Reg ; Reg = Reg >> amount;
#define ROLeftLong(Reg) LSL(Reg,1); if (gTestVal != 0) Reg = Reg | klongloBit; 
#define RORightLong(Reg) LSR(Reg,1); if (gTestVal != 0)  Reg = Reg | klonghiBit; 
#define ROXLeftLong(Reg) Reg = Reg << 1;if (xVal != 0) Reg = Reg | klongloBit;
#define ROXRightLong(Reg) Reg = Reg >> 1; if (xVal != 0) Reg = Reg | klonghiBit;
#define RORightLong4(Reg) xVal = Reg & 0x0000000F; xVal = xVal << 28; Reg = Reg >> 4; if (xVal != 0) Reg = Reg  | xVal;
#define TheLastKey(ArrayPtr) (uint32_t *) ((char *) ArrayPtr + (kkeyArraySize - klowKeySize ));

/* -------------------------------------------------------------------- */
/*	Permute tables for permutation					*/
/* -------------------------------------------------------------------- */

const byte N_PC1Tbl[58] = 
{
    /* ks permutation iteration =  1 */

	7, 15, 23, 31, 39, 47, 55,
     63,  6, 14, 22, 30, 38, 46,
     54, 62,  5, 13, 21, 29, 37,
     45, 53, 61,  4, 12, 20, 28, -1,	/* swap */
      1,  9, 17, 25, 33, 41, 49,
     57,  2, 10, 18, 26, 34, 42,
     50, 58,  3, 11, 19, 27, 35,
     43, 51, 59, 36, 44, 52, 60, -2	/* end-of-table */
};

const byte N_PC2Tbl[50] = {
    /* ks permutation iteration =  1 */

	50, 47, 53, 40, 63, 59, 61, 36,
	49, 58, 43, 54, 41, 45, 52, 60, -1,	/* swap */
	38, 56, 48, 57, 37, 44, 51, 62,
	19,  8, 29, 23, 13,  5, 30, 20,
	 9, 15, 27, 12, 16, 11, 21,  4,
	26,  7, 14, 18, 10, 24, 31, 28, -2	/* end-of-table */
};

const byte N_IPInvTbl[66] = {

	24, 56, 16, 48,  8, 40,  0, 32,
    25, 57, 17, 49,  9, 41,  1, 33,
    26, 58, 18, 50, 10, 42,  2, 34,
    27, 59, 19, 51, 11, 43,  3, 35, -1,	/* swap */
    28, 60, 20, 52, 12, 44,  4, 36,
    29, 61, 21, 53, 13, 45,  5, 37,
    30, 62, 22, 54, 14, 46,  6, 38,
	31, 63, 23, 55, 15, 47,  7, 39, -2	/* end-of-table */
};

const byte N_PTbl[33] = {
	16, 25, 12, 11,  3, 20,  4, 15,
	31, 17,  9,  6, 27, 14,  1, 22,
	30, 24,  8, 18,  0,  5, 29, 23,
	13, 19,  2, 26, 10, 21, 28,  7, -2	/* end-of-table */
};


/* -------------------------------------------------------------------- */
/*	SBox Tables for DES C code					*/
/* -------------------------------------------------------------------- */

const byte N_SBoxes[512] = 
{		/* S8 */
				13,  1,  2, 15,  8, 13,  4,  8,
         	 6, 10, 15,  3, 11,  7,  1,  4,
          	10, 12,  9,  5,  3,  6, 14, 11,
         	 5,  0,  0, 14, 12,  9,  7,  2,
           	 7,  2, 11,  1,  4, 14,  1,  7,
           	 9,  4, 12, 10, 14,  8,  2, 13,
          	 0, 15,  6, 12, 10,  9, 13,  0,
          	15,  3,  3,  5,  5,  6,  8, 11,
         //    S7
				 4, 13, 11,	 0,  2, 11, 14,  7,
				15,  4,  0,	 9,  8,	1, 13, 10,
				 3, 14, 12,	 3,  9,	5,  7, 12,
				 5,  2, 10, 15,  6,	8,  1,  6,
				 1,  6,  4, 11, 11, 13, 13,  8,
				12,  1,  3,	 4,  7, 10, 14,  7,
				10,  9, 15,	 5,  6,	0,  8, 15,
				 0, 14,  5,	 2,  9,	3,  2, 12,
         //    S6
				12, 10,  1, 15, 10,	4, 15,  2,
				 9,  7,  2, 12,  6,  9,  8,  5,
				 0,  6, 13,	 1,  3, 13,  4, 14,
				14,  0,  7, 11,  5,	3, 11,  8,
				 9,  4, 14,	 3, 15,	2,  5, 12,
				 2,  9,  8,	 5, 12, 15,  3, 10,
				 7, 11,  0, 14,  4,	1, 10,  7,
				 1,  6, 13,	 0, 11,	8,  6, 13,
         //    S5
				 2, 14, 12, 11,  4,  2,  1, 12,
				 7,  4, 10,	 7, 11, 13,  6,  1,
				 8,  5,  5,	 0,  3, 15, 15, 10,
				13,  3,  0,	 9, 14,	8,  9,  6,
				 4, 11,  2,	 8,  1, 12, 11,  7,
				10,  1, 13, 14,  7,	2,  8, 13,
				15,  6,  9, 15, 12,	0,  5,  9,
				 6, 10,  3,	 4,  0,	5, 14,  3,
          //    S4
				 7, 13, 13,	 8, 14, 11,  3,  5,
				 0,  6,  6, 15,  9,	0, 10,  3,
				 1,  4,  2,	 7,  8,  2,  5, 12,
				11,  1, 12, 10,  4, 14, 15,  9,
				10,  3,  6, 15,  9,  0,  0,  6,
				12, 10, 11,	 1,  7, 13, 13,  8,
				15,  9,  1,	 4,  3,	5, 14, 11,
				 5, 12,  2,	 7,  8,	2,  4, 14,
          //    S3
				10, 13,  0,	 7,  9,  0, 14,  9,
				 6,  3,  3,	 4, 15,	6,  5, 10,
				 1,  2, 13,	 8, 12,	5,  7, 14,
				11, 12,  4, 11,  2, 15,  8,  1,
				13,  1,  6, 10,  4, 13,  9,  0,
				 8,  6, 15,	 9,  3,	8,  0,  7,
				11,  4,  1, 15,  2, 14, 12,  3,
				 5, 11, 10,	 5, 14,	2,  7, 12,
          //    S2
				15,  3,  1, 13,  8,	4, 14,  7,
				 6, 15, 11,	 2,  3,	8,  4, 14,
				 9, 12,  7,	 0,  2,	1, 13, 10,
				12,  6,  0,	 9,  5, 11, 10,  5,
				 0, 13, 14,	 8,  7, 10, 11,  1,
				10,  3,  4, 15, 13,	4,  1,  2,
				 5, 11,  8,	 6, 12,	7,  6, 12,
				 9,  0,  3,	 5,  2, 14, 15,  9,
         //    S1
				14,  0, 4, 15, 13,  7,  1,	 4,
				 2, 14, 15,	2, 11, 13,  8,	 1,
				 3, 10, 10,	6,  6, 12, 12, 11,
				 5,  9,  9,	5,  0,  3,  7,	 8,
				 4, 15,  1, 12, 14, 8,  8,	 2,
				13,  4,  6,	 9,  2, 1, 11,	 7,
				15,  5, 12, 11,  9, 3,  7, 14,
				 3, 10, 10,	 0,  5, 6,  0, 13,

 };
 


void Permute(EncryptBlk *aBlkPTr, int32_t* aKeyPtr)
{
	register char bitPos;
	char* ArrayPtr;

	register uint32_t	keyLo;
	register uint32_t	keyHi;
	register uint32_t	loBits;
	register uint32_t	hiBits;
	register uint32_t	gTestVal;
	register uint32_t	gTemp;
	register uint32_t	arrayByte;

	bitPos = 0;
	hiBits = 0;
	loBits = 0;

	keyLo = aBlkPTr->keyLo;
	keyHi = aBlkPTr->keyHi;

	ArrayPtr = (char*) aKeyPtr;
	bitPos = *ArrayPtr ++; 		/* get source bit */

Loop:
	arrayByte = bitPos;
	ROLeftLong(hiBits);

	BitTest(5, arrayByte);
	BEQ(jump20);

	BitClear(5,arrayByte);
	BitTest(arrayByte,keyLo);
	BNE(jump30);
	BRA(jump40);
		
jump20:
	BitTest(arrayByte,keyHi);
	BEQ(jump40);	
		
jump30:
	BitSet(kbit0,hiBits);

jump40:
	bitPos = *ArrayPtr++;
	if (bitPos >= 0) goto Loop;
	EXchange(hiBits,loBits);
	if ( bitPos !=  -1 ) goto jump50;
	bitPos = *ArrayPtr++;
	goto Loop;
		
jump50:
	aBlkPTr->keyLo = loBits; /* low bits */
	aBlkPTr->keyHi = hiBits;
}

/* -------------------------------------------------------------------- */
/*	KeyScheduler							*/
/* -------------------------------------------------------------------- */

void KeySched(const EncryptBlk *Key, int32_t* keysArrayPtr, short version)
{
	EncryptBlk	permuteKey;
	uint32_t	*keyPtr;
	register uint32_t	keyLo;
	register uint32_t	keyHi;
	register short		shiftSchedule;
	register uint32_t	gTestVal;

	permuteKey.keyLo = Key->keyHi;
	permuteKey.keyHi = Key->keyLo;
	
	if (version > kVersion1 ) /* PPC and AppleShare 1.0 use version 1 (see RAndrews comment)*/
	{
		LSL(permuteKey.keyLo,1);
		LSL(permuteKey.keyHi,1);  
	} 
	
	keyPtr = (uint32_t *)keysArrayPtr;
	
	Permute(&permuteKey, (int32_t *)&N_PC1Tbl);


	keyLo = permuteKey.keyLo;
	keyHi = permuteKey.keyHi;

	LSL(keyLo,4);
	LSL(keyHi,4);
	
	
	shiftSchedule = 0xC081;
			
jump5:
	LSLword(shiftSchedule,1);
	BNE(jump20);
	
	LSL(keyHi,1);
	BEQ(jump10); 
	keyHi = keyHi | 16;

jump10:
	LSL(keyLo,1)
	BEQ(jump20);
	keyLo = keyLo | 16;

jump20:
	LSL(keyHi,1)
	BEQ(jump30);
	keyHi = keyHi | 16;
			
jump30:
	LSL(keyLo,1)
	BEQ(jump40);
	keyLo = keyLo | 16;

			
jump40:
	permuteKey.keyLo = keyHi;
	permuteKey.keyHi = keyLo;
		
	Permute((EncryptBlk *) &permuteKey,(int32_t*)&N_PC2Tbl);
	*keyPtr = permuteKey.keyHi;
	keyPtr ++ ;
	*keyPtr = permuteKey.keyLo;
	keyPtr ++ ;
		
	if (shiftSchedule != 0) goto jump5;
}


/* -------------------------------------------------------------------- */
/*	Encode and Decode + support routines				*/
/* -------------------------------------------------------------------- */

	
int32_t RotateExtended(uint16_t *theWord, uint32_t resultLo)
{
	register uint32_t	xVal = 0;
	register uint16_t	tempPiece;

	tempPiece = *theWord;
	xVal = tempPiece & kwordhiBit;
	tempPiece = tempPiece << 1;
	*theWord = tempPiece;
		
	ROXLeftLong(resultLo);		
	xVal = resultLo & klonghiBit;
	ROXLeftLong(resultLo);

	return resultLo;
}


void Extract(doubleLong *ExtractData, uint32_t *resultLow, uint32_t *resultHigh)
{
	short 	i;
	uint32_t	resultLo;
	uint32_t	resultHi;
	uint32_t	gTemp;

	resultLo = *resultLow;
	resultHi = *resultHigh;

	for (i = kkeySize; i > 0; i--) 
	{	
		resultLo = RotateExtended(&ExtractData->bits49to64, resultLo);
		resultLo = RotateExtended(&ExtractData->bits17to32, resultLo);
		resultLo = RotateExtended(&ExtractData->bits33to48, resultLo);
		resultLo = RotateExtended(&ExtractData->bits1to16, resultLo);

		EXchange(resultLo,resultHi);	
	}
	*resultLow = resultLo;
	*resultHigh = resultHi;
}



void InitialPermutation(EncryptBlk *sourceBlkPTr,EncryptBlk *resultBlkPTr )
{
	doubleLong 	dataToEncrypt;
	uint32_t 	resultLow = 0;
	uint32_t 	resultHi = 0;
	register uint32_t gTestVal = 0;

#if TARGET_RT_BIG_ENDIAN
	dataToEncrypt.bits49to64 = sourceBlkPTr->keyLo & klowWord;
	dataToEncrypt.bits33to48 = sourceBlkPTr->keyHi & klowWord;
	dataToEncrypt.bits17to32 = sourceBlkPTr->keyLo >> kwordSize;
	dataToEncrypt.bits1to16  = sourceBlkPTr->keyHi >> kwordSize;
#else
	dataToEncrypt.bits49to64 = CFSwapInt16(sourceBlkPTr->keyLo >> kwordSize);
	dataToEncrypt.bits33to48 = CFSwapInt16(sourceBlkPTr->keyHi >> kwordSize);
	dataToEncrypt.bits17to32 = CFSwapInt16(sourceBlkPTr->keyLo & klowWord);
	dataToEncrypt.bits1to16  = CFSwapInt16(sourceBlkPTr->keyHi & klowWord);
#endif
	
	Extract(&dataToEncrypt, &resultLow, &resultHi);

	RORightLong(resultLow)
	RORightLong(resultHi)

	Extract(&dataToEncrypt, &resultLow, &resultHi);
	
	resultBlkPTr->keyLo = resultLow;
	resultBlkPTr->keyHi = resultHi;
}



int32_t FRK(uint32_t theData,uint32_t keyHi,uint32_t keyLo)
{
	register uint32_t gTestVal = 0;
	register uint32_t xVal = 0;
	register uint16_t tempData = 0;
	register uint32_t result = 0;
	char tableVal;
	EncryptBlk encryptBlock;
	short counter;
	short count;
	short offset = 0;
	
	ROLeftLong(theData);
	for ( counter = kkeySize; counter > 0 ; )
	{
		tempData = theData;
		tempData = tempData ^ keyLo;
		tempData = tempData & 0x3F; /* mask off all but lowest 6 bits */

		tableVal = N_SBoxes[tempData + offset];
		result = result | tableVal;
		RORightLong4(result);
		if (--counter == 0) break;	
		RORightLong4(theData);
		
		offset = offset + 64;
		
		for (count = 6; count > 0; count --)
		{ 
			/*LRORightLong(keyHi);*/
			xVal  = keyHi & klongloBit;
			keyHi = keyHi >> 1;
				
			ROXRightLong(keyLo); 					
		}
	}
	
	encryptBlock.keyHi = result;
	encryptBlock.keyLo = keyHi;
	Permute(&encryptBlock, (int32_t*)&N_PTbl );
	return encryptBlock.keyLo;
}



void Encode (const int32_t *keysArrayPtr, int32_t Count, char * encryptData)
{
	DES(keysArrayPtr,Count,encryptData,kEncrypt);
}

void Decode (const int32_t *keysArrayPtr, int32_t Count, char * encryptData)
{
	DES(keysArrayPtr,Count,encryptData,kDecrypt);
}

void DES (const int32_t *keysArrayPtr, int32_t Count, char * encryptData,short mode)
{
	EncryptBlk *EncryptDataPtr;
	EncryptBlk resultData;
	EncryptBlk *ReturnData;
	short		swapCount;
	uint32_t	fResult;
	uint32_t	gTemp;
	uint32_t	keyHi;
	uint32_t	keyLo;
	uint32_t	*keyPtr;

	EncryptDataPtr	= (EncryptBlk *) encryptData;
	ReturnData = EncryptDataPtr;

	for ( ;Count > 0; Count -= kkeySize)
	{
		InitialPermutation(EncryptDataPtr, &resultData );

		if (mode == kEncrypt)
			keyPtr =  (uint32_t*)keysArrayPtr;
		else /* must be decrypt */
			keyPtr =  TheLastKey(keysArrayPtr); /* start at last key in array */

		for (swapCount = knumKeys; swapCount != 0; swapCount--)
		{
			if (mode == kEncrypt)
			{
				keyHi = *keyPtr++;
				keyLo = *keyPtr++;
			}
			else /* must be decrypt */
			{
				keyLo = *keyPtr--;
				keyHi = *keyPtr--;
			}
			
			fResult = FRK(resultData.keyLo,keyHi,keyLo);
			resultData.keyHi = resultData.keyHi ^ fResult;
			if (swapCount > 1) /* EXchange is multi-line macro and requires {}*/
			{
				EXchange(resultData.keyHi,resultData.keyLo);
			} 
		}
			
		EXchange(resultData.keyHi,resultData.keyLo);
		Permute(&resultData,(int32_t*)&N_IPInvTbl);
		EncryptDataPtr = (EncryptBlk *) (((char*) EncryptDataPtr) + kkeySize);   
	}

	ReturnData->keyLo = htonl(resultData.keyLo);
	ReturnData->keyHi = htonl(resultData.keyHi);
}

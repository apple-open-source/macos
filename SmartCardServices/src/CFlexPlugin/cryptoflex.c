/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please
 * obtain a copy of the License at http://www.apple.com/publicsource and
 * read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please
 * see the License for the specific language governing rights and
 * limitations under the License.
 */

/******************************************************************
 
        MUSCLE SmartCard Development ( http://www.linuxnet.com )
            Title  : cryptoflex.c
            Package: Cryptoflex Plugin
            Author : David Corcoran
		     Piotr Gorak
            Date   : 09/26/01
            License: Copyright (C) 2001 David Corcoran
                     <corcoran@linuxnet.com>
            Purpose: This abstracts the Card Edge Interface APDU's
	             into client side function calls.
 
********************************************************************/

#ifdef WIN32
#include "../win32/CFlexPlugin.h"
#endif

#ifndef __APPLE__
#include <musclecard.h>
#else
#include <PCSC/musclecard.h>
#endif

#include "cryptoflex.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define CRYPTOFLEX_RSA_KEYSTART     -1
#define CRYPTOFLEX_DES_KEYSTART     1
#define CRYPTOFLEX_OBJ_DIR          0x3FCE
#define CRYPTOFLEX_MSC_KEY_DIR      0x3FCF
#define CRYPTOFLEX_INFOBJ_ID        "#FFFE"
#define CRYPTOFLEX_INFOBJ_LEN       0x0080  /* 128 bytes */
#define CRYPTOFLEX_MAXMSC_KEY_NUM   0x0005  /* 3 public, 3 private 0-5 */
#define CRYPTOFLEX_MAX_KEYS         0x0006

typedef struct {
  MSCUChar8  pBuffer[MAX_BUFFER_SIZE];
  MSCULong32 bufferSize;
  MSCUChar8  apduResponse[MAX_BUFFER_SIZE];
  MSCULong32 apduResponseSize;
  LPSCARD_IO_REQUEST ioType;
} MSCTransmitBuffer, *MSCLPTransmitBuffer;


static MSCUChar8 keyInfoBytes[CRYPTOFLEX_INFOBJ_LEN];
static int suppressResponse = 0;

/* Some local function definitions */

MSC_RV convertPCSC( MSCLong32 );
int idToString( MSCString, MSCULong32 );
int stringToID( MSCPUShort16, MSCString );
int bytesToString( MSCString, MSCPUChar8 );
#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCSelect(MSCLPTokenConnection, MSCULong32 );
#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCGetResponse(MSCLPTokenConnection, MSCUChar8, MSCPUChar8 );
MSCUShort16 convertSW( MSCPUChar8 );
void MemCopy16( MSCPUChar8, MSCPUShort16 );
void MemCopy32( MSCPUChar8, MSCPULong32 );
void MemCopyTo16( MSCPUShort16, MSCPUChar8 );
void MemCopyTo32( MSCPULong32, MSCPUChar8 );
MSCUShort16 getUShort16( MSCPUChar8 );
void setUShort16( MSCPUChar8, MSCUShort16 );
#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSCLong32 SCardExchangeAPDU( MSCLPTokenConnection, MSCLPTransmitBuffer );
#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCReadAllocateObject( MSCLPTokenConnection, MSCString, 
				 MSCPUChar8*, MSCPULong32 );
#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCReadLargeObjectOffCB( MSCLPTokenConnection, 
				   MSCString, MSCULong32, 
				   MSCPUChar8, MSCULong32,
				   LPRWEventCallback rwCallback, 
				   MSCPVoid32 addParams );

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCWriteLargeObjectOffCB( MSCLPTokenConnection, 
				    MSCString,  MSCULong32, MSCPUChar8, 
				    MSCULong32, LPRWEventCallback rwCallback, 
				    MSCPVoid32 addParams );
#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCListKeys( MSCLPTokenConnection, MSCUChar8, MSCLPKeyInfo);
#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCWriteObject( MSCLPTokenConnection, MSCString, 
			  MSCULong32, MSCPUChar8, MSCUChar8 );
#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCReadObject( MSCLPTokenConnection, MSCString, 
			 MSCULong32, MSCPUChar8, MSCUChar8 );
#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCListObjects( MSCLPTokenConnection, 
			  MSCUChar8, MSCLPObjectInfo );

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCIdentifyToken( MSCLPTokenConnection );

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV mapCryptoflexKeys(MSCLPTokenConnection pConnection, MSCUChar8 keyType,
			 MSCUShort16 keySize, MSCUChar8 mscKeyNum,
			 MSCPUChar8 cflKeyNum);
#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCCreateObject( MSCLPTokenConnection, 
			   MSCString, MSCULong32,
			   MSCLPObjectACL );
#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCDeleteObject( MSCLPTokenConnection, 
			   MSCString, MSCUChar8 );

MSC_RV mapCryptoflexKeys(MSCLPTokenConnection pConnection, MSCUChar8 keyType,
			 MSCUShort16 keySize, MSCUChar8 mscKeyNum,
			 MSCPUChar8 cflKeyNum) {

  MSCLong32 rv;
  MSCKeyInfo keyStruct;
  MSCUChar8 targetCFNum;
  char highestKey;
  int i;
  
  typedef struct {
    MSCUChar8 keyNumCF;
    MSCUChar8 keyType;
  } _keyMask;

  _keyMask keyMask[MSC_MAX_KEYS];

  targetCFNum = 0xFF; 

  if ( keyType == MSC_KEY_DES || keyType == MSC_KEY_3DES || 
       keyType == MSC_KEY_3DES3 ) {
    highestKey = CRYPTOFLEX_DES_KEYSTART;  /* Des keys start from 2 */
  } else if ( keyType == MSC_KEY_RSA_PUBLIC || 
	      keyType == MSC_KEY_RSA_PRIVATE ||
	      keyType == MSC_KEY_RSA_PRIVATE_CRT ) {
    highestKey = CRYPTOFLEX_RSA_KEYSTART;  /* RSA keys start from 0 */
  } else {
    return MSC_INVALID_PARAMETER;
  }


  for (i=0; i < MSC_MAX_KEYS; i++) {
    keyMask[i].keyNumCF = 0xFF;
    keyMask[i].keyType  = 0xFF;
  }


  /* Algorithm:

     List keys 

     If a match on numbers occurs:

     Errors:  
     - If they key type/size doesn't match the existing key.

     Otherwise:

     - Find the next available spot and generate the keys
  */

  rv = PL_MSCListKeys(pConnection, MSC_SEQUENCE_RESET, &keyStruct);

  if ( rv != MSC_SEQUENCE_END ) {
    
    do {
      
      if ( keyStruct.keyNum == mscKeyNum ) {
	
	if ( targetCFNum != 0xFF ) {
	  /* 2 keys with the same number, ouch */
	  return MSC_INTERNAL_ERROR;
	}
	
	if ( keyStruct.keyType == keyType &&
	     keyStruct.keySize == keySize ) {
	  targetCFNum = keyStruct.keyPartner;
	} else {
	  return MSC_INVALID_PARAMETER;
	}
      }
      
      /* Caching - indice in mask is MSC key num, keyNumCF is
	 the cryptoflex key number, and type is the key type */
      
      keyMask[keyStruct.keyNum].keyNumCF = keyStruct.keyPartner;
      keyMask[keyStruct.keyNum].keyType  = keyStruct.keyType;
      
      rv = PL_MSCListKeys(pConnection, MSC_SEQUENCE_NEXT, &keyStruct);
    } while ( rv != MSC_SEQUENCE_END );
  }

  if (targetCFNum != 0xFF) {
    /* matching key num, type, and size - regenerate */
    *cflKeyNum = targetCFNum;
    return MSC_SUCCESS;
  } else {
    /* No matching number - lets find one for that type of key */

    for (i=0; i < MSC_MAX_KEYS; i++) {
      if ( keyMask[i].keyType == keyType ) {
	if ( keyMask[i].keyNumCF > highestKey ) {
	  highestKey = keyMask[i].keyNumCF;
	}
      }
    }

    /* Only support one more DES key, for now */
    
    if (keyType == MSC_KEY_DES || keyType == MSC_KEY_3DES || 
	keyType == MSC_KEY_3DES3) {
      if  (highestKey != CRYPTOFLEX_DES_KEYSTART) {
	return MSC_UNSUPPORTED_FEATURE;
      }
    }
    
    *cflKeyNum = highestKey + 1;
    return MSC_SUCCESS;
  }

  return MSC_INVALID_PARAMETER;
}


void MemCopyReverse(unsigned char *output, unsigned char *input, int length) {

  int i, j;

  j=0;

  for (i=length-1; i >=0; i--) {
    output[j++] = input[i];
  }

}

/* Private helpers */

unsigned char ACL2Byte(MSCLPObjectACL pObjectACL)
{

  unsigned char retByte;

  retByte = 0x00;

  if ( pObjectACL->readPermission == MSC_AUT_ALL ) {
    retByte = 0x00;
  } else if ( pObjectACL->readPermission == MSC_AUT_NONE ) {
    retByte |= 0xF0;
  }

  if ( pObjectACL->writePermission == MSC_AUT_ALL ) {
    retByte |= 0x00;
  } else if (  pObjectACL->writePermission == MSC_AUT_NONE ) {
    retByte |= 0x0F;
  }

  /* Check for CHV 1 */
  if ( pObjectACL->readPermission & MSC_AUT_PIN_1 ) {
    retByte |= 0x10;
  }

  if ( pObjectACL->writePermission & MSC_AUT_PIN_1 ) {
    retByte |= 0x01;
  }

  /* Check for AUT */
  if ( pObjectACL->readPermission & MSC_AUT_PIN_0 ) {
    retByte |= 0x40;
  }

  if ( pObjectACL->writePermission & MSC_AUT_PIN_0 ) {
    retByte |= 0x04;
  }

  return retByte;
}

void Byte2ACL(unsigned char inByte, MSCLPObjectACL pObjectACL)
{

  pObjectACL->readPermission   = 0;
  pObjectACL->writePermission  = 0;
  pObjectACL->deletePermission = 0;  

  if ( (inByte/256) == 0x00 ) {
    pObjectACL->readPermission = MSC_AUT_ALL;
  } else if ((inByte/256) == 0xF ) {
    pObjectACL->readPermission = MSC_AUT_NONE;
  }

  if ( (inByte%256) == 0x00 ) {
    pObjectACL->writePermission = MSC_AUT_ALL;
  } else if ((inByte%256) == 0xF ) {
    pObjectACL->writePermission = MSC_AUT_NONE;
  }

  /* Check for CHV 1 */
  if ( inByte & 0x10 ) {
    pObjectACL->readPermission |= MSC_AUT_PIN_1;
  }

  if ( inByte & 0x01 ) {
    pObjectACL->writePermission |= MSC_AUT_PIN_1;
  }

  /* Check for AUT */
  if ( inByte & 0x40 ) {
    pObjectACL->readPermission |= MSC_AUT_PIN_0;
  }

  if ( inByte & 0x04 ) {
    pObjectACL->writePermission |= MSC_AUT_PIN_0;
  }

  /* Delete permission is static */
  pObjectACL->deletePermission = MSC_AUT_PIN_1;

}



MSC_RV PL_MSCReadKeyInfo( MSCLPTokenConnection pConnection, 
			  MSCLPKeyInfo keyInfo ) {
  MSCLong32 rv;
  MSCUChar8 currentPointer;

  /* Read the whole file and cache it until it is reset */
  /* Must do this for performance reasons               */

  if ( keyInfo == 0 ) {
    rv = PL_MSCReadObject(pConnection, CRYPTOFLEX_INFOBJ_ID, 
			  0, keyInfoBytes, 
			  MSC_SIZEOF_KEYINFO*CRYPTOFLEX_MAX_KEYS);
    return rv;
  }


  currentPointer = MSC_SIZEOF_KEYINFO * keyInfo->keyNum;

  keyInfo->keyNum  = keyInfoBytes[currentPointer];
  currentPointer += MSC_SIZEOF_KEYNUMBER;
  keyInfo->keyType = keyInfoBytes[currentPointer];
  currentPointer += MSC_SIZEOF_KEYTYPE;
  keyInfo->keyPartner = keyInfoBytes[currentPointer];
  currentPointer += MSC_SIZEOF_KEYPARTNER;
  keyInfo->keyMapping = keyInfoBytes[currentPointer];
  currentPointer += MSC_SIZEOF_KEYMAPPING;
  MemCopyTo16(&keyInfo->keySize, &keyInfoBytes[currentPointer]);
  currentPointer += MSC_SIZEOF_KEYSIZE;
  
  /* copy the policy */

  MemCopyTo16(&keyInfo->keyPolicy.cipherMode, 
	      &keyInfoBytes[currentPointer]);
  currentPointer += MSC_SIZEOF_POLICYVALUE;
  MemCopyTo16(&keyInfo->keyPolicy.cipherDirection, 
	      &keyInfoBytes[currentPointer]);
  currentPointer += MSC_SIZEOF_POLICYVALUE;

  /* copy the ACL */

  MemCopyTo16(&keyInfo->keyACL.readPermission, &keyInfoBytes[currentPointer]);
  currentPointer += MSC_SIZEOF_ACLVALUE;
  MemCopyTo16(&keyInfo->keyACL.writePermission, &keyInfoBytes[currentPointer]);
  currentPointer += MSC_SIZEOF_ACLVALUE;
  MemCopyTo16(&keyInfo->keyACL.usePermission, &keyInfoBytes[currentPointer]);
  currentPointer += MSC_SIZEOF_ACLVALUE;  

  return MSC_SUCCESS;
}


MSC_RV PL_MSCWriteKeyInfo( MSCLPTokenConnection pConnection, MSCUChar8 keyNum, 
			   MSCUChar8 keyType, MSCUChar8 keyPartner,
			   MSCUChar8 keyMapping, MSCUShort16 keySize,
			   MSCLPKeyPolicy keyPolicy, MSCLPKeyACL keyACL ) {

  MSCLong32 rv;

  MSCUChar8 keyInfoBytes[MSC_SIZEOF_KEYINFO];
  MSCUChar8 currentPointer;

  currentPointer = 0;
  keyInfoBytes[currentPointer] = keyNum;
  currentPointer += MSC_SIZEOF_KEYNUMBER;
  keyInfoBytes[currentPointer] = keyType;
  currentPointer += MSC_SIZEOF_KEYTYPE;
  keyInfoBytes[currentPointer] = keyPartner;
  currentPointer += MSC_SIZEOF_KEYPARTNER;
  keyInfoBytes[currentPointer] = keyMapping;
  currentPointer += MSC_SIZEOF_KEYMAPPING;
  MemCopy16(&keyInfoBytes[currentPointer], &keySize);
  currentPointer += MSC_SIZEOF_KEYSIZE;
  
  /* copy the policy */
  
  MemCopy16(&keyInfoBytes[currentPointer], &keyPolicy->cipherMode);
  currentPointer += MSC_SIZEOF_ACLVALUE;
  MemCopy16(&keyInfoBytes[currentPointer], &keyPolicy->cipherDirection);
  currentPointer += MSC_SIZEOF_ACLVALUE;  

  /* copy the acl */

  MemCopy16(&keyInfoBytes[currentPointer], &keyACL->readPermission);
  currentPointer += MSC_SIZEOF_ACLVALUE;
  MemCopy16(&keyInfoBytes[currentPointer], &keyACL->writePermission);
  currentPointer += MSC_SIZEOF_ACLVALUE;
  MemCopy16(&keyInfoBytes[currentPointer], &keyACL->usePermission);
  currentPointer += MSC_SIZEOF_ACLVALUE;  

  rv = PL_MSCWriteObject(pConnection, CRYPTOFLEX_INFOBJ_ID, 
			 MSC_SIZEOF_KEYINFO*keyNum, 
			 keyInfoBytes, MSC_SIZEOF_KEYINFO);
  return rv;
}


MSC_RV PL_MSCVerifyKey(MSCLPTokenConnection pConnection,
                    MSCPUChar8 key, MSCUChar8 key_length) {

  MSCLong32 rv;
  MSCTransmitBuffer transmitBuffer;
  MSCPUChar8 apduResponse;
  MSCPUChar8 pBuffer;

  pBuffer      = transmitBuffer.pBuffer;
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA] = CLA_F0;
  pBuffer[OFFSET_INS] = 0x2A;
  pBuffer[OFFSET_P1]  = 0x00;
  pBuffer[OFFSET_P2]  = 0x01;
  pBuffer[OFFSET_P3]  = key_length;

  memcpy(&pBuffer[OFFSET_DATA], key, key_length);
  transmitBuffer.bufferSize = pBuffer[OFFSET_P3] + 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  
  rv = SCardExchangeAPDU(pConnection, &transmitBuffer);

  if(rv != SCARD_S_SUCCESS) {
    return convertPCSC(rv);
  }

  if(transmitBuffer.apduResponseSize == 2) {
    return convertSW(apduResponse);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }
}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCWriteFramework( MSCLPTokenConnection pConnection,
			  MSCLPInitTokenParams pInitParams ) {

  MSCLong32 rv;
  MSCULong32 rCV;
  MSCUChar8 rx[200];

  MSCUChar8 deleteObjDir[7]   = {0xF0, 0xE4, 0x00, 0x00, 0x02, 0x3F, 0xCE};
  MSCUChar8 deleteInfFile[7]  = {0xF0, 0xE4, 0x00, 0x00, 0x02, 0xFF, 0xFE};
  MSCUChar8 deleteKeyDir[7]   = {0xF0, 0xE4, 0x00, 0x00, 0x02, 0x3F, 0xCF};
  MSCUChar8 deletePubFile[7]  = {0xF0, 0xE4, 0x00, 0x00, 0x02, 0x10, 0x12};
  MSCUChar8 deletePrvFile[7]  = {0xF0, 0xE4, 0x00, 0x00, 0x02, 0x00, 0x12};
  MSCUChar8 deletePinFile[7]  = {0xF0, 0xE4, 0x00, 0x00, 0x02, 0x00, 0x00};

  MSCUChar8 verifyKey[13]     = {0xF0, 0x2A, 0x00, 0x01, 0x08, 0x2C, 0x15, 
				 0xE5, 0x26, 0xE9, 0x3E, 0x8A, 0x19};
  
  MSCUChar8 createObjDir[21]  = {0xF0, 0xE0, 0x00, 0x00, 0x10, 0xFF, 0xFF, 
				 0x61, 0xA8, 0x3F, 0xCE, 0x38, 0x00, 0x00, 
				 0x10, 0x00, 0x01, 0x03, 0x00, 0x00, 0x00};

  MSCUChar8 createInfFile[21] = {0xF0, 0xE0, 0x00, 0x01, 0x10, 0xFF, 0xFF, 
				 0x00, 0x80, 0xFF, 0xFE, 0x01, 0x00, 0x01,
				 0x44, 0x44, 0x01, 0x03, 0x00, 0x00, 0x00};
  
  MSCUChar8 createKeyDir[21]  = {0xF0, 0xE0, 0x00, 0x00, 0x10, 0xFF, 0xFF, 
				 0x05, 0x50, 0x3F, 0xCF, 0x38, 0x00, 0x00, 
				 0x44, 0x00, 0x01, 0x03, 0x00, 0x11, 0x00};
  
  /* Note: the Write Binary 8 LSB should be changed for AUT0 */
  
  MSCUChar8 createPrivFile[21] = {0xF0, 0xE0, 0x00, 0x01, 0x10, 0xFF, 0xFF, 
				  0x02, 0x98, 0x00, 0x12, 0x01, 0x00, 0xF1, 
				  0xFF, 0x11, 0x01, 0x03, 0x00, 0x00, 0x00};
  
  MSCUChar8 createPubFile[21] = { 0xF0, 0xE0, 0x00, 0x01, 0x10, 0xF1, 0xFF, 
				  0x02, 0x98, 0x10, 0x12, 0x01, 0x00, 0x01, 
				  0xFF, 0x44, 0x01, 0x03, 0x00, 0x00, 0x00};

  MSCUChar8 createPinFile[21] = { 0xF0, 0xE0, 0x00, 0x01, 0x10, 0xFF, 0xFF, 
				  0x00, 0x17, 0x00, 0x00, 0x01, 0x00, 0xF0,
				  0xFF, 0x44, 0x01, 0x03, 0x00, 0x00, 0x00};
  
  MSCUChar8 writeDefaultPin[28] = { 0xC0, 0xD6, 0x00, 0x00, 0x17, 0xFF, 0xFF, 
				    0xFF, 'M', 'u', 's', 'c', 'l', 'e', 
				    '0', '0', 0x05, 0x05, 'M', 'u', 's', 'c', 
				    'l', 'e', '0', '1', 0x05, 0x05};

  MSCUChar8 writeDefaultAAK[17] = { 0xC0, 0xD6, 0x00, 0x0D, 0x0C, 0x08, 0x00,
				    'M', 'u', 's', 'c', 'l', 'e', '0', '0',
				    0x0A, 0x0A };
  
  /* FIX :: Add support for alternative transport keys */

  if ( pInitParams->transportBehavior == MSC_INIT_DEFAULT_KEY ) {
    /* Do nothing */
  } else if ( pInitParams->transportBehavior == MSC_INIT_IGNORE_KEY ) {
    /* Do nothing */
  } else if ( pInitParams->transportBehavior == MSC_INIT_USE_KEY ) {
    if ( pInitParams->transportKeyLen != 8 ) {
      return MSC_INVALID_PARAMETER;
    }
    memcpy(&verifyKey[5], pInitParams->transportKey, 8);
  }
    
  if ( pInitParams->objectMemory == 0 ) {
    pInitParams->objectMemory = 8500;
  } 
  
  if (pInitParams->newTransportKeyLen == 8) {
    memcpy(&writeDefaultAAK[7], pInitParams->newTransportKey, 8);
  }

  if (pInitParams->defaultCHVTries != 0) {
    writeDefaultPin[16] = pInitParams->defaultCHVTries;
    writeDefaultPin[17] = pInitParams->defaultCHVTries;
  }

  if (pInitParams->defaultCHVUnblockTries != 0) {
    writeDefaultPin[26] = pInitParams->defaultCHVUnblockTries;
    writeDefaultPin[27] = pInitParams->defaultCHVUnblockTries;
  }

  if (pInitParams->defaultCHVLen == 8) {
    memcpy(&writeDefaultPin[8], pInitParams->defaultCHV, 8);
  }

  if (pInitParams->defaultCHVUnblockSize == 8) {
    memcpy(&writeDefaultPin[18], pInitParams->defaultCHVUnblock, 8);
  }

  createObjDir[7] = pInitParams->objectMemory / 256;
  createObjDir[8] = pInitParams->objectMemory % 256;
  
  if ( pInitParams->transportBehavior != MSC_INIT_IGNORE_KEY ) {
    rv = PL_MSCSelect(pConnection, 0x3F00);
    if ( rv != MSC_SUCCESS ) return rv;
    
    rCV = 100;
    rv = SCardTransmit(pConnection->hCard, SCARD_PCI_T0, 
		       verifyKey, 13, 0, rx, &rCV); 
    if ( rv != SCARD_S_SUCCESS ) return convertPCSC(rv);
    if ( convertSW(rx) != MSC_SUCCESS ) return convertSW(rx);
  }
  
  /*************************************/
  /* Delete this file structure off of */
  /* the card if it already exists     */
  /*************************************/
  
/*************************************************************************/

  PL_MSCSelect(pConnection, 0x3F00);
  PL_MSCSelect(pConnection, 0x3FCE);

  rCV = 100;
  rv = SCardTransmit(pConnection->hCard, SCARD_PCI_T0, 
		     deleteInfFile, 7, 0, rx, &rCV); 
  if ( rv != SCARD_S_SUCCESS ) return convertPCSC(rv);

  PL_MSCSelect(pConnection, 0x3F00);
  PL_MSCSelect(pConnection, 0x3FCF);

  rCV = 100;
  rv = SCardTransmit(pConnection->hCard, SCARD_PCI_T0, 
		     deletePrvFile, 7, 0, rx, &rCV); 
  if ( rv != SCARD_S_SUCCESS ) return convertPCSC(rv);

  PL_MSCSelect(pConnection, 0x3F00);
  
  rCV = 100;
  rv = SCardTransmit(pConnection->hCard, SCARD_PCI_T0, 
		     deletePinFile, 7, 0, rx, &rCV); 
  if ( rv != SCARD_S_SUCCESS ) return convertPCSC(rv);

  PL_MSCSelect(pConnection, 0x3F00);
  PL_MSCSelect(pConnection, 0x3FCF);

  rCV = 100;
  rv = SCardTransmit(pConnection->hCard, SCARD_PCI_T0, 
		     deletePubFile, 7, 0, rx, &rCV); 
  if ( rv != SCARD_S_SUCCESS ) return convertPCSC(rv);

  PL_MSCSelect(pConnection, 0x3F00);
  
  rCV = 100;
  rv = SCardTransmit(pConnection->hCard, SCARD_PCI_T0, 
		     deleteKeyDir, 7, 0, rx, &rCV); 
  if ( rv != SCARD_S_SUCCESS ) return convertPCSC(rv);

  PL_MSCSelect(pConnection, 0x3F00);
  
  rCV = 100;
  rv = SCardTransmit(pConnection->hCard, SCARD_PCI_T0, 
		     deleteObjDir, 7, 0, rx, &rCV); 
  if ( rv != SCARD_S_SUCCESS ) return convertPCSC(rv);


 /*************************************/
 /* Add the new file structure        */
 /*************************************/

/*************************************************************************/

  PL_MSCSelect(pConnection, 0x3F00);

  rCV = 100;
  rv = SCardTransmit(pConnection->hCard, SCARD_PCI_T0, 
		     createObjDir, 21, 0, rx, &rCV); 
  if ( rv != SCARD_S_SUCCESS ) return convertPCSC(rv);
  if ( convertSW(rx) != MSC_SUCCESS ) return convertSW(rx);

/*************************************************************************/

  PL_MSCSelect(pConnection, 0x3F00);
  PL_MSCSelect(pConnection, 0x3FCE);

  rCV = 100;
  rv = SCardTransmit(pConnection->hCard, SCARD_PCI_T0, 
		     createInfFile, 21, 0, rx, &rCV); 
  if ( rv != SCARD_S_SUCCESS ) return convertPCSC(rv);
  if ( convertSW(rx) != MSC_SUCCESS ) return convertSW(rx);

/*************************************************************************/

  PL_MSCSelect(pConnection, 0x3F00);  

  rCV = 100;
  rv = SCardTransmit(pConnection->hCard, SCARD_PCI_T0, 
		     createKeyDir, 21, 0, rx, &rCV); 
  if ( rv != SCARD_S_SUCCESS ) return convertPCSC(rv);
  if ( convertSW(rx) != MSC_SUCCESS ) return convertSW(rx);

/*************************************************************************/

  PL_MSCSelect(pConnection, 0x3F00);
  PL_MSCSelect(pConnection, 0x3FCF);

  rCV = 100;
  rv = SCardTransmit(pConnection->hCard, SCARD_PCI_T0, 
		     createPubFile, 21, 0, rx, &rCV); 
  if ( rv != SCARD_S_SUCCESS ) return convertPCSC(rv);
  if ( convertSW(rx) != MSC_SUCCESS ) return convertSW(rx);

/*************************************************************************/

  PL_MSCSelect(pConnection, 0x3F00);
  PL_MSCSelect(pConnection, 0x3FCF);

  rCV = 100;
  rv = SCardTransmit(pConnection->hCard, SCARD_PCI_T0, 
		     createPrivFile, 21, 0, rx, &rCV); 
  if ( rv != SCARD_S_SUCCESS ) return convertPCSC(rv);
  if ( convertSW(rx) != MSC_SUCCESS ) return convertSW(rx);

/*************************************************************************/

  PL_MSCSelect(pConnection, 0x3F00);

  rCV = 100;
  rv = SCardTransmit(pConnection->hCard, SCARD_PCI_T0, 
		     createPinFile, 21, 0, rx, &rCV); 
  if ( rv != SCARD_S_SUCCESS ) return convertPCSC(rv);
  if ( convertSW(rx) != MSC_SUCCESS ) return convertSW(rx);

/*************************************************************************/

  PL_MSCSelect(pConnection, 0x3F00);
  PL_MSCSelect(pConnection, 0x0000);

  rCV = 100;
  rv = SCardTransmit(pConnection->hCard, SCARD_PCI_T0, 
		     writeDefaultPin, 28, 0, rx, &rCV); 
  if ( rv != SCARD_S_SUCCESS ) return convertPCSC(rv);
  if ( convertSW(rx) != MSC_SUCCESS ) return convertSW(rx);

/*************************************************************************/

  PL_MSCSelect(pConnection, 0x3F00);
  PL_MSCSelect(pConnection, 0x0011);
  
  rCV = 100;
  rv = SCardTransmit(pConnection->hCard, SCARD_PCI_T0, 
		     writeDefaultAAK, 17, 0, rx, &rCV); 
  if ( rv != SCARD_S_SUCCESS ) return convertPCSC(rv);
  if ( convertSW(rx) != MSC_SUCCESS ) return convertSW(rx);

  return MSC_SUCCESS;
}

/* MSC Functions */

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCGetStatus( MSCLPTokenConnection pConnection, 
		     MSCLPStatusInfo pStatusInfo ) {

  MSCLong32 rv;
  MSCTransmitBuffer transmitBuffer;
  MSCPUChar8 apduResponse;
  MSCPUChar8 pBuffer;
  MSCObjectInfo objInfo;
  MSCULong32 totalMemory, freeMemory;

  PL_MSCSelect(pConnection, 0x3F00);

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CLA_C0;
  pBuffer[OFFSET_INS]    = 0xA4;
  pBuffer[OFFSET_P1]     = 0x00;
  pBuffer[OFFSET_P2]     = 0x00;
  pBuffer[OFFSET_P3]     = 0x02;
  pBuffer[OFFSET_DATA]   = 0x3F;
  pBuffer[OFFSET_DATA+1] = 0xCE;
 
  transmitBuffer.bufferSize = 7;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU(pConnection, &transmitBuffer);
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    return convertSW(apduResponse);
  }

  
  freeMemory  = apduResponse[2] * 0x100 + apduResponse[3];
  totalMemory = freeMemory;

  pStatusInfo->appVersion  = 0xFF;
  pStatusInfo->swVersion   = 0xFF;
  pStatusInfo->usedPINs    = 0x01;
  pStatusInfo->usedKeys    = 0x00;
  pStatusInfo->loggedID    = pConnection->loggedIDs;

  rv = PL_MSCListObjects( pConnection, MSC_SEQUENCE_RESET, &objInfo );

  while (rv == MSC_SUCCESS) {
    totalMemory += objInfo.objectSize;
    rv = PL_MSCListObjects( pConnection, MSC_SEQUENCE_NEXT, &objInfo );
  }

  pStatusInfo->totalMemory = totalMemory;
  pStatusInfo->freeMemory  = freeMemory - CRYPTOFLEX_INFOBJ_LEN - 16;

  return MSC_SUCCESS;
}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCGetCapabilities( MSCLPTokenConnection pConnection, MSCULong32 Tag,
			   MSCPUChar8 Value, MSCPULong32 Length ) {

  MSCULong32  ulValue;
  MSCUShort16 usValue;
  MSCUChar8  ucValue;
  MSCUChar8  tagType;

  /* 4 - MSCULong32, 2 - MSCUShort16, 1 - MSCUChar8 */

  ulValue = 0; usValue = 0; ucValue = 0; tagType = 0;

  switch(Tag) {

  case MSC_TAG_SUPPORT_FUNCTIONS:
    ulValue = MSC_SUPPORT_GENKEYS | MSC_SUPPORT_EXPORTKEY | 
      MSC_SUPPORT_COMPUTECRYPT | MSC_SUPPORT_LISTKEYS | MSC_SUPPORT_IMPORTKEY |
      MSC_SUPPORT_VERIFYPIN | MSC_SUPPORT_CHANGEPIN | 
      MSC_SUPPORT_UNBLOCKPIN | MSC_SUPPORT_LISTPINS |
      MSC_SUPPORT_CREATEOBJECT | MSC_SUPPORT_DELETEOBJECT | 
      MSC_SUPPORT_WRITEOBJECT | MSC_SUPPORT_READOBJECT |
      MSC_SUPPORT_LISTOBJECTS | MSC_SUPPORT_LOGOUTALL |
      MSC_SUPPORT_GETCHALLENGE;
    tagType = 4;
    break;

  case MSC_TAG_SUPPORT_CRYPTOALG:
    ulValue = MSC_SUPPORT_RSA;
    tagType = 4;
    break;

  case MSC_TAG_CAPABLE_RSA:
    ulValue = MSC_CAPABLE_RSA_1024 | MSC_CAPABLE_RSA_NOPAD | 
              MSC_CAPABLE_RSA_KEYGEN;
    tagType = 4;
    break;

  case MSC_TAG_CAPABLE_OBJ_ATTR:
    ulValue = 0;
    tagType = 4;
    break;

  case MSC_TAG_CAPABLE_OBJ_IDSIZE:
    ucValue = 2;
    tagType = 1;
    break;

  case MSC_TAG_CAPABLE_OBJ_AUTH:
    usValue = MSC_AUT_ALL;
    tagType = 2;
    break;

  case MSC_TAG_CAPABLE_OBJ_MAXNUM:
    ulValue = 100;
    tagType = 4;
    break;

  case MSC_TAG_CAPABLE_PIN_ATTR:
    ulValue = MSC_CAPABLE_PIN_RESET;
    tagType = 4;
    break;

  case MSC_TAG_CAPABLE_PIN_MAXNUM:
    ucValue = 2;
    tagType = 1;
    break;

  case MSC_TAG_CAPABLE_PIN_MINSIZE:
    ucValue = 1;
    tagType = 1;
    break;

  case MSC_TAG_CAPABLE_PIN_MAXSIZE:
    ucValue = 8;
    tagType = 1;
    break;
    
  case MSC_TAG_CAPABLE_PIN_CHARSET:
    ulValue = MSC_CAPABLE_PIN_A_Z | MSC_CAPABLE_PIN_a_z | 
      MSC_CAPABLE_PIN_0_9 | MSC_CAPABLE_PIN_CALC | MSC_CAPABLE_PIN_NONALPHA;
    tagType = 4;
    break;

 case MSC_TAG_CAPABLE_PIN_POLICY:
   ulValue = 0;
   tagType = 4;
   break;

  case MSC_TAG_CAPABLE_ID_STATE:
    ucValue = 0;
    tagType = 1;
    break;

  case MSC_TAG_CAPABLE_RANDOM_MAX:
    ucValue = 32;
    tagType = 1;
    break;

  case MSC_TAG_CAPABLE_RANDOM_MIN:
    ucValue = 8;
    tagType = 1;
    break;

  case MSC_TAG_CAPABLE_KEY_AUTH:
    usValue = MSC_AUT_PIN_1;
    tagType = 2;
    break;

  case MSC_TAG_CAPABLE_PIN_AUTH:
    usValue = MSC_AUT_NONE;
    tagType = 2;
    break;

  default:
    return MSC_INVALID_PARAMETER;
  }

  switch(tagType) {
  case 1:
    memcpy(Value, &ucValue, 1);
    break;
  case 2:
    memcpy(Value, &usValue, 2);
    break;
  case 4:
    memcpy(Value, &ulValue, 4);
    break;
  }

  *Length = tagType;

  return MSC_SUCCESS;

}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCExtendedFeature( MSCLPTokenConnection pConnection, 
			   MSCULong32 extFeature, MSCPUChar8 outData, 
			   MSCULong32 outLength, MSCPUChar8 inData,
			   MSCPULong32 inLength ) {

  return MSC_UNSUPPORTED_FEATURE;
}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCGenerateKeys( MSCLPTokenConnection pConnection, 
			   MSCUChar8 prvKeyNum, MSCUChar8 pubKeyNum, 
			   MSCLPGenKeyParams pParams ) {
  
  MSCLong32 rv;
  MSCTransmitBuffer transmitBuffer;
  MSCPUChar8 apduResponse;
  MSCUChar8 pubKeyNumBak;
  MSCUChar8 prvKeyNumBak;
  MSCKeyACL privACL;
  MSCKeyACL pubACL;
  MSCPUChar8 pBuffer;

  /* PG: a common key pair number can only be set in Cryptoflex,
     so enforce that pubKeyNum and prvKeyNum match. */

  /* DC: Musclecard has each key number as a unique number but
     keys should be in sequence - priv 0, pub 1 ... 
     Here we will set the pubKeyNum to be the same as the 
     private key number, but public keys will be one + the
     private key number
  */

  if ( pubKeyNum > CRYPTOFLEX_MAXMSC_KEY_NUM ||
       prvKeyNum > CRYPTOFLEX_MAXMSC_KEY_NUM ) {
    return MSC_INVALID_PARAMETER;
  }

  if ( pParams->algoType != MSC_GEN_ALG_RSA_CRT ) {
    return MSC_INVALID_PARAMETER;
  }

  prvKeyNumBak = prvKeyNum;
  pubKeyNumBak = pubKeyNum;

  rv = mapCryptoflexKeys(pConnection, MSC_KEY_RSA_PRIVATE_CRT,
			 pParams->keySize, prvKeyNumBak,
			 &prvKeyNum);
  
  if ( rv != MSC_SUCCESS ) return rv;

  rv = mapCryptoflexKeys(pConnection, MSC_KEY_RSA_PUBLIC,
			 pParams->keySize, pubKeyNumBak,
			 &pubKeyNum);

  if ( rv != MSC_SUCCESS ) return rv;

  rv = PL_MSCSelect(pConnection, 0x3F00);
  rv = PL_MSCSelect(pConnection, 0x3FCF);

  if ( rv != MSC_SUCCESS ) {
    return MSC_UNSUPPORTED_FEATURE;
  }


  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CLA_F0;
  pBuffer[OFFSET_INS]    = INS_MSC_GEN_KEYPAIR;
  pBuffer[OFFSET_P1]     = prvKeyNum;

  switch(pParams->keySize) {
  case 1024:
    pBuffer[OFFSET_P2] = 0x80;
    break;
  case 768:
    pBuffer[OFFSET_P2] = 0x60;
    break;
  case 512:
    pBuffer[OFFSET_P2] = 0x40;
    break;
  default:
    return MSC_INVALID_PARAMETER;
  }

  /* PG: length of the public exponent, e */
  
  if ( pParams->keyGenOptions != MSC_OPT_DEFAULT ) {
    pBuffer[OFFSET_P3] = pParams->keyGenOptions;
    /* PG: the public exponent value */
    memcpy(&pBuffer[OFFSET_DATA], pParams->pOptParams, 
	   pParams->optParamsSize);
    
    transmitBuffer.bufferSize = 5 + pParams->keyGenOptions;
    
  } else {
    /* Take the defaults */
    pBuffer[OFFSET_P3]     = 0x04; /* Length of the following */
    pBuffer[OFFSET_DATA]   = 0x01; /* Common public exponent  */
    pBuffer[OFFSET_DATA+1] = 0x00;
    pBuffer[OFFSET_DATA+2] = 0x01;
    pBuffer[OFFSET_DATA+3] = 0x00;
    transmitBuffer.bufferSize = 9;    
  }


  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU(pConnection, &transmitBuffer);
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {

    if ( convertSW(apduResponse) == MSC_SUCCESS ) {

      privACL.readPermission  = MSC_AUT_NONE;
      privACL.writePermission = MSC_AUT_PIN_1;
      privACL.usePermission   = MSC_AUT_PIN_1;

      rv = PL_MSCWriteKeyInfo( pConnection, prvKeyNumBak, 
			       MSC_KEY_RSA_PRIVATE_CRT, 
			       prvKeyNum, pubKeyNumBak,
			       pParams->keySize, &pParams->privateKeyPolicy, 
			       &privACL );

      if ( rv != MSC_SUCCESS ) return rv;

      pubACL.readPermission  = MSC_AUT_ALL;
      pubACL.writePermission = MSC_AUT_PIN_1;
      pubACL.usePermission   = MSC_AUT_PIN_1;

      rv = PL_MSCWriteKeyInfo( pConnection, pubKeyNumBak, MSC_KEY_RSA_PUBLIC, 
			       pubKeyNum, prvKeyNumBak, pParams->keySize, 
			       &pParams->publicKeyPolicy, &pubACL );
      return rv;
    }


    return convertSW(apduResponse);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }
}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCImportKey( MSCLPTokenConnection pConnection, MSCUChar8 keyNum,
                        MSCLPKeyACL pKeyACL, 
			MSCPUChar8 pKeyBlob, MSCULong32 keyBlobSize, 
			MSCLPKeyPolicy keyPolicy, MSCPVoid32 pAddParams, 
			MSCUChar8 addParamsSize ) {

  MSCLong32 rv;
  MSCUChar8 keyBlobCryptoflex[2500];
  MSCULong32 currentPointer;
  MSCKeyACL currentACL;
  MSCUChar8 keyType;
  MSCUChar8 keyNumBak;
  MSCUShort16 keySize;
  MSCULong32 expSize;

  keyNumBak = keyNum;
  keyType   = pKeyBlob[OFFSET_KEYTYPE];
  MemCopyTo16(&keySize, &pKeyBlob[OFFSET_KEYSIZE]);


  if ( keyType != MSC_KEY_RSA_PRIVATE_CRT && 
       keyType != MSC_KEY_DES &&
       keyType != MSC_KEY_RSA_PUBLIC ) 
    {
      return MSC_UNSUPPORTED_FEATURE;
    }

  rv = mapCryptoflexKeys(pConnection, keyType,
			 keySize, keyNumBak, &keyNum);

  if ( rv != MSC_SUCCESS ) return rv;

  if ( keyType == MSC_KEY_RSA_PRIVATE_CRT ) {
    
    if ( keySize != 1024 ) {
      return MSC_UNSUPPORTED_FEATURE;
    }

    rv = PL_MSCSelect(pConnection, 0x3F00);
    rv = PL_MSCSelect(pConnection, 0x3FCF);
    
    if ( rv != MSC_SUCCESS ) {
      return MSC_UNSUPPORTED_FEATURE;
    }

    /* Place the key in bytes in Cryptoflex style */
    
    currentPointer = 0;
    keyBlobCryptoflex[currentPointer] = CF_1024_FULLSIZE_1;
    currentPointer += CF_SIZEOF_MSBLEN;
    keyBlobCryptoflex[currentPointer] = CF_1024_FULLSIZE_2;
    currentPointer += CF_SIZEOF_LSBLEN;
    keyBlobCryptoflex[currentPointer] = keyNum + 1;
    currentPointer += CF_SIZEOF_KEYNUM;
    
    /* Copy P */
    
    MemCopyReverse(&keyBlobCryptoflex[currentPointer], 
		   &pKeyBlob[MC_1024_OFFSET_P+2], CF_1024_COMPSIZE);
    
    currentPointer += CF_1024_COMPSIZE;
    
    /* Copy Q */
    
    MemCopyReverse(&keyBlobCryptoflex[currentPointer], 
		   &pKeyBlob[MC_1024_OFFSET_Q+2], CF_1024_COMPSIZE);
    
    currentPointer += CF_1024_COMPSIZE;
    
    /* Copy PQ */
    
    MemCopyReverse(&keyBlobCryptoflex[currentPointer], 
		   &pKeyBlob[MC_1024_OFFSET_PQ+2], CF_1024_COMPSIZE);
    
    currentPointer += CF_1024_COMPSIZE;
    
    /* Copy DP1 */
    
    MemCopyReverse(&keyBlobCryptoflex[currentPointer], 
		   &pKeyBlob[MC_1024_OFFSET_DP1+2], CF_1024_COMPSIZE);
    
    currentPointer += CF_1024_COMPSIZE;
    
    /* Copy DQ1 */
    
    MemCopyReverse(&keyBlobCryptoflex[currentPointer], 
		   &pKeyBlob[MC_1024_OFFSET_DQ1+2], CF_1024_COMPSIZE);
    
    currentPointer += CF_1024_COMPSIZE;
    
    rv = PL_MSCWriteLargeObjectOffCB( pConnection, "#0x0012",
				      keyNum * CF_1024_FULLSIZE,
				      keyBlobCryptoflex, CF_1024_FULLSIZE,
				      0, 0 ); 

    if ( rv != MSC_SUCCESS ) {
      return rv;
    }     

  } else if ( keyType == MSC_KEY_RSA_PUBLIC ) {
    
    if ( keySize != 1024 ) {
      return MSC_UNSUPPORTED_FEATURE;
    }

    rv = PL_MSCSelect(pConnection, 0x3F00);
    rv = PL_MSCSelect(pConnection, 0x3FCF);
    
    if ( rv != MSC_SUCCESS ) {
      return MSC_UNSUPPORTED_FEATURE;
    }

    /* Place the key in bytes in Cryptoflex style */
    
    currentPointer = 0;
    keyBlobCryptoflex[currentPointer] = 0x01;
    currentPointer += CF_SIZEOF_MSBLEN;
    keyBlobCryptoflex[currentPointer] = 0x47;
    currentPointer += CF_SIZEOF_LSBLEN;
    keyBlobCryptoflex[currentPointer] = keyNum + 1;
    currentPointer += CF_SIZEOF_KEYNUM;
    
    /* Copy N */
    memset(&keyBlobCryptoflex[currentPointer], 
	   0x00, MC_1024P_MODSIZE);
    
    currentPointer += MC_1024P_MODSIZE;
    
    /* Skip J0 + H */
    memset(&keyBlobCryptoflex[currentPointer], 0x00, 192);
    currentPointer += 192;

    /* Copy E */
    expSize = (pKeyBlob[MC_1024P_EXP] * 256)+pKeyBlob[MC_1024P_EXP+1];
    memset(&keyBlobCryptoflex[currentPointer], 0x00, 0x04);

    MemCopyReverse(&keyBlobCryptoflex[currentPointer], 
		   &pKeyBlob[MC_1024P_EXP+2], expSize);
    
    rv = PL_MSCWriteLargeObjectOffCB( pConnection, "#0x1012",
				      keyNum * 0x0147,
				      keyBlobCryptoflex, 0x0147,
				      0, 0 ); 

    if ( rv != MSC_SUCCESS ) {
      return rv;
    }     

  } else if ( keyType == MSC_KEY_DES ) {

    if ( keySize != 64 ) {
      return MSC_INVALID_PARAMETER;
    }

    /* FIX :: Must be careful here - old manual list an RFU byte first
              but the new manual lists no RFU byte at all
    */
    
    keyBlobCryptoflex[0] = 0x08; /* key size */
    keyBlobCryptoflex[1] = 0x00; /* key type */
    memcpy(&keyBlobCryptoflex[2], &pKeyBlob[MC_DES_OFFSET_KEY+2], 8);
    keyBlobCryptoflex[10] = 0x0A;
    keyBlobCryptoflex[11] = 0x0A;

    rv = PL_MSCWriteObject( pConnection, "#0x0011", CF_DES_OFFSET_UKEY,
			    keyBlobCryptoflex, 0x0C );
			 
    if ( rv != MSC_SUCCESS ) return rv;
    
  } else {    
    /* RSA public would go here */
    
    rv =  MSC_UNSUPPORTED_FEATURE;
  }

  switch(keyType) {
  case MSC_KEY_RSA_PRIVATE_CRT:
  case MSC_KEY_RSA_PRIVATE:
    currentACL.readPermission  = MSC_AUT_NONE;
    currentACL.writePermission = MSC_AUT_PIN_1;
    currentACL.usePermission   = MSC_AUT_PIN_1;	
    break;
    
  case MSC_KEY_RSA_PUBLIC:
    currentACL.readPermission  = MSC_AUT_ALL;
    currentACL.writePermission = MSC_AUT_PIN_1;
    currentACL.usePermission   = MSC_AUT_PIN_1;
    break;
    
  case MSC_KEY_DES:
  case MSC_KEY_3DES:
  case MSC_KEY_3DES3:
    currentACL.readPermission  = MSC_AUT_PIN_0;
    currentACL.writePermission = MSC_AUT_PIN_0;
    currentACL.usePermission   = MSC_AUT_ALL;
    break;
    
  default:
    return MSC_UNSUPPORTED_FEATURE;
  }
  
  rv = PL_MSCWriteKeyInfo( pConnection, keyNumBak, keyType, 
			   keyNum, 0xFF, keySize, keyPolicy, 
			   &currentACL );
    
  return rv;
}
 
#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCExportKey( MSCLPTokenConnection pConnection, MSCUChar8 keyNum,
		     MSCPUChar8 pKeyBlob, MSCPULong32 keyBlobSize, 
		     MSCPVoid32 pAddParams, MSCUChar8 addParamsSize ) {

  MSCLong32 rv;
  MSCUChar8 keyBuffer[2500];
  MSCKeyInfo keyStruct;
  MSCULong32 currentPointer;
  MSCULong32 blobSize;
  MSCUShort16 currentVal;
  MSCUChar8 keyNumCF;
  int i;

  i=0; blobSize=0; rv=0; currentPointer=0; keyNumCF=0; currentVal=0;

  if ( pConnection == 0 || keyBlobSize == 0 || pKeyBlob == 0 ) {
    return MSC_INVALID_PARAMETER;
  }

  keyStruct.keyNum = 0xFF;
  rv = MSCListKeys(pConnection, MSC_SEQUENCE_RESET, &keyStruct);

  if ( rv != MSC_SEQUENCE_END ) {
    do {
      if ( keyStruct.keyNum == keyNum ) {
	break;
      }
      rv = MSCListKeys(pConnection, MSC_SEQUENCE_NEXT, &keyStruct);
      /* FIX :: potential problem if error occurs ... */
    } while ( rv != MSC_SEQUENCE_END );
  }

  /* Match not found */
  if ( keyStruct.keyNum == 0xFF ) {
    return MSC_INVALID_PARAMETER;
  }

  if ( keyStruct.keyType != MSC_KEY_RSA_PUBLIC ) {
    return MSC_UNSUPPORTED_FEATURE;
  }
    
  keyNumCF = keyStruct.keyPartner;
  
  /* not sure if this is right */

  rv =  PL_MSCReadLargeObjectOffCB( pConnection, "#0x1012", 
				    keyNumCF*CF_1024P_FULLSIZE, 
				    keyBuffer,  CF_1024P_FULLSIZE, 0, 0);   

  if ( rv != MSC_SUCCESS ) return rv;

  /* Copy to a MuscleCard defined RSA_PUB key blob as defined
      in the protocol spec 
  */
  
  pKeyBlob[OFFSET_ENCODING] = MSC_BLOB_ENC_PLAIN;
  pKeyBlob[OFFSET_KEYTYPE]  = MSC_KEY_RSA_PUBLIC;  

  currentVal = CF_1024_KEYSIZE;
  MemCopy16(&pKeyBlob[OFFSET_KEYSIZE], &currentVal);

  currentVal = CF_1024P_MODSIZE;
  MemCopy16(&pKeyBlob[MC_1024P_MOD], &currentVal);
  MemCopyReverse(&pKeyBlob[MC_1024P_MOD + 2], 
		 &keyBuffer[CF_1024P_MOD], CF_1024P_MODSIZE);

  currentVal = CF_1024P_EXPSIZE;
  MemCopy16(&pKeyBlob[MC_1024P_EXP], &currentVal);
  MemCopyReverse(&pKeyBlob[MC_1024P_EXP + 2], 
		 &keyBuffer[CF_1024P_EXP], CF_1024P_EXPSIZE);

  *keyBlobSize = MC_1024P_FULLSIZE; 

  return MSC_SUCCESS;
}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCComputeCrypt( MSCLPTokenConnection pConnection,
			MSCLPCryptInit cryptInit, MSCPUChar8 pInputData,
			MSCULong32 inputDataSize, MSCPUChar8 pOutputData,
			MSCPULong32 outputDataSize ) {

  MSCLong32 rv;
  MSCTransmitBuffer transmitBuffer;
  MSCPUChar8 apduResponse;
  MSCPUChar8 pBuffer;
  MSCUChar8 cflKeyNum;

  if ( cryptInit->cipherMode == MSC_MODE_RSA_NOPAD ) {
    /* Some RSA */
    rv = mapCryptoflexKeys(pConnection, MSC_KEY_RSA_PRIVATE_CRT,
			   1024, cryptInit->keyNum,
			   &cflKeyNum);


    if ( rv != MSC_SUCCESS ) return rv;
  } else if ( cryptInit->cipherMode == MSC_MODE_DES_ECB_NOPAD ) {
    /* Some DES */
    rv = mapCryptoflexKeys(pConnection, MSC_KEY_DES,
			   64, cryptInit->keyNum,
			   &cflKeyNum);			   
    if ( rv != MSC_SUCCESS ) return rv;
  } else {
    return MSC_UNSUPPORTED_FEATURE;
  }

  rv = PL_MSCSelect(pConnection, 0x3F00);
  rv = PL_MSCSelect(pConnection, CRYPTOFLEX_MSC_KEY_DIR);

  if ( rv != MSC_SUCCESS ) return rv;

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CLA_C0;
  pBuffer[OFFSET_INS]    = INS_COMPUTE_CRYPT;
  pBuffer[OFFSET_P1]     = 0x00;
  pBuffer[OFFSET_P2]     = cflKeyNum;

  /* PG: 0x40, 0x60, 0x80 - possible lengths of the cryptogram */
  /* DC: Only support for 1023 bit RSA for now */
  if(inputDataSize != 8 && inputDataSize != 128)
    return MSC_INVALID_PARAMETER;

  pBuffer[OFFSET_P3]     = inputDataSize;

  /* PG: 64, 96 or 128 bytes (respectively) to be encrypted
     NOTE: you must enter the data in LSB format */
  MemCopyReverse(&pBuffer[OFFSET_DATA], pInputData, inputDataSize);

  transmitBuffer.bufferSize = 5 + inputDataSize;

  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU(pConnection, &transmitBuffer);
  
  if(rv != SCARD_S_SUCCESS) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == inputDataSize + 2 ) {
    *outputDataSize = transmitBuffer.apduResponseSize - 2;
    MemCopyReverse(pOutputData, apduResponse,
		   *outputDataSize);

    return MSC_SUCCESS;
  } else if ( transmitBuffer.apduResponseSize == 2 ) {
    return convertSW(apduResponse);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }
}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCExtAuthenticate( MSCLPTokenConnection pConnection, 
			      MSCUChar8 keyNum,
			      MSCUChar8 cipherMode, 
			      MSCUChar8 cipherDirection,
			      MSCPUChar8 pData, 
			      MSCULong32 dataSize )
{

  MSCLong32 rv;
  MSCTransmitBuffer transmitBuffer;
  MSCPUChar8 apduResponse;
  MSCPUChar8 pBuffer;

  /* PG: Cryptoflex uses DES for external authentication */

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CLA_C0;
  pBuffer[OFFSET_INS]    = INS_EXT_AUTH;
  pBuffer[OFFSET_P1]     = 0x00;
  pBuffer[OFFSET_P2]     = 0x00;
  /* PG: the length of the input data
     (a 1-byte key number plus a 6-byte truncated cryptogram) */
  pBuffer[OFFSET_P3]     = 0x07;

  pBuffer[OFFSET_DATA] = keyNum;

  /* The cryptogram must be 6 bytes long  */
  memcpy(&pBuffer[OFFSET_DATA + 1], pData, 6);

  transmitBuffer.bufferSize =  pBuffer[OFFSET_P3] + 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU(pConnection, &transmitBuffer);
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    return convertSW(apduResponse);
  } else {
    /* dumpBinary(apduResponse, transmitBuffer.apduResponseSize); */
    return convertSW(&apduResponse[transmitBuffer.apduResponseSize-2]);
  }

}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCListKeys( MSCLPTokenConnection pConnection, MSCUChar8 seqOption,
		       MSCLPKeyInfo pKeyInfo ) {

    /* PG: Since we don't provide neither AUT nor PRO protection
       it's useless to give information about these keys */

  /* Dave: One key pair is by default in directory 3FCF 
     more work needs to be done here
  */


  MSCLong32 rv;
  static MSCUChar8 keyByteMask[CRYPTOFLEX_MAXMSC_KEY_NUM+1];
  static int sequenceNumber = 0;
  int i, j;

  if ( seqOption == MSC_SEQUENCE_RESET ) {
    rv = PL_MSCReadKeyInfo(pConnection, 0);
    if ( rv != MSC_SUCCESS ) return rv;

    for (i=0; i <= CRYPTOFLEX_MAXMSC_KEY_NUM; i++) {
      pKeyInfo->keyNum = i;
      rv = PL_MSCReadKeyInfo(pConnection, pKeyInfo);

      if ( rv != MSC_SUCCESS ) return rv;
      
      /* Check if the key entry is present or missing */
      if ( pKeyInfo->keyNum == 0 && pKeyInfo->keyType == 0 &&
	   pKeyInfo->keyPartner == 0 && pKeyInfo->keySize == 0 ) {
	keyByteMask[i] = 0;
      } else {
	keyByteMask[i] = 1;
      }

    }
    
    sequenceNumber = 1;
  } else {
    sequenceNumber += 1;
  }

  j = 0;

  for (i=0; i < sequenceNumber; i++) {
    do {
      if ( keyByteMask[j] == 1 ) {
	j += 1;
	break;
      }

      j += 1;

      if ( j > CRYPTOFLEX_MAXMSC_KEY_NUM ) 
	return MSC_SEQUENCE_END;

    } while (1);
  }


  pKeyInfo->keyNum = j - 1;
  rv = PL_MSCReadKeyInfo(pConnection, pKeyInfo);
  
  return rv;
}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCCreatePIN( MSCLPTokenConnection pConnection, MSCUChar8 pinNum,
		     MSCUChar8 pinAttempts, MSCPUChar8 pPinCode,
		     MSCULong32 pinCodeSize, MSCPUChar8 pUnblockCode,
		     MSCUChar8 unblockCodeSize ) {

  return MSC_UNSUPPORTED_FEATURE;
}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCVerifyPIN( MSCLPTokenConnection pConnection, MSCUChar8 pinNum,
		     MSCPUChar8 pPinCode, MSCULong32 pinCodeSize ) {
  
  MSCLong32 rv;
  MSCTransmitBuffer transmitBuffer;
  MSCPUChar8 apduResponse;
  MSCPUChar8 pBuffer;

  /*
    if ( pinCodeSize != 8 ) {
    return MSC_INVALID_PARAMETER;
    }
  */

  rv = PL_MSCSelect(pConnection, 0x3F00);

  if ( pinNum == 0 ) {
    rv = PL_MSCVerifyKey(pConnection, pPinCode, pinCodeSize);
    if ( rv == MSC_SUCCESS ) { pConnection->loggedIDs |= MSC_AUT_PIN_0; }
    return rv;
  }

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CLA_C0;
  pBuffer[OFFSET_INS]    = INS_VERIFY_PIN;
  pBuffer[OFFSET_P1]     = 0x00;
  pBuffer[OFFSET_P2]     = pinNum;
  /* PIN code size is enforced by Cryptoflex */
  pBuffer[OFFSET_P3]     = 0x08;


  memset(&pBuffer[OFFSET_DATA], 0xFF, 8);
  memcpy(&pBuffer[OFFSET_DATA], pPinCode, pinCodeSize);
  
  transmitBuffer.bufferSize =  pBuffer[OFFSET_P3] + 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU(pConnection, &transmitBuffer);
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    if ( convertSW(apduResponse) == MSC_SUCCESS ) { 
      /* FIX :: Only did 4 PINs */
      switch(pinNum) {
	case 1:
	  pConnection->loggedIDs |= MSC_AUT_PIN_1;
	break;
	case 2:
	  pConnection->loggedIDs |= MSC_AUT_PIN_2;
	break;
	case 3:
	  pConnection->loggedIDs |= MSC_AUT_PIN_3;
	break;
	case 4:
	  pConnection->loggedIDs |= MSC_AUT_PIN_4;
	break;
      };
    }

    return convertSW(apduResponse);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }

}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCChangePIN( MSCLPTokenConnection pConnection, MSCUChar8 pinNum,
			MSCPUChar8 pOldPinCode, MSCUChar8 oldPinCodeSize,
			MSCPUChar8 pNewPinCode, MSCUChar8 newPinCodeSize ) {

  MSCLong32 rv, rvb;
  MSCTransmitBuffer transmitBuffer;
  MSCPUChar8 apduResponse;
  MSCPUChar8 pBuffer;

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  if ( pinNum == 0 ) {

    if ( oldPinCodeSize != 8 || newPinCodeSize != 8 ) {
      return MSC_INVALID_PARAMETER;
    }

    rv = PL_MSCVerifyKey(pConnection, pOldPinCode, oldPinCodeSize);

    if ( rv != MSC_SUCCESS ) {
      return rv;
    }

    rv = PL_MSCSelect(pConnection, 0x3F00);
    rv = PL_MSCSelect(pConnection, 0x0011);
    
    if ( rv != MSC_SUCCESS ) {
      return MSC_UNSUPPORTED_FEATURE;
    }

    pBuffer[OFFSET_CLA]    = CLA_C0;
    pBuffer[OFFSET_INS]    = INS_WRITE_OBJ;
    pBuffer[OFFSET_P1]     = 0x00;
    pBuffer[OFFSET_P2]     = 0x0D; /* offset for key 1 */
    pBuffer[OFFSET_P3]     = 0x0C;

    pBuffer[OFFSET_DATA]    = 0x08;
    pBuffer[OFFSET_DATA+1]  = 0x00;
    memcpy(&pBuffer[OFFSET_DATA+2], pNewPinCode, 8);

    pBuffer[OFFSET_DATA+10] = 0x05;
    pBuffer[OFFSET_DATA+11] = 0x05;
    transmitBuffer.bufferSize = pBuffer[OFFSET_P3] + 5;

  } else {
    /* FIX: Add support for additional PINs */
    /* PG: PIN code size is enforced by Cryptoflex */

    rv = PL_MSCVerifyPIN(pConnection, pinNum, pOldPinCode, oldPinCodeSize);

    if ( rv != MSC_SUCCESS ) {
      return rv;
    }

    rv = PL_MSCSelect(pConnection, 0x3F00);
    rv = PL_MSCSelect(pConnection, 0x0000);

    if ( rv != MSC_SUCCESS ) {
      return rv;
    }

    pBuffer[OFFSET_CLA]    = CLA_C0;
    pBuffer[OFFSET_INS]    = INS_WRITE_OBJ;
    pBuffer[OFFSET_P1]     = 0x00;
    pBuffer[OFFSET_P2]     = 0x00;
    pBuffer[OFFSET_P3]     = 0x0B;
    pBuffer[OFFSET_DATA]   = 0xFF;
    pBuffer[OFFSET_DATA+1] = 0xFF;
    pBuffer[OFFSET_DATA+2] = 0xFF;


    memset(&pBuffer[OFFSET_DATA+3], 0xFF, 8);
    memcpy(&pBuffer[OFFSET_DATA+3], pNewPinCode, newPinCodeSize);

    transmitBuffer.bufferSize = pBuffer[OFFSET_P3] + 5;
  }

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU(pConnection, &transmitBuffer);
  
  rvb = PL_MSCSelect(pConnection, 0x3F00);
  rvb = PL_MSCSelect(pConnection, CRYPTOFLEX_OBJ_DIR);
  
  if ( rvb != MSC_SUCCESS ) {
    return rvb;
  }

  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    return convertSW(apduResponse);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }

}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCUnblockPIN( MSCLPTokenConnection pConnection, MSCUChar8 pinNum,
		      MSCPUChar8 pUnblockCode, MSCULong32 unblockCodeSize ) {

  MSCLong32 rv;
  MSCTransmitBuffer transmitBuffer;
  MSCPUChar8 apduResponse;
  MSCPUChar8 pBuffer;

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CLA_F0;
  pBuffer[OFFSET_INS]    = INS_UNBLOCK_PIN;
  pBuffer[OFFSET_P1]     = 0x00;
  pBuffer[OFFSET_P2]     = pinNum;
  pBuffer[OFFSET_P3]     = 0x10;

  /* PG: Code length is enforced by Cryptoflex */
  memcpy(&pBuffer[OFFSET_DATA], pUnblockCode, 8);

  /* PG: API PROBLEM: Cryptoflex requires that after the
     unblocking code the new PIN code must be entered */

  memcpy(&pBuffer[OFFSET_DATA + 8], "Muscle00", 8);

  transmitBuffer.bufferSize = pBuffer[OFFSET_P3] + 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU(pConnection, &transmitBuffer);
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    return convertSW(apduResponse);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }
}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCListPINs( MSCLPTokenConnection pConnection, MSCPUShort16 pPinBitMask ) {

    /* PG: There are only two well-known PINs on Cryptoflex card */
  *pPinBitMask = 0x00000003;
  return MSC_SUCCESS;
}

MSC_RV PL_MSCCreateObject( MSCLPTokenConnection pConnection, 
			   MSCString objectID, MSCULong32 objectSize,
			   MSCLPObjectACL pObjectACL ) {

  MSCLong32 rv;
  MSCTransmitBuffer transmitBuffer;
  MSCPUChar8 apduResponse;
  MSCPUChar8 pBuffer;
  MSCULong32 currentPointer;
  MSCUShort16 _objectSize;
  MSCUShort16 _objectID;

  /* PG: conversions */
  _objectSize = 256 * ((objectSize & 0xFF00) >> 8 ) + (objectSize & 0x00FF);

  if ( stringToID(&_objectID, objectID) ) 
    return MSC_INVALID_PARAMETER;


  PL_MSCSelect(pConnection, 0x3F00);
  PL_MSCSelect(pConnection, CRYPTOFLEX_OBJ_DIR);

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CLA_F0;
  pBuffer[OFFSET_INS]    = INS_CREATE_OBJ;
  pBuffer[OFFSET_P1]     = 0x00;
  /* PG: Number of records - used only for linear EFs */
  pBuffer[OFFSET_P2]     = 0x00;
  /* PG: Assumption: transparent EF not protected by PRO AC */
  pBuffer[OFFSET_P3]     = 0x10;

  currentPointer = 0;

  /* PG: RFU */
  pBuffer[OFFSET_DATA]   = 0xFF;
  pBuffer[OFFSET_DATA+1] = 0xFF;
  currentPointer = 2;

  MemCopy16(&pBuffer[OFFSET_DATA+currentPointer], &_objectSize);
  currentPointer += 2;

  MemCopy16(&pBuffer[OFFSET_DATA+currentPointer], &_objectID);
  currentPointer += 2;

  /* PG: Assumption: transparent EF */
  pBuffer[OFFSET_DATA+currentPointer] = 0x01;
  currentPointer++;

  /* PG: Access conditions */
  /* PG: Availibility of Increase and Decrease commands
     - 0x00 since these commands do not apply to transparent EFs*/
  pBuffer[OFFSET_DATA+currentPointer] = 0x00;
  currentPointer++;

  /* PG: Read/Write permissions */
  pBuffer[OFFSET_DATA+currentPointer] = ACL2Byte(pObjectACL);
  currentPointer++;

  /* PG: RFU */
  pBuffer[OFFSET_DATA+currentPointer] = 0xFF;
  currentPointer++;

  /* PG: Rehabilitate/Invalidate - never possible, since
     we cannot invoke these commands with this API */
  pBuffer[OFFSET_DATA+currentPointer] = 0xFF;
  currentPointer++;

  /* PG: Validation status - activated */
  pBuffer[OFFSET_DATA+currentPointer] = 0x01;
  currentPointer++;

  /* PG: Length of the input data from byte 14 to EOF
     in case of transparent EF must be 0x03 */
  pBuffer[OFFSET_DATA+currentPointer] = 0x03;
  currentPointer++;

  /* PG: Key numbers for AC */
  pBuffer[OFFSET_DATA+currentPointer] = 0x11;
  currentPointer++;
  pBuffer[OFFSET_DATA+currentPointer] = 0x11;
  currentPointer++;
  pBuffer[OFFSET_DATA+currentPointer] = 0x11;
  currentPointer++;

  transmitBuffer.bufferSize = pBuffer[OFFSET_P3] + 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU(pConnection, &transmitBuffer);
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    return convertSW(apduResponse);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }

}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCDeleteObject( MSCLPTokenConnection pConnection, 
			   MSCString objectID, MSCUChar8 zeroFlag ) {

  MSCLong32 rv;
  MSCTransmitBuffer transmitBuffer;
  MSCPUChar8 apduResponse;
  MSCPUChar8 pBuffer;
  MSCULong32 currentPointer;
  MSCUShort16 _objectID;

  /* PG: conversions */
  if ( stringToID(&_objectID, objectID) ) 
    return MSC_INVALID_PARAMETER;

  PL_MSCSelect(pConnection, 0x3F00);
  PL_MSCSelect(pConnection, CRYPTOFLEX_OBJ_DIR);

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CLA_F0;
  pBuffer[OFFSET_INS]    = INS_DELETE_OBJ;
  pBuffer[OFFSET_P1]     = 0x00;
  pBuffer[OFFSET_P2]     = 0x00;
  pBuffer[OFFSET_P3]     = 0x02;

  currentPointer = 0;

  MemCopy16(&pBuffer[OFFSET_DATA+currentPointer], &_objectID);

  transmitBuffer.bufferSize = pBuffer[OFFSET_P3] + 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU(pConnection, &transmitBuffer);
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    return convertSW(apduResponse);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }

}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCWriteObject( MSCLPTokenConnection pConnection, 
			  MSCString objectID, 
			  MSCULong32 offset, MSCPUChar8 pInputData, 
			  MSCUChar8 dataSize ) {

  MSCLong32 rv;
  MSCTransmitBuffer transmitBuffer;
  MSCPUChar8 apduResponse;
  MSCPUChar8 pBuffer;
  MSCULong32 currentPointer;
  MSCUShort16 _objectID;

  if ( stringToID(&_objectID, objectID) ) 
    return MSC_INVALID_PARAMETER;

  PL_MSCSelect(pConnection, 0x3F00);
  if(_objectID == 0x0012) {
    PL_MSCSelect(pConnection, CRYPTOFLEX_MSC_KEY_DIR);
  } else if (_objectID == 0x1012 ) {
    PL_MSCSelect(pConnection, CRYPTOFLEX_MSC_KEY_DIR);
  } else if ( _objectID == 0x0011 ) {
    /* stay in root */
  } else {
    PL_MSCSelect(pConnection, CRYPTOFLEX_OBJ_DIR);
  }

  PL_MSCSelect(pConnection, _objectID);

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CLA_C0;
  pBuffer[OFFSET_INS]    = INS_WRITE_OBJ;
  pBuffer[OFFSET_P1]     = (offset & 0xFF00) >> 8;
  pBuffer[OFFSET_P2]     = offset & 0x00FF;
  pBuffer[OFFSET_P3]     = dataSize;
  
  currentPointer = 0;
  
  memcpy(&pBuffer[OFFSET_DATA+currentPointer], pInputData, dataSize);
  transmitBuffer.bufferSize = pBuffer[OFFSET_P3] + 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU(pConnection, &transmitBuffer);
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    return convertSW(apduResponse);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }

}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCReadObject( MSCLPTokenConnection pConnection, MSCString objectID, 
			 MSCULong32 offset, MSCPUChar8 pOutputData, 
			 MSCUChar8 dataSize ) {

  MSCLong32 rv;
  MSCTransmitBuffer transmitBuffer;
  MSCPUChar8 apduResponse;
  MSCPUChar8 pBuffer;
  MSCUShort16 _objectID;


  if ( stringToID(&_objectID, objectID) ) {
    return MSC_INVALID_PARAMETER;
  }

  PL_MSCSelect(pConnection, 0x3F00);

  if ( _objectID == 0x1012 ) {
    PL_MSCSelect(pConnection, CRYPTOFLEX_MSC_KEY_DIR);
  } else {
    PL_MSCSelect(pConnection, CRYPTOFLEX_OBJ_DIR);
  }

  PL_MSCSelect(pConnection, _objectID);

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CLA_C0;
  pBuffer[OFFSET_INS]    = INS_READ_OBJ;
  pBuffer[OFFSET_P1]     = (offset & 0xFF00) >> 8;
  pBuffer[OFFSET_P2]     = offset & 0x00FF;
  pBuffer[OFFSET_P3]     = dataSize;

  transmitBuffer.bufferSize = 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU(pConnection, &transmitBuffer);
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }
  if ( transmitBuffer.apduResponseSize == 2 ) {
    return convertSW(apduResponse);
  } else if (transmitBuffer.apduResponseSize == dataSize + 2 ) {
    memcpy(pOutputData, apduResponse, dataSize);
    return convertSW(&apduResponse[dataSize]);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }
}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCListObjects( MSCLPTokenConnection pConnection, 
			  MSCUChar8 seqOption, 
			  MSCLPObjectInfo pObjectInfo ) {

  MSCLong32 rv;
  static int sequenceNumber;
  MSCTransmitBuffer transmitBuffer;
  MSCTransmitBuffer transmitBufferB;
  MSCPUChar8 apduResponse;
  MSCPUChar8 apduResponseB;
  MSCPUChar8 pBuffer;
  MSCPUChar8 pBufferB;
  MSCULong32 currentPointer;
  MSCUChar8 cTemp[2];
  int i;

  rv=0; currentPointer=0;

  if ( seqOption == MSC_SEQUENCE_RESET ) {
    sequenceNumber = 1;

  } else {
    sequenceNumber += 1;
  }

  while (1) {

    rv = PL_MSCSelect(pConnection, 0x3F00);
    rv = PL_MSCSelect(pConnection, CRYPTOFLEX_OBJ_DIR);

    if ( rv != MSC_SUCCESS ) {
      return MSC_UNSUPPORTED_FEATURE;
    }
    
    pBuffer = transmitBuffer.pBuffer; 
    apduResponse = transmitBuffer.apduResponse;
    
    pBuffer[OFFSET_CLA]    = CLA_F0;
    pBuffer[OFFSET_INS]    = INS_LIST_OBJECTS;
    pBuffer[OFFSET_P1]     = 0x00;
    pBuffer[OFFSET_P2]     = 0x00;
    pBuffer[OFFSET_P3]     = 0x09;
    
    transmitBuffer.bufferSize = 5;

    for (i=0; i < sequenceNumber; i++ ) {
      /* Set up the APDU exchange */
      transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
      rv = SCardExchangeAPDU(pConnection, &transmitBuffer);
      
      if ( rv != SCARD_S_SUCCESS ) {
	return convertPCSC(rv);
      }
      
      if ( transmitBuffer.apduResponseSize == 2 ) {
	/* PG: TODO: define a constant here */
	if ( convertSW(apduResponse) == MSC_OBJECT_NOT_FOUND ) {  /* Vinnie 1740 */
	  /* Must have finished */
	  return MSC_SEQUENCE_END;
	} else {
	  return convertSW(apduResponse);
	}	
      } 
    }   /* End of for ... loop */
    
    if (transmitBuffer.apduResponseSize == pBuffer[OFFSET_P3] + 2 ) {

      /* PG: Omit 8 most significant 8 bytes which  are only used
	 to indicate whether the size of an object was rounded up */
      
      memcpy(cTemp, &apduResponse[2], 2);
      bytesToString(pObjectInfo->objectID, cTemp);

      if ( strcmp(pObjectInfo->objectID, CRYPTOFLEX_INFOBJ_ID) == 0 ) {
	sequenceNumber += 1;
	continue;
      }

      /* PG: skipping to byte 7, where access conditions begin */
      
      Byte2ACL(apduResponse[6], &pObjectInfo->objectACL);
      
      /* Whoever did Cryptoflex mask is an idiot - I can't believe they
	 round to the nearest 4 bytes on the size for Dir Next 
      */
      
      apduResponseB = transmitBufferB.apduResponse;
      pBufferB      = transmitBufferB.pBuffer;

      pBufferB[OFFSET_CLA]    = CLA_C0;
      pBufferB[OFFSET_INS]    = 0xA4;
      pBufferB[OFFSET_P1]     = 0x00;
      pBufferB[OFFSET_P2]     = 0x00;
      pBufferB[OFFSET_P3]     = 0x02;
      pBufferB[OFFSET_DATA]   = cTemp[0];
      pBufferB[OFFSET_DATA+1] = cTemp[1];
      
      transmitBufferB.bufferSize = 7;
      
      /* Set up the APDU exchange */
      transmitBufferB.apduResponseSize = MSC_MAXSIZE_BUFFER;
      rv = SCardExchangeAPDU( pConnection, &transmitBufferB );
      
      if ( rv != SCARD_S_SUCCESS ) {
	return convertPCSC(rv);
      }
      
      if ( transmitBufferB.apduResponseSize == 2 ) {
	return convertSW(apduResponse);
      }
      
      pObjectInfo->objectSize = apduResponseB[2] * 0x100 + apduResponseB[3];
       
      return convertSW(&apduResponseB[15]);

    } else {
      return MSC_UNSPECIFIED_ERROR;
    }


    break;
  }

  return MSC_UNSPECIFIED_ERROR;
}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCLogoutAll( MSCLPTokenConnection pConnection ) {

  MSCLong32 rv;
  MSCTransmitBuffer transmitBuffer;
  MSCPUChar8 apduResponse;
  MSCPUChar8 pBuffer;

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CLA_F0;
  pBuffer[OFFSET_INS]    = INS_LOGOUT_ALL;
  /* PG: Cryptoflex Logout AC command can be used to reset
     multiple access conditions - however, setting P1 to 0x07
     simply means "Logout All" */
  pBuffer[OFFSET_P1]     = 0x07;
  pBuffer[OFFSET_P2]     = 0x00;
  pBuffer[OFFSET_P3]     = 0x00;

  transmitBuffer.bufferSize = 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU(pConnection, &transmitBuffer);
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    rv = convertSW(apduResponse);

    if ( rv == MSC_SUCCESS ) {
      pConnection->loggedIDs = 0;
    }

    return rv;
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }

}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCGetChallenge( MSCLPTokenConnection pConnection, MSCPUChar8 pSeed,
			MSCUShort16 seedSize, MSCPUChar8 pRandomData,
			MSCUShort16 randomDataSize ) {

  MSCLong32 rv;
  MSCULong32 currentPointer;
  MSCTransmitBuffer transmitBuffer;
  MSCPUChar8 apduResponse;
  MSCPUChar8 pBuffer;

  /* PG: randomDataSize is the only parameter
     that can be passed to Cryptoflex */

  if ( pRandomData == 0 ) {
    return MSC_INVALID_PARAMETER;
  }

  if ( randomDataSize == 0 ) {
    return MSC_INVALID_PARAMETER;
  }

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA] = CLA_C0;
  pBuffer[OFFSET_INS] = INS_GET_CHALLENGE;
  pBuffer[OFFSET_P1]  = 0x00;
  pBuffer[OFFSET_P2]  = 0x00;
  pBuffer[OFFSET_P3]  = randomDataSize;

  currentPointer = 0;

  transmitBuffer.bufferSize = 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU(pConnection, &transmitBuffer);
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  /* PG: or randomDataSize + 4 ??? */
  if(transmitBuffer.apduResponseSize == randomDataSize + 2)
  {
    memcpy(pRandomData, apduResponse, randomDataSize);
    return convertSW(&apduResponse[transmitBuffer.apduResponseSize-2]);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }
}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCGetObjectAttributes( MSCLPTokenConnection pConnection,
				  MSCString objectID,
				  MSCLPObjectInfo pObjectInfo ) {

  MSC_RV rv;
  MSCObjectInfo objInfo;

  if ( pConnection == NULL ) return MSC_INVALID_PARAMETER; 

  rv = PL_MSCListObjects( pConnection, MSC_SEQUENCE_RESET, &objInfo );

  if ( rv != MSC_SEQUENCE_END && rv != MSC_SUCCESS) {
    return rv;
  }

  if ( rv == MSC_SEQUENCE_END ) {
    return MSC_OBJECT_NOT_FOUND;
  }

  if (strncmp(objectID, objInfo.objectID, MSC_MAXSIZE_OBJID) == 0 ) {
    pObjectInfo->objectSize = objInfo.objectSize;
    pObjectInfo->objectACL.readPermission = 
      objInfo.objectACL.readPermission;
    pObjectInfo->objectACL.writePermission = 
      objInfo.objectACL.writePermission;
    pObjectInfo->objectACL.deletePermission = 
      objInfo.objectACL.deletePermission;
    strncpy(pObjectInfo->objectID, objectID, MSC_MAXSIZE_OBJID);
    return MSC_SUCCESS;
  }
  
  do {
    rv = PL_MSCListObjects( pConnection, MSC_SEQUENCE_NEXT, &objInfo );
    if (strncmp(objectID, objInfo.objectID, MSC_MAXSIZE_OBJID) == 0 ) 
      break;
  } while ( rv == MSC_SUCCESS );
  
  if ( rv != MSC_SEQUENCE_END && rv != MSC_SUCCESS ) {
    return rv;
  }

  if ( rv == MSC_SEQUENCE_END ) {
    return MSC_OBJECT_NOT_FOUND;
  }

  pObjectInfo->objectSize = objInfo.objectSize;
  pObjectInfo->objectACL.readPermission = 
    objInfo.objectACL.readPermission;
  pObjectInfo->objectACL.writePermission = 
    objInfo.objectACL.writePermission;
  pObjectInfo->objectACL.deletePermission = 
    objInfo.objectACL.deletePermission;
  strncpy(pObjectInfo->objectID, objectID, MSC_MAXSIZE_OBJID);

  return MSC_SUCCESS;  
}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCReadAllocateObject( MSCLPTokenConnection pConnection, 
				 MSCString objectID, MSCPUChar8 *pOutputData, 
				 MSCPULong32 dataSize ) {

  MSC_RV rv;
  MSCObjectInfo objInfo;

  if ( pConnection == NULL ) return MSC_INVALID_PARAMETER; 

  if ( pOutputData == 0 ) {
    return MSC_INVALID_PARAMETER;
  }

  rv = PL_MSCGetObjectAttributes( pConnection, objectID, &objInfo );
  
  if ( rv != MSC_SUCCESS ) {
    *dataSize = 0;
    *pOutputData = 0;
    return rv;
  }

  *pOutputData = (MSCPUChar8)malloc(sizeof(MSCUChar8)*objInfo.objectSize);

  return PL_MSCReadLargeObjectOffCB( pConnection, objectID, 0,
				     *pOutputData, objInfo.objectSize,
				     0, 0 );
}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCWriteLargeObjectOffCB( MSCLPTokenConnection pConnection, 
				    MSCString objectID, MSCULong32 offSet, 
				    MSCPUChar8 pInputData, 
				    MSCULong32 dataSize, 
				    LPRWEventCallback rwCallback,
				    MSCPVoid32 addParams ) {
  MSC_RV rv;
  MSCULong32 objectSize;
  int totalSteps, stepInterval;
  MSC_RV (*callBackFunction)(void*, int);
  int i;
   
  callBackFunction = (MSC_RV(*)(void*, int)) rwCallback;
  objectSize       = dataSize;
  rv               = MSC_UNSPECIFIED_ERROR;


  /* Figure out the number of steps total and present this
     in a percent step basis 
  */

  totalSteps = objectSize/MSC_SIZEOF_KEYPACKET + 1;
  stepInterval = MSC_PERCENT_STEPSIZE / totalSteps;


  for (i=0; i < objectSize/MSC_SIZEOF_KEYPACKET; i++) {
    rv = PL_MSCWriteObject( pConnection, objectID, 
			    i*MSC_SIZEOF_KEYPACKET + offSet, 
			    &pInputData[i*MSC_SIZEOF_KEYPACKET], 
			    MSC_SIZEOF_KEYPACKET );
    if ( rv != MSC_SUCCESS ) { return rv; }

    if ( rwCallback ) {    
      if ((*callBackFunction)(addParams, stepInterval*i) == MSC_CANCELLED) {
	return MSC_CANCELLED;
      }
    }
    
  }
  
  
  if ( objectSize%MSC_SIZEOF_KEYPACKET ) {
    rv = PL_MSCWriteObject( pConnection, objectID, 
			    i*MSC_SIZEOF_KEYPACKET + offSet, 
			    &pInputData[i*MSC_SIZEOF_KEYPACKET], 
			    objectSize%MSC_SIZEOF_KEYPACKET );
    
    if ( rv != MSC_SUCCESS ) { return rv; }
  }    
  
  if ( rwCallback ) {
    (*callBackFunction)(addParams, MSC_PERCENT_STEPSIZE); 
  }  
  return rv;
}


#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCReadLargeObjectOffCB( MSCLPTokenConnection pConnection, 
				   MSCString objectID, MSCULong32 offSet, 
				   MSCPUChar8 pOutputData, 
				   MSCULong32 dataSize, 
				   LPRWEventCallback rwCallback,
				   MSCPVoid32 addParams ) {
  
  MSC_RV rv;
  MSCULong32 objectSize;
  int totalSteps, stepInterval;
  MSC_RV (*callBackFunction)(void*, int);
  int i;
  
  callBackFunction = (MSC_RV(*)(void*, int)) rwCallback;
  objectSize       = dataSize;
  rv               = MSC_UNSPECIFIED_ERROR;
  
  /* Figure out the number of steps total and present this
     in a percent step basis 
  */
  
  totalSteps = objectSize/MSC_SIZEOF_KEYPACKET + 1;
  stepInterval = MSC_PERCENT_STEPSIZE / totalSteps;
  
  for (i=0; i < objectSize/MSC_SIZEOF_KEYPACKET; i++) {
    rv = PL_MSCReadObject( pConnection, objectID, 
                        i*MSC_SIZEOF_KEYPACKET + offSet, 
                        &pOutputData[i*MSC_SIZEOF_KEYPACKET], 
                        MSC_SIZEOF_KEYPACKET );
    if ( rv != MSC_SUCCESS ) { return rv; }
    
    if ( rwCallback ) {
      if ((*callBackFunction)(addParams, stepInterval*i) == MSC_CANCELLED) {
	return MSC_CANCELLED;
      }
    }
  }
  
  if ( objectSize%MSC_SIZEOF_KEYPACKET ) {
    rv = PL_MSCReadObject( pConnection, objectID, 
                        i*MSC_SIZEOF_KEYPACKET + offSet, 
                        &pOutputData[i*MSC_SIZEOF_KEYPACKET], 
                        objectSize%MSC_SIZEOF_KEYPACKET );
    
    if ( rv != MSC_SUCCESS ) { return rv; }
  }    

  if ( rwCallback ) {  
    (*callBackFunction)(addParams, MSC_PERCENT_STEPSIZE); 
  }

  return rv;
}

int bytesToString( MSCString objectString, MSCPUChar8 objBytes ) {

  MSCUShort16 objInt;
  
  /* Cryptoflex must truncate objectID's to 16 bits */

  MemCopyTo16(&objInt, objBytes);

  if ( objBytes[0] == 0xFF && objBytes[1] == 0xFE ) {
    snprintf(objectString, MSC_MAXSIZE_OBJID, "#%X", objInt);
    return 0;
  }

  snprintf(objectString, MSC_MAXSIZE_OBJID, "%c%c", objBytes[0],
	   objBytes[1]);
  
  return 0;
}

int idToString( MSCString objectString, MSCULong32 objectID ) {

  MSCUShort16 objInt;
  MSCUChar8 objBytes[MSC_MAXSIZE_OBJID];

  objInt = (MSCUShort16)objectID;
  MemCopy16(objBytes, &objInt);


  if ( objBytes[0] == 0xFF && objBytes[1] == 0xFE ) {
    snprintf(objectString, MSC_MAXSIZE_OBJID, "#%X", objInt);
    return 0;
  }

  /* Cryptoflex must truncate objectID's to 16 bits */

  snprintf(objectString, MSC_MAXSIZE_OBJID, "%c%c", objBytes[0],
	   objBytes[1]);

  return 0;
}

int stringToID( MSCPUShort16 objectID, MSCString objectString ) {

  MSCUShort16 localID;
  MSCUChar8 objBytes[MSC_MAXSIZE_OBJID];

  localID = 0;

  if ( strncmp(CRYPTOFLEX_INFOBJ_ID, objectString, 
	       MSC_MAXSIZE_OBJID) == 0 ) {
    *objectID = 0xFFFE;
    return 0;
  } else if ( strncmp("#0x0011", objectString,
	       MSC_MAXSIZE_OBJID) == 0 ) { 
    *objectID = 0x0011;
    return 0;
  } else if ( strncmp("#0x0012", objectString,
		      MSC_MAXSIZE_OBJID) == 0 ) { 
    *objectID = 0x0012;
    return 0;
  } else if ( strncmp("#0x1012", objectString,
		      MSC_MAXSIZE_OBJID) == 0 ) { 
    *objectID = 0x1012;
    return 0;
  } 

  if (strlen(objectString) > CF_SIZEOF_OBJID) {
    return -1;
  }

  objBytes[0] = objectString[0];
  objBytes[1] = objectString[1];
    
  if (strlen(objectString) == 1) {
    objBytes[1] = 0x00; /* PAD with 0x00 */
  }

  MemCopyTo16(&localID, objBytes);

  if ( localID == 0 ) {
    return -1;
  }

  *objectID = (MSCUShort16)localID;
  return 0;
}


#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCSelect(MSCLPTokenConnection pConnection,
                 MSCULong32 fileID) {

  MSCLong32 rv;
  MSCTransmitBuffer transmitBuffer;
  MSCPUChar8 apduResponse;
  MSCPUChar8 pBuffer;
  MSCULong32 currentPointer;

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA] = CLA_C0;
  pBuffer[OFFSET_INS] = 0xA4;
  pBuffer[OFFSET_P1]  = 0x00;
  pBuffer[OFFSET_P2]  = 0x00;
  pBuffer[OFFSET_P3]  = 0x02;

  currentPointer = 0;

  pBuffer[OFFSET_DATA+currentPointer] = fileID / 256;
  currentPointer++;
  pBuffer[OFFSET_DATA+currentPointer] = fileID % 256;
  currentPointer++;

  transmitBuffer.bufferSize = pBuffer[OFFSET_P3] + 5;
  
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;

  suppressResponse = 1;
  rv = SCardExchangeAPDU(pConnection, &transmitBuffer);
  suppressResponse = 0;

  if(rv != SCARD_S_SUCCESS) {
    return convertPCSC(rv);
  }

  if(transmitBuffer.apduResponseSize == 2) {
    if(apduResponse[0] == 0x61) {
      return MSC_SUCCESS;
    } else {
      return convertSW(apduResponse);
    }
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }
}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCGetResponse(MSCLPTokenConnection pConnection, MSCUChar8 len, 
		      MSCPUChar8 buf)
{
  MSCLong32 rv;
  MSCTransmitBuffer transmitBuffer;
  MSCPUChar8 apduResponse;
  MSCPUChar8 pBuffer;
  MSCULong32 currentPointer;

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA] = CLA_C0;
  pBuffer[OFFSET_INS] = 0xC0;
  pBuffer[OFFSET_P1]  = 0x00;
  pBuffer[OFFSET_P2]  = 0x00;
  pBuffer[OFFSET_P3]  = len;

  currentPointer = 0;

  transmitBuffer.bufferSize = 5;

  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU(pConnection, &transmitBuffer);
  
  if(rv != SCARD_S_SUCCESS) {
    return convertPCSC(rv);
  }

  if(transmitBuffer.apduResponseSize == 2) {
    return convertSW(apduResponse);
  } else if(transmitBuffer.apduResponseSize == len + 2){
    memcpy(buf, apduResponse, len);
    currentPointer += len;
    return convertSW(&apduResponse[currentPointer]);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }
}

MSCUShort16 convertSW(MSCPUChar8 pBuffer) {
  MSCUShort16 retValue;
  MSCUShort16 newValue;

  retValue  = pBuffer[0] * 0x100;
  retValue += pBuffer[1];

  switch(retValue) {
  case CFMSC_SUCCESS:
    newValue = MSC_SUCCESS;
    break;
  case CFMSC_NO_MEMORY_LEFT:
  case CFMSC_NO_MEMORY_LEFT_1:
    newValue = MSC_NO_MEMORY_LEFT;
    break;
  case CFMSC_AUTH_FAILED:
    newValue = MSC_AUTH_FAILED;
    break;
  case CFMSC_OPERATION_NOT_ALLOWED:
    newValue = MSC_OPERATION_NOT_ALLOWED;
    break;
  case CFMSC_INCONSISTENT_STATUS:
    newValue = MSC_INCONSISTENT_STATUS;
    break;
  case CFMSC_UNSUPPORTED_FEATURE:
    newValue = MSC_UNSUPPORTED_FEATURE;
    break;
  case CFMSC_UNAUTHORIZED:
    newValue = MSC_UNAUTHORIZED;
    break;
  case CFMSC_OBJECT_NOT_FOUND:
    newValue = MSC_OBJECT_NOT_FOUND;
    break;
  case CFMSC_OBJECT_EXISTS:
    newValue = MSC_OBJECT_EXISTS;
    break;
  case CFMSC_INCORRECT_ALG:
    newValue = MSC_INCORRECT_ALG;
    break;
  case CFMSC_SIGNATURE_INVALID:
    newValue = MSC_SIGNATURE_INVALID;
    break;
  case CFMSC_IDENTITY_BLOCKED:
    newValue = MSC_IDENTITY_BLOCKED;
    break;
  case CFMSC_UNSPECIFIED_ERROR:
    newValue = MSC_UNSPECIFIED_ERROR;
    break;
  case CFMSC_TRANSPORT_ERROR:
    newValue = MSC_TRANSPORT_ERROR;
    break;
  case CFMSC_INVALID_PARAMETER:
    newValue = MSC_INVALID_PARAMETER;
    break;
  case CFMSC_SEQUENCE_END:
    newValue = MSC_SEQUENCE_END;
    break;
  case CFMSC_INTERNAL_ERROR:
    newValue = MSC_INTERNAL_ERROR;
    break;
  case CFMSC_CANCELLED:
    newValue = MSC_CANCELLED;
    break;
  default:
    newValue = retValue;
    break;
  }

  return newValue;
}

void MemCopy16(MSCPUChar8 destValue, MSCPUShort16 srcValue) {
  destValue[0] = (*srcValue & 0xFF00) >> 8;
  destValue[1] = (*srcValue & 0x00FF);
}

void MemCopy32(MSCPUChar8 destValue, MSCPULong32 srcValue) {
  destValue[0] = (*srcValue >> 24);
  destValue[1] = (*srcValue & 0x00FF0000) >> 16;
  destValue[2] = (*srcValue & 0x0000FF00) >> 8;
  destValue[3] = (*srcValue & 0x000000FF);
}

void MemCopyTo16(MSCPUShort16 destValue, MSCPUChar8 srcValue) {

  *destValue  = srcValue[0] * 0x100;
  *destValue += srcValue[1];

}

void MemCopyTo32(MSCPULong32 destValue, MSCPUChar8 srcValue) {

  *destValue  = srcValue[0] * 0x1000000;
  *destValue += srcValue[1] * 0x10000;
  *destValue += srcValue[2] * 0x100;
  *destValue += srcValue[3];

}

MSCUShort16 getUShort16(MSCPUChar8 srcValue) {
  return ( (((MSCUShort16)srcValue[0]) << 8) || srcValue[1] );
}

void setUShort16(MSCPUChar8 dstValue, MSCUShort16 srcValue) {
  MemCopyTo16(&srcValue, dstValue);
}


MSCLong32 SCardExchangeAPDU( MSCLPTokenConnection pConnection, 
			     MSCLPTransmitBuffer transmitBuffer ) {
  
  MSCLong32 rv, ret;
  MSCULong32 originalLength;
  MSCUChar8 getResponse[5] = {0xC0, 0xC0, 0x00, 0x00, 0x00};
  MSCULong32 dwActiveProtocol;

#ifdef MSC_DEBUG
  int i;
#endif

  originalLength = transmitBuffer->apduResponseSize;

  while (1) {
    
#ifdef MSC_DEBUG
    printf("->: ");
    
    for (i=0; i < transmitBuffer->bufferSize; i++) {
      printf("%02x ", transmitBuffer->pBuffer[i]);
    } printf("\n");
#endif
    
    while(1) {
      transmitBuffer->apduResponseSize = originalLength;
      
      rv =  SCardTransmit(pConnection->hCard, pConnection->ioType,
			  transmitBuffer->pBuffer, 
			  transmitBuffer->bufferSize, 0,
			  transmitBuffer->apduResponse, 
			  &transmitBuffer->apduResponseSize );
      
      if ( rv == SCARD_S_SUCCESS ) {
	break;
	
      } else if ( rv == SCARD_W_RESET_CARD ) {
	pConnection->tokenInfo.tokenType |= MSC_TOKEN_TYPE_RESET;
	ret = SCardReconnect(pConnection->hCard, pConnection->shareMode, 
			     SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
			     SCARD_LEAVE_CARD, &dwActiveProtocol );
	
	PL_MSCIdentifyToken(pConnection);
	
	if ( ret == SCARD_S_SUCCESS ) {
	  continue;
	}
	
      } else if ( rv == SCARD_W_REMOVED_CARD ) {
	/* Push REMOVED_TOKEN back to the application */
	pConnection->tokenInfo.tokenType = MSC_TOKEN_TYPE_REMOVED;
	return rv;
      } else {
	/* Must be a BIG, BAD, ERROR */
#ifdef MSC_DEBUG
	printf("Transmit error: %s\n", pcsc_stringify_error(rv));
#endif
	return rv;
      }
    }
    
#ifdef MSC_DEBUG
    printf("<-: ");
    
    for (i=0; i < transmitBuffer->apduResponseSize; i++) {
      printf("%02x ", transmitBuffer->apduResponse[i]);
    } printf("\n");
#endif
    
    if ( suppressResponse == 1 ) {
      /* Do not do the Get Response */
      break;
    }

    if ( transmitBuffer->apduResponseSize == 2 && 
	 transmitBuffer->apduResponse[0] == 0x61 ) {
#ifdef MSC_DEBUG
      printf("->: 0xC0 0xC0 0x00 0x00 %02x\n", 
	     transmitBuffer->apduResponse[1]);
#endif
      getResponse[4] = transmitBuffer->apduResponse[1];
      transmitBuffer->apduResponseSize   = originalLength;
      rv =  SCardTransmit(pConnection->hCard, pConnection->ioType,
			  getResponse, 5, 0,
			  transmitBuffer->apduResponse, 
			  &transmitBuffer->apduResponseSize );
      
      if ( rv == SCARD_S_SUCCESS ) {
#ifdef MSC_DEBUG	
	printf("<-: ");
	
	for (i=0; i < transmitBuffer->apduResponseSize; i++) {
	  printf("%02x ", transmitBuffer->apduResponse[i]);
	} printf("\n");
#endif
	break;
      } else if ( rv == SCARD_W_RESET_CARD ) {
	pConnection->tokenInfo.tokenType |= MSC_TOKEN_TYPE_RESET;
	ret = SCardReconnect(pConnection->hCard, pConnection->shareMode, 
			     SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
			     SCARD_LEAVE_CARD, &dwActiveProtocol );
	
	PL_MSCIdentifyToken(pConnection);
	
	if ( ret == SCARD_S_SUCCESS ) {
	  continue;
	}
	
      } else if ( rv == SCARD_W_REMOVED_CARD ) {
	/* Push REMOVED_TOKEN back to the application */
	pConnection->tokenInfo.tokenType = MSC_TOKEN_TYPE_REMOVED;
	return rv;
      } else {
#ifdef MSC_DEBUG
	printf("Transmit error: %s\n", pcsc_stringify_error(rv));
#endif
	return rv;
      } 
    }         

    break;
  }  /* End of while */
  
  
  return rv;
}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCIdentifyToken( MSCLPTokenConnection pConnection ) {

  MSCLong32 rv;
 
  rv = PL_MSCSelect(pConnection, 0x3F00);
  rv = PL_MSCSelect(pConnection, 0x3FCE);

  pConnection->loggedIDs = 0;

  return rv;
}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCFinalizePlugin( MSCLPTokenConnection pConnection ) {

  return MSC_SUCCESS;
}

#ifdef WIN32
CFLEXPLUGIN_API
#endif
MSC_RV PL_MSCInitializePlugin( MSCLPTokenConnection pConnection ) {

  return MSC_SUCCESS;
}


MSC_RV convertPCSC( MSCLong32 pcscCode ) {

  switch(pcscCode) {
  case SCARD_S_SUCCESS:
    return MSC_SUCCESS;
  case SCARD_E_INVALID_HANDLE:
    return MSC_INVALID_HANDLE;
  case SCARD_E_SHARING_VIOLATION:
    return MSC_SHARING_VIOLATION;
  case SCARD_W_REMOVED_CARD:
    return MSC_TOKEN_REMOVED;
  case SCARD_E_NO_SMARTCARD:
    return MSC_TOKEN_REMOVED;
  case SCARD_W_RESET_CARD:
    return MSC_TOKEN_RESET;
  case SCARD_W_INSERTED_CARD:
    return MSC_TOKEN_INSERTED;
  case SCARD_E_NO_SERVICE:
    return MSC_SERVICE_UNRESPONSIVE;
  case SCARD_E_UNKNOWN_CARD:
  case SCARD_W_UNSUPPORTED_CARD:
  case SCARD_E_CARD_UNSUPPORTED:
    return MSC_UNRECOGNIZED_TOKEN;
  case SCARD_E_INVALID_PARAMETER:
  case SCARD_E_INVALID_VALUE:
  case SCARD_E_UNKNOWN_READER:
  case SCARD_E_PROTO_MISMATCH:
  case SCARD_E_READER_UNAVAILABLE:
    return MSC_INVALID_PARAMETER;
  case SCARD_E_CANCELLED:
    return MSC_CANCELLED;
  case SCARD_E_TIMEOUT:
    return MSC_TIMEOUT_OCCURRED;

  default:
    return MSC_INTERNAL_ERROR;
  }
}

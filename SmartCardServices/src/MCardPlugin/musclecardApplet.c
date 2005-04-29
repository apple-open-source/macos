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
            Title  : musclecardApplet.c
            Package: MuscleCard Plugin
            Author : David Corcoran
	             Tommaso Cucinotta
            Date   : 09/26/01
            License: Copyright (C) 2001-2002 David Corcoran
                     <corcoran@linuxnet.com>
            Purpose: This abstracts the Card Edge Interface APDU's
	             into client side function calls.
 
********************************************************************/
#ifdef WIN32
#include "../win32/MCardPlugin.h"
#endif

#ifndef __APPLE__
#include <musclecard.h>
#else
#include <PCSC/musclecard.h>
#endif

#include "musclecardApplet.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MSC_DEBUG 1

/* Local transport structure */
typedef struct {
  MSCUChar8  pBuffer[MAX_BUFFER_SIZE];
  MSCULong32 bufferSize;
  MSCUChar8  apduResponse[MAX_BUFFER_SIZE];
  MSCULong32 apduResponseSize;
  LPSCARD_IO_REQUEST ioType;
} MSCTransmitBuffer, *MSCLPTransmitBuffer;

/* Some locally defined functions */

MSC_RV convertPCSC( MSCLong32 );
int idToString( char*, MSCULong32 );
int stringToID( MSCPULong32, char* );
MSCUShort16 convertSW( MSCPUChar8 );
void MemCopy16( MSCPUChar8, MSCPUShort16 );
void MemCopy32( MSCPUChar8, MSCPULong32 );
void MemCopyTo16( MSCPUShort16, MSCPUChar8 ); 
void MemCopyTo32( MSCPULong32, MSCPUChar8 );
MSCUShort16 getUShort16( MSCPUChar8 );
void setUShort16( MSCPUChar8, MSCUShort16 );
MSCLong32 SCardExchangeAPDU( MSCLPTokenConnection, MSCLPTransmitBuffer );
MSC_RV lcMSCGetObjectAttributes( MSCLPTokenConnection, 
				 MSCString, MSCLPObjectInfo );

/* MSC Functions */


MSC_RV PL_MSCGetStatus( MSCLPTokenConnection pConnection, 
			MSCLPStatusInfo pStatusInfo ) {

  MSCLong32 rv;
  MSCTransmitBuffer transmitBuffer;
  MSCPUChar8 apduResponse; MSCPUChar8 pBuffer;

  MSCULong32 currentPointer;

  rv=0; currentPointer=0;

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CardEdge_CLA;
  pBuffer[OFFSET_INS]    = INS_GET_STATUS;
  pBuffer[OFFSET_P1]     = 0x00;
  pBuffer[OFFSET_P2]     = 0x00;
  pBuffer[OFFSET_LC]     = MSC_SIZEOF_STATUS;

  transmitBuffer.bufferSize = 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU( pConnection, &transmitBuffer );
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    return convertSW(apduResponse);
  } else if (transmitBuffer.apduResponseSize == pBuffer[OFFSET_LC] + 2 ) {
    currentPointer = 0;
    MemCopyTo16(&pStatusInfo->appVersion, &apduResponse[currentPointer]);
    currentPointer += MSC_SIZEOF_VERSION;

    MemCopyTo16(&pStatusInfo->swVersion, &apduResponse[currentPointer]);
    currentPointer += MSC_SIZEOF_VERSION;
    
    MemCopyTo32(&pStatusInfo->totalMemory, &apduResponse[currentPointer]); 
    currentPointer += MSC_SIZEOF_FREEMEM;

    MemCopyTo32(&pStatusInfo->freeMemory, &apduResponse[currentPointer]); 
    currentPointer += MSC_SIZEOF_FREEMEM;

    pStatusInfo->usedPINs = apduResponse[currentPointer];
    currentPointer += MSC_SIZEOF_IDUSED;

    pStatusInfo->usedKeys = apduResponse[currentPointer];
    currentPointer += MSC_SIZEOF_IDUSED;

    MemCopyTo16(&pStatusInfo->loggedID, &apduResponse[currentPointer]);
    currentPointer += MSC_SIZEOF_LOGIDS;
    
    return convertSW(&apduResponse[currentPointer]);

  } else {
    return MSC_UNSPECIFIED_ERROR;
  }

}

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
    ulValue = MSC_SUPPORT_GENKEYS | MSC_SUPPORT_IMPORTKEY |
      MSC_SUPPORT_EXPORTKEY | MSC_SUPPORT_COMPUTECRYPT | 
      MSC_SUPPORT_EXTAUTH | MSC_SUPPORT_LISTKEYS |
      MSC_SUPPORT_CREATEPIN |
      MSC_SUPPORT_VERIFYPIN | MSC_SUPPORT_CHANGEPIN | 
      MSC_SUPPORT_UNBLOCKPIN | MSC_SUPPORT_LISTPINS | 
      MSC_SUPPORT_CREATEOBJECT | MSC_SUPPORT_DELETEOBJECT | 
      MSC_SUPPORT_WRITEOBJECT | MSC_SUPPORT_READOBJECT |
      MSC_SUPPORT_LISTOBJECTS | MSC_SUPPORT_LOGOUTALL |
      MSC_SUPPORT_GETCHALLENGE;
    tagType = 4;
    break;

  case MSC_TAG_SUPPORT_CRYPTOALG:
    ulValue = MSC_SUPPORT_RSA | 
      MSC_SUPPORT_DES | MSC_SUPPORT_3DES;
    tagType = 4;
    break;

  case MSC_TAG_CAPABLE_RSA:
    ulValue = MSC_CAPABLE_RSA_1024 | MSC_CAPABLE_RSA_768 |
      MSC_CAPABLE_RSA_NOPAD | MSC_CAPABLE_RSA_KEYGEN;
    tagType = 4;
    break;

  case MSC_TAG_CAPABLE_DES:
    ulValue = MSC_CAPABLE_DES_ECB;
    tagType = 4;
    break;

  case MSC_TAG_CAPABLE_3DES:
    ulValue = 0;
    tagType = 4;
    break;

  case MSC_TAG_CAPABLE_OBJ_ATTR:
    ulValue = MSC_CAPABLE_OBJ_ZERO;
    tagType = 4;
    break;

  case MSC_TAG_CAPABLE_OBJ_IDSIZE:
    ucValue = 4;
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
    ulValue = MSC_CAPABLE_PIN_LEAVE;
    tagType = 4;
    break;

  case MSC_TAG_CAPABLE_PIN_MAXNUM:
    ucValue = 8;
    tagType = 1;
    break;

  case MSC_TAG_CAPABLE_PIN_MINSIZE:
    ucValue = 4;
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
    ucValue = MSC_CAPABLE_ID_STATE;
    tagType = 1;
    break;

  case MSC_TAG_CAPABLE_RANDOM_MAX:
    ucValue = 128;
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
    usValue = MSC_AUT_ALL;
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

MSC_RV PL_MSCWriteFramework( MSCLPTokenConnection pConnection,
			     MSCLPInitTokenParams pInitParams ) {

  MSCLong32 rv;

  MSCPUChar8 apduResponse; MSCPUChar8 pBuffer;
  MSCTransmitBuffer transmitBuffer;
  MSCULong32 currentPointer;

  pBuffer = transmitBuffer.pBuffer; apduResponse = transmitBuffer.apduResponse;

  if (pInitParams->transportKeyLen > 8 || 
      pInitParams->newTransportKeyLen > 8 ||
      pInitParams->defaultCHVLen > 8 || 
      pInitParams->defaultCHVUnblockSize > 8 ) {
    
    return MSC_INVALID_PARAMETER;
  }

  /* Select the applet */
  PL_MSCIdentifyToken(pConnection); 

  if (pInitParams->newTransportKeyLen == 0) {
    /* Just use the old transport key */
    memcpy(pInitParams->newTransportKey, pInitParams->transportKey, 
	   pInitParams->transportKeyLen);
    pInitParams->newTransportKeyLen = pInitParams->transportKeyLen;
  }

  pBuffer[OFFSET_CLA]    = CardEdge_CLA;
  pBuffer[OFFSET_INS]    = INS_WRITE_FRAMEWORK;
  pBuffer[OFFSET_P1]     = 0x00;
  pBuffer[OFFSET_P2]     = 0x00;
  pBuffer[OFFSET_LC]     = 
    MSC_SIZEOF_PINSIZE + pInitParams->transportKeyLen + 
    MSC_SIZEOF_PINTRIES + MSC_SIZEOF_PINTRIES + 
    MSC_SIZEOF_PINSIZE + pInitParams->newTransportKeyLen +
    MSC_SIZEOF_PINSIZE + pInitParams->newTransportKeyLen + 
    MSC_SIZEOF_PINTRIES + MSC_SIZEOF_PINTRIES +
    MSC_SIZEOF_PINSIZE + pInitParams->defaultCHVLen +
    MSC_SIZEOF_PINSIZE + pInitParams->defaultCHVUnblockSize +
    MSC_SIZEOF_OBJECTSIZE + MSC_SIZEOF_MINIACL + MSC_SIZEOF_MINIACL +
    MSC_SIZEOF_MINIACL;

  currentPointer = 0;

  /* Transport key to verify */

  pBuffer[OFFSET_DATA+currentPointer] = pInitParams->transportKeyLen;

  currentPointer += MSC_SIZEOF_PINSIZE;

  memcpy(&pBuffer[OFFSET_DATA+currentPointer], pInitParams->transportKey, 
	 pInitParams->transportKeyLen);
  
  currentPointer += pInitParams->transportKeyLen;

  /* New transport key and attributes */
  
  pBuffer[OFFSET_DATA+currentPointer] = 4;
  currentPointer += MSC_SIZEOF_PINTRIES;    
  pBuffer[OFFSET_DATA+currentPointer] = 1;  /* One chance to unblock */ 
  currentPointer += MSC_SIZEOF_PINTRIES;    

  pBuffer[OFFSET_DATA+currentPointer] = pInitParams->newTransportKeyLen;

  currentPointer += MSC_SIZEOF_PINSIZE;

  memcpy(&pBuffer[OFFSET_DATA+currentPointer], pInitParams->newTransportKey, 
	 pInitParams->newTransportKeyLen);
  
  currentPointer += pInitParams->newTransportKeyLen;

  /* Write Admin Pin Unblock (Same as Admin) */

  pBuffer[OFFSET_DATA+currentPointer] = pInitParams->newTransportKeyLen;

  currentPointer += MSC_SIZEOF_PINSIZE;

  memcpy(&pBuffer[OFFSET_DATA+currentPointer], pInitParams->newTransportKey, 
	 pInitParams->newTransportKeyLen);
  
  currentPointer += pInitParams->newTransportKeyLen;

  /* Write User PIN */

  pBuffer[OFFSET_DATA+currentPointer] = pInitParams->defaultCHVTries;
  currentPointer += MSC_SIZEOF_PINTRIES;    
  pBuffer[OFFSET_DATA+currentPointer] = pInitParams->defaultCHVUnblockTries; 
  currentPointer += MSC_SIZEOF_PINTRIES;    

  pBuffer[OFFSET_DATA+currentPointer] = pInitParams->defaultCHVLen;

  currentPointer += MSC_SIZEOF_PINSIZE;

  memcpy(&pBuffer[OFFSET_DATA+currentPointer], pInitParams->defaultCHV, 
	 pInitParams->defaultCHVLen);
  
  currentPointer += pInitParams->defaultCHVLen;

  /* Write User Pin Unblock */

  pBuffer[OFFSET_DATA+currentPointer] = pInitParams->defaultCHVUnblockSize;

  currentPointer += MSC_SIZEOF_PINSIZE;

  memcpy(&pBuffer[OFFSET_DATA+currentPointer], pInitParams->defaultCHVUnblock, 
	 pInitParams->defaultCHVUnblockSize);
  
  currentPointer += pInitParams->defaultCHVUnblockSize;

  
  MemCopy32(&pBuffer[OFFSET_DATA+currentPointer], 
	    &pInitParams->objectMemory);
  
  currentPointer += MSC_SIZEOF_OBJECTSIZE;


  /* Anyone can create objects */
  pBuffer[OFFSET_DATA+currentPointer] = 0x00;
  currentPointer += MSC_SIZEOF_MINIACL;
  /* Keys only after user pin verified */
  pBuffer[OFFSET_DATA+currentPointer] = 0x02;
  currentPointer += MSC_SIZEOF_MINIACL;
  /* Pins only after admin pin verified */
  pBuffer[OFFSET_DATA+currentPointer] = 0x01;
  currentPointer += MSC_SIZEOF_MINIACL;

  transmitBuffer.bufferSize = pBuffer[OFFSET_LC] + 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU( pConnection, &transmitBuffer );
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    return convertSW(apduResponse);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }

}

MSC_RV PL_MSCExtendedFeature( MSCLPTokenConnection pConnection, 
			      MSCULong32 extFeature, MSCPUChar8 outData, 
			      MSCULong32 outLength, MSCPUChar8 inData,
			      MSCPULong32 inLength ) {

  return MSC_UNSUPPORTED_FEATURE;
}

MSC_RV PL_MSCGenerateKeys( MSCLPTokenConnection pConnection, 
			   MSCUChar8 prvKeyNum, MSCUChar8 pubKeyNum, 
			   MSCLPGenKeyParams pParams ) {
  
  MSCLong32 rv;

  MSCPUChar8 apduResponse; MSCPUChar8 pBuffer;
  MSCTransmitBuffer transmitBuffer;
  MSCULong32 currentPointer;

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CardEdge_CLA;
  pBuffer[OFFSET_INS]    = INS_MSC_GEN_KEYPAIR;
  pBuffer[OFFSET_P1]     = prvKeyNum;
  pBuffer[OFFSET_P2]     = pubKeyNum;
  pBuffer[OFFSET_LC]     = 16 + pParams->optParamsSize;

  currentPointer  = 0;

  /* Algorithm Type */
  pBuffer[OFFSET_DATA] = pParams->algoType;
  currentPointer += MSC_SIZEOF_ALGOTYPE;

  /* Key Size */
  MemCopy16(&pBuffer[OFFSET_DATA+currentPointer], &pParams->keySize);
  currentPointer += MSC_SIZEOF_KEYSIZE;

  /* Private Key ACL */
  MemCopy16(&pBuffer[OFFSET_DATA+currentPointer], 
	    &pParams->privateKeyACL.readPermission);

  currentPointer += MSC_SIZEOF_ACLVALUE;

  MemCopy16(&pBuffer[OFFSET_DATA+currentPointer], 
	    &pParams->privateKeyACL.writePermission);

  currentPointer += MSC_SIZEOF_ACLVALUE;

  MemCopy16(&pBuffer[OFFSET_DATA+currentPointer], 
	    &pParams->privateKeyACL.usePermission);

  currentPointer += MSC_SIZEOF_ACLVALUE;

  /* Public Key ACL */

  MemCopy16(&pBuffer[OFFSET_DATA+currentPointer], 
	    &pParams->publicKeyACL.readPermission);

  currentPointer += MSC_SIZEOF_ACLVALUE;

  MemCopy16(&pBuffer[OFFSET_DATA+currentPointer], 
	    &pParams->publicKeyACL.writePermission);

  currentPointer += MSC_SIZEOF_ACLVALUE;

  MemCopy16(&pBuffer[OFFSET_DATA+currentPointer], 
	    &pParams->publicKeyACL.usePermission);

  currentPointer += MSC_SIZEOF_ACLVALUE;

  /* Key Generation Options */
  pBuffer[OFFSET_DATA+currentPointer] = pParams->keyGenOptions;
  currentPointer += MSC_SIZEOF_GENOPTIONS;


  /* Key Generation Options Data */
  memcpy(&pBuffer[OFFSET_DATA+currentPointer], pParams->pOptParams, 
	 pParams->optParamsSize);

  transmitBuffer.bufferSize = pBuffer[OFFSET_LC] + 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU( pConnection, &transmitBuffer );		
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    return convertSW(apduResponse);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }
}

MSC_RV PL_MSCImportKey( MSCLPTokenConnection pConnection, MSCUChar8 keyNum,
                        MSCLPKeyACL pKeyACL, 
			MSCPUChar8 pKeyBlob, MSCULong32 keyBlobSize, 
			MSCLPKeyPolicy keyPolicy, 
			MSCPVoid32 pAddParams, MSCUChar8 addParamsSize ) {

  MSCLong32 rv;

  MSCPUChar8 apduResponse; MSCPUChar8 pBuffer;
  MSCTransmitBuffer transmitBuffer;
  MSCULong32 currentPointer;
  int i; MSCObjectACL acl;
  
  acl.readPermission   = MSC_AUT_PIN_1;
  acl.writePermission  = MSC_AUT_PIN_1;
  acl.deletePermission = MSC_AUT_PIN_1;

  rv = PL_MSCCreateObject(pConnection, IN_OBJECT_ID,
			  keyBlobSize, &acl);
  if (rv != MSC_SUCCESS)
    return rv;

  i=0;

  /* Take the key and write it to the default object */
  for (i=0; i < keyBlobSize/MSC_SIZEOF_KEYPACKET; i++) {
    rv = PL_MSCWriteObject( pConnection, IN_OBJECT_ID, 
			    i*MSC_SIZEOF_KEYPACKET, 
			    &pKeyBlob[i*MSC_SIZEOF_KEYPACKET], 
			    MSC_SIZEOF_KEYPACKET );
    if ( rv != MSC_SUCCESS ) { return rv; }
  }

  if ( keyBlobSize%MSC_SIZEOF_KEYPACKET ) {
    rv = PL_MSCWriteObject( pConnection, IN_OBJECT_ID, 
			    i*MSC_SIZEOF_KEYPACKET, 
			    &pKeyBlob[i*MSC_SIZEOF_KEYPACKET], 
			    keyBlobSize%MSC_SIZEOF_KEYPACKET );
    if ( rv != MSC_SUCCESS ) {
      PL_MSCDeleteObject(pConnection, IN_OBJECT_ID, MSC_ZF_WRITE_ZERO);
      return rv;
    }
  }    

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CardEdge_CLA;
  pBuffer[OFFSET_INS]    = INS_IMPORT_KEY;
  pBuffer[OFFSET_P1]     = keyNum;
  pBuffer[OFFSET_P2]     = 0x00;
  pBuffer[OFFSET_LC]     = MSC_SIZEOF_ACLSTRUCT + addParamsSize;

  currentPointer = 0;

  MemCopy16(&pBuffer[OFFSET_DATA+currentPointer], 
	    &pKeyACL->readPermission);

  currentPointer += MSC_SIZEOF_ACLVALUE;

  MemCopy16(&pBuffer[OFFSET_DATA+currentPointer], 
	    &pKeyACL->writePermission);

  currentPointer += MSC_SIZEOF_ACLVALUE;

  MemCopy16(&pBuffer[OFFSET_DATA+currentPointer], 
	    &pKeyACL->usePermission);

  currentPointer += MSC_SIZEOF_ACLVALUE;

  memcpy(&pBuffer[OFFSET_DATA+currentPointer], pAddParams, addParamsSize);

  transmitBuffer.bufferSize = pBuffer[OFFSET_LC] + 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU( pConnection, &transmitBuffer );

  PL_MSCDeleteObject(pConnection, IN_OBJECT_ID, MSC_ZF_WRITE_ZERO);

  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    return convertSW(apduResponse);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }

}

MSC_RV PL_MSCExportKey( MSCLPTokenConnection pConnection, MSCUChar8 keyNum,
			MSCPUChar8 pKeyBlob, MSCPULong32 keyBlobSize, 
			MSCPVoid32 pAddParams,
			MSCUChar8 addParamsSize ) {

  MSCLong32 rv;

  MSCPUChar8 apduResponse; MSCPUChar8 pBuffer;
  MSCTransmitBuffer transmitBuffer;
  MSCULong32 currentPointer;
  MSCULong32 blobSize;
  MSCObjectInfo objInfo;
  int i;

  i=0; blobSize=0; rv=0; currentPointer=0;

  if ( pConnection == 0 || keyBlobSize == 0 || pKeyBlob == 0 ) {
    return MSC_INVALID_PARAMETER;
  }

  pBuffer = transmitBuffer.pBuffer; apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CardEdge_CLA;
  pBuffer[OFFSET_INS]    = INS_EXPORT_KEY;
  pBuffer[OFFSET_P1]     = keyNum;
  pBuffer[OFFSET_P2]     = 0x00;
  pBuffer[OFFSET_LC]     = addParamsSize + 1;

  pBuffer[OFFSET_DATA]   = MSC_BLOB_ENC_PLAIN;

  if ( pAddParams != 0 ) {
    memcpy(&pBuffer[OFFSET_DATA+1], pAddParams, addParamsSize);
  }

  transmitBuffer.bufferSize = pBuffer[OFFSET_LC] + 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU( pConnection, &transmitBuffer );
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize != 2 ) {
    return MSC_UNSPECIFIED_ERROR;
  }

  if ( convertSW(apduResponse) != MSC_SUCCESS ) {
    return convertSW(apduResponse);
  }

  do {
    
    /* Get the objects size */
    rv = lcMSCGetObjectAttributes( pConnection, OUT_OBJECT_ID, &objInfo );

    if ( rv != MSC_SUCCESS ) { break; }
    
    if ( objInfo.objectSize > *keyBlobSize ) {
      *keyBlobSize = objInfo.objectSize;
      rv = MSC_NO_MEMORY_LEFT;
      break;
    }
    
    *keyBlobSize = objInfo.objectSize;
    blobSize = objInfo.objectSize;
    
    /* Read the key from the default object */
    for (i=0; i < blobSize/MSC_SIZEOF_KEYPACKET; i++) {
      rv = PL_MSCReadObject( pConnection, OUT_OBJECT_ID, 
			     i*MSC_SIZEOF_KEYPACKET, 
			     &pKeyBlob[i*MSC_SIZEOF_KEYPACKET], 
			     MSC_SIZEOF_KEYPACKET );
      if ( rv != MSC_SUCCESS ) { break; }
    }
    
    if ( blobSize%MSC_SIZEOF_KEYPACKET ) {
      rv = PL_MSCReadObject( pConnection, OUT_OBJECT_ID, 
			     i*MSC_SIZEOF_KEYPACKET, 
			     &pKeyBlob[i*MSC_SIZEOF_KEYPACKET], 
			     blobSize%MSC_SIZEOF_KEYPACKET );
      
      if ( rv != MSC_SUCCESS ) { break; }
    }    

  } while (0);

  /* Delete the default output object */
  PL_MSCDeleteObject( pConnection, OUT_OBJECT_ID, MSC_ZF_WRITE_ZERO );

  return rv;
}

MSC_RV PL_MSCComputeCrypt( MSCLPTokenConnection pConnection,
			   MSCLPCryptInit cryptInit, MSCPUChar8 pInputData,
			   MSCULong32 inputDataSize, MSCPUChar8 pOutputData,
			   MSCPULong32 outputDataSize ) {

  MSCLong32 rv;

  MSCPUChar8 apduResponse; MSCPUChar8 pBuffer;
  MSCUShort16 outSize;
  MSCUChar8 dataLocation;
  MSCTransmitBuffer transmitBuffer;
  MSCPUChar8 ppRecvBuffer;
  MSCULong32 currentPointer;
  MSCObjectACL objACL;

  pBuffer = transmitBuffer.pBuffer; apduResponse = transmitBuffer.apduResponse;

  /******************************************/
  /* Do the MSC_CIPHER_INIT portion of the code */
  /******************************************/

  pBuffer[OFFSET_CLA]    = CardEdge_CLA;
  pBuffer[OFFSET_INS]    = INS_COMPUTE_CRYPT;
  pBuffer[OFFSET_P1]     = cryptInit->keyNum;
  pBuffer[OFFSET_P2]     = MSC_CIPHER_INIT;
  
  /* Store init in object */
  if ( cryptInit->optParamsSize + MSC_SIZEOF_CIPHERMODE + 
       MSC_SIZEOF_CIPHERDIR + MSC_SIZEOF_DATALOCATION > 
       MSC_MAXSIZEOF_APDU_DATALEN + MSC_SIZEOF_OPTLEN) 
    {
      
      pBuffer[OFFSET_LC]     = MSC_SIZEOF_CIPHERMODE + 
	MSC_SIZEOF_CIPHERDIR + MSC_SIZEOF_DATALOCATION +
	MSC_SIZEOF_OPTLEN;
      
      dataLocation = DL_OBJECT;
      
    /* Store init in apdu */
    } else {
      pBuffer[OFFSET_LC]     = cryptInit->optParamsSize + 
	MSC_SIZEOF_CIPHERMODE + MSC_SIZEOF_CIPHERDIR + 
	MSC_SIZEOF_DATALOCATION + MSC_SIZEOF_OPTLEN;
      
      dataLocation = DL_APDU;
    }
  
  currentPointer  = 0;

  /* Cipher mode */
  pBuffer[OFFSET_DATA] = cryptInit->cipherMode;
  currentPointer += MSC_SIZEOF_CIPHERMODE;

  /* Cipher direction */
  pBuffer[OFFSET_DATA+currentPointer] = cryptInit->cipherDirection;

  /* FIX - Forcing Encrypt Mode */
  if (cryptInit->cipherDirection == MSC_DIR_SIGN) {
    pBuffer[OFFSET_DATA+currentPointer] = MSC_DIR_ENCRYPT;
  }

  currentPointer += MSC_SIZEOF_CIPHERDIR;  

  pBuffer[OFFSET_DATA+currentPointer] = dataLocation;
  currentPointer += MSC_SIZEOF_DATALOCATION;

  MemCopy16(&pBuffer[OFFSET_DATA+currentPointer], &cryptInit->optParamsSize);
  currentPointer += MSC_SIZEOF_OPTLEN;

  /* TODO: memcopy and MSCCreateObject/WriteObject needed here */
  /* Opt Parameters are not used in this version of the spec */

  transmitBuffer.bufferSize = pBuffer[OFFSET_LC] + 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU( pConnection, &transmitBuffer );
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize != 2 ) {
    return MSC_UNSPECIFIED_ERROR;
  }

  if ( convertSW(apduResponse) != MSC_SUCCESS ) {
    return convertSW(apduResponse);
  }


  if ( (inputDataSize + MSC_SIZEOF_CIPHERMODE + MSC_SIZEOF_CIPHERDIR
	+ MSC_SIZEOF_DATALOCATION) > MSC_MAXSIZEOF_APDU_DATALEN ) {

    /*********************************************/
    /* Do the MSC_CIPHER_PROCESS portion of the code */
    /*********************************************/

    /* TODO : I don't want to do this now */

    pBuffer[OFFSET_P2]     = MSC_CIPHER_PROCESS;
    pBuffer[OFFSET_LC]     = 0; /* TODO */
    
    currentPointer  = 0;
        
    return MSC_UNSUPPORTED_FEATURE;

  } else {

    /*******************************************/
    /* Do the MSC_CIPHER_FINAL portion of the code */
    /*******************************************/

    pBuffer[OFFSET_P2]        = MSC_CIPHER_FINAL;
    currentPointer            = 0;

    if ( inputDataSize + MSC_SIZEOF_DATALOCATION > 
	 MSC_MAXSIZEOF_APDU_DATALEN ) 
      {
	/* Too big, put in object first */
	pBuffer[OFFSET_LC]    = MSC_SIZEOF_DATALOCATION;
	pBuffer[OFFSET_DATA]  = DL_OBJECT;
	dataLocation          = DL_OBJECT;

	objACL.readPermission   = MSC_AUT_PIN_1;
	objACL.writePermission  = MSC_AUT_PIN_1;
	objACL.deletePermission = MSC_AUT_PIN_1;

	rv = PL_MSCCreateObject(pConnection, IN_OBJECT_ID, inputDataSize,
				&objACL);

	if ( rv != MSC_SUCCESS ) {
	  return rv;
	}

	rv = PL_MSCWriteLargeObject(pConnection, IN_OBJECT_ID, pInputData,
				 inputDataSize);

	if ( rv != MSC_SUCCESS ) {
	  return rv;
	}

      } else {
	/* Can be placed into an apdu */
	pBuffer[OFFSET_LC]    = MSC_SIZEOF_DATALOCATION + 2 + inputDataSize;
	pBuffer[OFFSET_DATA]  = DL_APDU;
	dataLocation          = DL_APDU;
	currentPointer       += MSC_SIZEOF_DATALOCATION;
	{
	  // I'm not sure if inputDataSize has the right length for MemCopy16()
	  MSCUShort16 value = inputDataSize;
	  MemCopy16(&pBuffer[OFFSET_DATA + currentPointer], &value);
	  currentPointer	     += 2;
	}
	memcpy(&pBuffer[OFFSET_DATA+currentPointer], pInputData, 
	       inputDataSize);
      }    

    transmitBuffer.bufferSize = pBuffer[OFFSET_LC] + 5;

    /* Set up the APDU exchange */
    transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
    rv = SCardExchangeAPDU( pConnection, &transmitBuffer );

    if ( rv != SCARD_S_SUCCESS ) {
      return convertPCSC(rv);
    }

    /* Operation must have failed */
    if ( (transmitBuffer.apduResponseSize == 2)&&
	 (dataLocation == DL_APDU) ) {
      return convertSW(apduResponse);

      /* Output stored into apdu */
    } else if ( (transmitBuffer.apduResponseSize > 2)&&
		(dataLocation == DL_APDU) ) {
      MemCopyTo16(&outSize, apduResponse);
      memcpy(pOutputData, &apduResponse[MSC_SIZEOF_CRYPTLEN], outSize);
      *outputDataSize = outSize;
      return convertSW(&apduResponse[MSC_SIZEOF_CRYPTLEN+outSize]);

      /* Output stored into an object */
    } else if ( (transmitBuffer.apduResponseSize == 2)&&
		(dataLocation == DL_OBJECT) ) {
      rv = PL_MSCReadAllocateObject( pConnection, OUT_OBJECT_ID,
				     &ppRecvBuffer, outputDataSize );
      if ( rv == MSC_SUCCESS ) {
	/* Write Data Chunk Size */
	setUShort16(ppRecvBuffer, *outputDataSize);
	/* Write Data */
	memcpy(pOutputData, &ppRecvBuffer[MSC_SIZEOF_CRYPTLEN], 
	       getUShort16(ppRecvBuffer));
      }

      if ( ppRecvBuffer ) {
	free(ppRecvBuffer);
      }

      return rv;
    } else {
      return MSC_UNSPECIFIED_ERROR;
    }
  } 
}

MSC_RV PL_MSCExtAuthenticate( MSCLPTokenConnection pConnection, 
			      MSCUChar8 keyNum,
			      MSCUChar8 cipherMode, MSCUChar8 cipherDirection,
			      MSCPUChar8 pData, MSCULong32 dataSize )
{

  MSCLong32 rv;

  MSCPUChar8 apduResponse; MSCPUChar8 pBuffer;
  MSCTransmitBuffer transmitBuffer;
  MSCULong32 currentPointer;
  unsigned char dataLocation;

  pBuffer = transmitBuffer.pBuffer; apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CardEdge_CLA;
  pBuffer[OFFSET_INS]    = INS_EXT_AUTH;
  pBuffer[OFFSET_P1]     = keyNum;
  pBuffer[OFFSET_P2]     = 0x00;

  if (dataSize + MSC_SIZEOF_CIPHERMODE + MSC_SIZEOF_CIPHERDIR +
      MSC_SIZEOF_DATALOCATION < MSC_MAXSIZEOF_APDU_DATALEN ) {
    dataLocation = DL_APDU;
    pBuffer[OFFSET_LC] = MSC_SIZEOF_CIPHERMODE + MSC_SIZEOF_CIPHERDIR +
      MSC_SIZEOF_DATALOCATION + 2 + dataSize;
  } else {
    dataLocation = DL_OBJECT;
    pBuffer[OFFSET_LC] = MSC_SIZEOF_CIPHERMODE + MSC_SIZEOF_CIPHERDIR +
      MSC_SIZEOF_DATALOCATION;
  }

  pBuffer[OFFSET_DATA+MSC_SIZEOF_CIPHERMODE] = dataLocation;

  currentPointer = 0;

  pBuffer[OFFSET_DATA+currentPointer] = cipherMode;

  currentPointer += MSC_SIZEOF_CIPHERMODE;

  pBuffer[OFFSET_DATA+currentPointer] = cipherDirection;
  
  currentPointer += MSC_SIZEOF_CIPHERDIR;

  pBuffer[OFFSET_DATA+currentPointer] = dataLocation;
  
  currentPointer += MSC_SIZEOF_DATALOCATION;

  {
    MSCUShort16 ush = dataSize;
    MemCopy16(&pBuffer[OFFSET_DATA+currentPointer], &ush);
    currentPointer += 2;
  }

  memcpy(&pBuffer[OFFSET_DATA+currentPointer], pData, 
	 dataSize);
  
  transmitBuffer.bufferSize =  pBuffer[OFFSET_LC] + 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU( pConnection, &transmitBuffer );
  
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

MSC_RV PL_MSCListKeys( MSCLPTokenConnection pConnection, MSCUChar8 seqOption,
		       MSCLPKeyInfo pKeyInfo ) {

  MSCLong32 rv;

  MSCPUChar8 apduResponse; MSCPUChar8 pBuffer;
  MSCULong32 currentPointer;
  MSCTransmitBuffer transmitBuffer;

  pBuffer = transmitBuffer.pBuffer; apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CardEdge_CLA;
  pBuffer[OFFSET_INS]    = INS_LIST_KEYS;
  pBuffer[OFFSET_P1]     = seqOption;
  pBuffer[OFFSET_P2]     = 0x00;
  pBuffer[OFFSET_LC]     = MSC_SIZEOF_KEYINFO;

  transmitBuffer.bufferSize = 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU( pConnection, &transmitBuffer );
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    if ( convertSW(apduResponse) == MSC_SUCCESS ) {
      /* Must have finished */
      return MSC_SEQUENCE_END;
    } else {
      return convertSW(apduResponse);
    }
  } else if (transmitBuffer.apduResponseSize == MSC_SIZEOF_KEYINFO + 2 ) {
    
    currentPointer = 0;

    /* Key Number */
    pKeyInfo->keyNum  = apduResponse[currentPointer];
    currentPointer += MSC_SIZEOF_KEYNUMBER;

    /* Key Type */
    pKeyInfo->keyType = apduResponse[currentPointer];
    currentPointer += MSC_SIZEOF_KEYTYPE;

    /* Key Partner */
    pKeyInfo->keyPartner = apduResponse[currentPointer];
    currentPointer += MSC_SIZEOF_KEYPARTNER;

    /* Key Size */
    MemCopyTo16(&pKeyInfo->keySize, 
		&apduResponse[currentPointer]);

    currentPointer += MSC_SIZEOF_KEYSIZE;

    /* Key ACL */
    MemCopyTo16(&pKeyInfo->keyACL.readPermission, 
		&apduResponse[currentPointer]);

    currentPointer += MSC_SIZEOF_ACLVALUE;
    
    MemCopyTo16(&pKeyInfo->keyACL.writePermission, 
		&apduResponse[currentPointer]);
    
    currentPointer += MSC_SIZEOF_ACLVALUE;

    MemCopyTo16(&pKeyInfo->keyACL.usePermission, 
		&apduResponse[currentPointer]);

    currentPointer += MSC_SIZEOF_ACLVALUE;
    
    switch(pKeyInfo->keyType)
    {
        case MSC_KEY_RSA_PUBLIC:
        case MSC_KEY_RSA_PRIVATE:
        case MSC_KEY_RSA_PRIVATE_CRT:
            pKeyInfo->keyPolicy.cipherMode = MSC_KEYPOLICY_MODE_RSA_NOPAD;
            pKeyInfo->keyPolicy.cipherDirection = MSC_KEYPOLICY_DIR_SIGN | MSC_KEYPOLICY_DIR_VERIFY |
                MSC_KEYPOLICY_DIR_ENCRYPT | MSC_KEYPOLICY_DIR_DECRYPT;
            break;
        case MSC_KEY_DES:
        case MSC_KEY_3DES:
        case MSC_KEY_3DES3:
            pKeyInfo->keyPolicy.cipherMode = MSC_KEYPOLICY_MODE_DES_ECB_NOPAD;
            pKeyInfo->keyPolicy.cipherDirection = MSC_KEYPOLICY_DIR_ENCRYPT | MSC_KEYPOLICY_DIR_DECRYPT;
            break;
            
        default:
            pKeyInfo->keyPolicy.cipherMode = 0;
            pKeyInfo->keyPolicy.cipherDirection = 0;
            break;
    }

    return convertSW(&apduResponse[currentPointer]);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }

}

MSC_RV PL_MSCCreatePIN( MSCLPTokenConnection pConnection, MSCUChar8 pinNum,
			MSCUChar8 pinAttempts, MSCPUChar8 pPinCode,
			MSCULong32 pinCodeSize, MSCPUChar8 pUnblockCode,
			MSCUChar8 unblockCodeSize ) {

  MSCLong32 rv;

  MSCPUChar8 apduResponse; MSCPUChar8 pBuffer;
  MSCTransmitBuffer transmitBuffer;
  MSCULong32 currentPointer;

  pBuffer = transmitBuffer.pBuffer; apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CardEdge_CLA;
  pBuffer[OFFSET_INS]    = INS_CREATE_PIN;
  pBuffer[OFFSET_P1]     = pinNum;
  pBuffer[OFFSET_P2]     = pinAttempts;
  pBuffer[OFFSET_LC]     = MSC_SIZEOF_PINSIZE + MSC_SIZEOF_PINSIZE + 
    pinCodeSize + unblockCodeSize;

  currentPointer = 0;

  pBuffer[OFFSET_DATA+currentPointer] = pinCodeSize;

  currentPointer += MSC_SIZEOF_PINSIZE;

  memcpy(&pBuffer[OFFSET_DATA+currentPointer], pPinCode, 
	 pinCodeSize);
  
  currentPointer += pinCodeSize;

  pBuffer[OFFSET_DATA+currentPointer] = unblockCodeSize;

  currentPointer += MSC_SIZEOF_PINSIZE;

  memcpy(&pBuffer[OFFSET_DATA+currentPointer], pUnblockCode,
	 unblockCodeSize);

  transmitBuffer.bufferSize = pBuffer[OFFSET_LC] + 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU( pConnection, &transmitBuffer );
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    return convertSW(apduResponse);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }

}

MSC_RV PL_MSCVerifyPIN( MSCLPTokenConnection pConnection, MSCUChar8 pinNum,
			MSCPUChar8 pPinCode, MSCULong32 pinCodeSize ) {
  
  MSCLong32 rv;

  MSCPUChar8 apduResponse; MSCPUChar8 pBuffer;
  MSCTransmitBuffer transmitBuffer;

  pBuffer = transmitBuffer.pBuffer; apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CardEdge_CLA;
  pBuffer[OFFSET_INS]    = INS_VERIFY_PIN;
  pBuffer[OFFSET_P1]     = pinNum;
  pBuffer[OFFSET_P2]     = 0x00;
  pBuffer[OFFSET_LC]     = pinCodeSize;

  memcpy(&pBuffer[OFFSET_DATA], pPinCode, pinCodeSize);
  
  transmitBuffer.bufferSize =  pBuffer[OFFSET_LC] + 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU( pConnection, &transmitBuffer );
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    return convertSW(apduResponse);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }

}

MSC_RV PL_MSCChangePIN( MSCLPTokenConnection pConnection, MSCUChar8 pinNum,
			MSCPUChar8 pOldPinCode, MSCUChar8 oldPinCodeSize,
			MSCPUChar8 pNewPinCode, MSCUChar8 newPinCodeSize ) {

  MSCLong32 rv;

  MSCPUChar8 apduResponse; MSCPUChar8 pBuffer;
  MSCTransmitBuffer transmitBuffer;
  MSCULong32 currentPointer;

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CardEdge_CLA;
  pBuffer[OFFSET_INS]    = INS_CHANGE_PIN;
  pBuffer[OFFSET_P1]     = pinNum;
  pBuffer[OFFSET_P2]     = 0x00;
  pBuffer[OFFSET_LC]     = MSC_SIZEOF_PINSIZE + MSC_SIZEOF_PINSIZE + 
    oldPinCodeSize + newPinCodeSize;

  currentPointer = 0;

  pBuffer[OFFSET_DATA+currentPointer] = oldPinCodeSize;

  currentPointer += MSC_SIZEOF_PINSIZE;

  memcpy(&pBuffer[OFFSET_DATA+currentPointer], pOldPinCode, 
	 oldPinCodeSize);
  
  currentPointer += oldPinCodeSize;

  pBuffer[OFFSET_DATA+currentPointer] = newPinCodeSize;

  currentPointer += MSC_SIZEOF_PINSIZE;

  memcpy(&pBuffer[OFFSET_DATA+currentPointer], pNewPinCode,
	 newPinCodeSize);

  transmitBuffer.bufferSize = pBuffer[OFFSET_LC] + 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU( pConnection, &transmitBuffer );
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    return convertSW(apduResponse);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }

}

MSC_RV PL_MSCUnblockPIN( MSCLPTokenConnection pConnection, MSCUChar8 pinNum,
			 MSCPUChar8 pUnblockCode, 
			 MSCULong32 unblockCodeSize ) {

  MSCLong32 rv;

  MSCPUChar8 apduResponse; MSCPUChar8 pBuffer;
  MSCTransmitBuffer transmitBuffer;

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CardEdge_CLA;
  pBuffer[OFFSET_INS]    = INS_UNBLOCK_PIN;
  pBuffer[OFFSET_P1]     = pinNum;
  pBuffer[OFFSET_P2]     = 0x00;
  pBuffer[OFFSET_LC]     = unblockCodeSize;

  memcpy(&pBuffer[OFFSET_DATA], pUnblockCode, unblockCodeSize);

  transmitBuffer.bufferSize = pBuffer[OFFSET_LC] + 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU( pConnection, &transmitBuffer );
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    return convertSW(apduResponse);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }
}

MSC_RV PL_MSCListPINs( MSCLPTokenConnection pConnection, 
		       MSCPUShort16 pPinBitMask ) {

  MSCLong32 rv;

  MSCPUChar8 apduResponse; MSCPUChar8 pBuffer;
  MSCTransmitBuffer transmitBuffer;

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CardEdge_CLA;
  pBuffer[OFFSET_INS]    = INS_LIST_PINS;
  pBuffer[OFFSET_P1]     = 0x00;
  pBuffer[OFFSET_P2]     = 0x00;
  pBuffer[OFFSET_LC]     = 0x02;

  transmitBuffer.bufferSize = 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU( pConnection, &transmitBuffer );
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    return convertSW(apduResponse);
  } else if ( transmitBuffer.apduResponseSize == 4 ) {
    *pPinBitMask  = 0x100 * apduResponse[0];
    *pPinBitMask += apduResponse[1];
    return convertSW(&apduResponse[2]);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }

}

MSC_RV PL_MSCCreateObject( MSCLPTokenConnection pConnection, 
			   MSCString objectID, MSCULong32 objectSize, 
			   MSCLPObjectACL pObjectACL ) {

  MSCLong32 rv;

  MSCPUChar8 apduResponse; MSCPUChar8 pBuffer;
  MSCTransmitBuffer transmitBuffer;
  MSCULong32 currentPointer;
  MSCULong32 _objectID;

  if ( stringToID(&_objectID, objectID) ) 
    return MSC_INVALID_PARAMETER;

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CardEdge_CLA;
  pBuffer[OFFSET_INS]    = INS_CREATE_OBJ;
  pBuffer[OFFSET_P1]     = 0x00;
  pBuffer[OFFSET_P2]     = 0x00;
  pBuffer[OFFSET_LC]     = MSC_SIZEOF_OBJECTID + MSC_SIZEOF_OBJECTSIZE +
    MSC_SIZEOF_ACLSTRUCT;

  currentPointer = 0;

  MemCopy32(&pBuffer[OFFSET_DATA+currentPointer], &_objectID);
  
  currentPointer += MSC_SIZEOF_OBJECTID;
  
  MemCopy32(&pBuffer[OFFSET_DATA+currentPointer], &objectSize);
  
  currentPointer += MSC_SIZEOF_OBJECTSIZE;

  MemCopy16(&pBuffer[OFFSET_DATA+currentPointer], 
	    &pObjectACL->readPermission);

  currentPointer += MSC_SIZEOF_ACLVALUE;

  MemCopy16(&pBuffer[OFFSET_DATA+currentPointer], 
	    &pObjectACL->writePermission);

  currentPointer += MSC_SIZEOF_ACLVALUE;

  MemCopy16(&pBuffer[OFFSET_DATA+currentPointer], 
	    &pObjectACL->deletePermission);

  transmitBuffer.bufferSize = pBuffer[OFFSET_LC] + 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU( pConnection, &transmitBuffer );
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    return convertSW(apduResponse);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }

}

MSC_RV PL_MSCDeleteObject( MSCLPTokenConnection pConnection, 
			   MSCString objectID, MSCUChar8 zeroFlag ) {

  MSCLong32 rv;

  MSCPUChar8 apduResponse; MSCPUChar8 pBuffer;
  MSCTransmitBuffer transmitBuffer;
  MSCULong32 currentPointer;
  MSCULong32 _objectID;

  if ( stringToID(&_objectID, objectID) ) 
    return MSC_INVALID_PARAMETER;

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CardEdge_CLA;
  pBuffer[OFFSET_INS]    = INS_DELETE_OBJ;
  pBuffer[OFFSET_P1]     = 0x00;
  pBuffer[OFFSET_P2]     = zeroFlag;
  pBuffer[OFFSET_LC]     = MSC_SIZEOF_OBJECTID;

  currentPointer = 0;

  MemCopy32(&pBuffer[OFFSET_DATA+currentPointer], &_objectID);

  transmitBuffer.bufferSize = pBuffer[OFFSET_LC] + 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU( pConnection, &transmitBuffer );
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    return convertSW(apduResponse);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }

}

MSC_RV PL_MSCWriteObject( MSCLPTokenConnection pConnection, 
			  MSCString objectID, 
			  MSCULong32 offset, MSCPUChar8 pInputData, 
			  MSCUChar8 dataSize ) {

  MSCLong32 rv;

  MSCPUChar8 apduResponse; MSCPUChar8 pBuffer;
  MSCTransmitBuffer transmitBuffer;
  MSCULong32 currentPointer;
  MSCULong32 _objectID;

  if ( stringToID(&_objectID, objectID) ) 
    return MSC_INVALID_PARAMETER;

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CardEdge_CLA;
  pBuffer[OFFSET_INS]    = INS_WRITE_OBJ;
  pBuffer[OFFSET_P1]     = 0x00;
  pBuffer[OFFSET_P2]     = 0x00;
  pBuffer[OFFSET_LC]     = MSC_SIZEOF_OBJECTID + MSC_SIZEOF_OFFSET +
    MSC_SIZEOF_RWDATA + dataSize;
  
  currentPointer = 0;
  
  MemCopy32(&pBuffer[OFFSET_DATA+currentPointer], &_objectID);
  
  currentPointer = MSC_SIZEOF_OBJECTID;
  
  MemCopy32(&pBuffer[OFFSET_DATA+currentPointer], &offset);
  
  currentPointer += MSC_SIZEOF_OFFSET;

  pBuffer[OFFSET_DATA+currentPointer] =  dataSize;
  currentPointer += MSC_SIZEOF_RWDATA;

  memcpy(&pBuffer[OFFSET_DATA+currentPointer], pInputData, dataSize);
  transmitBuffer.bufferSize = pBuffer[OFFSET_LC] + 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU( pConnection, &transmitBuffer );
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    return convertSW(apduResponse);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }

}

MSC_RV PL_MSCReadObject( MSCLPTokenConnection pConnection, MSCString objectID, 
			 MSCULong32 offset, MSCPUChar8 pOutputData, 
			 MSCUChar8 dataSize ) {

  MSCLong32 rv;

  MSCPUChar8 apduResponse; MSCPUChar8 pBuffer;
  MSCTransmitBuffer transmitBuffer;
  MSCULong32 currentPointer;
  MSCULong32 _objectID;

  if ( stringToID(&_objectID, objectID) ) 
    return MSC_INVALID_PARAMETER;

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CardEdge_CLA;
  pBuffer[OFFSET_INS]    = INS_READ_OBJ;
  pBuffer[OFFSET_P1]     = 0x00;
  pBuffer[OFFSET_P2]     = 0x00;
  pBuffer[OFFSET_LC]     = MSC_SIZEOF_OBJECTID + MSC_SIZEOF_OFFSET +
    MSC_SIZEOF_RWDATA;

  currentPointer = 0;

  MemCopy32(&pBuffer[OFFSET_DATA+currentPointer], &_objectID);

  currentPointer = MSC_SIZEOF_OBJECTID;

  MemCopy32(&pBuffer[OFFSET_DATA+currentPointer], &offset);
  
  currentPointer += MSC_SIZEOF_OFFSET;
  pBuffer[OFFSET_DATA+currentPointer] =  dataSize;
  transmitBuffer.bufferSize = pBuffer[OFFSET_LC] + 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU( pConnection, &transmitBuffer );
  
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

MSC_RV PL_MSCListObjects( MSCLPTokenConnection pConnection, 
			  MSCUChar8 seqOption, 
			  MSCLPObjectInfo pObjectInfo ) {
  
  MSCLong32 rv;
  
  MSCPUChar8 apduResponse; MSCPUChar8 pBuffer;
  MSCTransmitBuffer transmitBuffer;
  MSCULong32 currentPointer;
  MSCULong32 _objectID;
  
  rv=0; currentPointer=0;

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CardEdge_CLA;
  pBuffer[OFFSET_INS]    = INS_LIST_OBJECTS;
  pBuffer[OFFSET_P1]     = seqOption;
  pBuffer[OFFSET_P2]     = 0x00;
  pBuffer[OFFSET_LC]     = MSC_SIZEOF_OBJECTID + MSC_SIZEOF_OBJECTSIZE
    + MSC_SIZEOF_ACLSTRUCT;

  transmitBuffer.bufferSize = 5;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU( pConnection, &transmitBuffer );
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    if ( convertSW(apduResponse) == MSC_SUCCESS ) {
      /* Must have finished */
      return MSC_SEQUENCE_END;
    } else {
      return convertSW(apduResponse);
    }
  } else if (transmitBuffer.apduResponseSize == pBuffer[OFFSET_LC] + 2 ) {
    currentPointer = 0;
    MemCopyTo32(&_objectID, &apduResponse[currentPointer]);
    idToString(pObjectInfo->objectID, _objectID);

    currentPointer += MSC_SIZEOF_OBJECTID;

    MemCopyTo32(&pObjectInfo->objectSize, &apduResponse[currentPointer]); 

    currentPointer += MSC_SIZEOF_OBJECTSIZE;

    MemCopyTo16(&pObjectInfo->objectACL.readPermission, 
		&apduResponse[currentPointer]);

    currentPointer += MSC_SIZEOF_ACLVALUE;
    
    MemCopyTo16(&pObjectInfo->objectACL.writePermission, 
		&apduResponse[currentPointer]);
    
    currentPointer += MSC_SIZEOF_ACLVALUE;

    MemCopyTo16(&pObjectInfo->objectACL.deletePermission, 
		&apduResponse[currentPointer]);

    currentPointer += MSC_SIZEOF_ACLVALUE;

    return convertSW(&apduResponse[currentPointer]);

  } else {
    return MSC_UNSPECIFIED_ERROR;
  }

}

MSC_RV PL_MSCLogoutAll( MSCLPTokenConnection pConnection ) {

  MSCLong32 rv;

  MSCPUChar8 apduResponse; MSCPUChar8 pBuffer;
  MSCTransmitBuffer transmitBuffer;

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA]    = CardEdge_CLA;
  pBuffer[OFFSET_INS]    = INS_LOGOUT_ALL;
  pBuffer[OFFSET_P1]     = 0x00;
  pBuffer[OFFSET_P2]     = 0x00;
  pBuffer[OFFSET_LC]     = 0x02;
  pBuffer[OFFSET_DATA]   = 0x00;
  pBuffer[OFFSET_DATA+1] = 0x00;

  transmitBuffer.bufferSize = 7;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU( pConnection, &transmitBuffer );
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( transmitBuffer.apduResponseSize == 2 ) {
    return convertSW(apduResponse);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }

}

MSC_RV PL_MSCGetChallenge( MSCLPTokenConnection pConnection, MSCPUChar8 pSeed,
			MSCUShort16 seedSize, MSCPUChar8 pRandomData,
			MSCUShort16 randomDataSize ) {

  MSCLong32 rv;
  MSCULong32 currentPointer;

  MSCPUChar8 apduResponse; MSCPUChar8 pBuffer;
  MSCTransmitBuffer transmitBuffer;
  MSCUShort16 chall_size;

  if ( pRandomData == 0 ) {
    return MSC_INVALID_PARAMETER;
  }

  if ( randomDataSize == 0 ) {
    return MSC_INVALID_PARAMETER;
  }

  pBuffer = transmitBuffer.pBuffer; 
  apduResponse = transmitBuffer.apduResponse;

  pBuffer[OFFSET_CLA] = CardEdge_CLA;
  pBuffer[OFFSET_INS] = INS_GET_CHALLENGE;
  pBuffer[OFFSET_P1]  = 0x00;

  /* We must dump the random to an object if it is too large */
  if ( randomDataSize > 255 ) {
    pBuffer[OFFSET_P2]  = DL_OBJECT;
  } else {
    pBuffer[OFFSET_P2]  = DL_APDU;
  }

  /* Short + Short + size of seed */
  pBuffer[OFFSET_LC] = MSC_SIZEOF_RANDOMSIZE +
    MSC_SIZEOF_SEEDLENGTH + seedSize;

  currentPointer = 0;

  MemCopy16(&pBuffer[OFFSET_DATA+currentPointer], &randomDataSize);

  currentPointer = MSC_SIZEOF_RANDOMSIZE;

  MemCopy16(&pBuffer[OFFSET_DATA+currentPointer], &seedSize);
  
  currentPointer += MSC_SIZEOF_SEEDLENGTH;
  
  if (seedSize > 0)
    memcpy(&pBuffer[OFFSET_DATA+currentPointer], pSeed, 
	   seedSize);
  
  transmitBuffer.bufferSize = 5 + MSC_SIZEOF_RANDOMSIZE +
    MSC_SIZEOF_SEEDLENGTH + seedSize;

  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU( pConnection, &transmitBuffer );
  
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  /* dumpBinary(apduResponse, transmitBuffer.apduResponseSize); */
  MemCopyTo16(&chall_size, apduResponse);

  if (( transmitBuffer.apduResponseSize == randomDataSize + 4 ) && 
      (chall_size == randomDataSize)) {
    memcpy(pRandomData, apduResponse + 2, randomDataSize);
    return convertSW(&apduResponse[transmitBuffer.apduResponseSize-2]);
  } else if ( transmitBuffer.apduResponseSize == 2 ) {
    return convertSW(&apduResponse[transmitBuffer.apduResponseSize-2]);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }

}

MSC_RV lcMSCGetObjectAttributes( MSCLPTokenConnection pConnection, 
				 MSCString objectID, 
				 MSCLPObjectInfo pObjectInfo ) {
  MSC_RV rv;
  MSCString pObjectID;
  MSCChar8 pStringID[MSC_MAXSIZE_OBJID];
  MSCObjectInfo objInfo;

  if ( pConnection == NULL ) return MSC_INVALID_PARAMETER; 

  rv = PL_MSCListObjects( pConnection, MSC_SEQUENCE_RESET, &objInfo );

  if ( rv != MSC_SEQUENCE_END && rv != MSC_SUCCESS) {
    return rv;
  }

  if ( rv == MSC_SEQUENCE_END ) {
    return MSC_OBJECT_NOT_FOUND;
  }

  if ( strcmp(objectID, IN_OBJECT_ID) == 0 ) {
    idToString( pStringID, 0xFFFFFFFE );
    pObjectID = pStringID;
  } else if ( strcmp(objectID, OUT_OBJECT_ID) == 0 ) {
    idToString( pStringID, 0xFFFFFFFF );
    pObjectID = pStringID;
  } else {
    pObjectID = objectID;
  }

  if (strncmp(pObjectID, objInfo.objectID, MSC_MAXSIZE_OBJID) == 0 ) {
    pObjectInfo->objectSize = objInfo.objectSize;
    pObjectInfo->objectACL.readPermission = 
      objInfo.objectACL.readPermission;
    pObjectInfo->objectACL.writePermission = 
      objInfo.objectACL.writePermission;
    pObjectInfo->objectACL.deletePermission = 
      objInfo.objectACL.deletePermission;
    strncpy(pObjectInfo->objectID, pObjectID, MSC_MAXSIZE_OBJID);
    return MSC_SUCCESS;
  }
  
  do {
    rv = PL_MSCListObjects( pConnection, MSC_SEQUENCE_NEXT, &objInfo );
    if (strncmp(pObjectID, objInfo.objectID, MSC_MAXSIZE_OBJID) == 0 ) 
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
  strncpy(pObjectInfo->objectID, pObjectID, MSC_MAXSIZE_OBJID);

  return MSC_SUCCESS;  
}

MSC_RV PL_MSCReadAllocateObject( MSCLPTokenConnection pConnection, 
				 MSCString objectID, MSCPUChar8 *pOutputData, 
				 MSCPULong32 dataSize ) {

  MSC_RV rv;
  MSCObjectInfo objInfo;

  MSCULong32 objectSize;

  if ( pOutputData == 0 ) {
    return MSC_INVALID_PARAMETER;
  }

  rv = lcMSCGetObjectAttributes( pConnection, objectID, &objInfo );

  if ( rv != MSC_SUCCESS ) {
    *dataSize = 0;
    *pOutputData = 0;
    return rv;
  }
  
  objectSize   = objInfo.objectSize;
  *dataSize    = objectSize;
  *pOutputData = (MSCPUChar8)malloc(sizeof(MSCUChar8)*objectSize);

  return PL_MSCReadLargeObject( pConnection, objectID, *pOutputData, 
				objectSize );

}

MSC_RV PL_MSCReadLargeObject( MSCLPTokenConnection pConnection, 
			      MSCString objectID, MSCPUChar8 pOutputData, 
			      MSCULong32 dataSize ) {

  MSC_RV rv;
  MSCULong32 objectSize;
  int i;

  objectSize = dataSize;
  rv         = MSC_UNSPECIFIED_ERROR;

  /* Read the key from the default object */
  for (i=0; i < objectSize/MSC_SIZEOF_KEYPACKET; i++) {
    rv = PL_MSCReadObject( pConnection, objectID, 
			   i*MSC_SIZEOF_KEYPACKET, 
			   &pOutputData[i*MSC_SIZEOF_KEYPACKET], 
			   MSC_SIZEOF_KEYPACKET );
    if ( rv != MSC_SUCCESS ) { return rv; }
  }
  
  if ( objectSize%MSC_SIZEOF_KEYPACKET ) {
    rv = PL_MSCReadObject( pConnection, objectID, 
			   i*MSC_SIZEOF_KEYPACKET, 
			   &pOutputData[i*MSC_SIZEOF_KEYPACKET], 
			   objectSize%MSC_SIZEOF_KEYPACKET );

    if ( rv != MSC_SUCCESS ) { return rv; }
  }    

  return rv;
}

MSC_RV PL_MSCWriteLargeObject( MSCLPTokenConnection pConnection, 
			       MSCString objectID, MSCPUChar8 pInputData, 
			       MSCULong32 dataSize ) {
  MSC_RV rv;
  MSCULong32 objectSize;
  int i;

  objectSize = dataSize;
  rv         = MSC_UNSPECIFIED_ERROR;

  /* Read the key from the default object */
  for (i=0; i < objectSize/MSC_SIZEOF_KEYPACKET; i++) {
    rv = PL_MSCWriteObject( pConnection, objectID, 
			    i*MSC_SIZEOF_KEYPACKET, 
			    &pInputData[i*MSC_SIZEOF_KEYPACKET], 
			    MSC_SIZEOF_KEYPACKET );
    if ( rv != MSC_SUCCESS ) { return rv; }
  }

  
  if ( objectSize%MSC_SIZEOF_KEYPACKET ) {
    rv = PL_MSCWriteObject( pConnection, objectID, 
			    i*MSC_SIZEOF_KEYPACKET, 
			    &pInputData[i*MSC_SIZEOF_KEYPACKET], 
			    objectSize%MSC_SIZEOF_KEYPACKET );
    
    if ( rv != MSC_SUCCESS ) { return rv; }
  }    
  
  return rv;
}

int idToString( char *objectString, MSCULong32 objectID ) {

  MSCUChar8 objBytes[MSC_MAXSIZE_OBJID];

  MemCopy32(objBytes, &objectID);

  snprintf(objectString, MSC_MAXSIZE_OBJID, "%c%c%c%c", objBytes[0],
	   objBytes[1], objBytes[2], objBytes[3]);
  return 0;
}

int stringToID( MSCPULong32 objectID, char *objectString ) {

  MSCULong32 localID;
  MSCUChar8 objBytes[MSC_MAXSIZE_OBJID];
  int i;

  localID = 0;

  if ( strcmp(objectString, IN_OBJECT_ID) == 0 ) {
    *objectID = 0xFFFFFFFE;
    return 0;
  } else if ( strcmp(objectString, OUT_OBJECT_ID) == 0 ) {
    *objectID = 0xFFFFFFFF;
    return 0;
  }

  if (strlen(objectString) > MSC_SIZEOF_OBJECTID) {
    return -1;
  }

  objBytes[0] = objectString[0];
  objBytes[1] = objectString[1];
  objBytes[2] = objectString[2];
  objBytes[3] = objectString[3];
   
  for (i=strlen(objectString); i < 4; i++) {
    objBytes[i] = 0x00; /* Pad with NULL */
  }

  MemCopyTo32(&localID, objBytes);

  if ( localID == 0 ) {
    return -1;
  }

  *objectID = localID;
  return 0;
}


/* Some local helper functions */

MSCUShort16 convertSW(MSCPUChar8 pBuffer) {
  MSCUShort16 retValue;

  retValue  = pBuffer[0] * 0x100;
  retValue += pBuffer[1];

  return retValue;
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
  MSCUChar8 getResponse[5] = {0x00, 0xC0, 0x00, 0x00, 0x00};
  MSCULong32 dwActiveProtocol;
  int i;

  originalLength = transmitBuffer->apduResponseSize;

  while (1) {
    
#ifdef MSC_DEBUG
    printf("->: ");
    
#define DEBUG_INS(a, b) if ((a) == (b)) printf("[" #b "] ");
    DEBUG_INS(transmitBuffer->pBuffer[OFFSET_INS], INS_MSC_GEN_KEYPAIR);
    DEBUG_INS(transmitBuffer->pBuffer[OFFSET_INS], INS_IMPORT_KEY);
    DEBUG_INS(transmitBuffer->pBuffer[OFFSET_INS], INS_EXPORT_KEY);
    DEBUG_INS(transmitBuffer->pBuffer[OFFSET_INS], INS_COMPUTE_CRYPT);
    DEBUG_INS(transmitBuffer->pBuffer[OFFSET_INS], INS_CREATE_PIN);
    DEBUG_INS(transmitBuffer->pBuffer[OFFSET_INS], INS_VERIFY_PIN);
    DEBUG_INS(transmitBuffer->pBuffer[OFFSET_INS], INS_CHANGE_PIN);
    DEBUG_INS(transmitBuffer->pBuffer[OFFSET_INS], INS_UNBLOCK_PIN);
    DEBUG_INS(transmitBuffer->pBuffer[OFFSET_INS], INS_LOGOUT_ALL);
    DEBUG_INS(transmitBuffer->pBuffer[OFFSET_INS], INS_GET_CHALLENGE);
    DEBUG_INS(transmitBuffer->pBuffer[OFFSET_INS], INS_EXT_AUTH);
    DEBUG_INS(transmitBuffer->pBuffer[OFFSET_INS], INS_CREATE_OBJ);
    DEBUG_INS(transmitBuffer->pBuffer[OFFSET_INS], INS_DELETE_OBJ);
    DEBUG_INS(transmitBuffer->pBuffer[OFFSET_INS], INS_READ_OBJ);
    DEBUG_INS(transmitBuffer->pBuffer[OFFSET_INS], INS_WRITE_OBJ);
    DEBUG_INS(transmitBuffer->pBuffer[OFFSET_INS], INS_LIST_OBJECTS);
    DEBUG_INS(transmitBuffer->pBuffer[OFFSET_INS], INS_LIST_PINS);
    DEBUG_INS(transmitBuffer->pBuffer[OFFSET_INS], INS_LIST_KEYS);
    DEBUG_INS(transmitBuffer->pBuffer[OFFSET_INS], INS_GET_STATUS);
    
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
	printf("Transmit error: %x\n", (unsigned int) rv);
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
    
    if ( transmitBuffer->apduResponseSize == 2 && 
	 transmitBuffer->apduResponse[0] == 0x61 ) {
#ifdef MSC_DEBUG
      printf("->: 0x00 0xC0 0x00 0x00 %02x\n", 
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
	printf("Transmit error: %x\n", (unsigned int) rv);
#endif
	return rv;
      } 

    } /* if needs get response */        

    break;
  }  /* End of while */
  
  
  return rv;
}

MSC_RV PL_MSCIdentifyToken( MSCLPTokenConnection pConnection ) {

  MSCLong32 rv;
  MSCTransmitBuffer transmitBuffer;
  MSCPUChar8 apduResponse; MSCPUChar8 pBuffer;
 
  pBuffer = transmitBuffer.pBuffer;
  apduResponse = transmitBuffer.apduResponse;
 
  pBuffer[OFFSET_CLA]    = 0x00;
  pBuffer[OFFSET_INS]    = 0xA4;
  pBuffer[OFFSET_P1]     = 0x04;
  pBuffer[OFFSET_P2]     = 0x00;
  pBuffer[OFFSET_LC]     = pConnection->tokenInfo.tokenAppLen;
 
  memcpy(&pBuffer[OFFSET_DATA], pConnection->tokenInfo.tokenApp, 
	 pConnection->tokenInfo.tokenAppLen);
 
  transmitBuffer.bufferSize = 5 + pConnection->tokenInfo.tokenAppLen;
 
  /* Set up the APDU exchange */
  transmitBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU( pConnection, &transmitBuffer );
 
  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }
 
  if ( transmitBuffer.apduResponseSize == 2 && apduResponse[0] == 0x90 ) {
    return MSC_SUCCESS;
  } else {
    return MSC_UNSUPPORTED_FEATURE;
  }
}

MSC_RV PL_MSCInitializePlugin( MSCLPTokenConnection pConnection ) {

  return MSC_SUCCESS;
}

MSC_RV PL_MSCFinalizePlugin( MSCLPTokenConnection pConnection ) {

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

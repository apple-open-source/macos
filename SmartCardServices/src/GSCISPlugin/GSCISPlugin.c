/******************************************************************
 GSCIS
        MUSCLE SmartCard Development ( http://www.musclecard.com )
            Title  : GSCISPlugin.c
            Package: GSCISPlugin
            Author : David Corcoran
            Date   : 02/19/02
            License: Copyright (C) 2002 David Corcoran
                     <corcoran@linuxnet.com>
            Purpose: A MuscleCard plugin for GSCIS compliant cards.
	             
 
********************************************************************/

#ifndef __APPLE__
#include <musclecard.h>
#else
#include <PCSC/musclecard.h>
#endif

#include "GSCISPlugin.h"
#include <string.h>
#include <stdio.h>


typedef struct {
  MSCUChar8  pBuffer[MAX_BUFFER_SIZE];
  MSCULong32 bufferSize;
  MSCUChar8  apduResponse[MAX_BUFFER_SIZE];
  MSCULong32 apduResponseSize;
  LPSCARD_IO_REQUEST ioType;
} MSCTransmitBuffer, *MSCLPTransmitBuffer;

/* internal function */
MSCLong32 SCardExchangeAPDU( MSCLPTokenConnection, MSCLPTransmitBuffer );
MSC_RV convertPCSC( MSCLong32 );

MSC_RV PL_MSCWriteFramework( MSCLPTokenConnection pConnection,
			  MSCLPInitTokenParams pInitParams ) {

  return MSC_UNSUPPORTED_FEATURE;
}

MSC_RV PL_MSCSelectAID( MSCLPTokenConnection pConnection, MSCPUChar8 aidValue,
  	             MSCULong32 aidSize ) {

  /* Make sure the right card is there, select the specified applet
     if needed.  If no applet specified, select the default applet
  */

  return MSC_UNSUPPORTED_FEATURE;
}

MSC_RV PL_MSCInitializePlugin( MSCLPTokenConnection pConnection ) {

  return MSC_SUCCESS;
}

MSC_RV PL_MSCFinalizePlugin( MSCLPTokenConnection pConnection ) {

  return MSC_SUCCESS;
}

MSC_RV PL_MSCIdentifyToken( MSCLPTokenConnection pConnection ) {

  return MSC_SUCCESS;
}

MSC_RV PL_MSCGetStatus( MSCLPTokenConnection pConnection, 
		     MSCLPStatusInfo pStatusInfo ) {

  return MSC_UNSUPPORTED_FEATURE;
}

MSC_RV PL_MSCGetCapabilities( MSCLPTokenConnection pConnection, MSCULong32 Tag,
			   MSCPUChar8 Value, MSCPULong32 Length ) {

  return MSC_UNSUPPORTED_FEATURE;
}

MSC_RV PL_MSCExtendedFeature( MSCLPTokenConnection pConnection, 
			   MSCULong32 extFeature,
			   MSCPUChar8 outData, MSCULong32 outLength, 
			   MSCPUChar8 inData, MSCPULong32 inLength ) {

  return MSC_UNSUPPORTED_FEATURE;
}

MSC_RV PL_MSCGenerateKeys( MSCLPTokenConnection pConnection, MSCUChar8 prvKeyNum,
			MSCUChar8 pubKeyNum, MSCLPGenKeyParams pParams ) {
 
  return MSC_UNSUPPORTED_FEATURE;
}

MSC_RV PL_MSCImportKey( MSCLPTokenConnection pConnection, MSCUChar8 keyNum,
		     MSCLPKeyACL pKeyACL, MSCPUChar8 pKeyBlob, 
		     MSCULong32 keyBlobSize, MSCLPKeyPolicy keyPolicy,
		     MSCPVoid32 pAddParams, MSCUChar8 addParamsSize ) {

  return MSC_UNSUPPORTED_FEATURE;
}
 
MSC_RV PL_MSCExportKey( MSCLPTokenConnection pConnection, MSCUChar8 keyNum,
		     MSCPUChar8 pKeyBlob, MSCPULong32 keyBlobSize, 
		     MSCPVoid32 pAddParams, MSCUChar8 addParamsSize ) {

  return MSC_UNSUPPORTED_FEATURE;
}

MSC_RV PL_MSCComputeCrypt( MSCLPTokenConnection pConnection,
			MSCLPCryptInit cryptInit, MSCPUChar8 pInputData,
			MSCULong32 inputDataSize, MSCPUChar8 pOutputData,
			MSCPULong32 outputDataSize ) {

  return MSC_UNSUPPORTED_FEATURE;
}

MSC_RV PL_MSCExtAuthenticate( MSCLPTokenConnection pConnection, MSCUChar8 keyNum,
			   MSCUChar8 cipherMode, MSCUChar8 cipherDirection,
			   MSCPUChar8 pData, MSCULong32 dataSize ) {

  return MSC_UNSUPPORTED_FEATURE;
}

MSC_RV PL_MSCListKeys( MSCLPTokenConnection pConnection, MSCUChar8 seqOption,
		    MSCLPKeyInfo pKeyInfo ) {

  return MSC_UNSUPPORTED_FEATURE;
}

MSC_RV PL_MSCCreatePIN( MSCLPTokenConnection pConnection, MSCUChar8 pinNum,
		     MSCUChar8 pinAttempts, MSCPUChar8 pPinCode,
		     MSCULong32 pinCodeSize, MSCPUChar8 pUnblockCode,
		     MSCUChar8 unblockCodeSize ) {

  return MSC_UNSUPPORTED_FEATURE;
}

MSC_RV PL_MSCVerifyPIN( MSCLPTokenConnection pConnection, MSCUChar8 pinNum,
		     MSCPUChar8 pPinCode, MSCULong32 pinCodeSize ) {

  return MSC_UNSUPPORTED_FEATURE;
}

MSC_RV PL_MSCChangePIN( MSCLPTokenConnection pConnection, MSCUChar8 pinNum,
		     MSCPUChar8 pOldPinCode, MSCUChar8 oldPinCodeSize,
		     MSCPUChar8 pNewPinCode, MSCUChar8 newPinCodeSize ) {

  return MSC_UNSUPPORTED_FEATURE;
}

MSC_RV PL_MSCUnblockPIN( MSCLPTokenConnection pConnection, MSCUChar8 pinNum,
		      MSCPUChar8 pUnblockCode, MSCULong32 unblockCodeSize ) {

  return MSC_UNSUPPORTED_FEATURE;
}

MSC_RV PL_MSCListPINs( MSCLPTokenConnection pConnection, 
		    MSCPUShort16 pPinBitMask ) {

  
  return MSC_UNSUPPORTED_FEATURE;
}

MSC_RV PL_MSCCreateObject( MSCLPTokenConnection pConnection, MSCString objectID,
			MSCULong32 objectSize, MSCLPObjectACL pObjectACL ) {

  return MSC_UNSUPPORTED_FEATURE;
}

MSC_RV PL_MSCDeleteObject( MSCLPTokenConnection pConnection, 
			MSCString objectID, MSCUChar8 zeroFlag ) {

  return MSC_UNSUPPORTED_FEATURE;
}

MSC_RV PL_MSCWriteObject( MSCLPTokenConnection pConnection, MSCString objectID, 
		       MSCULong32 offset, MSCPUChar8 pInputData, 
		       MSCUChar8 dataSize ) {

  return MSC_UNSUPPORTED_FEATURE;
}

MSC_RV PL_MSCReadObject( MSCLPTokenConnection pConnection, MSCString objectID, 
		      MSCULong32 offset, MSCPUChar8 pOutputData, 
		      MSCUChar8 dataSize ) {

  return MSC_UNSUPPORTED_FEATURE;
}

MSC_RV PL_MSCListObjects( MSCLPTokenConnection pConnection, MSCUChar8 seqOption, 
                       MSCLPObjectInfo pObjectInfo ) {

  return MSC_UNSUPPORTED_FEATURE;
}

MSC_RV PL_MSCLogoutAll( MSCLPTokenConnection pConnection ) {

  return MSC_UNSUPPORTED_FEATURE;
}

MSC_RV PL_MSCGetChallenge( MSCLPTokenConnection pConnection, MSCPUChar8 pSeed,
			MSCUShort16 seedSize, MSCPUChar8 pRandomData,
			MSCUShort16 randomDataSize ) {

  return MSC_UNSUPPORTED_FEATURE;
}



MSCLong32 SCardExchangeAPDU( MSCLPTokenConnection pConnection, 
			     MSCLPTransmitBuffer transmitBuffer ) {
  
  MSCLong32 rv, ret;
  MSCULong32 originalLength;
  MSCUChar8 getResponse[5] = {0x00, 0xC0, 0x00, 0x00, 0x00};
  MSCULong32 dwActiveProtocol;

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

#ifdef MSC_NOT_DEFINED    
    if ( suppressResponse == 1 ) {
      /* Do not do the Get Response */
      break;
    }
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
	printf("Transmit error: %s\n", pcsc_stringify_error(rv));
#endif
	return rv;
      } 
    }         

    break;
  }  /* End of while */
  
  
  return rv;
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

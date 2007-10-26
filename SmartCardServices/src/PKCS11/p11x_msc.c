/******************************************************************************
** 
**  $Id: p11x_msc.c,v 1.2 2003/02/13 20:06:40 ghoo Exp $
**
**  Package: PKCS-11
**  Author : Chris Osgood <oznet@mac.com>
**  License: Copyright (C) 2002 Schlumberger Network Solutions
**           <http://www.slb.com/sns>
**  Purpose: MSC wrappers.  This whole module does practically nothing other
**           than proxy calls to the MSC libraries.  This is mostly just to
**           provide a layer of protection in case the MSC API changes.
**           msc_ComputeCrypt does handle some cached PIN stuff.
** 
******************************************************************************/

#include "cryptoki.h"

MSC_RV msc_ListTokens(
  MSCULong32            listScope,        /* defines the scope to return */
  MSCLPTokenInfo        tokenArray,       /* token struct array          */
  MSCPULong32           arrayLength       /* Length of array             */
)
{
    MSC_RV rv;

    rv = MSCListTokens(
      listScope,
      tokenArray,
      arrayLength
    );

    return rv;
}

    /* Establishes a connection to the specified token */
MSC_RV msc_EstablishConnection( 
  MSCLPTokenInfo        tokenStruct,       /* The struct of token   */
  MSCULong32            sharingMode,       /* Mode of sharing       */
  MSCPUChar8            applicationName,   /* The applet ID/Name    */
  MSCULong32            nameSize,          /* The ID/Name Size      */
  MSCLPTokenConnection  pConnection        /* Returned connection   */
)
{
    MSC_RV rv;

    rv = MSCEstablishConnection( 
      tokenStruct,
      sharingMode,
      applicationName,
      nameSize,
      pConnection
    );

    return rv;
}

    /* Releases a connection to the specified token */
MSC_RV msc_ReleaseConnection( 
  MSCLPTokenConnection  pConnection,       /* Connection handle     */
  MSCULong32            endAction          /* Action to perform     */
)
{
    MSC_RV rv;

    rv = MSCReleaseConnection( 
      pConnection,
      endAction
    );

    return rv;
}

    /* Blocks for an event to occur on a token */
MSC_RV msc_WaitForTokenEvent( 
  MSCLPTokenInfo        tokenArray,        /* Array of token structs */
  MSCULong32            arraySize,	   /* Size of the array      */
  MSCULong32            timeoutValue       /* Timeout                */
)
{
    MSC_RV rv;

    rv = MSCWaitForTokenEvent( 
      tokenArray,
      arraySize,
      timeoutValue
    );

    return rv;
}

    /* Cancels a pending MSCWaitForTokenEvent */
MSC_RV msc_CancelEventWait( 
  void                                     /* No parameters          */
)
{
    MSC_RV rv;

    rv = MSCCancelEventWait();

    return rv;
}

    /* Registers a callback function for event change */
MSC_RV msc_CallbackForTokenEvent(
  MSCLPTokenInfo        tokenArray,        /* Array of token structs */
  MSCULong32            arraySize,         /* Size of the array      */
  MSCCallBack           callBack,          /* Callback function      */
  MSCPVoid32            appData            /* Application data       */
)
{
    MSC_RV rv = SCARD_E_UNSUPPORTED_FEATURE;
#if 0
    rv = MSCCallbackForTokenEvent(
      tokenArray,
      arraySize,
      callBack,
      appData
    );
#endif
    return rv;
}

    /* Cancels all callback registrations */
MSC_RV msc_CallbackCancelEvent()
{
    MSC_RV rv = SCARD_E_UNSUPPORTED_FEATURE;
#if 0
    rv = MSCCallbackCancelEvent();
#endif
    return rv;
}

    /* Locks a transaction to the token */
MSC_RV msc_BeginTransaction(
  MSCLPTokenConnection  pConnection       /* Connection handle          */
)
{
    MSC_RV rv;

    rv = MSCBeginTransaction(
      pConnection
    );			       

    return rv;
}

    /* Releases a locked transaction to the token */
MSC_RV msc_EndTransaction(
  MSCLPTokenConnection  pConnection,      /* Connection handle          */
  MSCULong32            endAction         /* Action to perform on token */
)
{
    MSC_RV rv;

    rv = MSCEndTransaction(
      pConnection,
      endAction
    );

    return rv;
}

    /* Pre-personalization function */
MSC_RV msc_WriteFramework( 
  MSCLPTokenConnection  pConnection, 
  MSCLPInitTokenParams  pInitParams 
)
{
    MSC_RV rv;

    rv = MSCWriteFramework( 
      pConnection, 
      pInitParams 
    );

    return rv;
}

MSC_RV msc_GetStatus(
  MSCLPTokenConnection  pConnection, 
  MSCLPStatusInfo       pStatusInfo
)
{
    MSC_RV rv;

    rv = MSCGetStatus(
      pConnection, 
      pStatusInfo
    );

    return rv;
}

MSC_RV msc_GetCapabilities(
  MSCLPTokenConnection  pConnection,
  MSCULong32            Tag,
  MSCPUChar8            Value,
  MSCPULong32           Length
)
{
    MSC_RV rv;

    rv = MSCGetCapabilities(
      pConnection,
      Tag,
      Value,
      Length
    );

    return rv;
}

MSC_RV msc_ExtendedFeature( 
  MSCLPTokenConnection  pConnection, 
  MSCULong32            extFeature,
  MSCPUChar8            outData, 
  MSCULong32            outLength, 
  MSCPUChar8            inData,
  MSCPULong32           inLength 
)
{
    MSC_RV rv;

    rv = MSCExtendedFeature( 
      pConnection, 
      extFeature,
      outData, 
      outLength, 
      inData,
      inLength 
    );

    return rv;
}

MSC_RV msc_GenerateKeys(
  MSCLPTokenConnection	pConnection,
  MSCUChar8		prvKeyNum,
  MSCUChar8		pubKeyNum,
  MSCLPGenKeyParams	pParams
)
{
    MSC_RV rv;

    rv = MSCGenerateKeys(
      pConnection,
      prvKeyNum,
      pubKeyNum,
      pParams
    );

    return rv;
}

MSC_RV msc_ImportKey(
  MSCLPTokenConnection	pConnection,
  MSCUChar8		keyNum,
  MSCLPKeyACL		pKeyACL,
  MSCPUChar8	        pKeyBlob,
  MSCULong32            keyBlobSize,
  MSCLPKeyPolicy        keyPolicy,
  MSCPVoid32		pAddParams,
  MSCUChar8		addParamsSize
)
{
    MSC_RV rv;

    rv = MSCImportKey(
      pConnection,
      keyNum,
      pKeyACL,
      pKeyBlob,
      keyBlobSize,
      keyPolicy,
      pAddParams,
      addParamsSize
    );

    return rv;
}

MSC_RV msc_ExportKey(
  MSCLPTokenConnection	pConnection,
  MSCUChar8		keyNum,
  MSCPUChar8	        pKeyBlob,
  MSCPULong32           keyBlobSize,
  MSCPVoid32		pAddParams,
  MSCUChar8		addParamsSize
)
{
    MSC_RV rv;

    rv = MSCExportKey(
      pConnection,
      keyNum,
      pKeyBlob,
      keyBlobSize,
      pAddParams,
      addParamsSize
    );

    return rv;
}

MSC_RV msc_ComputeCrypt( 
  MSCLPTokenConnection  pConnection,
  MSCLPCryptInit        cryptInit, 
  MSCPUChar8            pInputData,
  MSCULong32            inputDataSize, 
  MSCPUChar8            pOutputData,
  MSCPULong32           outputDataSize
)
{
    MSC_RV rv = MSC_SUCCESS;
    CK_ULONG i;

    /* Handles cached PIN's */
    for (i = 0; i < 2; i++)
    {
        rv = MSCComputeCrypt( 
          pConnection,
          cryptInit, 
          pInputData,
          inputDataSize, 
          pOutputData,
          outputDataSize
        );
    
        if (rv != MSC_SUCCESS)
        {
            if (CKR_ERROR(slot_ReverifyPins()))
                break;
        }
        else
            break;
    }

    return rv;
}

MSC_RV msc_ExtAuthenticate(
  MSCLPTokenConnection	pConnection,
  MSCUChar8	        keyNum,
  MSCUChar8             cipherMode, 
  MSCUChar8             cipherDirection,
  MSCPUChar8	        pData,
  MSCULong32	        dataSize
)
{
    MSC_RV rv;

    rv = MSCExtAuthenticate(
      pConnection,
      keyNum,
      cipherMode, 
      cipherDirection,
      pData,
      dataSize
    );

    return rv;
}

MSC_RV msc_ListKeys(
  MSCLPTokenConnection	pConnection,
  MSCUChar8		seqOption,
  MSCLPKeyInfo		pKeyInfo
)
{
    MSC_RV rv;

    rv = MSCListKeys(
      pConnection,
      seqOption,
      pKeyInfo
    );

    return rv;
}

MSC_RV msc_CreatePIN(
  MSCLPTokenConnection	pConnection,
  MSCUChar8		pinNum,
  MSCUChar8		pinAttempts,
  MSCPUChar8	        pPinCode,
  MSCULong32		pinCodeSize,
  MSCPUChar8	        pUnblockCode,
  MSCUChar8		unblockCodeSize
)
{
    MSC_RV rv;

    rv = MSCCreatePIN(
      pConnection,
      pinNum,
      pinAttempts,
      pPinCode,
      pinCodeSize,
      pUnblockCode,
      unblockCodeSize
    );

    return rv;
}

MSC_RV msc_VerifyPIN(
  MSCLPTokenConnection	pConnection,
  MSCUChar8		pinNum,
  MSCPUChar8	        pPinCode,
  MSCULong32		pinCodeSize
)
{
    MSC_RV rv;

    rv = MSCVerifyPIN(
      pConnection,
      pinNum,
      pPinCode,
      pinCodeSize
    );

    return rv;
}

MSC_RV msc_ChangePIN(
  MSCLPTokenConnection	pConnection,
  MSCUChar8		pinNum,
  MSCPUChar8	        pOldPinCode,
  MSCUChar8		oldPinCodeSize,
  MSCPUChar8	        pNewPinCode,
  MSCUChar8		newPinCodeSize
)
{
    MSC_RV rv;

    rv = MSCChangePIN(
      pConnection,
      pinNum,
      pOldPinCode,
      oldPinCodeSize,
      pNewPinCode,
      newPinCodeSize
    );

    return rv;
}

MSC_RV msc_UnblockPIN(
  MSCLPTokenConnection	pConnection,
  MSCUChar8		pinNum,
  MSCPUChar8	        pUnblockCode,
  MSCULong32		unblockCodeSize
)
{
    MSC_RV rv;

    rv = MSCUnblockPIN(
      pConnection,
      pinNum,
      pUnblockCode,
      unblockCodeSize
    );

    return rv;
}

MSC_RV msc_ListPINs(
  MSCLPTokenConnection	pConnection,
  MSCPUShort16	        pPinBitMask
)
{
    MSC_RV rv;

    rv = MSCListPINs(
      pConnection,
      pPinBitMask
    );

    return rv;
}

MSC_RV msc_CreateObject(
  MSCLPTokenConnection	pConnection,
  MSCString 	        objectID,
  MSCULong32		objectSize,
  MSCLPObjectACL	pObjectACL
)
{
    MSC_RV rv;

    rv = MSCCreateObject(
      pConnection,
      objectID,
      objectSize,
      pObjectACL
    );

    return rv;
}

MSC_RV msc_DeleteObject(
  MSCLPTokenConnection	pConnection,
  MSCString  		objectID,
  MSCUChar8		zeroFlag
)
{
    MSC_RV rv;

    rv = MSCDeleteObject(
      pConnection,
      objectID,
      zeroFlag
    );

    return rv;
}

MSC_RV msc_WriteObject(
  MSCLPTokenConnection	pConnection,
  MSCString 		objectID,
  MSCULong32		offset,
  MSCPUChar8        pInputData,
  MSCULong32		dataSize
)
{
    MSC_RV rv;

    rv = MSCWriteObject(
      pConnection,
      objectID,
      offset,
      pInputData,
      dataSize,
      0, /* Fixme: rwCallback */
      0 /* Fixme: addParams */
    );

    return rv;
}

MSC_RV msc_ReadObject(
  MSCLPTokenConnection	pConnection,
  MSCString 		objectID,
  MSCULong32		offset,
  MSCPUChar8	        pOutputData,
  MSCULong32		dataSize
)
{
    MSC_RV rv;

    rv = MSCReadObject(
      pConnection,
      objectID,
      offset,
      pOutputData,
      dataSize,
      0, /* Fixme: rwCallback */
      0 /* Fixme: addParams */
    );

    return rv;
}

MSC_RV msc_ListObjects(
  MSCLPTokenConnection	pConnection,
  MSCUChar8		seqOption,
  MSCLPObjectInfo	pObjectInfo
)
{
    MSC_RV rv;

    rv = MSCListObjects(
      pConnection,
      seqOption,
      pObjectInfo
    );

    return rv;
}

MSC_RV msc_LogoutAll(
  MSCLPTokenConnection	pConnection
)
{
    MSC_RV rv;

    rv = MSCLogoutAll(
      pConnection
    );

    return rv;
}

MSC_RV msc_GetChallenge(
  MSCLPTokenConnection	pConnection,
  MSCPUChar8	        pSeed,
  MSCUShort16	        seedSize,
  MSCPUChar8	        pRandomData,
  MSCUShort16	        randomDataSize
)
{
    MSC_RV rv;

    rv = MSCGetChallenge(
      pConnection,
      pSeed,
      seedSize,
      pRandomData,
      randomDataSize
    );

    return rv;
}

MSC_RV msc_GetKeyAttributes( 
  MSCLPTokenConnection  pConnection, 
  MSCUChar8             keyNumber,
  MSCLPKeyInfo          pKeyInfo 
)
{
    MSC_RV rv;

    rv = MSCGetKeyAttributes( 
      pConnection, 
      keyNumber,
      pKeyInfo 
    );

    return rv;
}

MSC_RV msc_GetObjectAttributes( 
  MSCLPTokenConnection  pConnection, 
  MSCString             objectID,
  MSCLPObjectInfo       pObjectInfo 
)
{
    MSC_RV rv;

    rv = MSCGetObjectAttributes( 
      pConnection, 
      objectID,
      pObjectInfo 
    );

    return rv;
}

MSC_RV msc_ReadAllocateObject( 
  MSCLPTokenConnection  pConnection, 
  MSCString             objectID,
  MSCPUChar8*           pOutputData, 
  MSCPULong32           dataSize 
)
{
    MSC_RV rv;

    rv = MSCReadAllocateObject( 
      pConnection, 
      objectID,
      pOutputData, 
      dataSize,
      0, /* Fixme: rwCallback */
      0 /* Fixme: addParams */
    );

    return rv;
}


MSCUChar8 msc_IsTokenReset(MSCLPTokenConnection pConnection)
{
    MSCUChar8 rv;

    rv = MSCIsTokenReset(pConnection);

    return rv;
}

MSCUChar8 msc_ClearReset(MSCLPTokenConnection pConnection)
{
    MSCUChar8 rv;

    rv = MSCClearReset(pConnection);

    return rv;
}

MSCUChar8 msc_IsTokenMoved(MSCLPTokenConnection pConnection)
{
    MSCUChar8 rv;

    rv = MSCIsTokenMoved(pConnection);

    return rv;
}

MSCUChar8 msc_IsTokenChanged(MSCLPTokenConnection pConnection)
{
    MSCUChar8 rv;

    rv = MSCIsTokenChanged(pConnection);

    return rv;
}

MSCUChar8 msc_IsTokenKnown(MSCLPTokenConnection pConnection)
{
    MSCUChar8 rv;

    rv = MSCIsTokenKnown(pConnection);

    return rv;
}


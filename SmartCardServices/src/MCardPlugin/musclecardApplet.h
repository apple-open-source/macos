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
            Title  : musclecardApplet.h
            Package: Musclecard Plugin
            Author : David Corcoran
            Date   : 10/02/01
            License: Copyright (C) 2001 David Corcoran
                     <corcoran@linuxnet.com>
            Purpose: This abstracts the MUSCLE Card Edge Inteface
 
 
********************************************************************/

#ifndef __musclecardApplet_h__
#define __musclecardApplet_h__

#ifdef __cplusplus
  extern "C" {
#endif

// Data Locations admitted in ComputeCrypt()
#define DL_APDU          0x01
#define DL_OBJECT        0x02

/* Some useful offsets in the buffer */
#define OFFSET_CLA	0x00
#define OFFSET_INS	0x01
#define OFFSET_P1	0x02
#define OFFSET_P2	0x03
#define OFFSET_P3	0x04
#define OFFSET_LC       0x04
#define OFFSET_DATA	0x05

    // Import/Export Object ID
#define IN_OBJECT_ID   "0xFFFFFFFE"
#define OUT_OBJECT_ID  "0xFFFFFFFF"

    // code of CLA byte in the command APDU header
#define CardEdge_CLA   0xB0

    /****************************************
     *          Instruction codes           *
     ****************************************/

#define INS_WRITE_FRAMEWORK  0x2A
    // Keys' use and management
#define INS_MSC_GEN_KEYPAIR      0x30
#define INS_IMPORT_KEY           0x32
#define INS_EXPORT_KEY           0x34
#define INS_COMPUTE_CRYPT        0x36

    // External authentication
#define INS_CREATE_PIN      0x40
#define INS_VERIFY_PIN      0x42
#define INS_CHANGE_PIN      0x44
#define INS_UNBLOCK_PIN     0x46
#define INS_LOGOUT_ALL      0x60
#define INS_GET_CHALLENGE   0x62
#define INS_EXT_AUTH        0x38

    // Objects' use and management
#define INS_CREATE_OBJ      0x5A
#define INS_DELETE_OBJ      0x52
#define INS_READ_OBJ        0x56
#define INS_WRITE_OBJ       0x54

    // Status information
#define INS_LIST_OBJECTS    0x58
#define INS_LIST_PINS       0x48
#define INS_LIST_KEYS       0x3A
#define INS_GET_STATUS      0x3C

    /* Sizes of particular objects */
#define MSC_SIZEOF_OBJECTID               4
#define MSC_SIZEOF_OBJECTSIZE             4
#define MSC_SIZEOF_KEYINFO                11
#define MSC_SIZEOF_STATUS                 16
#define MSC_SIZEOF_VERSION                2
#define MSC_SIZEOF_FREEMEM                4
#define MSC_SIZEOF_LOGIDS                 2
#define MSC_SIZEOF_ADDINFO                8
#define MSC_SIZEOF_OPTLEN                 2
#define MSC_SIZEOF_GENOPTIONS             1
#define MSC_SIZEOF_KEYSIZE                2
#define MSC_SIZEOF_KEYNUMBER              1
#define MSC_SIZEOF_KEYTYPE                1
#define MSC_SIZEOF_KEYPARTNER             1
#define MSC_SIZEOF_CIPHERMODE             1
#define MSC_SIZEOF_CIPHERDIR              1
#define MSC_SIZEOF_CRYPTLEN               2
#define MSC_SIZEOF_ALGOTYPE               1
#define MSC_SIZEOF_IDUSED                 1
#define MSC_SIZEOF_OFFSET                 4
#define MSC_SIZEOF_ACLSTRUCT              6
#define MSC_SIZEOF_RWDATA                 1
#define MSC_SIZEOF_PINSIZE                1
#define MSC_SIZEOF_PINTRIES               1
#define MSC_SIZEOF_CIPHERMODE             1
#define MSC_SIZEOF_CIPHERDIR              1
#define MSC_SIZEOF_DATALOCATION           1
#define MSC_SIZEOF_ACLVALUE               2
#define MSC_SIZEOF_SEEDLENGTH             2
#define MSC_SIZEOF_RANDOMSIZE             2
#define MSC_SIZEOF_MINIACL                1

    /* Selects applet - Not to be used by applications */
#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCInitializePlugin( 
  MSCLPTokenConnection  pConnection
);

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCFinalizePlugin( 
  MSCLPTokenConnection  pConnection 
);

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCIdentifyToken( 
  MSCLPTokenConnection  pConnection 
);

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCWriteFramework( 
  MSCLPTokenConnection  pConnection, 
  MSCLPInitTokenParams  pInitParams 
);

    /*****************************************************************/
    /* Core Musclecard functions                                      */
    /* These functions coorespond directly to internal library        */
    /* functions.                                                     */
    /*****************************************************************/

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCGetStatus(
  MSCLPTokenConnection  pConnection, 
  MSCLPStatusInfo       pStatusInfo
);

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCGetCapabilities(
  MSCLPTokenConnection  pConnection,
  MSCULong32            Tag,
  MSCPUChar8            Value,
  MSCPULong32           Length
);

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCExtendedFeature( 
  MSCLPTokenConnection  pConnection, 
  MSCULong32            extFeature,
  MSCPUChar8            outData, 
  MSCULong32            outLength, 
  MSCPUChar8            inData,
  MSCPULong32           inLength 
);

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCGenerateKeys(
  MSCLPTokenConnection	pConnection,
  MSCUChar8		prvKeyNum,
  MSCUChar8		pubKeyNum,
  MSCLPGenKeyParams	pParams
);

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCImportKey(
  MSCLPTokenConnection	pConnection,
  MSCUChar8		keyNum,
  MSCLPKeyACL		pKeyACL,
  MSCPUChar8	        pKeyBlob,
  MSCULong32            keyBlobSize,
  MSCLPKeyPolicy        keyPolicy,
  MSCPVoid32		pAddParams,
  MSCUChar8		addParamsSize
);

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCExportKey(
  MSCLPTokenConnection	pConnection,
  MSCUChar8		keyNum,
  MSCPUChar8	        pKeyBlob,
  MSCPULong32           keyBlobSize,
  MSCPVoid32		pAddParams,
  MSCUChar8		addParamsSize
);

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCComputeCrypt( 
  MSCLPTokenConnection  pConnection,
  MSCLPCryptInit        cryptInit, 
  MSCPUChar8            pInputData,
  MSCULong32            inputDataSize, 
  MSCPUChar8            pOutputData,
  MSCPULong32           outputDataSize
);

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCExtAuthenticate(
  MSCLPTokenConnection	pConnection,
  MSCUChar8	        keyNum,
  MSCUChar8             cipherMode, 
  MSCUChar8             cipherDirection,
  MSCPUChar8	        pData,
  MSCULong32	        dataSize
);

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCListKeys(
  MSCLPTokenConnection	pConnection,
  MSCUChar8		seqOption,
  MSCLPKeyInfo		pKeyInfo
);

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCCreatePIN(
  MSCLPTokenConnection	pConnection,
  MSCUChar8		pinNum,
  MSCUChar8		pinAttempts,
  MSCPUChar8	        pPinCode,
  MSCULong32		pinCodeSize,
  MSCPUChar8	        pUnblockCode,
  MSCUChar8		unblockCodeSize
);

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCVerifyPIN(
  MSCLPTokenConnection	pConnection,
  MSCUChar8		pinNum,
  MSCPUChar8	        pPinCode,
  MSCULong32		pinCodeSize
);

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCChangePIN(
  MSCLPTokenConnection	pConnection,
  MSCUChar8		pinNum,
  MSCPUChar8	        pOldPinCode,
  MSCUChar8		oldPinCodeSize,
  MSCPUChar8	        pNewPinCode,
  MSCUChar8		newPinCodeSize
);

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCUnblockPIN(
  MSCLPTokenConnection	pConnection,
  MSCUChar8		pinNum,
  MSCPUChar8	        pUnblockCode,
  MSCULong32		unblockCodeSize
);

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCListPINs(
  MSCLPTokenConnection	pConnection,
  MSCPUShort16	        pPinBitMask
);

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCCreateObject(
  MSCLPTokenConnection	pConnection,
  MSCString 	        objectID,
  MSCULong32		objectSize,
  MSCLPObjectACL	pObjectACL
);

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCDeleteObject(
  MSCLPTokenConnection	pConnection,
  MSCString  		objectID,
  MSCUChar8		zeroFlag
);

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCWriteObject(
  MSCLPTokenConnection	pConnection,
  MSCString 		objectID,
  MSCULong32		offset,
  MSCPUChar8	        pInputData,
  MSCUChar8		dataSize
);

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCReadObject(
  MSCLPTokenConnection	pConnection,
  MSCString 		objectID,
  MSCULong32		offset,
  MSCPUChar8	        pOutputData,
  MSCUChar8		dataSize
);

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCListObjects(
  MSCLPTokenConnection	pConnection,
  MSCUChar8		seqOption,
  MSCLPObjectInfo	pObjectInfo
);

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCLogoutAll(
  MSCLPTokenConnection	pConnection
);

#ifdef WIN32
MCARDPLUGIN_API
#endif
MSC_RV PL_MSCGetChallenge(
  MSCLPTokenConnection	pConnection,
  MSCPUChar8	        pSeed,
  MSCUShort16	        seedSize,
  MSCPUChar8	        pRandomData,
  MSCUShort16	        randomDataSize
);

    /*****************************************************************/
    /* Extended Musclecard functions                                  */
    /* These functions do not coorespond to internal library funcions */
    /* but rather use them to provide some extended functionality.    */
    /*****************************************************************/

MSC_RV PL_MSCGetKeyAttributes( 
  MSCLPTokenConnection  pConnection, 
  MSCUChar8             keyNumber,
  MSCLPKeyInfo          pKeyInfo 
);

MSC_RV PL_MSCGetObjectAttributes( 
  MSCLPTokenConnection  pConnection, 
  MSCString             objectID,
  MSCLPObjectInfo       pObjectInfo 
);

MSC_RV PL_MSCWriteLargeObject( 
  MSCLPTokenConnection  pConnection, 
  MSCString             objectID,
  MSCPUChar8            pInputData, 
  MSCULong32            dataSize 
);

MSC_RV PL_MSCReadLargeObject( 
  MSCLPTokenConnection  pConnection, 
  MSCString             objectID,
  MSCPUChar8            pOutputData, 
  MSCULong32            dataSize 
);

MSC_RV PL_MSCReadAllocateObject( 
  MSCLPTokenConnection  pConnection, 
  MSCString             objectID,
  MSCPUChar8*           pOutputData, 
  MSCPULong32           dataSize 
);


#ifdef __cplusplus
  }
#endif

#endif /* __musclecardApplet_h__ */

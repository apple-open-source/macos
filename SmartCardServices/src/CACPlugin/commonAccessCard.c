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
 
            Title  : commonAccessCard.c
            Package: CAC Plugin
            Author : David Corcoran
            Date   : 02/06/02
            License: Copyright (C) 2002 David Corcoran
                     <corcoran@linuxnet.com>

            Purpose: This abstracts the CAC APDU's
	             into client side function calls.
 
********************************************************************/

#ifdef WIN32
#include "../win32/CACPlugin.h"
#endif

#ifndef __APPLE__
#include <musclecard.h>
#else
#include <PCSC/musclecard.h>
#endif
#include "commonAccessCard.h"
#include <zlib.h>
#include <string.h>
#include <stdio.h>

/* Define to turn on APDU debugging */
/* #define MSC_DEBUG 1 */

static int suppressResponse = 0;
static int tlvsCached       = 0;

static MSCUShort16 pntbSize = 0;
static MSCUShort16 pnvbSize = 0;
static MSCUShort16 pltbSize = 0;
static MSCUShort16 plvbSize = 0;
static MSCUShort16 bstbSize = 0;
static MSCUShort16 bsvbSize = 0;
static MSCUShort16 obtbSize = 0;
static MSCUShort16 obvbSize = 0;

typedef struct {
  MSCUChar8  pBuffer[MAX_BUFFER_SIZE];
  MSCULong32 bufferSize;
  MSCUChar8  apduResponse[MAX_BUFFER_SIZE];
  MSCULong32 apduResponseSize;
  LPSCARD_IO_REQUEST ioType;
} MSCTransmitBuffer, *MSCLPTransmitBuffer;

MSC_RV convertPCSC( MSCLong32 );
MSCUShort16 convertSW( MSCPUChar8 );
void MemCopy16( MSCPUChar8, MSCPUShort16 );
void MemCopy32( MSCPUChar8, MSCPULong32 );
void MemCopyTo16( MSCPUShort16, MSCPUChar8 );
void MemCopyTo32( MSCPULong32, MSCPUChar8 );
MSCUShort16 getUShort16( MSCPUChar8 );
void setUShort16( MSCPUChar8, MSCUShort16 );

MSCLong32 SCardExchangeAPDU( MSCLPTokenConnection pConnection, 
			                 MSCLPTransmitBuffer transmitBuffer );
  
#define CAC_IDCERT_OBJID            "C7"
#define CAC_ECRYCERT_OBJID          "C3"
#define CAC_ESIGCERT_OBJID          "C5"
#define CAC_IDCERTATT_OBJID         "c7"
#define CAC_ECRYCERTATT_OBJID       "c3"
#define CAC_ESIGCERTATT_OBJID       "c5"
#define CAC_IDKEYATT_OBJID          "k7"
#define CAC_ECRYKEYATT_OBJID        "k3"
#define CAC_ESIGKEYATT_OBJID        "k5"

#define CAC_PNTB_OBJID             "PNTB"
#define CAC_PNVB_OBJID             "PNVB"

#define CAC_PLTB_OBJID             "PLTB"
#define CAC_PLVB_OBJID             "PLVB"

#define CAC_BSTB_OBJID             "BSTB"
#define CAC_BSVB_OBJID             "BSVB"

#define CAC_OBTB_OBJID             "OBTB"
#define CAC_OBVB_OBJID             "OBVB"

#define CAC_LABEL_ID               "Identification"
#define CAC_LABEL_ESIG             "Email Certificate"
#define CAC_LABEL_ECRY             "Email Certificate"
#define CAC_MAXSIZE_CERT           4000
#define CAC_MAXSIZE_SIGNDATA       0x80
#define CAC_KEYNUM_ID                 7
#define CAC_KEYNUM_ESIG               5
#define CAC_KEYNUM_ECRY               3


/* ID Applet */
MSCUChar8 CAC_APPLET_ID_AID[7]       = 
{0xA0, 0x00, 0x00, 0x00, 0x79, 0x03, 0x00}; 
/* PKI ID Instance */
MSCUChar8 CAC_APPLET_PKI_ID_AID[7]   = 
{0xA0, 0x00, 0x00, 0x00, 0x79, 0x01, 0x00}; 
/* PKI Email Signature Instance */
MSCUChar8 CAC_APPLET_PKI_ESIG_AID[7] = 
{0xA0, 0x00, 0x00, 0x00, 0x79, 0x01, 0x01}; 
/* PKI Email Encryption Instance */
MSCUChar8 CAC_APPLET_PKI_ECRY_AID[7] = 
{0xA0, 0x00, 0x00, 0x00, 0x79, 0x01, 0x02}; 
MSCUChar8 CAC_APPLET_CONT_PN_AID[7] = 
{0xA0, 0x00, 0x00, 0x00, 0x79, 0x02, 0x00}; 
MSCUChar8 CAC_APPLET_CONT_PL_AID[7] = 
{0xA0, 0x00, 0x00, 0x00, 0x79, 0x02, 0x01}; 
MSCUChar8 CAC_APPLET_CONT_BS_AID[7] = 
{0xA0, 0x00, 0x00, 0x00, 0x79, 0x02, 0x02}; 
MSCUChar8 CAC_APPLET_CONT_OB_AID[7] = 
{0xA0, 0x00, 0x00, 0x00, 0x79, 0x02, 0x03};

static MSCULong32 cacIDCertAttrSize     = 48;
static MSCULong32 cacECryptCertAttrSize = 45;
static MSCULong32 cacESignCertAttrSize  = 45;
static MSCULong32 cacIDKeyAttrSize      = 245;
static MSCULong32 cacECryptKeyAttrSize  = 245;
static MSCULong32 cacESignKeyAttrSize   = 245;

static MSCUChar8 cacIDCertAttr[48] = {
  0x00, 0x63, 0x37, 0x00, 0x00, 0x00, 0x29,
  0x00, 0x00, 0x00, 0x80, 0x00, 0x04,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
  0x01, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x08,
  'I', 'd', 'e', 'n', 't', 'i', 't', 'y',
  0x00, 0x00, 0x01, 0x02, 0x00, 0x01,
  CAC_KEYNUM_ID
};

static MSCUChar8 cacESignCertAttr[45] = {
  0x00, 0x63, 0x35, 0x00, 0x00, 0x00, 0x26,
  0x00, 0x00, 0x00, 0x80, 0x00, 0x04,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
  0x01, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x05,
  'E', 'm', 'a', 'i', 'l',
  0x00, 0x00, 0x01, 0x02, 0x00, 0x01,
  CAC_KEYNUM_ESIG
};

static MSCUChar8 cacECryptCertAttr[45] = {
  0x00, 0x63, 0x33, 0x00, 0x00, 0x00, 0x26,
  0x00, 0x00, 0x00, 0x80, 0x00, 0x04,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x04,
  0x01, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x03, 0x00, 0x05,
  'E', 'm', 'a', 'i', 'l',
  0x00, 0x00, 0x01, 0x02, 0x00, 0x01,
  CAC_KEYNUM_ECRY
};


static MSCUChar8 cacIDKeyAttr[245] = {
  0x00,               /* Object type */
  0x6B, 0x37,         /* Object id   */
  0x00, 0x00,         /* Next id     */
  0x00, 0xEE,         /* Data len    */

  0x00, 0x00, 0x00, 0x00, 0x00, 0x04,  /* CKA_CLASS                */
  0x03, 0x00, 0x00, 0x00,              /* Value                    */

  0x00, 0x00, 0x01, 0x02, 0x00, 0x01,  /* CKA_ID                   */
  CAC_KEYNUM_ID,                       /* Value                    */

  0x00, 0x00, 0x01, 0x62, 0x00, 0x01,  /* CKA_EXTRACTABLE          */
  0x00,                                /* Value                    */
  0x00, 0x00, 0x01, 0x09, 0x00, 0x01,  /* CKA_SIGN_RECOVER         */
  0x00,                                /* Value                    */
  0x00, 0x00, 0x01, 0x0C, 0x00, 0x01,  /* CKA_DERIVE               */
  0x00,                                /* Value                    */
  0x00, 0x00, 0x01, 0x70, 0x00, 0x01,  /* CKA_MODIFIABLE           */
  0x00,                                /* Value                    */
  0x00, 0x00, 0x01, 0x07, 0x00, 0x01,  /* CKA_UNWRAP               */
  0x01,                                /* Value                    */
  0x00, 0x00, 0x01, 0x05, 0x00, 0x01,  /* CKA_DECRYPT              */
  0x01,                                /* Value                    */
  0x00, 0x00, 0x00, 0x02, 0x00, 0x01,  /* CKA_PRIVATE              */
  0x01,                                /* Value                    */
  0x00, 0x00, 0x01, 0x08, 0x00, 0x01,  /* CKA_SIGN                 */
  0x01,                                /* Value                    */
  0x00, 0x00, 0x01, 0x64, 0x00, 0x01,  /* CKA_NEVER_EXTRACTABLE    */
  0x01,                                /* Value                    */
  0x00, 0x00, 0x01, 0x65, 0x00, 0x01,  /* CKA_ALWAYS_SENSITIVE     */
  0x01,                                /* Value                    */
  0x00, 0x00, 0x01, 0x03, 0x00, 0x01,  /* CKA_SENSITIVE            */
  0x01,

  0x00, 0x00, 0x01, 0x00, 0x00, 0x04,  /* CKA_KEY_TYPE             */
  0x00, 0x00, 0x00, 0x00,              /* Value                    */

  0x00, 0x00, 0x01, 0x20, 0x00, 0x80,  /* CKA_MODULUS              */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

};

static MSCUChar8 cacECryptKeyAttr[245] = {
  0x00,               /* Object type */
  0x6B, 0x33,         /* Object id   */
  0x00, 0x00,         /* Next id     */
  0x00, 0xEE,         /* Data len    */

  0x00, 0x00, 0x00, 0x00, 0x00, 0x04,  /* CKA_CLASS                */
  0x03, 0x00, 0x00, 0x00,              /* Value                    */

  0x00, 0x00, 0x01, 0x02, 0x00, 0x01,  /* CKA_ID                   */
  CAC_KEYNUM_ECRY,                     /* Value                    */

  0x00, 0x00, 0x01, 0x62, 0x00, 0x01,  /* CKA_EXTRACTABLE          */
  0x00,                                /* Value                    */
  0x00, 0x00, 0x01, 0x09, 0x00, 0x01,  /* CKA_SIGN_RECOVER         */
  0x00,                                /* Value                    */
  0x00, 0x00, 0x01, 0x0C, 0x00, 0x01,  /* CKA_DERIVE               */
  0x00,                                /* Value                    */
  0x00, 0x00, 0x01, 0x70, 0x00, 0x01,  /* CKA_MODIFIABLE           */
  0x00,                                /* Value                    */
  0x00, 0x00, 0x01, 0x07, 0x00, 0x01,  /* CKA_UNWRAP               */
  0x01,                                /* Value                    */
  0x00, 0x00, 0x01, 0x05, 0x00, 0x01,  /* CKA_DECRYPT              */
  0x01,                                /* Value                    */
  0x00, 0x00, 0x00, 0x02, 0x00, 0x01,  /* CKA_PRIVATE              */
  0x01,                                /* Value                    */
  0x00, 0x00, 0x01, 0x08, 0x00, 0x01,  /* CKA_SIGN                 */
  0x01,                                /* Value                    */
  0x00, 0x00, 0x01, 0x64, 0x00, 0x01,  /* CKA_NEVER_EXTRACTABLE    */
  0x01,                                /* Value                    */
  0x00, 0x00, 0x01, 0x65, 0x00, 0x01,  /* CKA_ALWAYS_SENSITIVE     */
  0x01,                                /* Value                    */
  0x00, 0x00, 0x01, 0x03, 0x00, 0x01,  /* CKA_SENSITIVE            */
  0x01,

  0x00, 0x00, 0x01, 0x00, 0x00, 0x04,  /* CKA_KEY_TYPE             */
  0x00, 0x00, 0x00, 0x00,              /* Value                    */

  0x00, 0x00, 0x01, 0x20, 0x00, 0x80,  /* CKA_MODULUS              */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static MSCUChar8 cacESignKeyAttr[245] = {
  0x00,               /* Object type */
  0x6B, 0x35,         /* Object id   */
  0x00, 0x00,         /* Next id     */
  0x00, 0xEE,         /* Data len    */

  0x00, 0x00, 0x00, 0x00, 0x00, 0x04,  /* CKA_CLASS                */
  0x03, 0x00, 0x00, 0x00,              /* Value                    */

  0x00, 0x00, 0x01, 0x02, 0x00, 0x01,  /* CKA_ID                   */
  CAC_KEYNUM_ESIG,                     /* Value                    */

  0x00, 0x00, 0x01, 0x62, 0x00, 0x01,  /* CKA_EXTRACTABLE          */
  0x00,                                /* Value                    */
  0x00, 0x00, 0x01, 0x09, 0x00, 0x01,  /* CKA_SIGN_RECOVER         */
  0x00,                                /* Value                    */
  0x00, 0x00, 0x01, 0x0C, 0x00, 0x01,  /* CKA_DERIVE               */
  0x00,                                /* Value                    */
  0x00, 0x00, 0x01, 0x70, 0x00, 0x01,  /* CKA_MODIFIABLE           */
  0x00,                                /* Value                    */
  0x00, 0x00, 0x01, 0x07, 0x00, 0x01,  /* CKA_UNWRAP               */
  0x01,                                /* Value                    */
  0x00, 0x00, 0x01, 0x05, 0x00, 0x01,  /* CKA_DECRYPT              */
  0x01,                                /* Value                    */
  0x00, 0x00, 0x00, 0x02, 0x00, 0x01,  /* CKA_PRIVATE              */
  0x01,                                /* Value                    */
  0x00, 0x00, 0x01, 0x08, 0x00, 0x01,  /* CKA_SIGN                 */
  0x01,                                /* Value                    */
  0x00, 0x00, 0x01, 0x64, 0x00, 0x01,  /* CKA_NEVER_EXTRACTABLE    */
  0x01,                                /* Value                    */
  0x00, 0x00, 0x01, 0x65, 0x00, 0x01,  /* CKA_ALWAYS_SENSITIVE     */
  0x01,                                /* Value                    */
  0x00, 0x00, 0x01, 0x03, 0x00, 0x01,  /* CKA_SENSITIVE            */
  0x01,

  0x00, 0x00, 0x01, 0x00, 0x00, 0x04,  /* CKA_KEY_TYPE             */
  0x00, 0x00, 0x00, 0x00,              /* Value                    */

  0x00, 0x00, 0x01, 0x20, 0x00, 0x80,  /* CKA_MODULUS              */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static MSCUChar8 cacIDCert[CAC_MAXSIZE_CERT];
static MSCUChar8 cacESignCert[CAC_MAXSIZE_CERT];
static MSCUChar8 cacECryptCert[CAC_MAXSIZE_CERT];
static MSCUChar8 cacCompBuffer[CAC_MAXSIZE_CERT];
static MSCUShort16 cacIDCertSize     = 0;
static MSCUShort16 cacESignCertSize  = 0;
static MSCUShort16 cacECryptCertSize = 0;
static MSCUChar8 dataIsCached        = 0;


MSC_RV CACLoadAndCacheData(MSCLPTokenConnection pConnection);

MSC_RV CACLoadTLVSize(MSCLPTokenConnection pConnection, 
		      MSCPUChar8 demoPointer,
		      MSCULong32 demoSize,
		      MSCPUShort16 tlvSize,
		      MSCUChar8 tlvType);

MSC_RV CACLoadCertificate(MSCLPTokenConnection pConnection, MSCPUChar8 cert,
			  MSCPUShort16 certSize);

MSC_RV CACSelectInstance(MSCLPTokenConnection pConnection, MSCPUChar8 aid,
			 MSCULong32 aidLength);

#ifdef WIN32
CACPLUGIN_API
#endif
MSC_RV PL_MSCWriteFramework( MSCLPTokenConnection pConnection,
			     MSCLPInitTokenParams pInitParams ) {

  return MSC_UNSUPPORTED_FEATURE;
}

#ifdef WIN32
CACPLUGIN_API
#endif
MSC_RV PL_MSCGetStatus( MSCLPTokenConnection pConnection, 
			MSCLPStatusInfo pStatusInfo ) {

  pStatusInfo->appVersion  = 0xFF;
  pStatusInfo->swVersion   = 0xFF;
  pStatusInfo->freeMemory  = 0x00;
  pStatusInfo->totalMemory = 0x00;
  pStatusInfo->usedPINs    = 1;
  pStatusInfo->usedKeys    = 3;
  pStatusInfo->loggedID    = pConnection->loggedIDs;;


  return MSC_SUCCESS;
}

#ifdef WIN32
CACPLUGIN_API
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
    ulValue = MSC_SUPPORT_COMPUTECRYPT | MSC_SUPPORT_LISTKEYS |
      MSC_SUPPORT_VERIFYPIN | MSC_SUPPORT_LISTPINS | MSC_SUPPORT_READOBJECT |
      MSC_SUPPORT_LISTOBJECTS | MSC_SUPPORT_GETCHALLENGE | 
      MSC_SUPPORT_CHANGEPIN;
    tagType = 4;
    break;

  case MSC_TAG_SUPPORT_CRYPTOALG:
    ulValue = MSC_SUPPORT_RSA;
    tagType = 4;
    break;

  case MSC_TAG_CAPABLE_RSA:
    ulValue = MSC_CAPABLE_RSA_1024 | MSC_CAPABLE_RSA_NOPAD;
    tagType = 4;
    break;

  case MSC_TAG_CAPABLE_OBJ_ATTR:
    ulValue = 0;
    tagType = 4;
    break;

  case MSC_TAG_CAPABLE_OBJ_IDSIZE:
    ucValue = 16;
    tagType = 1;
    break;

  case MSC_TAG_CAPABLE_OBJ_AUTH:
    usValue = MSC_AUT_NONE;
    tagType = 2;
    break;

  case MSC_TAG_CAPABLE_OBJ_MAXNUM:
    ulValue = 17;
    tagType = 4;
    break;

  case MSC_TAG_CAPABLE_PIN_ATTR:
    ulValue = 0;
    tagType = 4;
    break;

  case MSC_TAG_CAPABLE_PIN_MAXNUM:
    ucValue = 1;
    tagType = 1;
    break;

  case MSC_TAG_CAPABLE_PIN_MINSIZE:
    ucValue = 8;
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
    ucValue = 8;
    tagType = 1;
    break;

  case MSC_TAG_CAPABLE_RANDOM_MIN:
    ucValue = 8;
    tagType = 1;
    break;

  case MSC_TAG_CAPABLE_KEY_AUTH:
    usValue = MSC_AUT_NONE;
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
CACPLUGIN_API
#endif
MSC_RV PL_MSCExtendedFeature( MSCLPTokenConnection pConnection, 
			      MSCULong32 extFeature,
			      MSCPUChar8 outData, MSCULong32 outLength, 
			      MSCPUChar8 inData, MSCPULong32 inLength ) {

  return MSC_UNSUPPORTED_FEATURE;
}

#ifdef WIN32
CACPLUGIN_API
#endif
MSC_RV PL_MSCGenerateKeys( MSCLPTokenConnection pConnection, 
			   MSCUChar8 prvKeyNum, MSCUChar8 pubKeyNum, 
			   MSCLPGenKeyParams pParams ) {
 
  return MSC_UNSUPPORTED_FEATURE;
}

#ifdef WIN32
CACPLUGIN_API
#endif
MSC_RV PL_MSCImportKey( MSCLPTokenConnection pConnection, MSCUChar8 keyNum,
                        MSCLPKeyACL pKeyACL, MSCPUChar8 pKeyBlob, 
			MSCULong32 keyBlobSize, MSCLPKeyPolicy keyPolicy, 
			MSCPVoid32 pAddParams, MSCUChar8 addParamsSize ) {

  return MSC_UNSUPPORTED_FEATURE;
}
 
#ifdef WIN32
CACPLUGIN_API
#endif
MSC_RV PL_MSCExportKey( MSCLPTokenConnection pConnection, MSCUChar8 keyNum,
			MSCPUChar8 pKeyBlob, MSCPULong32 keyBlobSize, 
			MSCPVoid32 pAddParams, MSCUChar8 addParamsSize ) {

  return MSC_UNSUPPORTED_FEATURE;
}

#ifdef WIN32
CACPLUGIN_API
#endif
MSC_RV PL_MSCComputeCrypt( MSCLPTokenConnection pConnection,
			   MSCLPCryptInit cryptInit, MSCPUChar8 pInputData,
			   MSCULong32 inputDataSize, MSCPUChar8 pOutputData,
			   MSCPULong32 outputDataSize ) {
  MSCLong32 rv;
  MSCPUChar8 txBuffer;
  MSCPUChar8 rxBuffer;
  MSCPUChar8 pkiPointer;
  MSCULong32 pkiSize;
  MSCTransmitBuffer tBuffer;

  rv = CACLoadAndCacheData(pConnection);
  if (rv != MSC_SUCCESS) return rv;

  if ( inputDataSize != CAC_MAXSIZE_SIGNDATA ) {
    return MSC_INVALID_PARAMETER;
  }

  if ( cryptInit->cipherMode != MSC_MODE_RSA_NOPAD ) {
    return MSC_INVALID_PARAMETER;
  }

  if ( cryptInit->cipherDirection != MSC_DIR_SIGN &&
       cryptInit->cipherDirection != MSC_DIR_ENCRYPT &&
	   cryptInit->cipherDirection != MSC_DIR_DECRYPT) {
    return MSC_INVALID_PARAMETER;
  }

  txBuffer = tBuffer.pBuffer;
  rxBuffer = tBuffer.apduResponse;

  if ( cryptInit->keyNum == CAC_KEYNUM_ID ) {
    if ( cacIDCertSize == 0 ) {
      return MSC_INVALID_PARAMETER;
    }
    
      pkiPointer      = CAC_APPLET_PKI_ID_AID;
      pkiSize         = sizeof(CAC_APPLET_PKI_ID_AID);

  } else if ( cryptInit->keyNum == CAC_KEYNUM_ESIG ) {
    if ( cacESignCertSize == 0 ) {
      return MSC_INVALID_PARAMETER;
    }

    pkiPointer      = CAC_APPLET_PKI_ESIG_AID;
    pkiSize         = sizeof(CAC_APPLET_PKI_ESIG_AID);

  } else if ( cryptInit->keyNum == CAC_KEYNUM_ECRY ) {
    if ( cacECryptCertSize == 0 ) {
      return MSC_INVALID_PARAMETER;
    }

    pkiPointer      = CAC_APPLET_PKI_ECRY_AID;
    pkiSize         = sizeof(CAC_APPLET_PKI_ECRY_AID);

  } else {
    return MSC_INVALID_PARAMETER;
  }

  /* Select the instance associated with this key */
  rv = CACSelectInstance( pConnection, pkiPointer, pkiSize );
  if ( rv != MSC_SUCCESS )
    return rv;

  tBuffer.bufferSize = inputDataSize + 5;

  /* Select AID */
  txBuffer[0] = 0x80; txBuffer[1] = 0x42;  txBuffer[2] = 0x00; 
  txBuffer[3] = 0x00; txBuffer[4] = inputDataSize;

  memcpy(&txBuffer[5], pInputData, inputDataSize);
  
  tBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
  rv = SCardExchangeAPDU(pConnection, &tBuffer);

  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( tBuffer.apduResponseSize != inputDataSize + 2 ) {
    return MSC_UNSPECIFIED_ERROR;
  } else if ( rxBuffer[tBuffer.apduResponseSize-2] != 0x90 ) {
    return MSC_UNSPECIFIED_ERROR;
  } 

  memcpy(pOutputData, rxBuffer, inputDataSize);
  *outputDataSize = inputDataSize;

  return MSC_SUCCESS;
}

#ifdef WIN32
CACPLUGIN_API
#endif
MSC_RV PL_MSCExtAuthenticate( MSCLPTokenConnection pConnection, 
			      MSCUChar8 keyNum, MSCUChar8 cipherMode, 
			      MSCUChar8 cipherDirection,
			      MSCPUChar8 pData, MSCULong32 dataSize ) {

  return MSC_UNSUPPORTED_FEATURE;
}

#ifdef WIN32
CACPLUGIN_API
#endif
MSC_RV PL_MSCListKeys( MSCLPTokenConnection pConnection, 
		       MSCUChar8 seqOption, MSCLPKeyInfo pKeyInfo ) {

  MSC_RV rv;
  static int seq = 0;

  rv = CACLoadAndCacheData(pConnection);
  if (rv != MSC_SUCCESS) return rv;

  if (seqOption == MSC_SEQUENCE_RESET) {
    seq = 0;
  }

  switch(seq) {

  case 0:
    if ( cacECryptCertSize != 0 ) {
      pKeyInfo->keySize = 1024;
      pKeyInfo->keyNum  = CAC_KEYNUM_ECRY;
      pKeyInfo->keyType = MSC_KEY_RSA_PRIVATE;
      break;
    } else {
      seq += 1;
    }
  case 1:
    if ( cacESignCertSize != 0 ) {
      pKeyInfo->keySize = 1024;
      pKeyInfo->keyNum  = CAC_KEYNUM_ESIG;
      pKeyInfo->keyType = MSC_KEY_RSA_PRIVATE;
      break;
    } else {
      seq += 1;
    }
  case 2:
    if ( cacIDCertSize != 0 ) {
      pKeyInfo->keySize = 1024;
      pKeyInfo->keyNum  = CAC_KEYNUM_ID;
      pKeyInfo->keyType = MSC_KEY_RSA_PRIVATE;
      break;
    } else {
      seq += 1;
    }

  case 3:
    return MSC_SEQUENCE_END;

  }

  /* For keys, use only - pin verification needed, no write */
  pKeyInfo->keyACL.readPermission  = MSC_AUT_NONE;
  pKeyInfo->keyACL.writePermission = MSC_AUT_NONE;
  pKeyInfo->keyACL.usePermission   = MSC_AUT_PIN_1;
  pKeyInfo->keyPolicy.cipherMode   = MSC_KEYPOLICY_MODE_RSA_NOPAD;
  pKeyInfo->keyPolicy.cipherDirection = MSC_KEYPOLICY_DIR_SIGN | MSC_KEYPOLICY_DIR_VERIFY |
    MSC_KEYPOLICY_DIR_ENCRYPT | MSC_KEYPOLICY_DIR_DECRYPT;  

  seq += 1;

  return MSC_SUCCESS;
}

#ifdef WIN32
CACPLUGIN_API
#endif
MSC_RV PL_MSCCreatePIN( MSCLPTokenConnection pConnection, MSCUChar8 pinNum,
			MSCUChar8 pinAttempts, MSCPUChar8 pPinCode,
			MSCULong32 pinCodeSize, MSCPUChar8 pUnblockCode,
			MSCUChar8 unblockCodeSize ) {

  return MSC_UNSUPPORTED_FEATURE;
}

#ifdef WIN32
CACPLUGIN_API
#endif
MSC_RV PL_MSCVerifyPIN( MSCLPTokenConnection pConnection, MSCUChar8 pinNum,
			MSCPUChar8 pPinCode, MSCULong32 pinCodeSize ) {
  
  MSCLong32 rv;
  MSCPUChar8 txBuffer;
  MSCPUChar8 rxBuffer;
  MSCUChar8 pinPad[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  MSCTransmitBuffer tBuffer;
  
  rv = CACLoadAndCacheData(pConnection);
  if (rv != MSC_SUCCESS) return rv;  
  
  txBuffer = tBuffer.pBuffer;
  rxBuffer = tBuffer.apduResponse;
  
  txBuffer[OFFSET_CLA] = 0x80;
  txBuffer[OFFSET_INS] = 0x20;
  txBuffer[OFFSET_P1]  = 0x00;
  txBuffer[OFFSET_P2]  = 0x00;
  txBuffer[OFFSET_P3]  = 0x08;
  
  if ( pinNum != 1 ) {
    return MSC_INVALID_PARAMETER;
  }

  if ( pinCodeSize > 8 ) {
    return MSC_INVALID_PARAMETER;
  }

  memcpy(pinPad, pPinCode, pinCodeSize);

#ifdef CAC_PROTECTED_MODE
  printf("Protected Mode: Injecting default pin\n");
  memcpy(&txBuffer[OFFSET_DATA], "77777777", 8);
  tBuffer.bufferSize = 8 + 5;
#else
  memcpy(&txBuffer[OFFSET_DATA], pinPad, 8);
  tBuffer.bufferSize = 8 + 5;
#endif

  tBuffer.apduResponseSize = MAX_BUFFER_SIZE;
  rv = SCardExchangeAPDU(pConnection, &tBuffer);
  
  if(rv != SCARD_S_SUCCESS) {
    return convertPCSC(rv);
  }

  if(tBuffer.apduResponseSize == 2) {
    if (rxBuffer[0] == 0x90 && rxBuffer[1] == 0x00 ) {
      pConnection->loggedIDs |= MSC_AUT_PIN_1;
      return MSC_SUCCESS;
    } else if ( rxBuffer[0] == 0x63 ) {
      pConnection->loggedIDs = 0;
      return MSC_AUTH_FAILED;
    } else {
      pConnection->loggedIDs = 0;
      return convertSW(rxBuffer);
    }
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }

}

#ifdef WIN32
CACPLUGIN_API
#endif
MSC_RV PL_MSCChangePIN( MSCLPTokenConnection pConnection, MSCUChar8 pinNum,
			MSCPUChar8 pOldPinCode, MSCUChar8 oldPinCodeSize,
			MSCPUChar8 pNewPinCode, MSCUChar8 newPinCodeSize ) {

  MSCLong32 rv;
  MSCPUChar8 txBuffer;
  MSCPUChar8 rxBuffer;
  MSCUChar8 pinPad[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  MSCUChar8 newPinPad[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  MSCTransmitBuffer tBuffer;
  
    /* Select the instance associated with this key */
  rv = CACSelectInstance( pConnection, CAC_APPLET_ID_AID, 7 );
  if ( rv != MSC_SUCCESS ) {
    return rv;
  }
 
  txBuffer = tBuffer.pBuffer;
  rxBuffer = tBuffer.apduResponse;
  
  txBuffer[OFFSET_CLA] = 0x80;
  txBuffer[OFFSET_INS] = 0x24;
  txBuffer[OFFSET_P1]  = 0x01;
  txBuffer[OFFSET_P2]  = 0x00;
  txBuffer[OFFSET_P3]  = 0x10;
  
  if ( pinNum != 1 ) {
    return MSC_INVALID_PARAMETER;
  }

  if ( oldPinCodeSize > 8 || newPinCodeSize > 8 ||
       oldPinCodeSize < 4 || newPinCodeSize < 4 ) {
    return MSC_INVALID_PARAMETER;
  }

  memcpy(pinPad, pOldPinCode, oldPinCodeSize);
  memcpy(newPinPad, pNewPinCode, newPinCodeSize);

  memcpy(&txBuffer[OFFSET_DATA], pinPad, 8);
  memcpy(&txBuffer[OFFSET_DATA+8], newPinPad, 8);
 
  tBuffer.bufferSize = 16 + 5;
  tBuffer.apduResponseSize = MAX_BUFFER_SIZE;
    
  rv = SCardExchangeAPDU(pConnection, &tBuffer);
  
  if(rv != SCARD_S_SUCCESS) {
    return convertPCSC(rv);
  }

  if(tBuffer.apduResponseSize == 2) {
    if (rxBuffer[0] == 0x90 && rxBuffer[1] == 0x00 ) {
      return MSC_SUCCESS;
    } else if ( rxBuffer[0] == 0x63 ) {
      return MSC_AUTH_FAILED;
    } else {
      return convertSW(rxBuffer);
    }
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }
}

#ifdef WIN32
CACPLUGIN_API
#endif
MSC_RV PL_MSCUnblockPIN( MSCLPTokenConnection pConnection, MSCUChar8 pinNum,
			 MSCPUChar8 pUnblockCode, 
			 MSCULong32 unblockCodeSize ) {

  return MSC_UNSUPPORTED_FEATURE;
}

#ifdef WIN32
CACPLUGIN_API
#endif
MSC_RV PL_MSCListPINs( MSCLPTokenConnection pConnection, 
		       MSCPUShort16 pPinBitMask ) {

  
  *pPinBitMask = 2;  /* Just one pin on CAC */
  return MSC_SUCCESS;
}

#ifdef WIN32
CACPLUGIN_API
#endif
MSC_RV PL_MSCCreateObject( MSCLPTokenConnection pConnection, 
			   MSCString objectID, MSCULong32 objectSize, 
			   MSCLPObjectACL pObjectACL ) {

  return MSC_UNSUPPORTED_FEATURE;
}

#ifdef WIN32
CACPLUGIN_API
#endif
MSC_RV PL_MSCDeleteObject( MSCLPTokenConnection pConnection, 
			   MSCString objectID, MSCUChar8 zeroFlag ) {

  return MSC_UNSUPPORTED_FEATURE;
}

#ifdef WIN32
CACPLUGIN_API
#endif
MSC_RV PL_MSCWriteObject( MSCLPTokenConnection pConnection, 
			  MSCString objectID, MSCULong32 offset, 
			  MSCPUChar8 pInputData, MSCUChar8 dataSize ) {

  return MSC_UNSUPPORTED_FEATURE;
}

#ifdef WIN32
CACPLUGIN_API
#endif
MSC_RV PL_MSCReadObject( MSCLPTokenConnection pConnection, MSCString objectID, 
			 MSCULong32 offset, MSCPUChar8 pOutputData, 
			 MSCUChar8 dataSize ) {

  MSCLong32 rv;
  MSCPUChar8 demoPointer;
  MSCULong32 demoSize;
  MSCPUChar8 itemValue;
  MSCULong32 itemSize;
  MSCPUChar8 txBuffer;
  MSCPUChar8 rxBuffer;
  MSCTransmitBuffer tBuffer;
  MSCUChar8 readLocation;

  demoSize     = 0; demoPointer = 0;
  itemValue    = 0; itemSize    = 0;
  readLocation = 0;

  txBuffer = tBuffer.pBuffer;
  rxBuffer = tBuffer.apduResponse;

  rv = CACLoadAndCacheData(pConnection);
  if (rv != MSC_SUCCESS) return rv;

  if ( strcmp(objectID, CAC_IDCERT_OBJID) == 0 ) {
      if ( cacIDCertSize == 0 ) {
      return MSC_INVALID_PARAMETER;
    }

    itemValue = cacIDCert;
    itemSize  = cacIDCertSize;

  } else if ( strcmp(objectID, CAC_ECRYCERT_OBJID) == 0 ) {
    if ( cacECryptCertSize == 0 ) {
      return MSC_INVALID_PARAMETER;
    }

    itemValue = cacECryptCert;
    itemSize  = cacECryptCertSize;

  } else if ( strcmp(objectID, CAC_ESIGCERT_OBJID) == 0 ) {
    if ( cacESignCertSize == 0 ) {
      return MSC_INVALID_PARAMETER;
    }

    itemValue = cacESignCert;
    itemSize  = cacESignCertSize;

  } else if ( strcmp(objectID, CAC_IDCERTATT_OBJID) == 0 ) {
    if ( cacIDCertSize == 0 ) {
      return MSC_INVALID_PARAMETER;
    }
  
    itemValue = cacIDCertAttr;
    itemSize  = cacIDCertAttrSize;

  } else if ( strcmp(objectID, CAC_ECRYCERTATT_OBJID) == 0 ) {
    if ( cacECryptCertSize == 0 ) {
      return MSC_INVALID_PARAMETER;
    }

    itemValue = cacECryptCertAttr;
    itemSize  = cacECryptCertAttrSize;

  } else if ( strcmp(objectID, CAC_ESIGCERTATT_OBJID) == 0 ) {
    if ( cacESignCertSize == 0 ) {
      return MSC_INVALID_PARAMETER;
    }
  
    itemValue = cacESignCertAttr;
    itemSize  = cacESignCertAttrSize;

  } else if ( strcmp(objectID, CAC_IDKEYATT_OBJID) == 0 ) {
    if ( cacIDCertSize == 0 ) {
      return MSC_INVALID_PARAMETER;
    }

    itemValue = cacIDKeyAttr;
    itemSize  = cacIDKeyAttrSize;

  } else if ( strcmp(objectID, CAC_ECRYKEYATT_OBJID) == 0 ) {
    if ( cacECryptCertSize == 0 ) {
      return MSC_INVALID_PARAMETER;
    }

    itemValue = cacECryptKeyAttr;
    itemSize  = cacECryptKeyAttrSize;

  } else if ( strcmp(objectID, CAC_ESIGKEYATT_OBJID) == 0 ) {
    if ( cacESignCertSize == 0 ) {
      return MSC_INVALID_PARAMETER;
    }

    itemValue = cacESignKeyAttr;
    itemSize  = cacESignKeyAttrSize;

  /* For the tags */
  } else if ( strcmp(objectID, CAC_PNTB_OBJID) == 0 ) {
    itemSize    = pntbSize;
    demoPointer = CAC_APPLET_CONT_PN_AID;
    demoSize    = sizeof(CAC_APPLET_CONT_PN_AID);
    readLocation = 1; /* T-Buffer */
  } else if ( strcmp(objectID, CAC_PLTB_OBJID) == 0 ) {
    itemSize    = pltbSize;
    demoPointer = CAC_APPLET_CONT_PL_AID;
    demoSize    = sizeof(CAC_APPLET_CONT_PL_AID);
    readLocation = 1; /* T-Buffer */
  } else if ( strcmp(objectID, CAC_BSTB_OBJID) == 0 ) {
    itemSize    = bstbSize;
    demoPointer = CAC_APPLET_CONT_BS_AID;
    demoSize    = sizeof(CAC_APPLET_CONT_BS_AID);
    readLocation = 1; /* T-Buffer */
  } else if ( strcmp(objectID, CAC_OBTB_OBJID) == 0 ) {
    itemSize    = obtbSize;
    demoPointer = CAC_APPLET_CONT_OB_AID;
    demoSize    = sizeof(CAC_APPLET_CONT_OB_AID);
    readLocation = 1; /* T-Buffer */

    /* For the values */
  } else if ( strcmp(objectID, CAC_PNVB_OBJID) == 0 ) {
    itemSize    = pnvbSize;
    demoPointer = CAC_APPLET_CONT_PN_AID;
    demoSize    = sizeof(CAC_APPLET_CONT_PN_AID);
    readLocation = 2; /* V-Buffer */
  } else if ( strcmp(objectID, CAC_PLVB_OBJID) == 0 ) {
    itemSize    = plvbSize;
    demoPointer = CAC_APPLET_CONT_PL_AID;
    demoSize    = sizeof(CAC_APPLET_CONT_PL_AID);
    readLocation = 2; /* V-Buffer */
  } else if ( strcmp(objectID, CAC_BSVB_OBJID) == 0 ) {
    itemSize    = bsvbSize;  
    demoPointer = CAC_APPLET_CONT_BS_AID;
    demoSize    = sizeof(CAC_APPLET_CONT_BS_AID);
    readLocation = 2; /* V-Buffer */
  } else if ( strcmp(objectID, CAC_OBVB_OBJID) == 0 ) {
    itemSize    = obvbSize;
    demoPointer = CAC_APPLET_CONT_OB_AID;
    demoSize    = sizeof(CAC_APPLET_CONT_OB_AID);
    readLocation = 2; /* V-Buffer */

  } else {
    return MSC_OBJECT_NOT_FOUND;
  }
  
  if (itemSize == 0) {
    return MSC_INVALID_PARAMETER;
  }
  
  if ( (dataSize + offset) > itemSize ) {
    return MSC_INVALID_PARAMETER;
  }
  
  /* Must be V-Data - this is not cached */
  if ( readLocation == 1 || readLocation == 2 ) {
    /* Select the correct applet */
    rv = CACSelectInstance( pConnection, demoPointer, demoSize );
    if ( rv != MSC_SUCCESS )
      return rv;
      
    txBuffer[OFFSET_CLA]    = 0x80;
    txBuffer[OFFSET_INS]    = 0x52;
    txBuffer[OFFSET_P1]     = (offset/256);
    txBuffer[OFFSET_P2]     = (offset%256);
    txBuffer[OFFSET_P3]     = 0x02;
    txBuffer[OFFSET_DATA]   = readLocation;
    txBuffer[OFFSET_DATA+1] = dataSize;
  
    tBuffer.bufferSize = 7;
    tBuffer.apduResponseSize = MAX_BUFFER_SIZE;
    rv = SCardExchangeAPDU(pConnection, &tBuffer);
    
    if(rv != SCARD_S_SUCCESS) {
      return convertPCSC(rv);
    }

   if (tBuffer.apduResponseSize != (dataSize + 2)) {
      return convertSW(rxBuffer);
    } else {
      itemValue = rxBuffer;
    }
  }
  
  memcpy(pOutputData, &itemValue[offset], dataSize);
  return MSC_SUCCESS;

}

#ifdef WIN32
CACPLUGIN_API
#endif
MSC_RV PL_MSCListObjects( MSCLPTokenConnection pConnection, 
			  MSCUChar8 seqOption, 
			  MSCLPObjectInfo pObjectInfo ) {

  static int seq = 0;
  MSCULong32 rv;
  MSCULong32 itemSize;

  rv = CACLoadAndCacheData(pConnection);
  if (rv != MSC_SUCCESS) {
    return rv;
  }

  if ( seqOption == MSC_SEQUENCE_RESET ) {
    seq = 0;
  }

  switch(seq) {

  case 0:
    /* Email Encryption Certificate */
    itemSize = cacECryptCertSize;
    if (itemSize != 0) { 
      strncpy(pObjectInfo->objectID, CAC_ECRYCERT_OBJID, MSC_MAXSIZE_OBJID);
      break;
    } else {
      seq += 1;
    }
  case 1:
    /* Email Signature Certificate */
    itemSize = cacESignCertSize;
    if (itemSize != 0) { 
      strncpy(pObjectInfo->objectID, CAC_ESIGCERT_OBJID, MSC_MAXSIZE_OBJID);
      break;
    } else {
      seq += 1;
    }

  case 2:
    /* ID Certificate */
    itemSize = cacIDCertSize;
    if (itemSize != 0) { 
      strncpy(pObjectInfo->objectID, CAC_IDCERT_OBJID, MSC_MAXSIZE_OBJID);
      break;
    } else {
      seq += 1;
    }

  case 3:
    /* ID Certificate attributes */
    itemSize = cacIDCertAttrSize;
    if (itemSize != 0) { 
      strncpy(pObjectInfo->objectID, CAC_IDCERTATT_OBJID, MSC_MAXSIZE_OBJID);
      break;
    } else {
      seq += 1;
    }

  case 4:
   /* Email Encryption Certificate attributes */
    itemSize = cacECryptCertAttrSize;
    if (itemSize != 0) { 
      strncpy(pObjectInfo->objectID, CAC_ECRYCERTATT_OBJID, MSC_MAXSIZE_OBJID);
      break;
    } else {
      seq += 1;
    }

  case 5:
   /* Email Signature Certificate attributes */
    itemSize = cacESignCertAttrSize;
    if (itemSize != 0) { 
      strncpy(pObjectInfo->objectID, CAC_ESIGCERTATT_OBJID, MSC_MAXSIZE_OBJID);
      break;
    } else {
      seq += 1;
    }

  case 6:
    /* ID Key attributes */
    itemSize = cacIDKeyAttrSize;
    if (itemSize != 0) { 
      strncpy(pObjectInfo->objectID, CAC_IDKEYATT_OBJID, MSC_MAXSIZE_OBJID);
      break;
    } else {
      seq += 1;
    }

  case 7:
   /* Email Encryption Key attributes */
    itemSize = cacECryptKeyAttrSize;
    if (itemSize != 0) { 
      strncpy(pObjectInfo->objectID, CAC_ECRYKEYATT_OBJID, MSC_MAXSIZE_OBJID);
      break;
    } else {
      seq += 1;
    }

  case 8:
   /* Email Signature Key attributes */
    itemSize = cacESignKeyAttrSize;
    if (itemSize != 0) { 
      strncpy(pObjectInfo->objectID, CAC_ESIGKEYATT_OBJID, MSC_MAXSIZE_OBJID);
      break;
    } else {
      seq += 1;
    }

  case 9:
    itemSize   = pntbSize;
    if (itemSize != 0) { 
      strncpy(pObjectInfo->objectID, CAC_PNTB_OBJID, MSC_MAXSIZE_OBJID);
      break;
    } else {
      seq += 1;
    }

  case 10:
    itemSize   = pnvbSize;
    if (itemSize != 0) { 
      strncpy(pObjectInfo->objectID, CAC_PNVB_OBJID, MSC_MAXSIZE_OBJID);
      break;
    } else {
      seq += 1;
    }

  case 11:
    itemSize   = pltbSize;
    if (itemSize != 0) { 
      strncpy(pObjectInfo->objectID, CAC_PLTB_OBJID, MSC_MAXSIZE_OBJID);
      break;
    } else {
      seq += 1;
    }

  case 12:
    itemSize   = plvbSize;
    if (itemSize != 0) { 
      strncpy(pObjectInfo->objectID, CAC_PLVB_OBJID, MSC_MAXSIZE_OBJID);
      break;
    } else {
      seq += 1;
    }

  case 13:
    itemSize   = bstbSize;
    if (itemSize != 0) { 
      strncpy(pObjectInfo->objectID, CAC_BSTB_OBJID, MSC_MAXSIZE_OBJID);
      break;
    } else {
      seq += 1;
    }

  case 14:
    itemSize   = bsvbSize;
    if (itemSize != 0) { 
      strncpy(pObjectInfo->objectID, CAC_BSVB_OBJID, MSC_MAXSIZE_OBJID);
      break;
    } else {
      seq += 1;
    }

  case 15:
    itemSize   = obtbSize;
    if (itemSize != 0) { 
      strncpy(pObjectInfo->objectID, CAC_OBTB_OBJID, MSC_MAXSIZE_OBJID);
      break;
    } else {
      seq += 1;
    }

  case 16:
    itemSize   = obvbSize;
    if (itemSize != 0) { 
      strncpy(pObjectInfo->objectID, CAC_OBVB_OBJID, MSC_MAXSIZE_OBJID);
      break;
    } else {
      seq += 1;
    }

  default:
    return MSC_SEQUENCE_END;
  }
  
  /* Set the ACL */

  switch(seq) {

  case 0:
  case 1:
  case 2:
    /* For all certificates */
    pObjectInfo->objectACL.readPermission   = MSC_AUT_ALL;
    pObjectInfo->objectACL.writePermission  = MSC_AUT_NONE;
    pObjectInfo->objectACL.deletePermission = MSC_AUT_NONE;    
    break;

  case 3:
  case 4:
  case 5:
    /* For all certificate attributes */
    pObjectInfo->objectACL.readPermission   = MSC_AUT_ALL;
    pObjectInfo->objectACL.writePermission  = MSC_AUT_NONE;
    pObjectInfo->objectACL.deletePermission = MSC_AUT_NONE;        
    break;

  case 6:
  case 7:
  case 8:
    /* For all key attributes */
    pObjectInfo->objectACL.readPermission   = MSC_AUT_ALL;
    pObjectInfo->objectACL.writePermission  = MSC_AUT_NONE;
    pObjectInfo->objectACL.deletePermission = MSC_AUT_NONE;  
    break;

  case 9:
  case 11:
  case 13:
  case 15:
    /* For all tag buffers */
    pObjectInfo->objectACL.readPermission   = MSC_AUT_ALL;
    pObjectInfo->objectACL.writePermission  = MSC_AUT_NONE;
    pObjectInfo->objectACL.deletePermission = MSC_AUT_NONE;
    break;
    
  case 10:
  case 12:
  case 14:
  case 16:
    /* For all value buffers */
    pObjectInfo->objectACL.readPermission   = MSC_AUT_PIN_1;
    pObjectInfo->objectACL.writePermission  = MSC_AUT_NONE;
    pObjectInfo->objectACL.deletePermission = MSC_AUT_NONE;
    break;
  }

  pObjectInfo->objectSize = itemSize;

  seq += 1;
  return MSC_SUCCESS;
}

#ifdef WIN32
CACPLUGIN_API
#endif
MSC_RV PL_MSCLogoutAll( MSCLPTokenConnection pConnection ) {

  return MSC_UNSUPPORTED_FEATURE;
}

#ifdef WIN32
CACPLUGIN_API
#endif
MSC_RV PL_MSCGetChallenge( MSCLPTokenConnection pConnection, MSCPUChar8 pSeed,
			   MSCUShort16 seedSize, MSCPUChar8 pRandomData,
			   MSCUShort16 randomDataSize ) {

  MSCLong32 rv;
  MSCPUChar8 txBuffer;
  MSCPUChar8 rxBuffer;
  MSCTransmitBuffer tBuffer;
  
  txBuffer = tBuffer.pBuffer;
  rxBuffer = tBuffer.apduResponse;

  txBuffer[OFFSET_CLA] = 0x80;
  txBuffer[OFFSET_INS] = 0x84;
  txBuffer[OFFSET_P1]  = 0x00;
  txBuffer[OFFSET_P2]  = 0x00;
  txBuffer[OFFSET_P3]  = 0x08;
  
  if ( randomDataSize != 8 ) {
    return MSC_INVALID_PARAMETER;
  }
  
  tBuffer.apduResponseSize = MAX_BUFFER_SIZE;
  tBuffer.bufferSize       = 5;

  rv = SCardExchangeAPDU(pConnection, &tBuffer);
  
  if(rv != SCARD_S_SUCCESS) {
    return convertPCSC(rv);
  }
  
  if(tBuffer.apduResponseSize == 2) {
    return convertSW(rxBuffer);
  } else {
    return MSC_UNSPECIFIED_ERROR;
  }

}

#ifdef WIN32
CACPLUGIN_API
#endif
MSC_RV PL_MSCIdentifyToken( MSCLPTokenConnection pConnection ) {

  MSCLong32 rv;
  MSCPUChar8 txBuffer;
  MSCPUChar8 rxBuffer;
  MSCTransmitBuffer tBuffer;
  
  txBuffer = tBuffer.pBuffer;
  rxBuffer = tBuffer.apduResponse;

  /* Select AID */
  txBuffer[0] = 0x00; txBuffer[1] = 0xA4;  txBuffer[2] = 0x04; 
  txBuffer[3] = 0x00; txBuffer[4] = sizeof(CAC_APPLET_PKI_ESIG_AID);  

  /* Copy PKI Applet */
  memcpy(&txBuffer[5], CAC_APPLET_PKI_ESIG_AID, 
	 sizeof(CAC_APPLET_PKI_ESIG_AID));

  tBuffer.bufferSize       = 5 + sizeof(CAC_APPLET_PKI_ESIG_AID); 
  tBuffer.apduResponseSize = sizeof(CAC_APPLET_PKI_ESIG_AID) + 5;

  suppressResponse = 1;
  rv = SCardExchangeAPDU( pConnection, &tBuffer );
  suppressResponse = 0;

  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( rxBuffer[0] != 0x61 ) {
    return MSC_UNSUPPORTED_FEATURE;
  }

  return MSC_SUCCESS;
}

#ifdef WIN32
CACPLUGIN_API
#endif
MSC_RV PL_MSCInitializePlugin( MSCLPTokenConnection pConnection ) {

  /* Clean up and make sure all is shut down. */

  return MSC_SUCCESS;
}

#ifdef WIN32
CACPLUGIN_API
#endif
MSC_RV PL_MSCFinalizePlugin( MSCLPTokenConnection pConnection ) {

  /* Clean up and make sure all is shut down. */

  tlvsCached = 0;
  pntbSize = 0;
  pnvbSize = 0;
  pltbSize = 0;
  plvbSize = 0;
  bstbSize = 0;
  bsvbSize = 0;
  obtbSize = 0;
  obvbSize = 0;
  cacIDCertSize     = 0;
  cacESignCertSize  = 0;
  cacECryptCertSize = 0;
  dataIsCached      = 0;
  pConnection->loggedIDs  = 0;
  
  return MSC_SUCCESS;
}

MSC_RV CACLoadTLVSize(MSCLPTokenConnection pConnection, 
		      MSCPUChar8 demoPointer,
		      MSCULong32 demoSize,
		      MSCPUShort16 tlvSize,
		      MSCUChar8 tlvType) {

  MSCLong32 rv;
  MSCPUChar8 txBuffer;
  MSCPUChar8 rxBuffer;
  MSCTransmitBuffer tBuffer;

  txBuffer = tBuffer.pBuffer;
  rxBuffer = tBuffer.apduResponse;

  if ( *tlvSize != 0 ) {
    /* Data already cached */
    return MSC_SUCCESS;
  }
  
  rv = CACSelectInstance( pConnection, demoPointer, demoSize );

  if ( rv != MSC_SUCCESS )
    return rv;
   
    txBuffer[OFFSET_CLA]    = 0x80;
    txBuffer[OFFSET_INS]    = 0x56;
    txBuffer[OFFSET_P1]     = 0x00;
    txBuffer[OFFSET_P2]     = 0x00;
    txBuffer[OFFSET_P3]     = 0x2E;
    
    tBuffer.bufferSize = 5;
    tBuffer.apduResponseSize = MAX_BUFFER_SIZE;
    rv = SCardExchangeAPDU(pConnection, &tBuffer);
  
    if(rv != SCARD_S_SUCCESS) {
      return convertPCSC(rv);
    }
    
    if ( tBuffer.apduResponseSize == 2 ) {
      if (rxBuffer[0] == 0x6C) {  
        /* We requested the wrong length, try again */
        
        txBuffer[OFFSET_P3] = rxBuffer[1];
        tBuffer.apduResponseSize = MAX_BUFFER_SIZE;
        rv = SCardExchangeAPDU(pConnection, &tBuffer);
  
        if(rv != SCARD_S_SUCCESS) {
          return convertPCSC(rv);
        }
        
      } else {
        /* Another status return */
        return convertSW(rxBuffer);
      }

    } else if (tBuffer.apduResponseSize != (txBuffer[OFFSET_P3] + 2)) {      
        return MSC_INTERNAL_ERROR;
    } 
    
    if ( tlvType == 0 ) {
      /* Get the Tag */
      *tlvSize = rxBuffer[28] + rxBuffer[29]*0x100;
    } else {
      /* Get the Value */
      *tlvSize = rxBuffer[30] + rxBuffer[31]*0x100;
    }
      
    return MSC_SUCCESS;

}

MSC_RV CACLoadCertificate(MSCLPTokenConnection pConnection, MSCPUChar8 cert,
			  MSCPUShort16 certSize) {

  MSCLong32 rv;
  MSCULong32 dataPosition;
  MSCULong32 defLength;
  MSCPUChar8 txBuffer;
  MSCPUChar8 rxBuffer;
  MSCTransmitBuffer tBuffer;
  
  txBuffer = tBuffer.pBuffer;
  rxBuffer = tBuffer.apduResponse;
  
  defLength = 0; dataPosition = 0;

  /* Getting certificate */
 
  txBuffer[0] = 0x80; txBuffer[1] = 0x36; txBuffer[2] = 0x00; 
  txBuffer[3] = 0x00; txBuffer[4] = 0x64;
 
 /* Reading the certificate */
 
  do {
    /* Read data in 0x64 byte chunks */
    tBuffer.bufferSize       = 0x05;
    tBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;
    rv = SCardExchangeAPDU( pConnection, &tBuffer );

    if ( rv != SCARD_S_SUCCESS ) { return convertPCSC(rv); }

    if ( tBuffer.apduResponseSize == 2 ) {
      if (( rxBuffer[0] == 0x69 ) && ( rxBuffer[1] == 0x81 )) {
          *certSize = 0;
          return MSC_OBJECT_NOT_FOUND;
      } else if (( rxBuffer[0] == 0x69 ) && ( rxBuffer[1] == 0x82 )) {
          *certSize = 0;
          return MSC_UNAUTHORIZED;

      }
    } else if ( tBuffer.apduResponseSize != txBuffer[4] + 2 ) {
        /* Card was removed during a transaction */
        return MSC_INTERNAL_ERROR;
    } else {
      memcpy(&cacCompBuffer[dataPosition], rxBuffer,
  	     tBuffer.apduResponseSize - 2); 
      dataPosition += tBuffer.apduResponseSize - 2;
      txBuffer[4] = rxBuffer[tBuffer.apduResponseSize-1];
    }
  } while ( rxBuffer[tBuffer.apduResponseSize-2] == 0x63 ); 

  /* Done reading certificate */

  if (cacCompBuffer[0] == 0x01) { /* this is compressed */
    defLength = CAC_MAXSIZE_CERT;
    rv = uncompress(cert, (uLongf *)&defLength, &cacCompBuffer[1], 
  		    dataPosition - 1);
    if ( rv != 0 ) {
      return MSC_INTERNAL_ERROR;
    }

    *certSize = defLength;

  } else {
    memcpy(cert, &cacCompBuffer[1], dataPosition - 1);
    *certSize = dataPosition - 1;
  }
  
  return MSC_SUCCESS;

}

MSC_RV CACSelectInstance(MSCLPTokenConnection pConnection, MSCPUChar8 aid,
			 MSCULong32 aidLength) {

  MSCLong32 rv;
  MSCPUChar8 txBuffer;
  MSCPUChar8 rxBuffer;
  MSCTransmitBuffer tBuffer;
  
  txBuffer = tBuffer.pBuffer;
  rxBuffer = tBuffer.apduResponse;

  /* Now we must select the PKI instance, read the 
     certificate and cache it, updating it's size  */

  tBuffer.bufferSize = aidLength + 5;
  tBuffer.apduResponseSize = MSC_MAXSIZE_BUFFER;

  /* Select AID */
  txBuffer[0] = 0x00; txBuffer[1] = 0xA4;  txBuffer[2] = 0x04; 
  txBuffer[3] = 0x00; txBuffer[4] = aidLength;

   memcpy(&txBuffer[5], aid, aidLength);

  suppressResponse = 1;
  rv = SCardExchangeAPDU( pConnection, &tBuffer );
  suppressResponse = 0;

  if ( rv != SCARD_S_SUCCESS ) {
    return convertPCSC(rv);
  }

  if ( rxBuffer[0] != 0x61 ) {
    return MSC_UNSUPPORTED_FEATURE;
  }

  return MSC_SUCCESS;
}

MSC_RV CACLoadAndCacheData(MSCLPTokenConnection pConnection) {

  MSCLong32 rv;
  MSCPUChar8 certPointer;
  MSCPUChar8 pkiPointer;
  MSCULong32 pkiSize;
  MSCPUChar8 demoPointer;
  MSCULong32 demoSize;
  MSCPUShort16 itemCache;
  unsigned short *certSizePointer;

  if ( dataIsCached == 1 ) {
    return MSC_SUCCESS;
  }

  pkiPointer      = CAC_APPLET_PKI_ID_AID;
  pkiSize         = sizeof(CAC_APPLET_PKI_ID_AID);
  certPointer     = cacIDCert;
  certSizePointer = &cacIDCertSize;
  
  if ( *certSizePointer == 0 ) {
    rv = CACSelectInstance( pConnection, pkiPointer, pkiSize );
    if ( rv == MSC_SUCCESS )
      rv = CACLoadCertificate(pConnection, certPointer, certSizePointer); 
      if ( rv == MSC_INTERNAL_ERROR ) { return rv; }
      
    if (*certSizePointer == 0) {
      cacIDCertAttrSize = 0;
      cacIDKeyAttrSize  = 0;
    }        
  }
  
  pkiPointer      = CAC_APPLET_PKI_ECRY_AID;
  pkiSize         = sizeof(CAC_APPLET_PKI_ECRY_AID);
  certPointer     = cacECryptCert;
  certSizePointer = &cacECryptCertSize;
  if ( *certSizePointer == 0 ) {
    rv = CACSelectInstance( pConnection, pkiPointer, pkiSize );
    if ( rv == MSC_SUCCESS )
      CACLoadCertificate(pConnection, certPointer, certSizePointer); 
      if ( rv == MSC_INTERNAL_ERROR ) { return rv; }
 
    if (*certSizePointer == 0) {
      cacECryptCertAttrSize = 0;
      cacECryptKeyAttrSize  = 0;
    } 
  }
  
  pkiPointer      = CAC_APPLET_PKI_ESIG_AID;
  pkiSize         = sizeof(CAC_APPLET_PKI_ESIG_AID);
  certPointer     = cacESignCert;
  certSizePointer = &cacESignCertSize;
  if ( *certSizePointer == 0 ) {
    rv = CACSelectInstance( pConnection, pkiPointer, pkiSize );
    if ( rv == MSC_SUCCESS )
      CACLoadCertificate(pConnection, certPointer, certSizePointer); 
      if ( rv == MSC_INTERNAL_ERROR ) { return rv; }
 
    if (*certSizePointer == 0) {
      cacESignCertAttrSize = 0;
      cacESignKeyAttrSize  = 0;
    } 


  }

  itemCache   = &pntbSize;
  demoPointer = CAC_APPLET_CONT_PN_AID;
  demoSize    = sizeof(CAC_APPLET_CONT_PN_AID);
  if ( *itemCache == 0 ) {
    rv = CACLoadTLVSize(pConnection, demoPointer, demoSize, itemCache, 0);
  }

  itemCache   = &pnvbSize;
  demoPointer = CAC_APPLET_CONT_PN_AID;
  demoSize    = sizeof(CAC_APPLET_CONT_PN_AID);
  if ( *itemCache == 0 ) {
    rv = CACLoadTLVSize(pConnection, demoPointer, demoSize, itemCache, 1);
  }
  
  itemCache   = &pltbSize;
  demoPointer = CAC_APPLET_CONT_PL_AID;
  demoSize    = sizeof(CAC_APPLET_CONT_PL_AID);
  if ( *itemCache == 0 ) {
    rv = CACLoadTLVSize(pConnection, demoPointer, demoSize, itemCache, 0);
    if ( rv == MSC_INTERNAL_ERROR ) { return rv; }
  }
  
  itemCache   = &plvbSize;
  demoPointer = CAC_APPLET_CONT_PL_AID;
  demoSize    = sizeof(CAC_APPLET_CONT_PL_AID);
  if ( *itemCache == 0 ) {
    rv = CACLoadTLVSize(pConnection, demoPointer, demoSize, itemCache, 1);
    if ( rv == MSC_INTERNAL_ERROR ) { return rv; }
  }
  
  itemCache   = &bstbSize;
  demoPointer = CAC_APPLET_CONT_BS_AID;
  demoSize    = sizeof(CAC_APPLET_CONT_BS_AID);
  if ( *itemCache == 0 ) {
    rv = CACLoadTLVSize(pConnection, demoPointer, demoSize, itemCache, 0);
    if ( rv == MSC_INTERNAL_ERROR ) { return rv; }
  }
  
  itemCache   = &bsvbSize;
  demoPointer = CAC_APPLET_CONT_BS_AID;
  demoSize    = sizeof(CAC_APPLET_CONT_BS_AID);
  if ( *itemCache == 0 ) {
    rv = CACLoadTLVSize(pConnection, demoPointer, demoSize, itemCache, 1);
    if ( rv == MSC_INTERNAL_ERROR ) { return rv; }
  }
  
  itemCache   = &obtbSize;
  demoPointer = CAC_APPLET_CONT_OB_AID;
  demoSize    = sizeof(CAC_APPLET_CONT_OB_AID);
  if ( *itemCache == 0 ) {
    rv = CACLoadTLVSize(pConnection, demoPointer, demoSize, itemCache, 0);
    if ( rv == MSC_INTERNAL_ERROR ) { return rv; }
  }
  
  itemCache   = &obvbSize;
  demoPointer = CAC_APPLET_CONT_OB_AID;
  demoSize    = sizeof(CAC_APPLET_CONT_OB_AID);
  if ( *itemCache == 0 ) {
    rv = CACLoadTLVSize(pConnection, demoPointer, demoSize, itemCache, 1);
    if ( rv == MSC_INTERNAL_ERROR ) { return rv; }
  }    

  dataIsCached = 1;
  return MSC_SUCCESS;

}


MSCUShort16 convertSW(MSCPUChar8 pBuffer) {
  MSCUShort16 retValue;
  MSCUShort16 newValue;

  retValue  = pBuffer[0] * 0x100;
  retValue += pBuffer[1];

  switch(retValue) {
  case CACMSC_SUCCESS:
    newValue = MSC_SUCCESS;
    break;
  case CACMSC_NO_MEMORY_LEFT:
    newValue = MSC_NO_MEMORY_LEFT;
    break;
  case CACMSC_AUTH_FAILED:
    newValue = MSC_AUTH_FAILED;
    break;
  case CACMSC_UNSUPPORTED_FEATURE:
    newValue = MSC_UNSUPPORTED_FEATURE;
    break;
  case CACMSC_UNAUTHORIZED:
    newValue = MSC_UNAUTHORIZED;
    break;
  case CACMSC_OBJECT_NOT_FOUND:
    newValue = MSC_OBJECT_NOT_FOUND;
    break;
  case CACMSC_OBJECT_EXISTS:
    newValue = MSC_OBJECT_EXISTS;
    break;
  case CACMSC_INCORRECT_ALG:
    newValue = MSC_INCORRECT_ALG;
    break;
  case CACMSC_IDENTITY_BLOCKED:
    newValue = MSC_IDENTITY_BLOCKED;
    break;
  case CACMSC_UNSPECIFIED_ERROR:
    newValue = MSC_UNSPECIFIED_ERROR;
    break;
  case CACMSC_INVALID_PARAMETER:
    newValue = MSC_INVALID_PARAMETER;
    break;
  case CACMSC_INTERNAL_ERROR:
    newValue = MSC_INTERNAL_ERROR;
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
  MSCUChar8 getResponse[5] = {0x00, 0xC0, 0x00, 0x00, 0x00};
  MSCULong32 dwActiveProtocol;

#ifdef MSC_DEBUG
  int i;
#endif

  originalLength = transmitBuffer->apduResponseSize;

  while (1) {
    
#ifdef MSC_DEBUG
    printf("[%02d]->: ", transmitBuffer->bufferSize); 
    
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



/*
 *  CCIDPropExt.c
 *  ifd-CCID
 *
 *  Created by JL on Sun Jul 20 2003.
 *  Copyright (c) 2003 Jean-Luc Giraud. All rights reserved.
 *  See COPYING file for license.
 *
 */

#include "global.h"

#include "tools.h"
#include "CCID.h"
#include "CCIDprivate.h"
#include "pcscdefines.h"
#include "CCIDPropExt.h"

typedef struct {
    DWORD dwVendorID;
    DWORD dwProductID;
    CCIDPropExt stPropExt;
} CCIDExtendedPropExt;

CCIDRv CCIDPropExtOpenChannelGemPCKeyTwin(DWORD lun);
CCIDRv CCIDPropExtOpenChannelGemPC433(DWORD lun);
CCIDRv CCIDPropExtIccPowerOnGemPC433(DWORD Lun, BYTE *abDataResp, DWORD *pdwDataRespLength, BYTE bStatus, BYTE bError);


static CCIDExtendedPropExt astCCIDPropExt[] =
{
    // GemPC433
    {0x8E6, 0x4433, {NULL, NULL, CCIDPropExtIccPowerOnGemPC433}},    
    // GemPCKey
    {0x8E6, 0x3438, {CCIDPropExtOpenChannelGemPCKeyTwin, NULL, NULL}},
    // GemPCTwin
    {0x8E6, 0x3437, {CCIDPropExtOpenChannelGemPCKeyTwin, NULL, NULL}},    
    
};


CCIDRv CCIDPropExtLookupExt(DWORD dwVendorID, DWORD dwProductID, CCIDPropExt *pstCCIDPropExt)
{
    WORD wIndex;

    for (wIndex = 0; wIndex < (sizeof(astCCIDPropExt)/sizeof(CCIDExtendedPropExt)); wIndex++)
    {
        if ( (astCCIDPropExt[wIndex].dwVendorID == dwVendorID)
             && (astCCIDPropExt[wIndex].dwProductID == dwProductID)
             )
        {
            pstCCIDPropExt->OpenChannel = astCCIDPropExt[wIndex].stPropExt.OpenChannel;
            pstCCIDPropExt->CloseChannel = astCCIDPropExt[wIndex].stPropExt.CloseChannel;
            pstCCIDPropExt->PowerOn = astCCIDPropExt[wIndex].stPropExt.PowerOn;
            return CCIDRv_OK;
        }
    }
    return CCIDRv_ERR_VALUE_NOT_FOUND;
}


// Used to set the GemPCKey and the GemPCTwin in APDU mode
// Connections need to be set-up before this call
CCIDRv CCIDPropExtOpenChannelGemPCKeyTwin(DWORD Lun)
{
    CCIDRv rv;

    //BYTE abGemPCTwinSetAPDUMode[] = {0xA0, 0x01}; (from FB)
    BYTE abGemPCTwinSetAPDUMode[] = {0xA0, 0x02};
    BYTE pcBuffer[100];
    BYTE bSpecificErrorCode;
    DWORD dwBufferLength = sizeof(pcBuffer);
    rv = CCID_Escape(Lun, abGemPCTwinSetAPDUMode, sizeof(abGemPCTwinSetAPDUMode),
                     pcBuffer, &dwBufferLength, &bSpecificErrorCode);
    if ( rv != CCIDRv_OK )
        return rv;
    // Successfull initialisation, update Exchange Level
    CCIDReaderStates[ LunToReaderLun(Lun)].dwExchangeLevel = CCID_CLASS_FEAT_EXC_LEVEL_SAPDU;
    return CCIDRv_OK;
}


// Used to set the GemPC433 in APDU mode
// Connections need to be set-up before this call
CCIDRv CCIDPropExtOpenChannelGemPC433(DWORD Lun)
{
    CCIDRv rv;
    BYTE abGemPC433SetISOMode[] = {0x1F, 0x01};
    BYTE pcBuffer[100];
    BYTE bSpecificErrorCode;
    DWORD dwBufferLength = sizeof(pcBuffer);
    rv = CCID_Escape(Lun, abGemPC433SetISOMode, sizeof(abGemPC433SetISOMode),
                     pcBuffer, &dwBufferLength, &bSpecificErrorCode);
    if ( rv != CCIDRv_OK )
        return rv;
    // Successfull initialisation, update Exchange Level
    CCIDReaderStates[ LunToReaderLun(Lun)].dwExchangeLevel = CCID_CLASS_FEAT_EXC_LEVEL_SAPDU;
    return CCIDRv_OK;
}

// Perform second power-up in ISO mode for the GemPC433
CCIDRv CCIDPropExtIccPowerOnGemPC433(DWORD Lun, BYTE *abDataResp, DWORD *pdwDataRespLength,
                                     BYTE bStatus, BYTE bError)
{
    CCIDRv rv;
    WORD wRdrLun;
    BYTE bMessageTypeResp;
    BYTE bMessageSpecificResp;
    BYTE abMessageSpecificCmd[3] = "\x00\x00\x00";

    // Check if it is a non EMV-compliant ATR issue
    if ( bError != 0xBB )
    {
        // Other error, exit
        return CCIDRv_ERR_UNSPECIFIED;
    }
    wRdrLun = LunToReaderLun(Lun);
    if ( wRdrLun >= PCSCLITE_MAX_CHANNELS)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Reader Lun too large: %d", wRdrLun);
        return CCIDRv_ERR_READER_LUN;
    }
    // First set reader in ISO mode
    if ( CCIDPropExtOpenChannelGemPC433(Lun) != CCIDRv_OK )
    {
        return CCIDRv_ERR_UNSPECIFIED;
    }
    // Power-up at 5V
    //+++ For the proper Power-Up sequence, see 7816-3:1997 section 4.2.2
    abMessageSpecificCmd[0] = 0x01;
    rv =  CCID_Exchange_Command(Lun, PC_to_RDR_IccPowerOn,
                                abMessageSpecificCmd,
                                (BYTE *)"", 0,
                                &bMessageTypeResp,
                                &bStatus, &bError,
                                &bMessageSpecificResp,
                                abDataResp, pdwDataRespLength, 0);
    if ( rv == CCIDRv_ERR_PRIVATE_ERROR )
    {
        // ICC Power On has a few specific error cases
        if ( CCIDGetICCStatus(bStatus) == CCID_ICC_STATUS_INACTIVE )
        {
            if ( bError == CCID_ERR_7 )
            {
                LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Power On mode not supported: %02X", abMessageSpecificCmd[0]);
                return CCIDRv_ERR_POWERON_MODE_UNSUPPORTED;
            }

            if ( bError == CCID_ERR_XFR_PARITY_ERROR )
            {
                LogMessage( __FILE__,  __LINE__, LogLevelCritical, "ATR Parity error");
                return CCIDRv_ERR_ATR_PARITY_ERROR;
            }

            if ( bError == CCID_ERR_BAD_ATR_TS )
            {
                LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Wrong TS in ATR");
                return CCIDRv_ERR_BAD_ATR_TS;
            }

            if ( bError == CCID_ERR_BAD_ATR_TCK )
            {
                LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Wrong TCK in ATR");
                return CCIDRv_ERR_BAD_ATR_TCK;
            }

            if ( bError == CCID_ERR_ICC_PROTOCOL_NOT_SUPPORTED )
            {
                LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Protocol not supported");
                return CCIDRv_ERR_PROTOCOL_NOT_SUPPORTED;
            }

            if ( bError == CCID_ERR_ICC_CLASS_NOT_SUPPORTED )
            {
                LogMessage( __FILE__,  __LINE__, LogLevelCritical, "ICC Class not supported");
                return CCIDRv_ERR_CLASS_NOT_SUPPORTED;
            }

        }
        if ( CCIDGetICCStatus(bStatus) == CCID_ICC_STATUS_ABSENT)
        {
            return CCIDRv_ERR_CARD_ABSENT;
        }
        return CCIDRv_ERR_UNSPECIFIED;
    }
    if ( bMessageTypeResp != RDR_to_PC_DataBlock )
    {
        return CCIDRv_ERR_WRONG_MESG_RESP_TYPE;
    }
    return CCIDRv_OK;    
}

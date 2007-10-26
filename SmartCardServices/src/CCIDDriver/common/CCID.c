/*
 *  CCID.c
 *  ifd-CCID
 *
 *  Created by JL on Sat Jun 28 2003.
 *  Copyright (c) 2003 Jean-Luc Giraud. All rights reserved.
 *  See COPYING file for license.
 *
 */
#include <CoreFoundation/CoreFoundation.h>
#include "global.h"

#include "tools.h"
#include "CCID.h"
#include "CCIDprivate.h"
#include "pcscdefines.h"
#include "CCIDPropExt.h"


CCIDReaderState CCIDReaderStates[PCSCLITE_MAX_CHANNELS];
static BYTE bCCIDInitialised = 0;

void CCIDParseDesc(BYTE * pcbuffer, CCIDClassDescriptor *classDesc)
{

    bcopy(pcbuffer, classDesc, sizeof(CCIDClassDescriptor));
    // Correct endianness of relevant fields
    classDesc->bcdCCID = CCIDToHostWord(classDesc->bcdCCID);
    classDesc->dwProtocols = CCIDToHostLong(classDesc->dwProtocols);
    classDesc->dwDefaultClock = CCIDToHostLong(classDesc->dwDefaultClock);
    classDesc->dwMaximumClock = CCIDToHostLong(classDesc->dwMaximumClock);
    classDesc->dwDataRate = CCIDToHostLong(classDesc->dwDataRate);
    classDesc->dwMaxDataRate = CCIDToHostLong(classDesc->dwMaxDataRate);
    classDesc->dwMaxIFSD = CCIDToHostLong(classDesc->dwMaxIFSD);
    classDesc->dwSynchProtocols = CCIDToHostLong(classDesc->dwSynchProtocols);
    classDesc->dwMechanical = CCIDToHostLong(classDesc->dwMechanical);
    classDesc->dwFeatures = CCIDToHostLong(classDesc->dwFeatures);
    classDesc->dwMaxCCIDMessageLength = CCIDToHostLong(classDesc->dwMaxCCIDMessageLength);
    classDesc->wLcdLayout = CCIDToHostWord(classDesc->wLcdLayout);
}

void CCIDPrintDesc(CCIDClassDescriptor classDesc)
{
   LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\tbLength                = 0x%02X\n", classDesc.bLength);
   LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\tbDescriptorType        = 0x%02X\n", classDesc.bDescriptorType);
   LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\tbcdCCID                = 0x%04X\n", classDesc.bcdCCID);
   LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\tbMaxSlotIndex          = %d\n", classDesc.bMaxSlotIndex);
   LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\tbVoltageSupport        = 0x%02X\n", classDesc.bVoltageSupport);
   LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\tdwProtocols            = 0x%08X\n", classDesc.dwProtocols);
   LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\tdwDefaultClock         = %ld\n", classDesc.dwDefaultClock);
   LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\tdwMaximumClock         = %ld\n", classDesc.dwMaximumClock);
   LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\tbNumDataRatesSupported = %d\n", classDesc.bNumCockSupported);
   LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\tdwDataRate             = %ld\n", classDesc.dwDataRate);
   LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\tdwMaxDataRate          = %ld\n", classDesc.dwMaxDataRate);
   LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\tbNumDataRatesSupported = %d\n", classDesc.bNumDataRatesSupported);
   LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\tdwMaxIFSD              = %ld\n", classDesc.dwMaxIFSD);
   LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\tdwSynchProtocols       = 0x%08X\n", classDesc.dwSynchProtocols);
   LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\tdwMechanical           = 0x%08X\n", classDesc.dwMechanical);
   LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\tdwFeatures             = 0x%08X\n", classDesc.dwFeatures);
   if ( classDesc.dwFeatures & CCID_CLASS_FEAT_AUTO_CONF_ATR )
   {
       LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\t\t Automatic conf. according to ATR");
   }
   if ( classDesc.dwFeatures & CCID_CLASS_FEAT_AUTO_ACT )
   {
       LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\t\t Automatic activation of ICC on insertion");
   }
   if ( classDesc.dwFeatures & CCID_CLASS_FEAT_AUTO_VOLT )
   {
       LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\t\t Automatic voltage selection");
   }
   if ( classDesc.dwFeatures & CCID_CLASS_FEAT_AUTO_CLOCK )
   {
       LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\t\t Automatic clock frequency change");
   }
   if ( classDesc.dwFeatures & CCID_CLASS_FEAT_AUTO_BAUD )
   {
       LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\t\t Automatic baud rate selection (freq, FI, DI)");
   }
   if ( classDesc.dwFeatures & CCID_CLASS_FEAT_AUTO_PPS_PROP )
   {
       LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\t\t Automatic parameter negociation (proprietary)");
   }
   if ( classDesc.dwFeatures & CCID_CLASS_FEAT_AUTO_PPS_CUR )
   {
       LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\t\t Automatic parameter negociation (current)");
   }
   if ( classDesc.dwFeatures & CCID_CLASS_FEAT_CLOCK_STOP )
   {
       LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\t\t Can set ICC in stop clock mode");
   }
   if ( classDesc.dwFeatures & CCID_CLASS_FEAT_NAD_NON_0 )
   {
       LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\t\t Non 00 value for NAD suppported");
   }
   if ( classDesc.dwFeatures & CCID_CLASS_FEAT_AUTO_IFSD )
   {
       LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\t\t Automatic IFSD exchange as first exchange");
   }
   if ( classDesc.dwFeatures & CCID_CLASS_FEAT_AUTO_IFSD )
   {
       LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\t\t Automatic IFSD exchange as first exchange");
   }
   if ( classDesc.dwFeatures & CCID_CLASS_FEAT_AUTO_IFSD )
   {
       LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\t\t Automatic IFSD exchange as first exchange");
   }
   switch ( classDesc.dwFeatures & CCID_CLASS_FEAT_EXC_LEVEL_MASK )
   {
       case CCID_CLASS_FEAT_EXC_LEVEL_CHAR:
           LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\t\t Character level exchanges");
           break;
       case CCID_CLASS_FEAT_EXC_LEVEL_TPDU:
           LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\t\t TPDU level exchanges");
           break;
       case CCID_CLASS_FEAT_EXC_LEVEL_SAPDU:
           LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\t\t Short APDU level exchanges");
           break;
       case CCID_CLASS_FEAT_EXC_LEVEL_LAPDU:
           LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\t\t Short and extended APDU level exchanges");
           break;
   }
   LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\tdwMaxCCIDMessageLength = %ld\n", classDesc.dwMaxCCIDMessageLength);
   LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\tbClassGetResponse      = 0x%02X\n", classDesc.bClassGetResponse);
   LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\tbClassEnvelope         = 0x%02X\n", classDesc.bClassEnvelope);
   LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\twLcdLayout             = 0x%04X\n", classDesc.wLcdLayout);
   LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\tbPINSupport            = 0x%02X\n", classDesc.bPINSupport);
   LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "\tbMaxCCIDBusySlots      = %d\n", classDesc.bMaxCCIDBusySlots); 
}


// Parses the bStatus byte returned in a Bulk-IN
// message to return bmICCStatus
BYTE CCIDGetICCStatus(BYTE bStatus)
{
    return (((bStatus) & MASK_ICC_STATUS) >> OFFSET_ICC_STATUS);
}
// Gets a pointer to a user friendly message
static const char* ICCStatusMessage[] =
{
    "ICC present and active",
    "ICC present and inactive",
    "No ICC",
    "RFU"
};
const char *CCIDGetICCStatusMessage(BYTE bICCStatus)
{
    if (bICCStatus < (sizeof(ICCStatusMessage)/sizeof(char*)))
        return ICCStatusMessage[bICCStatus];
    return "";
}
// Parses the bStatus byte returned in a Bulk-IN
// message to return bmCommandStatus
BYTE CCIDGetCommandStatus(BYTE bStatus)
{
    return (((bStatus) & MASK_COMMAND_STATUS) >> OFFSET_COMMAND_STATUS);
}
static const char* CommandStatusMessage[] =
{
    "Success",
    "Failed",
    "Time extension required",
    "RFU"
};

// Gets a pointer to a user friendly message
const char *CCIDGetCommandStatusMessage(BYTE bCommandStatus)
{
    if (bCommandStatus < (sizeof(CommandStatusMessage)/sizeof(char*)))
        return CommandStatusMessage[bCommandStatus];
    return "";
}

CCIDRv CCID_OpenChannel(DWORD Lun, DWORD ChannelID)
{
    WORD wRdrLun;
    TrRv rv;
    
    if ( !bCCIDInitialised )
    {
        //Intialise structure 
        bzero(CCIDReaderStates, sizeof(CCIDReaderStates));
        bCCIDInitialised = 1;
    }

    wRdrLun = LunToReaderLun(Lun);
    if ( wRdrLun >= PCSCLITE_MAX_CHANNELS)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                    "Reader Lun too large: %d", wRdrLun);
        return CCIDRv_ERR_READER_LUN;
    }
    if ( CCIDReaderStates[wRdrLun].used)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                    "Reader Lun already used: %d", wRdrLun);
        return CCIDRv_ERR_READER_LUN;
    }
    CCIDReaderStates[wRdrLun].used = 1;
    //++ SHOULD CHECK CHANNELID TO SEE WHICH TRANSPORT TO USE
    //++ This is to support CCID readers over different transport 
    //++ mechanism (serial or even serial from PCMCIA)
    CCIDReaderStates[wRdrLun].pTrFunctions = &TrFunctionTable[TrType_USB];
    // Create an "alias" to the transport functions
    TrFunctions *pTrFunctions =  CCIDReaderStates[wRdrLun].pTrFunctions;
    rv = pTrFunctions->Open(Lun, ChannelID);
    if ( rv != TrRv_OK )
    {
        CCIDReaderStates[wRdrLun].used = 0;
        return CCIDRv_ERR_TRANSPORT_ERROR;
    }
    
    BYTE bnbOfDesc;

    rv = pTrFunctions->GetConfigDescNumber(Lun, &bnbOfDesc);
    if ( rv != TrRv_OK )
    {
        CCIDReaderStates[wRdrLun].used = 0;
        // Call close to reset USB structures
        CCIDReaderStates[wRdrLun].pTrFunctions->Close(Lun);
        return CCIDRv_ERR_TRANSPORT_ERROR;
    }
    //+++ Configuration descriptor analysis could take place here.
    //++ For now, use configuration descriptor 0
    BYTE bSelectedConfDesc = 0;
    
    BYTE pcbufferDesc[CCID_DESC_SIZE];
    BYTE bbufferDescLength = sizeof(pcbufferDesc);
    rv = pTrFunctions->GetClassDesc(Lun, bSelectedConfDesc, CCID_DESC_TYPE,
                                    NULL, &bbufferDescLength);
    if ( rv != TrRv_OK )
    {
        CCIDReaderStates[wRdrLun].used = 0;
        // Call close to reset USB structures
        CCIDReaderStates[wRdrLun].pTrFunctions->Close(Lun);
        return CCIDRv_ERR_TRANSPORT_ERROR;
    }
    if (bbufferDescLength != CCID_DESC_SIZE)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                    "Incorrect Class Desc size %d ", bbufferDescLength);
        // Call close to reset USB structures
        CCIDReaderStates[wRdrLun].pTrFunctions->Close(Lun);
        return CCIDRv_ERR_CLASS_DESC_INVALID;
    }
    bbufferDescLength = sizeof(pcbufferDesc);
    rv = pTrFunctions->GetClassDesc(Lun, bSelectedConfDesc, CCID_DESC_TYPE,
                                    pcbufferDesc, &bbufferDescLength);
    if ( rv != TrRv_OK )
    {
        CCIDReaderStates[wRdrLun].used = 0;
        // Call close to reset USB structures
        CCIDReaderStates[wRdrLun].pTrFunctions->Close(Lun);
        return CCIDRv_ERR_TRANSPORT_ERROR;
    }
    // Convenience pointer
    CCIDClassDescriptor *pstClassDesc;
    pstClassDesc = &(CCIDReaderStates[wRdrLun].classDesc);
    CCIDParseDesc(pcbufferDesc, pstClassDesc);
    CCIDPrintDesc(CCIDReaderStates[wRdrLun].classDesc);

    // Initialise fields
    CCIDReaderStates[wRdrLun].bMaxSlotIndex = pstClassDesc->bMaxSlotIndex;
    CCIDReaderStates[wRdrLun].bMaxCCIDBusySlots = pstClassDesc->bMaxCCIDBusySlots;
    CCIDReaderStates[wRdrLun].dwMaxCCIDMessageLength = pstClassDesc->dwMaxCCIDMessageLength;
    CCIDReaderStates[wRdrLun].dwExchangeLevel = (pstClassDesc->dwFeatures)
        & CCID_CLASS_FEAT_EXC_LEVEL_MASK;

    // Make sure response struct is smaller than command struct
    assert(sizeof(CCIDMessageBulkOut) >= sizeof(CCIDMessageBulkIn));
    if ( CCIDReaderStates[wRdrLun].dwMaxCCIDMessageLength < sizeof(CCIDMessageBulkOut) )
    {
        // Can't do anything with this reader, its message length is even smaller
        // than the size of a minimal command
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                    "Reader dwMaxCCIDMessageLength property is too small: %d", 
                     CCIDReaderStates[wRdrLun].dwMaxCCIDMessageLength);
        CCIDReaderStates[wRdrLun].used = 0;
        // Call close to reset USB structures
        CCIDReaderStates[wRdrLun].pTrFunctions->Close(Lun);
        return CCIDRv_ERR_UNSPECIFIED;
        
    }
    //+++ Interrupt pipes not supported, hence 0 in parameter
    rv = pTrFunctions->SetupConnections(Lun, bSelectedConfDesc, 0);
    if ( rv != TrRv_OK )
    {
        CCIDReaderStates[wRdrLun].used = 0;
        // Call close to reset USB structures
        CCIDReaderStates[wRdrLun].pTrFunctions->Close(Lun);
        return CCIDRv_ERR_TRANSPORT_ERROR;
    }
    
    rv = pTrFunctions->GetVendorAndProductID(Lun, & (CCIDReaderStates[wRdrLun].dwVendorID),
                                             &(CCIDReaderStates[wRdrLun].dwProductID));
    if ( rv != TrRv_OK )
    {
        CCIDReaderStates[wRdrLun].used = 0;
        // Call close to reset USB structures
        CCIDReaderStates[wRdrLun].pTrFunctions->Close(Lun);
        return CCIDRv_ERR_TRANSPORT_ERROR;
    }
    CCIDPropExt stCCIDPropExt;
    rv = CCIDPropExtLookupExt(CCIDReaderStates[wRdrLun].dwVendorID,
                              CCIDReaderStates[wRdrLun].dwProductID,
                              &stCCIDPropExt);
    if ( rv == CCIDRv_OK )
    {
        if ( stCCIDPropExt.OpenChannel != NULL )
        {
            rv = stCCIDPropExt.OpenChannel(Lun);
            if ( rv != CCIDRv_OK )
            {
                CCIDReaderStates[wRdrLun].used = 0;
                // Call close to reset USB structures
                CCIDReaderStates[wRdrLun].pTrFunctions->Close(Lun);
                return rv;
            }
        }
    }
    
    return CCIDRv_OK;
}

CCIDRv CCID_CloseChannel(DWORD Lun)
{
    WORD wRdrLun;
    TrRv rv;

    wRdrLun = LunToReaderLun(Lun);
    if ( wRdrLun >= PCSCLITE_MAX_CHANNELS)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Reader Lun too large: %d", wRdrLun);
        return CCIDRv_ERR_READER_LUN;
    }
    if ( !CCIDReaderStates[wRdrLun].used)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                    "Reader Lun of unused reader: %d", wRdrLun);
        return CCIDRv_ERR_READER_LUN;
    }
    // Close USB connection
    rv = CCIDReaderStates[wRdrLun].pTrFunctions->Close(Lun);
    // Clean-up the structure
    bzero(&CCIDReaderStates[wRdrLun], sizeof(CCIDReaderState));
    if ( rv != TrRv_OK )
    {
        return CCIDRv_ERR_TRANSPORT_ERROR;
    }

    return CCIDRv_OK;
    
}


CCIDRv CCID_Exchange_Command(DWORD Lun, BYTE bMessageTypeCmd,
                             BYTE *abMessageSpecificCmd,
                             BYTE *abDataCmd, DWORD dwDataCmdLength,
                             BYTE *pbMessageTypeResp,
                             BYTE *pbStatus, BYTE *pbError,
                             BYTE *pbMessageSpecificResp,
                             BYTE *abDataResp, DWORD *pdwDataRespLength,
                             BYTE bTimeExtRetry)
{
    CCIDMessageBulkOut *pstmessage;
    CCIDMessageBulkIn  *pstresponse;
    WORD wSlot;
    WORD wRdrLun;
    TrRv trv = 0;
    BYTE *pcBuffer;
    DWORD dwRespLength;

    wRdrLun = LunToReaderLun(Lun);
    if ( wRdrLun >= PCSCLITE_MAX_CHANNELS)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Reader Lun too large: %d", wRdrLun);
        return CCIDRv_ERR_READER_LUN;
    }
    if ( !CCIDReaderStates[wRdrLun].used)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                    "Reader Lun of unused reader: %d", wRdrLun);
        return CCIDRv_ERR_READER_LUN;
    }
    wSlot = LunToSlotNb(Lun);
    if ( wSlot > CCIDReaderStates[wRdrLun].bMaxSlotIndex )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Slot Lun too large: %d", wSlot);
        return CCIDRv_ERR_SLOT_LUN;
    }
    if ( CCIDReaderStates[wRdrLun].bCurrentCCIDBusySlots
         >= CCIDReaderStates[wRdrLun].bMaxCCIDBusySlots )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Too many slots busy");
        return CCIDRv_ERR_SLOTS_BUSY;
    }

    if ( CCIDReaderStates[wRdrLun].dwMaxCCIDMessageLength < dwDataCmdLength )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Command too long for reader");
        return CCIDRv_ERR_UNSPECIFIED;
    }
    
    // Just to make sure buffer will be large enough for the response
    assert(sizeof(CCIDMessageBulkOut) >= sizeof(CCIDMessageBulkIn));

    // Malloc buffer of size of the message +maximum data for this slot
    //+++ Could be malloced once and for all at the structure creation
    pcBuffer = malloc(sizeof(CCIDMessageBulkOut)
                         + CCIDReaderStates[wRdrLun].dwMaxCCIDMessageLength);
    if ( pcBuffer == NULL )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Malloc failed");
        return CCIDRv_ERR_UNSPECIFIED;        
    }
    // See if this is a retry after a time extension request
    // For T=0, this means that we just have to jump to the read
    // and not attempt to write
    if (!bTimeExtRetry)
    {
        // Not a retry, send the command
        //++ bCurrentCCIDBusySlots should be here
        
        pstmessage = (CCIDMessageBulkOut *) pcBuffer;
        pstmessage->bMessageType = bMessageTypeCmd;
        pstmessage->dwLength = HostToCCIDLong(dwDataCmdLength);
        pstmessage->bSlot = wSlot;
        pstmessage->bSeq = CCIDReaderStates[wRdrLun].bSeq++;
        pstmessage->bMessageSpecific1 = abMessageSpecificCmd[0];
        pstmessage->bMessageSpecific2 = abMessageSpecificCmd[1];
        pstmessage->bMessageSpecific3 = abMessageSpecificCmd[2];
        // Copy the command data
        bcopy(abDataCmd, pcBuffer+sizeof(CCIDMessageBulkOut), dwDataCmdLength);
        
        trv = CCIDReaderStates[wRdrLun].pTrFunctions->Write(Lun,
                                                            sizeof(CCIDMessageBulkOut) +
                                                            dwDataCmdLength,
                                                            pcBuffer);
    }
    bzero(pcBuffer, sizeof(CCIDMessageBulkOut)+dwDataCmdLength);
    if ( trv != TrRv_OK )
    {
        free(pcBuffer);
        return CCIDRv_ERR_TRANSPORT_ERROR;
    }
    dwRespLength =  CCIDReaderStates[wRdrLun].dwMaxCCIDMessageLength;
    trv = CCIDReaderStates[wRdrLun].pTrFunctions->Read(Lun, &dwRespLength,
                                                       pcBuffer);
    if ( trv != TrRv_OK )
    {
        free(pcBuffer);
        return CCIDRv_ERR_TRANSPORT_ERROR;
    }
    if ( dwRespLength < sizeof(CCIDMessageBulkIn) )
    {
        free(pcBuffer);
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Reader returned too little data");
        return CCIDRv_ERR_UNSPECIFIED;        
    }
    pstresponse = (CCIDMessageBulkIn *) pcBuffer;
    // Now parse the returned value and copy it in the parameters
    *pbMessageTypeResp = pstresponse->bMessageType;
    *pbStatus = pstresponse->bStatus;
    *pbError = pstresponse->bError;
    *pbMessageSpecificResp = pstresponse->bMessageSpecific;
    dwRespLength -= sizeof(CCIDMessageBulkIn);
    BYTE bSeq = pstresponse->bSeq;
    // Check if the returned sequence byte matches that was sent
    if ( bSeq != ((BYTE)(CCIDReaderStates[wRdrLun].bSeq-1)))
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                    "CCID sequence byte returned by reader is wrong %d instead of %d",
                    bSeq, (CCIDReaderStates[wRdrLun].bSeq-1));
        return CCIDRv_ERR_WRONG_SEQUENCE;
    }
    
    
    LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "ICCStats: %s", 
                CCIDGetICCStatusMessage(CCIDGetICCStatus(*pbStatus)));
    LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "CmdStatus: %s", 
                CCIDGetCommandStatusMessage(CCIDGetCommandStatus(*pbStatus)));
    // Check for common errors
    BYTE bICCStatus = CCIDGetCommandStatus(*pbStatus) ;
    if ( CCIDGetCommandStatus(*pbStatus) ==  CCID_CMD_STATUS_TIME_REQ )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "Reader has requested time extension");
        return CCIDRv_ERR_TIME_REQUEST;
    }  
    // Modify return values only if we did not get a Time Request
    if (abDataResp != NULL)
    {
        if ( dwRespLength > *pdwDataRespLength )
        {
            dwRespLength = *pdwDataRespLength;
        }
        bcopy(pcBuffer + sizeof(CCIDMessageBulkIn), abDataResp, dwRespLength);
    }
    *pdwDataRespLength = dwRespLength;

    bzero(pcBuffer, sizeof(CCIDMessageBulkOut)+dwRespLength);
    free(pcBuffer);
    
    if ( CCIDGetCommandStatus(*pbStatus) ==  CCID_CMD_STATUS_FAILED )
    {
        // For all values pbICCStatus but RFU:
        if ( (bICCStatus != CCID_ICC_STATUS_RFU)
             &&
             (*pbError == CCID_ERR_CMD_SLOT_BUSY))
            return CCIDRv_ERR_SLOT_BUSY;
        
        if ( bICCStatus == CCID_ICC_STATUS_ABSENT )
        {
            if ( *pbError == CCID_ERR_5 )
            {
                LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "Command to inexistant slot");
                return CCIDRv_ERR_NO_SUCH_SLOT;
            }
            if ( *pbError == CCID_ERR_ICC_MUTE )
            {
                LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "Card Absent");
                return CCIDRv_ERR_CARD_ABSENT;
            }
        }
        if ( bICCStatus == CCID_ICC_STATUS_INACTIVE )
        {
            if ( *pbError == CCID_ERR_HW_ERROR )
            {
                LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "Hardware Error");
                return CCIDRv_ERR_HW_ERROR;
            }
            if ( *pbError == CCID_ERR_CMD_ABORTED )
            {
                LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "Command aborted");
                return CCIDRv_ERR_CMD_ABORTED;
            }
            if ( *pbError == CCID_ERR_BUSY_WITH_AUTO_SEQUENCE )
            {
                LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "Busy with auto sequence");
                return CCIDRv_ERR_BUSY_AUTO_SEQ;
            }
        }
        
        if ( (bICCStatus == CCID_ICC_STATUS_ACTIVE)
             &&
             (*pbError == CCID_ERR_0)
             )
        {
            LogMessage( __FILE__,  __LINE__, LogLevelVerbose, "Command unsupported");
            return CCIDRv_ERR_UNSUPPORTED_CMD;
        }

        // Return an unspecified error code
        // but higher layers might modify it
        return CCIDRv_ERR_PRIVATE_ERROR;
        
    }
    return CCIDRv_OK;
    
}


CCIDRv CCID_IccPowerOn(DWORD Lun, BYTE *abDataResp, DWORD *pdwDataRespLength)
{
    CCIDRv rv;
    WORD wRdrLun;
    BYTE bMessageTypeResp;
    BYTE bStatus;
    BYTE bError;
    BYTE bMessageSpecificResp;
    BYTE abMessageSpecificCmd[3] = "\x00\x00\x00";
    DWORD dwDataRespLengthBuffer;

    // Save value of the buffer size
    dwDataRespLengthBuffer = *pdwDataRespLength;
    wRdrLun = LunToReaderLun(Lun);
    if ( wRdrLun >= PCSCLITE_MAX_CHANNELS)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Reader Lun too large: %d", wRdrLun);
        return CCIDRv_ERR_READER_LUN;
    }
    if ( !CCIDReaderStates[wRdrLun].used)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                    "Reader Lun of unused reader: %d", wRdrLun);
        return CCIDRv_ERR_READER_LUN;
    }
    
    // Check if autopower is supported by CCID
    if (CCIDReaderStates[wRdrLun].classDesc.dwFeatures & CCID_CLASS_FEAT_AUTO_VOLT )
    {
        abMessageSpecificCmd[0] = 0x00;
    }
    else
    {
        // Power-up at 5V
        //+++ For the proper Power-Up sequence, see 7816-3:1997 section 4.2.2
        abMessageSpecificCmd[0] = 0x01;        
    }
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
                LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                            "Power On mode not supported: %02X", abMessageSpecificCmd[0]);
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
        // Might be a proprietary error
        // Check if there is customed extension
        CCIDPropExt stCCIDPropExt;
        rv = CCIDPropExtLookupExt(CCIDReaderStates[wRdrLun].dwVendorID,
                                  CCIDReaderStates[wRdrLun].dwProductID,
                                  &stCCIDPropExt);
        if ( rv != CCIDRv_OK )
        {
            return CCIDRv_ERR_UNSPECIFIED;
        }
        if ( stCCIDPropExt.PowerOn !=  NULL )
        {
            // Restore value of buffer
            *pdwDataRespLength = dwDataRespLengthBuffer;
            rv = stCCIDPropExt.PowerOn(Lun, abDataResp, pdwDataRespLength,
                                       bStatus, bError);
        }
    }
    if (  rv != CCIDRv_OK )
    {
        return rv;
    }
    if ( bMessageTypeResp != RDR_to_PC_DataBlock )
    {
        return CCIDRv_ERR_WRONG_MESG_RESP_TYPE;
    }
    return CCIDRv_OK;
}

CCIDRv CCID_IccPowerOff(DWORD Lun, BYTE *pbClockStatus)
{
    CCIDRv rv;
    WORD wRdrLun;
    BYTE bMessageTypeResp;
    BYTE bStatus;
    BYTE bError;
    BYTE bMessageSpecificResp;
    BYTE abMessageSpecificCmd[3] = "\x00\x00\x00";
    DWORD dwDataRespLength;

    wRdrLun = LunToReaderLun(Lun);
    if ( wRdrLun >= PCSCLITE_MAX_CHANNELS)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Reader Lun too large: %d", wRdrLun);
        return CCIDRv_ERR_READER_LUN;
    }
    rv =  CCID_Exchange_Command(Lun, PC_to_RDR_IccPowerOff,
                                abMessageSpecificCmd,
                                (BYTE *)"", 0,
                                &bMessageTypeResp,
                                &bStatus, &bError,
                                &bMessageSpecificResp,
                                NULL, &dwDataRespLength, 0);
    if ( rv == CCIDRv_ERR_PRIVATE_ERROR )
    {
        return CCIDRv_ERR_UNSPECIFIED;
    }
    if (  rv != CCIDRv_OK )
    {
        return rv;
    }
    if ( bMessageTypeResp != RDR_to_PC_SlotStatus )
    {
        return CCIDRv_ERR_WRONG_MESG_RESP_TYPE;
    }
    *pbClockStatus = bMessageSpecificResp;
    return CCIDRv_OK;
    
}

CCIDRv CCID_GetSlotStatus(DWORD Lun,  BYTE *pbStatus, BYTE *pbClockStatus)
{
    CCIDRv rv;
    WORD wRdrLun;
    BYTE bMessageTypeResp;
    BYTE bStatus;
    BYTE bError;
    BYTE bMessageSpecificResp;
    BYTE abMessageSpecificCmd[3] = "\x00\x00\x00";
    DWORD dwDataRespLength;

    wRdrLun = LunToReaderLun(Lun);
    if ( wRdrLun >= PCSCLITE_MAX_CHANNELS)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Reader Lun too large: %d", wRdrLun);
        return CCIDRv_ERR_READER_LUN;
    }
    rv =  CCID_Exchange_Command(Lun, PC_to_RDR_GetSlotStatus,
                                abMessageSpecificCmd,
                                (BYTE *)"", 0,
                                &bMessageTypeResp,
                                &bStatus, &bError,
                                &bMessageSpecificResp,
                                NULL, &dwDataRespLength, 0);
    if ( rv == CCIDRv_ERR_PRIVATE_ERROR )
    {
        return CCIDRv_ERR_UNSPECIFIED;
    }
    if (  rv != CCIDRv_OK )
    {
        return rv;
    }
    if ( bMessageTypeResp != RDR_to_PC_SlotStatus )
    {
        return CCIDRv_ERR_WRONG_MESG_RESP_TYPE;
    }
    *pbClockStatus = bMessageSpecificResp;
    *pbStatus = bStatus;
    return CCIDRv_OK;
    
}
CCIDRv CCID_XfrBlock(DWORD Lun, BYTE bBWI,
                     DWORD dwRequestedProtocol,
                     BYTE *abDataCmd, DWORD dwDataCmdLength,
                     BYTE *abDataResp, DWORD *pdwDataRespLength)
{
    WORD wRdrLun;
    wRdrLun = LunToReaderLun(Lun);

    if ( wRdrLun >= PCSCLITE_MAX_CHANNELS)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Reader Lun too large: %d", wRdrLun);
        return CCIDRv_ERR_READER_LUN;
    }
    if ( !CCIDReaderStates[wRdrLun].used)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                    "Reader Lun of unused reader: %d", wRdrLun);
        return CCIDRv_ERR_READER_LUN;
    }
    
    switch ( CCIDReaderStates[wRdrLun].dwExchangeLevel )
    {
        case CCID_CLASS_FEAT_EXC_LEVEL_CHAR:
        case CCID_CLASS_FEAT_EXC_LEVEL_LAPDU:
            return CCIDRv_ERR_READER_LEVEL_UNSUPPORTED;
        case CCID_CLASS_FEAT_EXC_LEVEL_TPDU:
            //+++ 1 for T=1 should be replaced by a #define
            if (dwRequestedProtocol == 1)
            {
                LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                            "Protocol type of card (T=1) not supported by this driver for this type of reader (TPDU)");                
                LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                            "An APDU-level reader should be used");                
                return CCIDRv_ERR_READER_LEVEL_UNSUPPORTED;
            }
            return CCID_XfrBlockTPDU(Lun, bBWI,
                                      abDataCmd, dwDataCmdLength,
                                      abDataResp, pdwDataRespLength);            
        case CCID_CLASS_FEAT_EXC_LEVEL_SAPDU:
            return CCID_XfrBlockSAPDU(Lun, bBWI,
                                     abDataCmd, dwDataCmdLength,
                                     abDataResp, pdwDataRespLength);
    }
    return CCIDRv_ERR_UNSPECIFIED;
}


CCIDRv CCID_XfrBlockSAPDU(DWORD Lun, BYTE bBWI,
                          BYTE *abDataCmd, DWORD dwDataCmdLength,
                          BYTE *abDataResp, DWORD *pdwDataRespLength)
{
    CCIDRv rv;
    WORD wRdrLun;
    BYTE bMessageTypeResp;
    BYTE bStatus;
    BYTE bError;
    BYTE bMessageSpecificResp;
    BYTE abMessageSpecificCmd[3] = "\x00\x00\x00";
    abMessageSpecificCmd[0] = bBWI;
    // For Short APDU, no need to change abMessageSpecificCmd[1,2]
    // as they are RFU at the moment
    
    wRdrLun = LunToReaderLun(Lun);
    if ( wRdrLun >= PCSCLITE_MAX_CHANNELS)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Reader Lun too large: %d", wRdrLun);
        return CCIDRv_ERR_READER_LUN;
    }
    if ( !CCIDReaderStates[wRdrLun].used)
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical,
                    "Reader Lun of unused reader: %d", wRdrLun);
        return CCIDRv_ERR_READER_LUN;
    }
    
    //+++ CCID spec not clear on what to do for APDUs where 
    // length(APDU)+sizeof(CCIDMessageBulkOut) > dwMaxCCIDMessageLength
    // so we just reject it
    if ( CCIDReaderStates[wRdrLun].classDesc.dwMaxCCIDMessageLength
         < (dwDataCmdLength+sizeof(CCIDMessageBulkOut)) )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, 
                    "dwMaxCCIDMessageLength is too small for supplied data");
        return CCIDRv_ERR_UNSPECIFIED;
    }

    rv = CCIDRv_ERR_UNSPECIFIED;
    BYTE bTimeExtRetry = 0;
    do
    {
        rv =  CCID_Exchange_Command(Lun, PC_to_RDR_XfrBlock,
                                    abMessageSpecificCmd,
                                    abDataCmd, dwDataCmdLength,
                                    &bMessageTypeResp,
                                    &bStatus, &bError,
                                    &bMessageSpecificResp,
                                    abDataResp, pdwDataRespLength, bTimeExtRetry);
        bTimeExtRetry = 1;
    }
    while  (rv == CCIDRv_ERR_TIME_REQUEST);

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
                LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Exchange Parity error");
                return CCIDRv_ERR_XFR_PARITY_ERROR;
            }

            if ( bError == CCID_ERR_XFR_OVERRUN )
            {
                LogMessage( __FILE__,  __LINE__, LogLevelCritical, "XFR overrun");
                return CCIDRv_ERR_XFR_OVERRUN;
            }

            if ( bError == CCID_ERR_1 )
            {
                LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Wrong dwLength");
                return CCIDRv_ERR_XFR_WRONG_DWLENGTH;
            }

            if ( bError == CCID_ERR_8 )
            {
                LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Wrong dwLevelParameter");
                return CCIDRv_ERR_XFR_WRONG_DWLEVELPARAMETER;
            }

        }
        if ( CCIDGetICCStatus(bStatus) == CCID_ICC_STATUS_ABSENT)
        {
            return CCIDRv_ERR_CARD_ABSENT;
        }        
        return CCIDRv_ERR_UNSPECIFIED;
    }
    if (  rv != CCIDRv_OK )
    {
        return rv;
    }
    if ( bMessageTypeResp != RDR_to_PC_DataBlock )
    {
        return CCIDRv_ERR_WRONG_MESG_RESP_TYPE;
    }

    return CCIDRv_OK;    
    
}




/*
 CCIDRv CCID_GetParameters(DWORD Lun,
                          BYTE *pbProtocolNum,
                          BYTE *abProtocolDataStructure,
                          DWORD *dwProtocolDataStructureLength);
CCIDRv CCID_ResetParameters(DWORD Lun,
                            BYTE *pbProtocolNum,
                            BYTE *abProtocolDataStructure,
                            DWORD *dwProtocolDataStructureLength);
CCIDRv CCID_SetParameter(DWORD Lun,
                         BYTE bProtocolNum,
                         BYTE *abSetProtocolDataStructure,
                         DWORD dwSetProtocolDataStructureLength,
                         BYTE *pbProtocolNum,
                         BYTE *abProtocolDataStructure,
                         DWORD *dwProtocolDataStructureLength);
*/

CCIDRv CCID_Escape(DWORD Lun,
                   BYTE *abDataCmd, DWORD dwDataCmdLength,
                   BYTE *abDataResp, DWORD *pdwDataRespLength,
                   BYTE *pbErrorSpecific)
{
    CCIDRv rv;
    BYTE bMessageTypeResp;
    BYTE bStatus;
    BYTE bError;
    BYTE bMessageSpecificResp;
    *pbErrorSpecific = 0;
    rv =  CCID_Exchange_Command(Lun, PC_to_RDR_Escape,
                                (BYTE *)"\x00\x00\x00",
                                abDataCmd, dwDataCmdLength,
                                &bMessageTypeResp,
                                &bStatus, &bError,
                                &bMessageSpecificResp,
                                abDataResp, pdwDataRespLength, 0);
    if ( rv == CCIDRv_ERR_PRIVATE_ERROR )
    {
        if ( CCIDGetICCStatus(bStatus) == CCID_ICC_STATUS_ACTIVE )
        {
            *pbErrorSpecific = bError;
            return CCIDRv_ERR_MANUFACTURER_ERROR;
        }

        return CCIDRv_ERR_UNSPECIFIED;
    }
    if (  rv != CCIDRv_OK )
    {
        return rv;
    }
    if ( bMessageTypeResp != RDR_to_PC_Escape )
    {
        LogMessage( __FILE__,  __LINE__, LogLevelCritical, "Wrong response type from reader");
        return CCIDRv_ERR_WRONG_MESG_RESP_TYPE;
    }

    return CCIDRv_OK;
}

//++ TPDU mode will only work with T=0 cards
CCIDRv CCID_XfrBlockTPDU(DWORD Lun, BYTE bBWI,
                          BYTE *abDataCmd, DWORD dwDataCmdLength,
                          BYTE *abDataResp, DWORD *pdwDataRespLength)
{
    DWORD dwDataRespLengthBuffer;
    CCIDRv rv;
    BYTE abGetResponse[] = {0x00, 0xC0, 0x00, 0x00, 0x00};
    BYTE bLe;
    DWORD dwAPDULength;
    
    // Save the value of the buffer length in a get response needs to be run
    dwDataRespLengthBuffer = *pdwDataRespLength;
    
    //++ For now, use the SAPDU call
    rv = CCID_XfrBlockSAPDU(Lun, bBWI,
                            abDataCmd, dwDataCmdLength,
                            abDataResp, pdwDataRespLength);

    if ( rv == CCIDRv_OK )
    {
        // Check if we did not just get a 4-byte Case 0 APDU
        if ( dwDataCmdLength < 5 )
        {
            // No Lc was provided
            return rv;
        }
        //++ This will not work for extended APDUs
        // Check if we received a fully qualified APDU Case 4 APDU
        // Size of the command should be
        // CLA + INS + P1 + P2 + Lc + value(Lc) + Le
        dwAPDULength = 1 + 1 + 1 + 1 + 1 + abDataCmd[4] + 1;
        if ( dwDataCmdLength ==  dwAPDULength )
        {
            bLe = abDataCmd[dwAPDULength-1];
            // This is really a Case 4 
            // Check if a Get response should be placed
            // Case 4S.3 : "Command accepted with information added"
            if ( (*pdwDataRespLength == 2)
                 && (abDataResp[0] = 0x61))
            {
                // Card sent a 61 Lx
                // Send get response with P3 = min(Le, Lx)
                // minimum is a bit odd as L? = 00 means 256 (0x100)
                if ( bLe == 0x00 )
                {
                    // abDataResp[1] can only be smaller that bLe
                    // so change value before the test
                    bLe = abDataResp[1];
                }
                if ( abDataResp[1] == 0x00 )
                {
                    // abDataResp[1] can only be smaller that bLe
                    // so change value before the test
                    abDataResp[1] = bLe;
                }
                abGetResponse[4] = (bLe < abDataResp[1]) ? bLe: abDataResp[1];
                // Set-up buffer response to initial value
                *pdwDataRespLength = dwDataRespLengthBuffer;
                rv = CCID_XfrBlockSAPDU(Lun, bBWI,
                                        abGetResponse, sizeof(abGetResponse),
                                        abDataResp, pdwDataRespLength);
                goto end;
            }
            // Case 4S.2 : "Command accepted"
            if ( (*pdwDataRespLength == 2)
                 && (abDataResp[0] = 0x90)
                 && (abDataResp[0] = 0x00))
            {
                // Card sent a 9000
                // Send a Get Response with the length
                // included in the APDU
                abGetResponse[4] = bLe;
                // Set-up buffer response to initial value
                *pdwDataRespLength = dwDataRespLengthBuffer;
                rv = CCID_XfrBlockSAPDU(Lun, bBWI,
                                        abGetResponse, sizeof(abGetResponse),
                                        abDataResp, pdwDataRespLength);
            }
        }
    }
end:
    return rv;    
}

/*

 CCIDRv CCID_IccClock(DWORD Lun, BYTE bClockCommand,
                      BYTE *pbClockStatus);
 CCIDRv CCID_T0APDU(DWORD Lun, BYTE bmChanges, BYTE bClassGetResponse
                    BYTE bClassEnvelope,
                    BYTE *pbClockStatus);
 // CCID_Secure IS CURRENTLY NOT SUPPORTED
 // FUNCTION PROTOTYPE WILL CHANGE WHEN IT IS
 CCIDRv CCID_Secure(DWORD Lun);
 CCIDRv CCID_Mechanical(DWORD Lun, BYTE bFunction,
                        BYTE *pbClockStatus);
 CCIDRv CCID_Abort(DWORD Lun, BYTE *pbClockStatus);
 CCIDRv CCID_SetDataRateAndClockFrequency(DWORD Lun,
                                          DWORD dwClockFrequencyCmd,
                                          DWORD dwDataRateCmd,
                                          DWORD *pdwClockFrequencyResp,
                                          DWORD *pdwDataRateResp);
*/ 

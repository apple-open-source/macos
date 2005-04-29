/*
 *
 *  Created by JL Giraud <jlgiraud@mac.com>.
 *  Copyright (c) 2003 Jean-Luc Giraud. All rights reserved.
 *  See COPYING file for license.
 *
 */

#ifndef __CCID_H__
#define __CCID_H__
#include <CoreFoundation/CoreFoundation.h>
#include "wintypes.h"
#include "pcscdefines.h"
#include "transport.h"

#define CCID_DESC_TYPE (0x21)
#define CCID_DESC_SIZE (0x36)
// Data strutuctures definitions
typedef struct
{
    UInt8  bLength                __attribute__ ((packed));
    UInt8  bDescriptorType        __attribute__ ((packed));
    UInt16 bcdCCID                __attribute__ ((packed));
    UInt8  bMaxSlotIndex          __attribute__ ((packed));
    UInt8  bVoltageSupport        __attribute__ ((packed));
    UInt32 dwProtocols            __attribute__ ((packed));
    UInt32 dwDefaultClock         __attribute__ ((packed));
    UInt32 dwMaximumClock         __attribute__ ((packed));
    UInt8  bNumCockSupported      __attribute__ ((packed));
    UInt32 dwDataRate             __attribute__ ((packed));
    UInt32 dwMaxDataRate          __attribute__ ((packed));
    UInt8  bNumDataRatesSupported __attribute__ ((packed));
    UInt32 dwMaxIFSD              __attribute__ ((packed));
    UInt32 dwSynchProtocols       __attribute__ ((packed));
    UInt32 dwMechanical           __attribute__ ((packed));
    UInt32 dwFeatures             __attribute__ ((packed));
    UInt32 dwMaxCCIDMessageLength __attribute__ ((packed));
    UInt8  bClassGetResponse      __attribute__ ((packed));
    UInt8  bClassEnvelope         __attribute__ ((packed));
    UInt16 wLcdLayout             __attribute__ ((packed));
    UInt8  bPINSupport            __attribute__ ((packed));
    UInt8  bMaxCCIDBusySlots      __attribute__ ((packed));
} CCIDClassDescriptor;


#define OFFSET_bLength                  0
#define OFFSET_bDescriptorType          1
#define OFFSET_bcdCCID                  2
#define OFFSET_bMaxSlotIndex            4
#define OFFSET_bVoltageSupport          5
#define OFFSET_dwProtocols              6
#define OFFSET_dwDefaultClock          10
#define OFFSET_dwMaximumClock          14
#define OFFSET_bNumCockSupported       18
#define OFFSET_dwDataRate              19
#define OFFSET_dwMaxDataRate           23
#define OFFSET_bNumDataRatesSupported  27
#define OFFSET_dwMaxIFSD               28
#define OFFSET_dwSynchProtocols        32
#define OFFSET_dwMechanical            36
#define OFFSET_dwFeatures              40
#define OFFSET_dwMaxCCIDMessageLength  44
#define OFFSET_bClassGetResponse       48
#define OFFSET_bClassEnvelope          49
#define OFFSET_wLcdLayout              50
#define OFFSET_bPINSupport             52
#define OFFSET_bMaxCCIDBusySlots       53


/*
     From CCID spec:
     offset, name, size
     0 bLength 1 
     1 bDescriptorType 1 
     2 bcdCCID 2 
     4 bMaxSlotIndex 1
     5 bVoltageSupport 1
     6 dwProtocols 4 
     10 dwDefaultClock 4
     14 dwMaximumClock 4
     18 bNumCockSupported 1
     19 dwDataRate 4 
     23 dwMaxDataRate 4
     27 bNumDataRatesSupported 1 
     28 dwMaxIFSD 4 
     32 dwSynchProtocols 4
     36 dwMechanical 4 
     40 dwFeatures 4
     44 dwMaxCCIDMessageLength 4 
     48 bClassGetResponse 1
     49 bClassEnvelope 1  
     50 wLcdLayout 2 
     52 bPINSupport 1
     53 bMaxCCIDBusySlots 1
     
     */


// Standard CCID message descriptor
typedef struct
{
    BYTE bMessageType                  __attribute__ ((packed));
    DWORD dwLength                     __attribute__ ((packed));
    BYTE bSlot                         __attribute__ ((packed));
    BYTE bSeq                          __attribute__ ((packed));
    BYTE bMessageSpecific1             __attribute__ ((packed));
    BYTE bMessageSpecific2             __attribute__ ((packed));
    BYTE bMessageSpecific3             __attribute__ ((packed));
} CCIDMessageBulkOut;

typedef struct
{
    BYTE bMessageType                  __attribute__ ((packed));
    DWORD dwLength                     __attribute__ ((packed));
    BYTE bSlot                         __attribute__ ((packed));
    BYTE bSeq                          __attribute__ ((packed));
    BYTE bStatus                       __attribute__ ((packed));
    BYTE bError                        __attribute__ ((packed));
    BYTE bMessageSpecific              __attribute__ ((packed));
} CCIDMessageBulkIn;

#define CCID_CLASS_PROTOCOL_T0  0x00000001
#define CCID_CLASS_PROTOCOL_T1  0x00000002



// Related to dwFeatures in Class Desc.
#define CCID_CLASS_FEAT_AUTO_CONF_ATR  0x00000002
#define CCID_CLASS_FEAT_AUTO_ACT       0x00000004
#define CCID_CLASS_FEAT_AUTO_VOLT      0x00000008
#define CCID_CLASS_FEAT_AUTO_CLOCK     0x00000010
#define CCID_CLASS_FEAT_AUTO_BAUD      0x00000020
#define CCID_CLASS_FEAT_AUTO_PPS_PROP  0x00000040
#define CCID_CLASS_FEAT_AUTO_PPS_CUR   0x00000080
#define CCID_CLASS_FEAT_CLOCK_STOP     0x00000100
#define CCID_CLASS_FEAT_NAD_NON_0      0x00000200
#define CCID_CLASS_FEAT_AUTO_IFSD      0x00000400
// MASk to get value of Exchange level
#define CCID_CLASS_FEAT_EXC_LEVEL_MASK  0x00070000
#define CCID_CLASS_FEAT_EXC_LEVEL_CHAR  0x00000000
#define CCID_CLASS_FEAT_EXC_LEVEL_TPDU  0x00010000
#define CCID_CLASS_FEAT_EXC_LEVEL_SAPDU 0x00020000
#define CCID_CLASS_FEAT_EXC_LEVEL_LAPDU 0x00040000


// Used to store the state of a slot for a reader
typedef struct {
    DWORD dwProtocol;
    DWORD nATRLength;
    UCHAR pcATRBuffer[MAX_ATR_SIZE];
    UCHAR bPowerFlags;    
} CCIDSlotState;

typedef struct {
    BYTE used;
    CCIDClassDescriptor classDesc;
    TrFunctions *pTrFunctions;
    BYTE bSeq;
    BYTE bMaxSlotIndex;
    BYTE bMaxCCIDBusySlots;
    BYTE bCurrentCCIDBusySlots;
    DWORD dwProductID;
    DWORD dwVendorID;
    DWORD dwMaxCCIDMessageLength;
    // Use this instead of class Desc as it maybe
    // modified in initialisation of some readers
    DWORD dwExchangeLevel;
    CCIDSlotState *slotStates;
} tIo;
// Used to store the state of a reader (per reader Lun)
typedef struct {
    BYTE used;
    CCIDClassDescriptor classDesc;
    TrFunctions *pTrFunctions;
    BYTE bSeq;
    BYTE bMaxSlotIndex;
    BYTE bMaxCCIDBusySlots;
    BYTE bCurrentCCIDBusySlots;
    DWORD dwProductID;
    DWORD dwVendorID;
    DWORD dwMaxCCIDMessageLength;
    // Use this instead of class Desc as it maybe
    // modified in initialisation of some readers
    DWORD dwExchangeLevel;
    CCIDSlotState *slotStates;
} CCIDReaderState;


// values of bMessageType
#define PC_to_RDR_IccPowerOn                   0x62
#define PC_to_RDR_IccPowerOff                  0x63
#define PC_to_RDR_GetSlotStatus                0x65
#define PC_to_RDR_XfrBlock                     0x6F
#define PC_to_RDR_GetParameters                0x6C
#define PC_to_RDR_ResetParameters              0x6D
#define PC_to_RDR_SetParameter                 0x61
#define PC_to_RDR_Escape                       0x6B
#define PC_to_RDR_IccClock                     0x6E
#define PC_to_RDR_T0APDU                       0x6A
#define PC_to_RDR_Secure                       0x69
#define PC_to_RDR_Mechanical                   0x71
#define PC_to_RDR_Abort                        0x72
#define PC_to_RDR_SetDataRateAndClockFrequency 0x73
#define RDR_to_PC_DataBlock                    0x80
#define RDR_to_PC_SlotStatus                   0x81
#define RDR_to_PC_Parameters                   0x82
#define RDR_to_PC_Escape                       0x83
#define RDR_to_PC_DataRateAndClockFrequency    0x84

typedef enum {
    CCIDRv_OK                                 = 0x00,
    CCIDRv_ERR_UNSPECIFIED                    = 0x01,
    CCIDRv_ERR_NO_IMPLEMENTED                 = 0x02,
    CCIDRv_ERR_READER_LUN                     = 0x03,
    CCIDRv_ERR_SLOT_LUN                       = 0x04,
    CCIDRv_ERR_SLOTS_BUSY                     = 0x05,
    CCIDRv_ERR_CLASS_DESC_INVALID             = 0x06,
    CCIDRv_ERR_TRANSPORT_ERROR                = 0x07,
    CCIDRv_ERR_VALUE_NOT_FOUND                = 0x08,
    CCIDRv_ERR_WRONG_MESG_RESP_TYPE           = 0x09,
    CCIDRv_ERR_TIME_REQUEST                   = 0x0A,
    CCIDRv_ERR_NO_SUCH_SLOT                   = 0x0B,
    CCIDRv_ERR_UNSUPPORTED_CMD                = 0x0C,
    CCIDRv_ERR_SLOT_BUSY                      ,
    //CCIDRv_ERR_NO_ICC_PRESENT               ,
    CCIDRv_ERR_CARD_ABSENT                    ,
    CCIDRv_ERR_HW_ERROR                       ,
    CCIDRv_ERR_CMD_ABORTED                    ,
    CCIDRv_ERR_BUSY_AUTO_SEQ                  ,
    CCIDRv_ERR_POWERON_MODE_UNSUPPORTED       ,
    CCIDRv_ERR_ICC_MUTE                       ,
    CCIDRv_ERR_ATR_PARITY_ERROR               ,
    CCIDRv_ERR_BAD_ATR_TS                     ,
    CCIDRv_ERR_BAD_ATR_TCK                    ,
    CCIDRv_ERR_PROTOCOL_NOT_SUPPORTED         ,
    CCIDRv_ERR_CLASS_NOT_SUPPORTED            ,
    CCIDRv_ERR_MANUFACTURER_ERROR             ,
    CCIDRv_ERR_READER_LEVEL_UNSUPPORTED       ,
    CCIDRv_ERR_XFR_PARITY_ERROR               ,
    CCIDRv_ERR_XFR_OVERRUN                    ,
    CCIDRv_ERR_XFR_WRONG_DWLENGTH             ,
    CCIDRv_ERR_XFR_WRONG_DWLEVELPARAMETER     ,
    CCIDRv_ERR_WRONG_SEQUENCE                 ,
    //    CCIDRv_ERR_
    //    CCIDRv_ERR_
    // Error code below means that higher CCID layer
    // should try to parse the status and error
    // returned by the reder.
    // This code should not be returned by any
    // non private CCID function
    CCIDRv_ERR_PRIVATE_ERROR
} CCIDRv;



void CCIDParseDesc(BYTE * pcbuffer, CCIDClassDescriptor *classDesc);
void CCIDPrintDesc(CCIDClassDescriptor classDesc);

// Parses the bStatus byte returned in a Bulk-IN
// message to return bmICCStatus
BYTE CCIDGetICCStatus(BYTE bStatus);
// Gets a pointer to a user friendly message
const char *CCIDGetICCStatusMessage(BYTE bICCStatus);
// Parses the bStatus byte returned in a Bulk-IN
// message to return bmCommandStatus
BYTE CCIDGetCommandStatus(BYTE bStatus);
// Gets a pointer to a user friendly message
const char *CCIDGetCommandStatusMessage(BYTE bCommandStatus);


CCIDRv CCID_OpenChannel(DWORD Lun, DWORD ChannelID);
CCIDRv CCID_CloseChannel(DWORD Lun);

                             

CCIDRv CCID_IccPowerOn(DWORD Lun, BYTE *abDataResp, DWORD *pdwDataRespLength);
CCIDRv CCID_IccPowerOff(DWORD Lun, BYTE *pbClockStatus);                
CCIDRv CCID_GetSlotStatus(DWORD Lun, BYTE *pbStatus, BYTE *pbClockStatus);
// XfrBlock expects to receive APDU level commands
// and manages the communication with the CCID reader
// transparently according to the reader communication level
CCIDRv CCID_XfrBlock(DWORD Lun, BYTE bBWI,
                     DWORD dwRequestedProtocol,
                     BYTE *abDataCmd, DWORD dwDataCmdLength,
                     BYTE *abDataResp, DWORD *pdwDataRespLength);
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


CCIDRv CCID_Escape(DWORD Lun,
                   BYTE *abDataCmd, DWORD dwDataCmdLength,
                   BYTE *abDataResp, DWORD *pdwDataRespLength,
                   BYTE *pbErrorSpecific);

CCIDRv CCID_IccClock(DWORD Lun, BYTE bClockCommand,
                     BYTE *pbClockStatus);
CCIDRv CCID_T0APDU(DWORD Lun, BYTE bmChanges, BYTE bClassGetResponse,
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




#define CCID_ERR_CMD_ABORTED                   0xFF
#define CCID_ERR_ICC_MUTE                      0xFE
#define CCID_ERR_XFR_PARITY_ERROR              0xFD
#define CCID_ERR_XFR_OVERRUN                   0xFC
#define CCID_ERR_HW_ERROR                      0xFB
#define CCID_ERR_BAD_ATR_TS                    0xF8
#define CCID_ERR_BAD_ATR_TCK                   0xF7
#define CCID_ERR_ICC_PROTOCOL_NOT_SUPPORTED    0xF6
#define CCID_ERR_ICC_CLASS_NOT_SUPPORTED       0xF5
#define CCID_ERR_PROCEDURE_BYTE_CONFLICT       0xF4
#define CCID_ERR_DEACTIVATED_PROTOCOL          0xF3
#define CCID_ERR_BUSY_WITH_AUTO_SEQUENCE       0xF2
#define CCID_ERR_PIN_TIMEOUT                   0xF0
#define CCID_ERR_PIN_CANCELLED                 0xEF
#define CCID_ERR_CMD_SLOT_BUSY                 0xE0

// Errors with no officla names in spec
#define CCID_ERR_0                             0x00
#define CCID_ERR_1                             0x01
#define CCID_ERR_5                             0x05
#define CCID_ERR_7                             0x07
#define CCID_ERR_8                             0x08


#define CCID_CMD_STATUS_SUCCESS                0
#define CCID_CMD_STATUS_FAILED                 1
#define CCID_CMD_STATUS_TIME_REQ               2
#define CCID_CMD_STATUS_RFU                    3

#define CCID_ICC_STATUS_ACTIVE                 0
#define CCID_ICC_STATUS_INACTIVE               1
#define CCID_ICC_STATUS_ABSENT                 2
#define CCID_ICC_STATUS_RFU                    3

#define CCID_SLOT_STATUS_CLK_RUNNING           0x00
#define CCID_SLOT_STATUS_CLK_STOPPED_H         0x01
#define CCID_SLOT_STATUS_CLK_STOPPED_L         0x02
#define CCID_SLOT_STATUS_CLK_STOPPED_UNKNOWN   0x03



#define MASK_ICC_STATUS (0x03)
#define OFFSET_ICC_STATUS (0)
#define MASK_COMMAND_STATUS (0xC0)
#define OFFSET_COMMAND_STATUS (6)




#endif
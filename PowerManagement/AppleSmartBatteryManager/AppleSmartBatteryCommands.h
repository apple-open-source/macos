/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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

#ifndef __AppleSmartBatteryCommands__
#define __AppleSmartBatteryCommands__


/*
 * This file contains constants defined in Smart Battery and Smart Battery 
 * System Manager specs. 
 *
 * kM - All system manager constants begin with kM
 * kB - All smart battery constants begin with kB
 *
 */


/* SMBus Device addresses */
enum {
    kSMBusAppleDoublerAddr                  = 0xd,
    kSMBusBatteryAddr                       = 0xb,
    kSMBusManagerAddr                       = 0xa,
    kSMBusChargerAddr                       = 0x9
};

/*  System Manager Commands                             */
/*  Smart Battery System Manager spec - rev 1.1         */
/*  Section 5 SBSM Interface                            */
enum {
    kMStateCmd                              = 0x01,
    kMStateContCmd                          = 0x02
};

/*  SBSM BatterySystemState bitfields                   */
/*  Smart Battery System Manager spec - rev 1.1         */
/*  Section 5.1                                         */
enum {
    kMChargingBatt_A_Bit                    = 0x0010,
    kMPresentBatt_A_Bit                     = 0x0001
};

/*  SBSM BatterySystemStateCont bitfields               */
/*  Smart Battery System Manager spec - rev 1.1         */
/*  Section 5.2                                         */
enum {
    kMACPresentBit                          = 0x0001,
    kMPowerNotGoodBit                       = 0x0002,
    kMCalibrateBit                          = 0x0040,
};

/*  Smart Battery Commands                              */
/*  Smart Battery Data Specification - rev 1.1          */
/*  Section 5.1 SMBus Host to Smart Battery Messages    */

/* kBManufactureDateCmd
 * Date is published in a bitfield per the Smart Battery Data spec rev 1.1
 * in section 5.1.26
 *   Bits 0...4 => day (value 1-31; 5 bits)
 *   Bits 5...8 => month (value 1-12; 4 bits)
 *   Bits 9...15 => years since 1980 (value 0-127; 7 bits)
 *
 * Custom commands not specific to 'Smart Battery Data spec' are defined
 * with lower 16-bits set to 0.
 */
enum {
    kBManufacturerAccessCmd           = 0x00,     // READ/WRITE WORD
    kBTemperatureCmd                  = 0x08,     // READ WORD
    kBVoltageCmd                      = 0x09,     // READ WORD
    kBCurrentCmd                      = 0x0a,     // READ WORD
    kBAverageCurrentCmd               = 0x0b,     // READ WORD
    kBMaxErrorCmd                     = 0x0c,     // READ WORD
    kBRemainingCapacityCmd            = 0x0f,     // READ WORD
    kBFullChargeCapacityCmd           = 0x10,     // READ WORD
    kBRunTimeToEmptyCmd               = 0x11,     // READ WORD
    kBAverageTimeToEmptyCmd           = 0x12,     // READ WORD
    kBAverageTimeToFullCmd            = 0x13,     // READ WORD
    kBChargingCurrentCmd              = 0x14,     // READ/WRITE WORD
    kBBatteryStatusCmd                = 0x16,     // READ WORD
    kBCycleCountCmd                   = 0x17,     // READ WORD
    kBDesignCapacityCmd               = 0x18,     // READ WORD
    kBManufactureDateCmd              = 0x1b,     // READ WORD
    kBSerialNumberCmd                 = 0x1c,     // READ WORD
    kBManufactureNameCmd              = 0x20,     // READ BLOCK
    kBDeviceNameCmd                   = 0x21,     // READ BLOCK
    kBManufactureDataCmd              = 0x23,     // READ BLOCK
    kBReadCellVoltage4Cmd             = 0x3c,     // READ WORD
    kBReadCellVoltage3Cmd             = 0x3d,     // READ WORD
    kBReadCellVoltage2Cmd             = 0x3e,     // READ WORD
    kBReadCellVoltage1Cmd             = 0x3f,     // READ WORD
    kBManufacturerInfoCmd             = 0x70,     // READ BLOCK
    kBDesignCycleCount9CCmd           = 0x9C,     // READ WORD
    kBPackReserveCmd                  = 0x8B,     // READ WORD
    kAdapterStatus                    = 0x0d00,
    kChargerDataCmd                   = 0x1100,
    kBatteryFCCDataCmd                = 0x1200,
    kBatteryDataCmd                   = 0x1300,
};


/*  Smart Battery Extended Registers                    */
/*  bq20z90-V110 + bq29330 Chipset Technical Reference Manual    */
/*  TI Literature SLUU264                               */
enum {
    kBExtendedPFStatusCmd             = 0x53,     // READ WORD
    kBExtendedOperationStatusCmd      = 0x54      // READ WORD
};

/* Apple Hardware Serial Number */
enum {
    kBAppleHardwareSerialCmd          = 0x76      // READ BLOCK
};

/*  Smart Battery Status Message Bits                   */
/*  Smart Battery Data Specification - rev 1.1          */
/*  Section 5.4 page 42                                 */
enum {
    kBTerminateChargeAlarmBit         = 0x4000,
    kBTerminateDischargeAlarmBit      = 0x0800,
    kBFullyChargedStatusBit           = 0x0020,
    kBFullyDischargedStatusBit        = 0x0010
};


//=====================================

/*
 * Support for external transactions (from user space)
 */
enum {
    kEXDefaultBatterySelector = 0,
    kEXManagerSelector = 1
};

enum {
    kEXReadWord     = 0,
    kEXWriteWord    = 1,
    kEXReadBlock    = 2,
    kEXWriteBlock   = 3,
    kEXReadByte     = 4,
    kEXWriteByte    = 5,
    kEXSendByte     = 6
};

enum {
    kEXFlagRetry  = 1,
    kEXFlagUsePEC = 2
};

#define MAX_SMBUS_DATA_SIZE     32

// * WriteBlock note
// rdar://5433060
// For block writes, clients always increment inByteCount +1
// greater than the actual byte count. (e.g. 32 bytes to write becomes a 33 byte count.)
// Other types of transactions are not affected by this workaround.
typedef struct {
    uint8_t         flags;
    uint8_t         type;
    uint8_t         batterySelector;
    uint8_t         address;
    uint8_t         inByteCount;
    uint8_t         inBuf[MAX_SMBUS_DATA_SIZE];
} EXSMBUSInputStruct;

typedef struct {
    uint32_t        status;
    uint32_t        outByteCount;
    uint8_t        outBuf[MAX_SMBUS_DATA_SIZE];
} EXSMBUSOutputStruct;

typedef enum {
    kASBMSMBUSReadWord = kEXReadWord,
    kASBMSMBUSWriteWord = kEXWriteWord,
    
    kASBMSMBUSReadBlock = kEXReadBlock,
    kASBMSMBUSWriteBlock = kEXWriteBlock,
    
    kASBMSMBUSReadByte = kEXReadByte,
    kASBMSMBUSWriteByte = kEXWriteByte,
    kASBMSMBUSSendByte = kEXSendByte,
    
    kASBMSMBUSExtendedReadWord,
    kASBMSMCReadBytes,
    kASBMSMCReadBool,
    kASBMSMCReadDictionary,
    kASBMInvalidOp
} ASBMgrOpType;


typedef uint8_t ASBMgrAddress;
typedef uint8_t ASBMgrCmd;

typedef void (*ASBMgrTransactionCompletion)(OSObject * target, void * ref,  IOReturn status, size_t bytes, uint8_t *data);


typedef struct {
    uint8_t     key;
    uint32_t    data;
} ASBMgrSmcTransaction;

typedef struct {
    ASBMgrOpType    opType;
#if TARGET_OS_OSX
    IOSMBusAddress	address;
    IOSMBusCommand	command;
#endif
    IOByteCount     outSize;
    uint8_t         *outData;
    bool            fullyDischarged;

    ASBMgrTransactionCompletion completionHandler;
} ASBMgrRequest;


#endif

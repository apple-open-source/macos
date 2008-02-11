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

#ifndef __AppleSmartBatteryKeys__
#define __AppleSmartBatteryKeys__


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
    kSMBusBatteryAddr                       = 0xb,
    kSMBusManagerAddr                       = 0xa,
    kSMBusChargerAddr                       = 0x9
};

/*  Charger Commands                                    */
/*  Smart Battery Charger spec - rev 1.1                */
/*  Section 5 Charger Interface                         */
enum {
    kCChargerModeCmd                        = 0x12
};

/*  ChargerMode bitfields                               */
enum {
    kCInhibitChargeBit                      = 0x01
};

/*  System Manager Commands                             */
/*  Smart Battery System Manager spec - rev 1.1         */
/*  Section 5 SBSM Interface                            */
enum {
    kMStateCmd                              = 0x01,
    kMStateContCmd                          = 0x02,
    kMInfoCmd                               = 0x04    
};

/*  SBSM BatterySystemState bitfields                   */
/*  Smart Battery System Manager spec - rev 1.1         */
/*  Section 5.1                                         */
enum {
    kMPoweredByACBit                        = 0x0000,
    kMPoweredByBatt_A_Bit                   = 0x0100,
    kMChargingBatt_A_Bit                    = 0x0010,
    kMPresentBatt_A_Bit                     = 0x0001
};

/*  SBSM BatterySystemStateCont bitfields               */
/*  Smart Battery System Manager spec - rev 1.1         */
/*  Section 5.2                                         */
enum {
    kMACPresentBit                          = 0x0001,
    kMPowerNotGoodBit                       = 0x0002,
    kMCalibrateRequestSupportBit            = 0x0004,
    kMCalibrateRequestBit                   = 0x0008,
    kMChargeInhibitBit                      = 0x0010,
    kMChargerPowerOnResetBit                = 0x0020,
    kMCalibrateBit                          = 0x0040,
/* Bits 14 & 15 convey information about charges that were
   terminated for temperature reasons
 */
    kMReservedNoChargeBit14                  = 0x4000,
    kMReservedNoChargeBit15                  = 0x8000
};

/*  SBSM BatterySystemStateInfo bitfields               */
/*  Smart Battery System Manager spec - rev 1.1         */
/*  Section 5.3                                         */
enum {
    kMBattPresent_A_Bit                     = 0x0001,
    kMBattPresent_B_Bit                     = 0x0002,
    kMBattPresent_C_Bit                     = 0x0004,
    kMBattPresent_D_Bit                     = 0x0008
};

/*  Smart Battery Commands                              */
/*  Smart Battery Data Specification - rev 1.1          */
/*  Section 5.1 SMBus Host to Smart Battery Messages    */
enum {
    kBManufacturerAccessCmd           = 0x00,     // READ/WRITE WORD
    kBRemainingCapacityAlarmCmd       = 0x01,     // READ/WRITE WORD
    kBRemainingTimeAlarmCmd           = 0x02,     // READ/WRITE WORD
    kBBatteryModeCmd                  = 0x03,     // READ/WRITE WORD
    kBAtRateCmd                       = 0x04,     // READ/WRITE WORD
    kBAtRateTimeToFullCmd             = 0x05,     // READ WORD
    kBAtRateTimeToEmptyCmd            = 0x06,     // READ WORD
    kBAtRateOKCmd                     = 0x07,     // READ WORD
    kBTemperatureCmd                  = 0x08,     // READ WORD
    kBVoltageCmd                      = 0x09,     // READ WORD
    kBCurrentCmd                      = 0x0a,     // READ WORD
    kBAverageCurrentCmd               = 0x0b,     // READ WORD
    kBMaxErrorCmd                     = 0x0c,     // READ WORD
    kBRelativeStateOfChargeCmd        = 0x0d,     // READ WORD
    kBAbsStateOfChargeCmd             = 0x0e,     // READ WORD
    kBRemainingCapacityCmd            = 0x0f,     // READ WORD
    kBFullChargeCapacityCmd           = 0x10,     // READ WORD
    kBRunTimeToEmptyCmd               = 0x11,     // READ WORD
    kBAverageTimeToEmptyCmd           = 0x12,     // READ WORD
    kBAverageTimeToFullCmd            = 0x13,     // READ WORD
    kBChargingCurrentCmd              = 0x14,     // READ/WRITE WORD
    kBBatteryStatusCmd                = 0x16,     // READ WORD
    kBCycleCountCmd                   = 0x17,     // READ WORD
    kBDesignCapacityCmd               = 0x18,     // READ WORD
    kBDesignVoltageCmd                = 0x19,     // READ WORD
    kBSpecificationInfoCmd            = 0x1a,     // READ WORD
    kBManufactureDateCmd              = 0x1b,     // READ WORD
    kBSerialNumberCmd                 = 0x1c,     // READ WORD
    kBManufactureNameCmd              = 0x20,     // READ BLOCK
    kBDeviceNameCmd                   = 0x21,     // READ BLOCK
    kBDeviceChemistryCmd              = 0x22,     // READ BLOCK
    kBManufactureDataCmd              = 0x23,     // READ BLOCK
/* Cell Voltage */
    kBReadCellVoltage4Cmd             = 0x3c,     // READ WORD
    kBReadCellVoltage3Cmd             = 0x3d,     // READ WORD
    kBReadCellVoltage2Cmd             = 0x3e,     // READ WORD
    kBReadCellVoltage1Cmd             = 0x3f      // READ WORD
};

/*  Battery Mode Bits                                   */
/*  Smart Battery Data Specification - rev 1.1          */
/*  Section 5.1.4 page 15                               */
enum {
    kBInternalChargeControllerBit     = 0x00,
    kBPrimaryBattSupportBit           = 0x01,
    kBConditionFlagBit                = 0x07,
    kBChargeControllerEnabledBit      = 0x08,
    kBPrimaryBatteryBit               = 0x09,
    kBAlarmModeBit                    = 0x0d,
    kBChargerModeBit                  = 0x0e,
    kBCapacityModeBit                 = 0x0f
};



/*  Smart Battery Status Message Bits                   */
/*  Smart Battery Data Specification - rev 1.1          */
/*  Section 5.4 page 42                                 */
enum {
    kBOverChargedAlarmBit             = 0x8000,
    kBTerminateChargeAlarmBit         = 0x4000,
    kBOverTempAlarmBit                = 0x1000,
    kBTerminateDischargeAlarmBit      = 0x0800,
    kBRemainingCapacityAlarmBit       = 0x0200,
    kBRemainingTimeAlarmBit           = 0x0100,
    
    kBInitializedStatusBit            = 0x0080,
    kBDischargingStatusBit            = 0x0040,
    kBFullyChargedStatusBit           = 0x0020,
    kBFullyDischargedStatusBit        = 0x0010
};



/*  Smart Battery Critical Messages                     */
/*  Smart Battery Data Specification - rev 1.1          */
/*  Section 5.4                                         */
enum {
    kBAlarmWarningMsg                 = 0x16
};

#endif

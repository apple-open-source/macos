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

#include <AppleFeatures/AppleFeatures.h>
#include <IOKit/IOService.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IOKitKeysPrivate.h>
#include <libkern/c++/OSObject.h>
#include <kern/clock.h>
#include "AppleSmartBatteryManager.h"
#include "AppleSmartBattery.h"
#include "AppleSmartBatteryKeys.h"
#include "AppleSmartBatteryKeysPrivate.h"
#include "AppleSmartBatteryCommands.h"
#include "AppleSmartBatteryHFDataClient.h"
#include "battery/adapter.h"
#include "battery/smcaccessoryinfo_defs.h"
#include "battery/powerTelemetry_defs.h"
#include "battery/powerDeliveryShared.h"
#include "battery/battery_data_log.h"

#if TARGET_OS_IPHONE || TARGET_OS_OSX_AS
#include "battery/charger.h"
#endif

typedef struct CommandStruct_s {
    uint32_t cmd;
    int addr;
    ASBMgrOpType opType;
    uint32_t smcKey;
    int nbytes;
    const OSSymbol *setItAndForgetItSym;
    int pathBits;
    bool supportDesktops;
} CommandStruct;

enum {
    kSecondsUntilValidOnWake    = 30,
    kPostChargeWaitSeconds      = 120,
    kPostDischargeWaitSeconds   = 120
};

#define abs(x) (((x)<0)?(-1*(x)):(x))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define VOIDPTR(arg)  ((void *)(uintptr_t)(arg))

#define SMCKEY2CHARS(key) \
    (fDisplayKeys ? (((key) >> 24) & 0xff) : ' '), (fDisplayKeys ? (((key) >> 16) & 0xff) : ' '),\
            (fDisplayKeys ? (((key) >> 8) & 0xff) : ' '), (fDisplayKeys ? ((key) & 0xff) : ' ')

#define ASSERT_GATED() \
do {  \
    if (fWorkLoop->inGate() != true) {   \
        panic("AppleSmartBattery: not inside workloop gate");  \
    } \
} while(false)


// Argument to transactionCompletion indicating we should start/re-start polling
#define kTransactionRestart     0x0
#define kFinishPolling          0x9999
#define kMaxBatteryCells        4

static const uint32_t kBatteryReadAllTimeout = 10000;       // 10 seconds


#if TARGET_OS_IPHONE || TARGET_OS_OSX_AS
#define kBatteryIdLength 17

#define kIOReportBatteryMaximumTemperatureID IOREPORT_MAKEID('m', 'a', 'x', ' ', 't', 'e', 'm', 'p')
#define kIOReportBatteryMinimumTemperatureID IOREPORT_MAKEID('m', 'i', 'n', ' ', 't', 'e', 'm', 'p')
#define kIOReportBatteryMaximumPackVoltageID IOREPORT_MAKEID('m', 'a', 'x', 'p', 'k', 'v', 'l', 't')
#define kIOReportBatteryMinimumPackVoltageID IOREPORT_MAKEID('m', 'i', 'n', 'p', 'k', 'v', 'l', 't')
#define kIOReportBatteryMaximumChargeCurrentID IOREPORT_MAKEID('m', 'a', 'x', 'c', 'h', 'g', ' ', 'i')
#define kIOReportBatteryMaximumDischargeCurrentID IOREPORT_MAKEID('m', 'a', 'x', 'd', 'c', 'h', 'g', 'i')
#define kIOReportBatteryMaximumOverChargedCurrentID IOREPORT_MAKEID('m', 'a', 'x', 'o', 'q', 'c', 'a', 'p')
#define kIOReportBatteryMaximumOverDischargedCurrentID IOREPORT_MAKEID('m', 'x', 'o', 'd', 'q', 'c', 'a', 'p')
#define kIOReportBatteryMaximumFCCID IOREPORT_MAKEID('m', 'a', 'x', ' ', ' ', 'f', 'c', 'c')
#define kIOReportBatteryMinimumFCCID IOREPORT_MAKEID('m', 'i', 'n', ' ', ' ', 'f', 'c', 'c')
#define kIOReportBatteryMaximumDeltaVoltageID IOREPORT_MAKEID('m', 'a', 'x', 'd', 'l', 'v', 'l', 't')
#define kIOReportBatteryMinimumDeltaVoltageID IOREPORT_MAKEID('m', 'i', 'n', 'd', 'l', 'v', 'l', 't')
#define kIOReportBatteryLowAvgCurrentLastRunID IOREPORT_MAKEID('l', 'a', 'v', 'i', 'l', 's', 'r', 'n')
#define kIOReportBatteryHighAvgCurrentLastRunID IOREPORT_MAKEID('h', 'a', 'v', 'i', 'l', 's', 'r', 'n')
#define kIOReportBatteryMaximumQmaxID IOREPORT_MAKEID('m', 'a', 'x', ' ', 'q', 'm', 'a', 'x')
#define kIOReportBatteryMinimumQmaxID IOREPORT_MAKEID('m', 'i', 'n', ' ', 'q', 'm', 'a', 'x')
#define kIOReportBatteryQmaxUpdSucCntID IOREPORT_MAKEID('q', 'm', 'a', 'x', 'U', 'p', 'S', 'C')
#define kIOReportBatteryQmaxUpdFailCntID IOREPORT_MAKEID('q', 'm', 'a', 'x', 'U', 'p', 'F', 'C')
#define kIOReportBatteryMaximumRa08ID IOREPORT_MAKEID('m', 'a', 'x', ' ', 'r', 'a', '0', '8')
#define kIOReportBatteryMinimumRa08ID IOREPORT_MAKEID('m', 'i', 'n', ' ', 'r', 'a', '0', '8')
#define kIOReportBatteryMaximumRa8ID IOREPORT_MAKEID('m', 'a', 'x', ' ', ' ', 'r', 'a', '8')
#define kIOReportBatteryMinimumRa8ID IOREPORT_MAKEID('m', 'i', 'n', ' ', ' ', 'r', 'a', '8')
#define kIOReportBatteryAvgTempID IOREPORT_MAKEID('a', 'v', 'g', ' ', 't', 'e', 'm', 'p')
#define kIOReportBatteryTempSampleID IOREPORT_MAKEID('t', 'e', 'm', 'p', 's', 'a', 'm', 'p')
#define kIOReportBatteryFlashWriteCntID IOREPORT_MAKEID('f', 'l', 's', 'w', 'r', 'c', 'n', 't')
#define kIOReportBatteryResetCntID IOREPORT_MAKEID('r', 's', 't', ' ', ' ', 'c', 'n', 't')
#define kIOReportBatteryRDISCntID IOREPORT_MAKEID('r', 'd', 'i', 's', ' ', 'c', 'n', 't')
#define kIOReportBatteryCycleCntLastQmaxID IOREPORT_MAKEID('c', 'y', 'c', 'n', 't', 'l', 'q', 'm')
#define kIOReportBatteryTotalOperatingTimeID IOREPORT_MAKEID('t', 'o', 't', 'o', 'p', 't', 'm', 'e')
#define kIOReportBatteryIDChanged IOREPORT_MAKEID('b', 'a', 't', 'i', 'd', 'c', 'h', 'g')

#define kIOReportBatteryNominalChargeCapacityID IOREPORT_MAKEID('n', 'o', 'c', 'h', 'g', 'c', 'a', 'p')
#define kIOReportBatteryChemIDID IOREPORT_MAKEID('c', 'h', 'e', 'm', ' ', ' ', 'i', 'd')

static const uint32_t kExternalConnectedDebounceMs = 1000;
#endif // TARGET_OS_IPHONE || TARGET_OS_OSX_AS

#define kIOReportNumberOfReporters  2
#define kReportCategoryBattery (kIOReportCategoryPower | kIOReportCategoryField | kIOReportCategoryPeripheral | kIOReportCategoryDebug)
#define kIOReportBatteryCycleCountID IOREPORT_MAKEID('c', 'y', 'c', 'l', 'e', 'c', 'n', 't')
#define kIOReportBatteryGroupName "Battery"

enum {
    INDUCTIVE_FW_CTRL_CMD_SET_CLOAK               = 0x7,
    INDUCTIVE_FW_CTRL_CMD_DISABLE_DEBUGPOWER      = 0x20,
    INDUCTIVE_FW_CTRL_CMD_DISABLE_DISP_COEX       = 0x25,
    INDUCTIVE_FW_CTRL_CMD_SET_DEMO_MODE           = 0x28,
};

// Class definition with constructor methods overloaded to handle basic data types
struct SMCAdapterParamHelper {
    uint32_t valid;           /* To log or not to log the parameter */
    const OSSymbol *keyObj;   /* Dict key obj associated with the parameter */
    OSObject *valObj;         /* ValueObj - a catch-all for OSArray/OSNumber/... */
    
    SMCAdapterParamHelper (uint32_t _valid, const OSSymbol *_key, const UInt32 _arr[], unsigned int _arrSz) : valid (_valid), keyObj (_key) {
        valObj = OSArray::withCapacity((int) _arrSz);
        if (valObj) {
            for (int i = 0; i < _arrSz; ++i) {
                SET_INT_IN_ARR((OSArray *)valObj, i, _arr [i], 8*sizeof(_arr [i]));
            }
        }
    }
     
    SMCAdapterParamHelper (uint32_t _valid, const OSSymbol *_key, UInt64 _val) : valid (_valid), keyObj (_key) {
        valObj = OSNumber::withNumber((unsigned long long)_val, 8*sizeof(_val));
    }
   
    SMCAdapterParamHelper (uint32_t _valid, const OSSymbol *_key, UInt32 _val) : valid (_valid), keyObj (_key) {
        valObj = OSNumber::withNumber((unsigned long long)_val, 8*sizeof(_val));
    }
    
    SMCAdapterParamHelper (uint32_t _valid, const OSSymbol *_key, UInt16 _val) : valid (_valid), keyObj (_key) {
        valObj = OSNumber::withNumber((unsigned long long)_val, 8*sizeof(_val));
    }

    SMCAdapterParamHelper (uint32_t _valid, const OSSymbol *_key, UInt8 _val) : valid (_valid), keyObj (_key) {
        valObj = OSNumber::withNumber((unsigned long long)_val, 8*sizeof(_val));
    }

    ~SMCAdapterParamHelper () {
        if (valObj) {
            OSSafeReleaseNULL(valObj);
        }
    }

    private:
    SMCAdapterParamHelper ();

};

// Keys we use to publish battery state in our IOPMPowerSource::properties array
// TODO: All of these would need to exist on iOS/gOS?
static const OSSymbol *_MaxErrSym                  = OSSymbol::withCStringNoCopy(kIOPMPSMaxErrKey);
static const OSSymbol *_DeviceNameSym              = OSSymbol::withCStringNoCopy(kIOPMDeviceNameKey);
static const OSSymbol *_FullyChargedSym            = OSSymbol::withCStringNoCopy(kIOPMFullyChargedKey);
static const OSSymbol *_AvgTimeToEmptySym          = OSSymbol::withCStringNoCopy(kIOPMPSAvgTimeToEmptyKey);
static const OSSymbol *_InstantTimeToEmptySym      = OSSymbol::withCStringNoCopy(kIOPMPSInstantTimeToEmptyKey);
static const OSSymbol *_AmperageSym                = OSSymbol::withCStringNoCopy(kIOPMPSAmperageKey);
static const OSSymbol *_VoltageSym                 = OSSymbol::withCStringNoCopy(kIOPMPSVoltageKey);
static const OSSymbol *_InstantAmperageSym         = OSSymbol::withCStringNoCopy(kIOPMPSInstantAmperageKey);
static const OSSymbol *_AvgTimeToFullSym           = OSSymbol::withCStringNoCopy(kIOPMPSAvgTimeToFullKey);
static const OSSymbol *_ManfDateSym                = OSSymbol::withCStringNoCopy(kIOPMPSManufactureDateKey);
static const OSSymbol *_DesignCapacitySym          = OSSymbol::withCStringNoCopy(kIOPMPSDesignCapacityKey);
static const OSSymbol *_TemperatureSym             = OSSymbol::withCStringNoCopy(kIOPMPSBatteryTemperatureKey);
static const OSSymbol *_ManufacturerDataSym        = OSSymbol::withCStringNoCopy(kIOPMPSManufacturerDataKey);
static const OSSymbol *_PFStatusSym                = OSSymbol::withCStringNoCopy(kIOPMPSPFStatusKey);
static const OSSymbol *_DesignCycleCount70Sym      = OSSymbol::withCStringNoCopy(kIOPMPSDesignCycleCount70Key);
static const OSSymbol *_DesignCycleCount9CSym      = OSSymbol::withCStringNoCopy(kIOPMPSDesignCycleCount9CKey);
static const OSSymbol *_PackReserveSym             = OSSymbol::withCStringNoCopy(kIOPMPSPackReserveKey);
static const OSSymbol *_OpStatusSym                = OSSymbol::withCStringNoCopy(kIOPMPSOpStatusKey);
static const OSSymbol *_PermanentFailureSym        = OSSymbol::withCStringNoCopy(kIOPMPSPermanentFailureKey);
static const OSSymbol *_FirmwareSerialNumberSym    = OSSymbol::withCStringNoCopy(kIOPMPSFirmwareSerialNumberKey);
static const OSSymbol *_rawExternalConnectedSym    = OSSymbol::withCStringNoCopy(kIOPMPSRawExternalConnectedKey);


#define kBootPathKey             "BootPathUpdated"
#define kFullPathKey             "FullPathUpdated"
#define kUserVisPathKey          "UserVisiblePathUpdated"

#define kBatt                   kSMBusBatteryAddr
#define kMgr                    kSMBusManagerAddr



#define super IOPMPowerSource

OSDefineMetaClassAndStructors(AppleSmartBattery,IOPMPowerSource)

/*
 * Fills up the Dictionary with PwrPortTelemetryLogParams0_t struct members.  
 * 
 * @param 'PwrPortTelemetryLogParams0_t Struct' whose members are loaded into 'outDict'
 *
 * */
static void populateAdapterParams0Dict (const PwrPortTelemetryLogParams0_t &params0, OSDictionary *outDict) 
{
    const PwrPortTelemetryLogParams0Mask_t &mask = params0.paramValidMask; 
  
    if (mask.mask == 0) {
        // No valid parameters to log. Bail early! 
        return; 
    } 
   
    SMCAdapterParamHelper params [] = {
        SMCAdapterParamHelper (mask.pdo                ,_kAsbPortControllerPortPDOSym            ,params0.pdo,     ARRAY_SIZE (params0.pdo)),
        SMCAdapterParamHelper (mask.portMode           ,_kAsbPortControllerPortModeSym           ,(UInt32) params0.portMode), // portMode is an enum. Help the compiler choose the constructor.
        SMCAdapterParamHelper (mask.fwVersion          ,_kAsbPortControllerFwVersionSym          ,params0.fwVersion),
        SMCAdapterParamHelper (mask.electionFailReason ,_kAsbPortControllerElectionFailReasonSym ,params0.electionFailReason),
        SMCAdapterParamHelper (mask.activeContractRdo  ,_kAsbPortControllerActiveContractRdoSym  ,params0.activeContractRdo),
        SMCAdapterParamHelper (mask.dnSt               ,_kAsbPortControllerDnStSym               ,params0.dnSt),
        SMCAdapterParamHelper (mask.fetStatus          ,_kAsbPortControllerFetStatusSym          ,params0.fetStatus),
        SMCAdapterParamHelper (mask.powerState         ,_kAsbPortControllerPowerStateSym         ,params0.powerState),
        SMCAdapterParamHelper (mask.uvdmStatus         ,_kAsbPortControllerUvdmStatusSym         ,params0.uvdmStatus),
        SMCAdapterParamHelper (mask.srcTypes           ,_kAsbPortControllerSrcTypesSym           ,params0.srcTypes),
        SMCAdapterParamHelper (mask.loserReason        ,_kAsbPortControllerLoserReasonSym        ,params0.loserReason),
        SMCAdapterParamHelper (mask.nPDOs              ,_kAsbPortControllerNPDOsSym              ,params0.nPDOs),
        SMCAdapterParamHelper (mask.nEprPDOs           ,_kAsbPortControllerNEprPDOsSym           ,params0.nEprPDOs),
        SMCAdapterParamHelper (mask.pdst               ,_kAsbPortControllerPDstSym               ,params0.pdst),
        SMCAdapterParamHelper (mask.capMismatch        ,_kAsbPortControllerCapMismatchSym        ,params0.capMismatch),
        SMCAdapterParamHelper (mask.bootFlags          ,_kAsbPortControllerBootFlagsSym          ,params0.bootFlags),
        SMCAdapterParamHelper (mask.sleepWakeInfo      ,_kAsbPortControllerSleepWakeDisTimeSym   ,params0.sleepWakeInfo.sleepDisabledTime),
        SMCAdapterParamHelper (mask.sleepWakeInfo      ,_kAsbPortControllerSleepWakeDisCauseSym  ,params0.sleepWakeInfo.cause),
        SMCAdapterParamHelper (mask.sleepWakeInfo      ,_kAsbPortControllerSleepWakeEnabledSym   ,params0.sleepWakeInfo.sleepEnabled),
    };

    for (int i = 0; i < ARRAY_SIZE(params); ++i) {
        if (params [i].valid && params[i].keyObj && params[i].valObj) {
            // If valid parameters are published by SMC, funnel them.
            outDict->setObject(params[i].keyObj, params[i].valObj);
        }
    }
}

/*
 * Fills up the Dictionary with  Port Controller Evt history buffer
 * 
 * @param bufIdx -> Oldest entry index
 * @param buffer -> Byte buffer holding the SMC Key read payload
 * @param bufSize -> Size of the buffer
 * @param outDict -> Output dictionary 
 *
 * */
static void populatePortControllerEvtBufferDict (int bufIdx, const uint8_t *buffer, int32_t bufSize,  OSDictionary *outDict) {
    
    // The event log circular buffer is unrolled to start from the oldest
    // (pointed by bufIdx) to the newest events in the registry to avoid
    // logging the buffer index as an additional parameter in the power log 

    if (bufIdx == 0) {

        // No unrolling as the start of buffer is the oldest entry.
        const OSData *valObj = OSData::withBytes(buffer, bufSize); 
        
        if (valObj) {
            outDict->setObject (_kAsbPortControllerEvtBufferSym, valObj); 
            OSSafeReleaseNULL(valObj);
        }
        // All done, just go!
        return; 
    }
    
    // Unwrap the buffer when index is non-zero
    OSData *valObj = OSData::withCapacity(bufSize);
    if (valObj) {
        
        valObj->appendBytes (&buffer [bufIdx], bufSize - bufIdx); 
        valObj->appendBytes (&buffer [0], bufIdx);
        
        outDict->setObject (_kAsbPortControllerEvtBufferSym, valObj); 
        OSSafeReleaseNULL(valObj);
    }

}

/*
 * Fills up the Dictionary with PwrPortTelemetryLogParams1_t struct members.  
 * 
 * @param 'PwrPortTelemetryLogParams1_t Struct' whose members are loaded into
 *        'outDict'
 *
 * */
static void populateAdapterParams1Dict (const PwrPortTelemetryLogParams1_t &params1, OSDictionary *outDict) 
{
    const PwrPortTelemetryLogParams1Mask_t &mask = params1.paramValidMask; 
 
    if (mask.mask == 0) {
        // No valid parameters to log. Bail early! 
        return; 
    } 

    SMCAdapterParamHelper params [] = {
        SMCAdapterParamHelper (mask.srdoCount            ,_kAsbPortControllerSrdoCountSym               ,params1.srdoCount),            
        SMCAdapterParamHelper (mask.srdoRetryCount       ,_kAsbPortControllerSrdoRetryCountSym          ,params1.srdoRetryCount),
        SMCAdapterParamHelper (mask.srdyCount            ,_kAsbPortControllerSrdyCountSym               ,params1.srdyCount),     
        SMCAdapterParamHelper (mask.srdyRejectCount      ,_kAsbPortControllerSrdyRejectCountSym         ,params1.srdyRejectCount),      
        SMCAdapterParamHelper (mask.shortDetectCount     ,_kAsbPortControllerShortDetectCountSym        ,params1.shortDetectCount),     
        SMCAdapterParamHelper (mask.srdoRejectCount      ,_kAsbPortControllerSrdoRejectCountSym         ,params1.srdoRejectCount),      
        SMCAdapterParamHelper (mask.vdoFailCount         ,_kAsbPortControllerVdoFailCountSym            ,params1.vdoFailCount),    
        SMCAdapterParamHelper (mask.i2cErrCount          ,_kAsbPortControllerI2cErrCountSym             ,params1.i2cErrCount),            
        SMCAdapterParamHelper (mask.surpriseAckCount     ,_kAsbPortControllerSurpriseAckCountSym        ,params1.surpriseAckCount),
        SMCAdapterParamHelper (mask.surpriseNackCount    ,_kAsbPortControllerSurpriseNackCountSym       ,params1.surpriseNackCount),   
        SMCAdapterParamHelper (mask.stuckCmdCount        ,_kAsbPortControllerStuckCmdCountSym           ,params1.stuckCmdCount),     
        SMCAdapterParamHelper (mask.wakeFailCount        ,_kAsbPortControllerWakeFailCountSym           ,params1.wakeFailCount),         
        SMCAdapterParamHelper (mask.attachCount          ,_kAsbPortControllerAttachCountSym             ,params1.attachCount),           
        SMCAdapterParamHelper (mask.detachCount          ,_kAsbPortControllerDetachCountSym             ,params1.detachCount),             
        SMCAdapterParamHelper (mask.pwrRoleSwapFailCount ,_kAsbPortControllerPwrRoleSwapFailCountSym    ,params1.pwrRoleSwapFailCount),
        SMCAdapterParamHelper (mask.pwrRoleSwapCount     ,_kAsbPortControllerPwrRoleSwapCountSym        ,params1.pwrRoleSwapCount),
        SMCAdapterParamHelper (mask.dataRoleSwapFailCount,_kAsbPortControllerDataRoleSwapFailCountSym   ,params1.dataRoleSwapFailCount),
        SMCAdapterParamHelper (mask.dataRoleSwapCount    ,_kAsbPortControllerDataRoleSwapCountSym       ,params1.dataRoleSwapCount),
        SMCAdapterParamHelper (mask.inpFetEnFailCount    ,_kAsbPortControllerInpFetEnFailCountSym       ,params1.inpFetEnFailCount),
        SMCAdapterParamHelper (mask.hardResetCount       ,_kAsbPortControllerHardResetCountSym          ,params1.hardResetCount),
        SMCAdapterParamHelper (mask.wakeCmdFailCount     ,_kAsbPortControllerWakeCmdFailCountSym        ,params1.wakeCmdFailCount),
        SMCAdapterParamHelper (mask.sleepCmdFailCount    ,_kAsbPortControllerSleepCmdFailCountSym       ,params1.sleepCmdFailCount),
        SMCAdapterParamHelper (mask.wakeTimeoutCount     ,_kAsbPortControllerWakeTimeoutCountSym        ,params1.wakeTimeoutCount),
    };
  
    for (int i = 0; i < ARRAY_SIZE(params); ++i) {
        if (params [i].valid && params[i].keyObj && params[i].valObj) {
            // If valid parameters are published by SMC, funnel them.
            outDict->setObject(params[i].keyObj, params[i].valObj);
        } 
    }
}

/*
 * Fills up the Dictionary with PwrPortTelemetryLogParams2_t struct members.  
 * 
 * @param 'PwrPortTelemetryLogParams2_t Struct' whose members are loaded into
 *        'outDict'
 *
 * */
static void populateAdapterParams2Dict (const PwrPortTelemetryLogParams2_t &params2, OSDictionary *outDict) 
{
  const PwrPortTelemetryLogParams2Mask_t &mask = params2.paramValidMask; 

  if (mask.mask == 0) {
      // No valid parameters to log. Bail early! 
      return; 
  }

  SMCAdapterParamHelper params [] = {
      SMCAdapterParamHelper (mask.irqCntAppLd        ,_kAsbPortControllerIrqCntAppLdSym       ,params2.irqCntAppLd),         
      SMCAdapterParamHelper (mask.irqCntHrdRst       ,_kAsbPortControllerIrqCntHrdRstSym      ,params2.irqCntHrdRst),       
      SMCAdapterParamHelper (mask.irqCntPlg          ,_kAsbPortControllerIrqCntPlgSym         ,params2.irqCntPlg),           
      SMCAdapterParamHelper (mask.irqCntStsUpd       ,_kAsbPortControllerIrqCntStsUpdSym      ,params2.irqCntStsUpd),        
      SMCAdapterParamHelper (mask.irqCntPwrStsUpd    ,_kAsbPortControllerIrqCntPwrStsUpdSym   ,params2.irqCntPwrStsUpd),     
      SMCAdapterParamHelper (mask.irqCntRxSrcCap     ,_kAsbPortControllerIrqCntRxSrcCapSym    ,params2.irqCntRxSrcCap),      
      SMCAdapterParamHelper (mask.irqCntPdStsUpd     ,_kAsbPortControllerIrqCntPdStsUpdSym    ,params2.irqCntPdStsUpd),      
      SMCAdapterParamHelper (mask.irqCntRxIdSop      ,_kAsbPortControllerIrqCntRxIdSopSym     ,params2.irqCntRxIdSop),       
      SMCAdapterParamHelper (mask.irqCntUvdmEnum     ,_kAsbPortControllerIrqCntUvdmEnumSym    ,params2.irqCntUvdmEnum),      
      SMCAdapterParamHelper (mask.irqCntUvdmStsUpd   ,_kAsbPortControllerIrqCntUvdmStsUpdSym  ,params2.irqCntUvdmStsUpd),    
      SMCAdapterParamHelper (mask.irqCntUsb2Plg      ,_kAsbPortControllerIrqCntUsb2PlgSym     ,params2.irqCntUsb2Plg),     
      SMCAdapterParamHelper (mask.irqCntUsb2Wak      ,_kAsbPortControllerIrqCntUsb2WakSym     ,params2.irqCntUsb2Wak),       
      SMCAdapterParamHelper (mask.irqCntConSrc       ,_kAsbPortControllerIrqCntConSrcSym      ,params2.irqCntConSrc),        
      SMCAdapterParamHelper (mask.irqCntRxSnkCap     ,_kAsbPortControllerIrqCntRxSnkCapSym    ,params2.irqCntRxSnkCap),      
      SMCAdapterParamHelper (mask.irqCntRxRdo        ,_kAsbPortControllerIrqCntRxRdoSym       ,params2.irqCntRxRdo),         
      SMCAdapterParamHelper (mask.irqCntAlert        ,_kAsbPortControllerIrqCntAlertSym       ,params2.irqCntAlert),         
      SMCAdapterParamHelper (mask.irqCntldcm         ,_kAsbPortControllerIrqCntldcmSym        ,params2.irqCntldcm),         
      SMCAdapterParamHelper (mask.irqCntWakeAck      ,_kAsbPortControllerIrqCntWakeAckSym     ,params2.irqCntWakeAck),         
  };


  for (int i = 0; i < ARRAY_SIZE(params); ++i) {
      if (params [i].valid && params[i].keyObj && params[i].valObj) {
          // If valid parameters are published by SMC, funnel them.
          outDict->setObject(params[i].keyObj, params[i].valObj);
      } 
  }
}

/******************************************************************************
 * AppleSmartBattery::smartBattery
 *
 ******************************************************************************/

AppleSmartBattery *
AppleSmartBattery::smartBattery(void)
{
    static int asbm = 0;
    AppleSmartBattery  *me;
    me = new AppleSmartBattery;

    if (asbm || (me && !me->init())) {
        me->release();
        asbm++;
        return NULL;
    }

    return me;
}

/******************************************************************************
 * AppleSmartBattery::init
 *
 ******************************************************************************/

bool AppleSmartBattery::init(void)
{
    if (!super::init()) {
        return false;
    }

    fProvider = NULL;
    fWorkLoop = NULL;

    return true;
}


/******************************************************************************
 * AppleSmartBattery::start
 *
 ******************************************************************************/
bool AppleSmartBattery::start(IOService *provider)
{
    fProvider = OSDynamicCast(AppleSmartBatteryManager, provider);

    if (!fProvider || !super::start(provider)) {
        return false;
    }

    _pollingNow             = false;
    _cancelPolling          = false;
    _rebootPolling          = false;
    fPermanentFailure       = false;
    fFullyDischarged        = false;
    fFullyCharged           = false;
    fBatteryPresent         = true;
    fACConnected            = -1;
    fInflowDisabled         = false;
    fCellVoltages           = NULL;
    fSystemSleeping         = false;
    fPowerServiceToAck      = NULL;
    fCapacityOverride       = false;

    initializeCommands();

    fInitialPollCountdown = kInitialPollCountdown;
#if TARGET_OS_OSX_X86
    fDisplayKeys = false;
#else
    fDisplayKeys = PE_i_can_has_debugger(NULL);
#endif // TARGET_OS_OSX_X86

    fWorkLoop = getWorkLoop();

    fBatteryReadAllTimer = IOTimerEventSource::timerEventSource(this,
                    OSMemberFunctionCast(IOTimerEventSource::Action,
                    this, &AppleSmartBattery::incompleteReadTimeOut));

    if (!fWorkLoop
      || (kIOReturnSuccess != fWorkLoop->addEventSource(fBatteryReadAllTimer))) {
        BM_ERRLOG("Failed to start timer event\n");
        return false;
    }

    _pollCtrlLock = IORWLockAlloc();
    if (!_pollCtrlLock) {
        BM_ERRLOG("Failed to allocate poll control lock\n");
        return false;
    }

    // Publish the intended period in seconds that our "time remaining"
    // estimate is wildly inaccurate after wake from sleep.
    setProperty(kIOPMPSInvalidWakeSecondsKey, kSecondsUntilValidOnWake, 32);

    // Publish the necessary time period (in seconds) that a battery
    // calibrating tool must wait to allow the battery to settle after
    // charge and after discharge.
    setProperty(kIOPMPSPostChargeWaitSecondsKey, kPostChargeWaitSeconds, 32);
    setProperty(kIOPMPSPostDishargeWaitSecondsKey, kPostDischargeWaitSeconds, 32);

    // zero out battery state with argument (do_update == true)
    clearBatteryState(false);


    // Kick off the 30 second timer and do an initial poll
    // No guarantee SMC is ready at this point
    pollBatteryState(kBoot);

    return true;
}



/******************************************************************************
 * AppleSmartBattery::initializeCommands
 *
 ******************************************************************************/
void AppleSmartBattery::initializeCommands(void)
{
    CommandStruct local_cmd[] =
    {
        {kTransactionRestart,       0,     kASBMInvalidOp,     0, 0, NULL,                      kUserVis, true,},
        {kBatteryDataCmd,           kBatt, kASBMSMCReadDictionary,   0, 0, NULL,                kUserVis, false},
        {kBVoltageCmd,              kBatt, kASBMSMBUSReadWord, 0, 0, voltageKey,                kUserVis, false},
        {kBCurrentCmd,              kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                      kUserVis, false},
        {kBAverageCurrentCmd,       kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                      kUserVis, false},
        {kBManufactureDataCmd,      kBatt, kASBMSMBUSReadBlock, 0, 0, _ManufacturerDataSym,     kBoot,    false},
        {kBAppleHardwareSerialCmd,  kBatt, kASBMSMBUSReadBlock, 0, 0, serialKey,                kBoot,    false},
#if TARGET_OS_OSX_X86
        {kMStateContCmd,            kMgr,  kASBMSMBUSReadWord, 0, 0, NULL,                      kUserVis, false},
        {kMStateCmd,                kMgr,  kASBMSMBUSReadWord, 0, 0, NULL,                      kUserVis, false},
        {kBBatteryStatusCmd,        kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                      kUserVis, false},
        {kBExtendedOperationStatusCmd, kBatt, kASBMSMBUSExtendedReadWord, 0, 0, _OpStatusSym,   kFull,    false},
        {kBManufactureNameCmd,      kBatt, kASBMSMBUSReadBlock, 0, 0, manufacturerKey,          kBoot,    false},
        {kBManufacturerInfoCmd,     kBatt, kASBMSMBUSReadBlock, 0, 0, NULL,                     kBoot,    false},
        {kBSerialNumberCmd,         kBatt, kASBMSMBUSReadWord, 0, 0, _FirmwareSerialNumberSym,  kBoot,    false},
        {kBMaxErrorCmd,             kBatt, kASBMSMBUSReadWord, 0, 0, _MaxErrSym,                kFull,    false},
        {kBRunTimeToEmptyCmd,       kBatt, kASBMSMBUSReadWord, 0, 0, _InstantTimeToEmptySym,    kFull,    false},
        {kBTemperatureCmd,          kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                      kFull,    false},
#else
        // Order of these entries is important.
        // kMStateCmd relies on data from ExternalChargeCmd and ExternalConnectedCmd
        {kAtCriticalLevelCmd,       kBatt, kASBMSMCReadBool,   0, 0, NULL,                        kUserVis, false},
        {kExternalChargeCapableCmd, kBatt, kASBMSMCReadBool,   0, 0, NULL,                        kUserVis, false},
        {kExternalConnectedCmd,     kBatt, kASBMSMCReadBool,   0, 0, NULL,                        kUserVis, true},
        {kRawExternalConnectedCmd,  kBatt, kASBMSMCReadBool,   0, 0, _rawExternalConnectedSym,    kUserVis, true},
        {kMStateCmd,                kMgr,  kASBMSMBUSReadWord, 0, 0, NULL,                        kUserVis, false},
        {kRawCurrentCapacityCmd,    kBatt, kASBMSMBUSReadWord, 0, 0, _RawCurrentCapacitySym,      kUserVis, false},
        {kRawMaxCapacityCmd,        kBatt, kASBMSMBUSReadWord, 0, 0, _RawMaxCapacitySym,          kUserVis, false},
        {kNominalChargeCapacityCmd, kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                        kUserVis, false},
        {kAbsoluteCapacityCmd,      kBatt, kASBMSMBUSReadWord, 0, 0, _AbsoluteCapacitySym,        kUserVis, false},
        {kRawBatteryVoltageCmd,     kBatt, kASBMSMBUSReadWord, 0, 0, _rawBatteryVoltageSym,       kUserVis, false},
        {kFullyChargedCmd,          kMgr,  kASBMSMCReadBool,   0, 0, _FullyChargedSym,            kUserVis, false},
        {kErrorConditionCmd,        kMgr,  kASBMSMBUSReadWord, 0, 0, NULL,                        kUserVis, false},
        {kChargerConfigurationCmd,  kBatt, kASBMSMBUSReadWord,   0, 0, _kChargerConfigurationSym, kUserVis, false},
        {kBootVoltageCmd,           kBatt, kASBMSMBUSReadWord,   0, 0, _kBootVoltageSym,          kFull,    false},
        {kBTemperatureCmd,          kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                        kUserVis, false},
        {kBVirtualTemperatureCmd,   kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                        kUserVis, false},
        {kBCellDisconnectCmd,       kBatt, kASBMSMBUSReadWord,   0, 0, _kBCellDisconnectCountSym, kFull,    false},
        {kCarrierModeCmd,           kBatt, kASBMSMBUSReadWord,   0, 0, NULL,                      kUserVis, false},
        {kKioskModeCmd,             kBatt, kASBMSMBUSReadWord,   0, 0, NULL,                      kUserVis, false},
#endif
        {kBCycleCountCmd,           kBatt, kASBMSMBUSReadWord, 0, 0, cycleCountKey,               kFull,    false},
        // TTE need AverageCurrent and ExternalConnected
        {kBAverageTimeToEmptyCmd,   kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                        kUserVis, false},
#if TARGET_OS_OSX
        {kBExtendedPFStatusCmd,     kBatt, kASBMSMBUSExtendedReadWord, 0, 0, _PFStatusSym,        kFull,    false},
        {kBDesignCycleCount9CCmd,   kBatt, kASBMSMBUSReadWord, 0, 0, _DesignCycleCount9CSym,      kBoot,    false},
        // TTF need AverageCurrent and ExternalConnected
        {kBAverageTimeToFullCmd,    kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                        kUserVis, false},
        {kBPackReserveCmd,          kBatt, kASBMSMBUSReadWord, 0, 0, _PackReserveSym,             kBoot,    false},
        {kBDeviceNameCmd,           kBatt, kASBMSMBUSReadBlock, 0, 0, _DeviceNameSym,             kBoot,    false},
#endif // TARGET_OS_OSX
        {kBRemainingCapacityCmd,    kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                        kUserVis, false},
        {kBFullChargeCapacityCmd,   kBatt, kASBMSMBUSReadWord, 0, 0, NULL,                        kUserVis, false},
        {kBDesignCapacityCmd,       kBatt, kASBMSMBUSReadWord, 0, 0, _DesignCapacitySym,          kBoot,    false},
        {kFinishPolling,            0,     kASBMInvalidOp,     0, 0, NULL,                        kUserVis, true,}
    };

    cmdTable.table = NULL;
    cmdTable.count = 0;

    if ((cmdTable.table = (CommandStruct *)IOMallocType(typeof(local_cmd)))) {
        cmdTable.count = ARRAY_SIZE(local_cmd);
        bcopy(&local_cmd, cmdTable.table, sizeof(local_cmd));
    }
}

CommandStruct *AppleSmartBattery::commandForState(uint32_t state)
{
    if (cmdTable.table) {
        for (int i=0; i<cmdTable.count; i++) {
            if (state == cmdTable.table[i].cmd) {
                return &cmdTable.table[i];
            }
        }
    }
    return NULL;
}

#if TARGET_OS_OSX_X86
bool AppleSmartBattery::doInitiateTransaction(const CommandStruct *cs)
{
    ASBMgrRequest req;

    req.opType = cs->opType;
    req.address = cs->addr;
    req.command = cs->cmd;
    req.fullyDischarged = fFullyDischarged;
    req.completionHandler = OSMemberFunctionCast(ASBMgrTransactionCompletion,
            this, &AppleSmartBattery::transactionCompletion);


    return fProvider->performTransaction(&req, (OSObject *)this, (void *)cs);
}
#endif // TARGET_OS_OSX_X86


void AppleSmartBattery::updateDictionaryInIOReg(const OSSymbol *sym, smcToRegistry *keys)
{
    OSDictionary *dict = smcKeysToDictionary(keys);
    if (!dict) {
        return;
    }

    if (!dict->getCount()) {
        OSSafeReleaseNULL(dict);
        return;
    }

    OSDictionary *prevDict = OSDynamicCast(OSDictionary, getProperty(sym));
    if (prevDict) {
        prevDict = OSDynamicCast(OSDictionary, prevDict->copyCollection());
    }

    if (prevDict) {
        prevDict->merge(dict);
        OSSafeReleaseNULL(dict);
    } else {
        prevDict = dict;
    }

    setProperty(sym, prevDict);
    OSSafeReleaseNULL(prevDict);
}

bool AppleSmartBattery::initiateTransactionGated(CommandStruct *cs)
{
    IOReturn ret = kIOReturnSuccess;

    ret = doInitiateTransaction(cs);
    if (ret != kIOReturnSuccess) {
        BM_ERRLOG("Command 0x%x failed with error 0x%x\n", cs->cmd, ret);
    }

    return ret;
}

bool AppleSmartBattery::initiateTransaction(CommandStruct *cs)
{
    if (cs->cmd == kFinishPolling) {
        this->handlePollingFinished(true);
        return true;
    }

    return fWorkLoop->runAction(OSMemberFunctionCast(IOWorkLoop::Action, this, &AppleSmartBattery::initiateTransactionGated),
                                this, cs);
}

/******************************************************************************
 * AppleSmartBattery::initiateNextTransaction
 *
 ******************************************************************************/
bool AppleSmartBattery::initiateNextTransaction(uint32_t state)
{
    int found_current_index = 0;
    CommandStruct *cs = NULL;

    if (!cmdTable.table) {
        return false;
    }

    // Find index for "state" in cmd_machine
    for (found_current_index = 0; found_current_index < cmdTable.count; found_current_index++) {
        if (cmdTable.table[found_current_index].cmd == state) {
            break;
        }
    }

    uint16_t machinePath;
    IORWLockRead(_pollCtrlLock);
    machinePath = _machinePath;
    IORWLockUnlock(_pollCtrlLock);

    // Find next state to read for _machinePath
    if (++found_current_index < cmdTable.count) {
        for (; found_current_index<cmdTable.count; found_current_index++) {
            if (!_batteryCellCount && !cmdTable.table[found_current_index].supportDesktops) {
                // skip battery-related commands on desktops
                continue;
            }
            if ((cmdTable.table[found_current_index].smcKey == kSMCNoOpKey) && (!fProvider->smbusSupported())) {
                continue;
            }
            if (cmdTable.table[found_current_index].pathBits >= machinePath) {
                cs = &cmdTable.table[found_current_index];
                break;
            }
        }
    }

    if (cs) {
        return initiateTransaction(cs);
    }

    return false;
}

static bool isWakeFromHibernate(void)
{
    UInt32 hibernateState = kIOHibernateStateInactive;

    OSObject* property = IOService::getPMRootDomain()->copyProperty(kIOHibernateStateKey);
    if(property != nullptr) {
        OSData *data = OSDynamicCast(OSData,property);
        if ((data != NULL) && (data->getLength() == sizeof(hibernateState))) {
            memcpy(&hibernateState, data->getBytesNoCopy(), sizeof(hibernateState));
        }
    }
    OSSafeReleaseNULL(property);
    return hibernateState == kIOHibernateStateWakingFromHibernate;
}
/******************************************************************************
 * AppleSmartBattery::handleSystemSleepWakeGated
 *
 * Caller must hold the gate.
 ******************************************************************************/
IOReturn AppleSmartBattery::handleSystemSleepWakeGated(IOService *powerService,bool isSystemSleep)
{
    IOReturn ret = kIOPMAckImplied;

    if (!powerService || (fSystemSleeping  == isSystemSleep)) {
        return kIOPMAckImplied;
    }

    if (fPowerServiceToAck) {
        fPowerServiceToAck->release();
        fPowerServiceToAck = 0;
    }

    IORWLockWrite(_pollCtrlLock);

    fSystemSleeping = isSystemSleep;
    if (fSystemSleeping) {
        // Stall PM until battery poll in progress is cancelled.
        if (_pollingNow) {
            fPowerServiceToAck = powerService;
            fPowerServiceToAck->retain();
            ret = (kBatteryReadAllTimeout * 1000);
        }
    }

    IORWLockUnlock(_pollCtrlLock);

    if(!fSystemSleeping && isWakeFromHibernate()) {
        fWorkLoop->runAction(OSMemberFunctionCast(IOWorkLoop::Action, this, &AppleSmartBattery::readShutdownData),this);
    }

    return ret;
}

IOReturn AppleSmartBattery::handleSystemSleepWake(
    IOService * powerService, bool isSystemSleep)
{
    IOReturn ret = kIOPMAckImplied;

    fWorkLoop->runAction(OSMemberFunctionCast(IOWorkLoop::Action, this, &AppleSmartBattery::handleSystemSleepWakeGated),
                         this, powerService, VOIDPTR(isSystemSleep));
    
    BM_LOG1("SmartBattery: handleSystemSleepWake(%d) = %u\n", isSystemSleep, (uint32_t) ret);

    return ret;
}

void AppleSmartBattery::acknowledgeSystemSleepWakeGated(void)
{
    if (fPowerServiceToAck) {
        fPowerServiceToAck->acknowledgeSetPowerState();
        fPowerServiceToAck->release();
        fPowerServiceToAck = 0;

        if (fBatteryReadAllTimer) {
            fBatteryReadAllTimer->cancelTimeout();
        }

        BM_LOG1("SmartBattery: final acknowledge of wake after reading all regs\n");
    }
}

void AppleSmartBattery::acknowledgeSystemSleepWake(void)
{
    fWorkLoop->runAction(OSMemberFunctionCast(IOWorkLoop::Action, this, &AppleSmartBattery::acknowledgeSystemSleepWakeGated),
                         this);
}


/******************************************************************************
 * AppleSmartBattery::pollBatteryState
 *
 * Asynchronously kicks off the register poll.
 ******************************************************************************/

#define kMinimumPollingFrequencyMS      1000

bool AppleSmartBattery::pollBatteryState(int type)
{
    bool ret;
    bool startPoll = false;

    IORWLockWrite(_pollCtrlLock);

    /* Don't perform any SMBus activity if a AppleSmartBatteryManagerUserClient
       has grabbed exclusive access
     */
    if (fProvider->hasExclusiveClient()) {
        BM_ERRLOG("AppleSmartBattery::pollBatteryState was stalled by an exclusive user client.\n");
        ret = false;
        goto unlock;
    }

    if (_pollingNow && (_machinePath <= type)) {
        /* We're already in the middle of a poll for a superset of 
         * the requested battery data.
         */
        BM_LOG1("AppleSmartBattery::pollBatteryState already polling (%d <= %d). Restarting poll\n", _machinePath, type);
    }
    else if (type != kUseLastPath) {
        _machinePath = type;
    }

    if (fInitialPollCountdown > 0 ||
        (_batteryCellCount && !properties->getObject(serialKey))) {
        // We're going out of our way to make sure that we get a successfull
        // initial poll at boot. Upgrade all early boot polls to kBoot.
        _machinePath = kBoot;
    }

    if (!_pollingNow) {
        BM_LOG1("Starting poll type %d\n", _machinePath);
        /* Start the battery polling state machine (resetting it if it's already in progress) */
        startPoll = true;
        ret = true;
    } else {
        /* Outstanding transaction in process; flag it to restart polling from
           scratch when this flag is noticed.
         */
        _rebootPolling = true;
        ret = true;
    }

unlock:
    IORWLockUnlock(_pollCtrlLock);

    if (startPoll) {
        transactionCompletion((void *)kTransactionRestart, 0, 0, NULL);
    }

    return ret;
}



void AppleSmartBattery::handleBatteryInserted(void)
{
    BM_LOG1("SmartBattery: battery inserted!\n");

    clearBatteryState(false);
    fInitialPollCountdown = kInitialPollCountdown;

    IORWLockWrite(_pollCtrlLock);
    _rebootPolling = true;
    IORWLockUnlock(_pollCtrlLock);

    pollBatteryState(kBoot);
    return;
}

void AppleSmartBattery::handleBatteryRemovedGated(void)
{
    if (fBatteryReadAllTimer) {
        fBatteryReadAllTimer->cancelTimeout();
    }
}

void AppleSmartBattery::handleBatteryRemoved(void)
{
    IORWLockWrite(_pollCtrlLock);

    if (_pollingNow) {
        _cancelPolling = true;
    }

    IORWLockUnlock(_pollCtrlLock);

    fWorkLoop->runAction(OSMemberFunctionCast(IOWorkLoop::Action, this, &AppleSmartBattery::handleBatteryRemovedGated),
                         this);

    clearBatteryState(true);

    BM_LOG1("SmartBattery: battery removed\n");

    acknowledgeSystemSleepWake();
    return;
}

void AppleSmartBattery::handleSetOverrideCapacityGated(uint16_t value, bool sticky)
{
    ASSERT_GATED();

    if (sticky) {
        fCapacityOverride = true;
        BM_LOG1("Capacity override is set to true\n");
    } else {
        fCapacityOverride = false;
    }

    setCurrentCapacity(value);
}

void AppleSmartBattery::handleSetOverrideCapacity(uint16_t value)
{
    OSDictionary *dict = OSDictionary::withCapacity(2);
    if (!dict) {
        return;
    }

    dict->setObject("StickyCapacityOverride", kOSBooleanTrue);
    SET_INTEGER_IN_DICT(dict, currentCapacityKey, value);

    fWorkLoop->runAction(OSMemberFunctionCast(IOWorkLoop::Action, this, &AppleSmartBattery::setPropertiesGated),
                         this, dict);

    OSSafeReleaseNULL(dict);
}

void AppleSmartBattery::handleSwitchToTrueCapacityGated(void)
{
    fCapacityOverride = false;
    BM_LOG1("Capacity override is set to false\n");
    return;
}

void AppleSmartBattery::handleSwitchToTrueCapacity(void)
{
    fWorkLoop->runAction(OSMemberFunctionCast(IOWorkLoop::Action, this, &AppleSmartBattery::handleSwitchToTrueCapacityGated),
                         this);
}

/******************************************************************************
 * incompleteReadTimeOut
 *
 * The complete battery read has not completed in the allowed timeframe.
 * We assume this is for several reasons:
 *    - The EC has dropped an SMBus packet (probably recoverable)
 *    - The EC has stalled an SMBus request; IOSMBusController is hung (probably not recoverable)
 *
 * Start the battery read over from scratch.
 *****************************************************************************/

void AppleSmartBattery::incompleteReadTimeOut(void)
{
    static unsigned int incompleteReadRetries = kIncompleteReadRetryMax;

    BM_ERRLOG("Failed to complete polling of all data within %d ms\n", kBatteryReadAllTimeout);

    /* Don't launch infinite re-tries if the system isn't completing my transactions
     *  (and thus probably leaking a lot of memory every time.
     *  Quit after kIncompleteReadRetryMax
     */
    handlePollingFinished(false);
    if (!fSystemSleeping && (0 < incompleteReadRetries)) {
        incompleteReadRetries--;
        pollBatteryState(kUseLastPath);
    }
}

#if TARGET_OS_IPHONE || TARGET_OS_OSX_AS
void AppleSmartBattery::externalConnectedTimeout(void)
{
    uint8_t connected;
    int ret = _smcReadKey('CHCE', sizeof(connected), &connected);
    if (ret) {
        return;
    }

    uint8_t charge_capable;
    ret = _smcReadKey('CHCC', sizeof(charge_capable), &charge_capable);
    if (ret) {
        return;
    }

    if (connected && charge_capable &&
        connected == fACConnected && charge_capable == fACChargeCapable) {
        return;
    }

    setExternalConnectedToIOPMPowerSource(connected);
    fACConnected = connected;
#if TARGET_OS_OSX
    IOPMrootDomain *rd = getPMRootDomain();
    if (rd) {
        if (connected) {
            rd->receivePowerNotification(kIOPMSetACAdaptorConnected | kIOPMSetValue);
        } else {
            rd->receivePowerNotification(kIOPMSetACAdaptorConnected);
        }
    }
#endif // TARGET_OS_OSX

    setExternalChargeCapable(charge_capable);
    fACChargeCapable = charge_capable;

    setIsCharging(false);

    updateStatus();
}
#endif // TARGET_OS_IPHONE || TARGET_OS_OSX_AS

static void applyOverrideDictTo(OSDictionary *overrides, OSDictionary *dst)
{
    OSCollectionIterator *iter = OSCollectionIterator::withCollection(overrides);
    if (!iter) {
        return;
    }

    const OSSymbol *key = nullptr;
    const OSObject *obj = nullptr;
    while ((key = (const OSSymbol *)iter->getNextObject()) && (obj = overrides->getObject(key))) {
        OSDictionary *tmp = OSDynamicCast(OSDictionary, obj);
        if (tmp) {
            OSObject *o = dst->getObject(key);
            OSDictionary *d = OSDynamicCast(OSDictionary, o);
            if (d) {
                // merge override dict into existing dict
                OSDictionary *copy = OSDictionary::withDictionary(d);
                if (!copy) {
                    continue;
                }

                applyOverrideDictTo(tmp, copy);
                dst->setObject(key, copy);
                OSSafeReleaseNULL(copy);
            } else {
                // overridden key does not exist or is not a dictionary
                dst->setObject(key, obj);
            }
        } else {
            dst->setObject(key, obj);
        }
    }
}

void AppleSmartBattery::applyPropertyOverridesGated(void)
{
    applyOverrideDictTo(_overrideDict, properties);
}

void AppleSmartBattery::handlePollingFinishedGated(bool visitedEntirePath, uint16_t machinePath)
{
    uint64_t now, nsec;

    if (fBatteryReadAllTimer) {
        fBatteryReadAllTimer->cancelTimeout();
    }

    if (visitedEntirePath) {
        const char *reportPathFinishedKey;
        clock_sec_t secs;
        clock_usec_t microsecs;

        if (kBoot == machinePath) {
            reportPathFinishedKey = kBootPathKey;
        } else if (kFull == machinePath) {
            reportPathFinishedKey = kFullPathKey;
        } else if (kUserVis == machinePath) {
            reportPathFinishedKey = kUserVisPathKey;
        } else {
            reportPathFinishedKey = NULL;
        }

        clock_get_calendar_microtime(&secs, &microsecs);
        if (reportPathFinishedKey) {
            setProperty(reportPathFinishedKey, secs, 32);
        }

        if (fInitialPollCountdown > 0) {
            fInitialPollCountdown--;
        } else {
        }

        OSNumber *num = OSNumber::withNumber(secs, 32);
        if (num) {
            setProperty(_kUpdateTime, num);
            num->release();
        }
#if TARGET_OS_OSX_X86
        rebuildLegacyIOBatteryInfoGated();
#endif // TARGET_OS_OSX_X86
        if (acAttach_ts) {
            clock_get_uptime(&now);
            SUB_ABSOLUTETIME(&now, &acAttach_ts);
            absolutetime_to_nanoseconds(now, &nsec);
            if (nsec < (60 * NSEC_PER_SEC)) {
                // In some cases, power adapter information is available thru multiple updates
                // from SMC(19657502). As power adapter info is not populated to registry, we are 
                // force setting the 'settingsChangedSinceUpdate' to make sure notifications are 
                // sent to clients.
                settingsChangedSinceUpdate = true;
            }
            else {
                // Zero this out to avoid time comparisions every time
                acAttach_ts = 0;
            }
        }
        BM_LOG1("SmartBattery: finished polling type %d\n", machinePath);

        // Apply any overrides
        if (_overrideDict) {
            applyPropertyOverridesGated();
        }

        updateStatus();
        if (_needRegisterService) {
            this->registerService();
            _needRegisterService = false;
        }
    } else {
        BM_ERRLOG("SmartBattery: abort polling\n");

        if (fBatteryReadAllTimer) {
            fBatteryReadAllTimer->setTimeoutMS(kBatteryReadAllTimeout);
        }
    }

    IOTimeStampEndConstant(SYSTEMCHARGING_ASBM_BATTERY_POLL);
}

void AppleSmartBattery::handlePollingFinished(bool visitedEntirePath)
{
    uint16_t machinePath;

    IORWLockRead(_pollCtrlLock);
    machinePath = _machinePath;
    IORWLockUnlock(_pollCtrlLock);

    fWorkLoop->runAction(OSMemberFunctionCast(IOWorkLoop::Action, this, &AppleSmartBattery::handlePollingFinishedGated),
                         this, VOIDPTR(visitedEntirePath), VOIDPTR(machinePath));

    IORWLockWrite(_pollCtrlLock);
    _pollingNow = false;
    IORWLockUnlock(_pollCtrlLock);

    acknowledgeSystemSleepWake();
}

bool AppleSmartBattery::handleSetItAndForgetIt(int state, int val, const uint8_t *str32, IOByteCount len)
{
    CommandStruct   *this_command = NULL;
    const OSData    *publishData;
    const OSSymbol  *publishSym;

    /* Set it and forget it
     *
     * These commands specify an OSSymbol in their CommandStruct.
     * We directly publish the data into these registry keys.
     */
    if ((this_command = commandForState(state)) && this_command->setItAndForgetItSym) {
        if ((this_command->opType == kASBMSMBUSReadWord) || (this_command->opType == kASBMSMBUSExtendedReadWord)) {
            SET_INTEGER_IN_PROPERTIES(this_command->setItAndForgetItSym, val, (unsigned int)len);
            return true;
        } else if (this_command->opType == kASBMSMBUSReadBlock) {
            if (state == kBManufactureDataCmd) {
                publishData = OSData::withBytes((const void *)str32, (unsigned int)len);
                if (publishData) {
                    setPSProperty(this_command->setItAndForgetItSym, (OSObject *)publishData);
                    publishData->release();
                    return true;
                }
            }
            else {
                publishSym = OSSymbol::withCString((const char *)str32);
                if (publishSym) {
                    setPSProperty(this_command->setItAndForgetItSym, (OSObject *)publishSym);
                    publishSym->release();
                    return true;
                }
            }
        }
        else if (this_command->opType == kASBMSMCReadBool) {
            setPSProperty(this_command->setItAndForgetItSym, (*str32) ? kOSBooleanTrue : kOSBooleanFalse);
            return true;
        }
        else if (this_command->opType == kASBMSMCReadDictionary) {
            // This is data is already set to registry as required
            return true;
        }
    }

    return false;
}

struct transactionCompletionGatedArgs {
    CommandStruct *cs;
    IOReturn status;
    IOByteCount inCount;
    uint8_t *inData;
    uint32_t nextState;
};

IOReturn AppleSmartBattery::transactionCompletionGated(struct transactionCompletionGatedArgs *args)
{
    IOByteCount inCount = args->inCount;
    uint8_t *inData = args->inData;
    bool            transaction_success = (args->status == kIOReturnSuccess);
    uint32_t        val = 0;
    uint64_t        val64 = 0;
    CommandStruct * cs = args->cs;
    uint32_t        cmd = kTransactionRestart;
    uint32_t        smcKey = 0;
    static unsigned int txnFailures;
    IOPMrootDomain  *rd;

    if (fSystemSleeping) {
        BM_ERRLOG("Aborting transactions as system is sleeping\n");
        return kIOReturnAborted;
    }

    args->nextState = kTransactionRestart;

    if (cs) {
        args->nextState = cmd = cs->cmd;
        smcKey = cs->smcKey;
    }

    if (cmd) {
        if (transaction_success) {
            txnFailures = 0;

            if (inData) {
                if (inCount == 1) {
                    val = inData[0];
                    BM_LOG2("smcReadKeyHostEndian for cmd 0x%x (key %c%c%c%c) %d\n", cmd, SMCKEY2CHARS(smcKey), val);
                }
                else if (inCount == 2) {
                    val = (inData[1] << 8) | inData[0];
                    BM_LOG2("smcReadKeyHostEndian for cmd 0x%x (key %c%c%c%c) %d\n", cmd, SMCKEY2CHARS(smcKey), val);
                }
                else if (inCount == 4) {
                    val = (inData[3] << 24) | (inData[2] << 16) | (inData[1] << 8) | inData[0];
                    BM_LOG2("smcReadKeyHostEndian for cmd 0x%x (key %c%c%c%c) %d\n", cmd, SMCKEY2CHARS(smcKey), val);
                }
                else if (inCount == 8) {
                    val = (inData[7] << 24) | (inData[6] << 16) | (inData[5] << 8) | inData[4];
                    val64 = (uint64_t)val << 32;
                    val = (inData[3] << 24) | (inData[2] << 16) | (inData[1] << 8) | inData[0];
                    val64 |= val;
                    BM_LOG2("smcReadKeyHostEndian for cmd 0x%x (key %c%c%c%c) 0x%llx\n", cmd, SMCKEY2CHARS(smcKey), val64);
                }
            }

            if (handleSetItAndForgetIt(cmd, val, inData, inCount)) {
                goto exit;
            }
        } else {
            txnFailures++;
            goto exit;
        }
    }

    // Restart?
    IORWLockWrite(_pollCtrlLock);

    if (!cmd || _rebootPolling) {
        // NULL cmd means we should start the state machine from scratch.
        cmd = args->nextState = kTransactionRestart;
        _rebootPolling = false;
        BM_LOG1("Restarting poll type %d\n", _machinePath);
    }

    IORWLockUnlock(_pollCtrlLock);

    switch (cmd) {
    case kTransactionRestart:
        IOTimeStampStartConstant(SYSTEMCHARGING_ASBM_BATTERY_POLL);

        IORWLockWrite(_pollCtrlLock);

        _cancelPolling = false;
        _pollingNow = true;
        txnFailures = 0;  // reset txnFailures to allow new battery polls

        IORWLockUnlock(_pollCtrlLock);

        /* Initialize battery read timeout to catch any longstanding stalls. */
        if (fBatteryReadAllTimer) {
            fBatteryReadAllTimer->cancelTimeout();
            fBatteryReadAllTimer->setTimeoutMS(kBatteryReadAllTimeout);
        }
        break;

    case kMStateContCmd: {
        // Determines if AC is plugged or unplugged
        // Determines if AC is "charge capable"
        /* If fInflowDisabled is currently set, then we acknowledge
         * our lack of AC power. inflow disable means the system is not drawing power from AC.
         * (Having inflow disabled is uncommon.)
         *
         * Even with inflow disabled, the AC bit is still true if AC
         * is attached. We zero the bit instead, so that it looks
         * more accurate in BatteryMonitor.
         */
        bool new_ac_connected = (!fInflowDisabled && (val & kMACPresentBit)) ? 1:0;

        // Tell IOPMrootDomain on ac connect/disconnect
        rd = getPMRootDomain();
        if (new_ac_connected != fACConnected) {
            if (new_ac_connected) {
                clock_get_uptime(&acAttach_ts);
                if (rd) rd->receivePowerNotification(kIOPMSetACAdaptorConnected | kIOPMSetValue);
            } else {
                acAttach_ts = 0;
                if (rd) rd->receivePowerNotification(kIOPMSetACAdaptorConnected);
            }
        }

        fACConnected = new_ac_connected;
        setExternalConnectedToIOPMPowerSource(fACConnected);

        setExternalChargeCapable((val & kMPowerNotGoodBit) ? false:true);

        } break;

    case kMStateCmd:
        // Determines if battery is present
        // Determines if battery is charging
#if TARGET_OS_OSX_X86
        fBatteryPresent = (val & kMPresentBatt_A_Bit) ? true : false;
        setBatteryInstalled(fBatteryPresent);
#endif // TARGET_OS_OSX_X86

        // If fChargeInhibit is currently set, then we acknowledge
        // our lack of charging and force the "isCharging" bit to false.
        //
        // charge inhibit means the battery will not charge, even if
        // AC is attached.
        // Without marking this lack of charging here, it can take
        // up to 30 seconds for the charge disable to be reflected in
        // the UI.

        if (fChargeInhibited) {
            setIsCharging(false);
        } else {
#if TARGET_OS_OSX_X86
           setIsCharging((val & kMChargingBatt_A_Bit) ? true:false);
#else
           // 'val' represents the ChargerStatus
           // IsCharging = ExternalConnected && ChargeCapable && !ChargeInhibted
           setIsCharging((val && fACConnected && fACChargeCapable) ? true : false);
           BM_LOG2("ChargerState:%d ExtConnected:%d ChargeCapable:%d\n", val, fACConnected, fACChargeCapable);
#endif
        }

        break;

    case kBBatteryStatusCmd:
        if (val & kBFullyChargedStatusBit) {
            fFullyCharged = true;
        } else {
            fFullyCharged = false;
        }

        if (val & kBFullyDischargedStatusBit) {
            if (!fFullyDischarged) {
                fFullyDischarged = true;

                // Immediately cancel AC Inflow disable
                fProvider->handleFullDischarge();
            }
        } else {
            fFullyDischarged = false;
        }

        /* Detect battery permanent failure
         * Permanent battery failure is marked by
         * (TerminateDischarge & TerminateCharge) bits being set simultaneously.
         */
        if ((val
            & (kBTerminateDischargeAlarmBit | kBTerminateChargeAlarmBit))
            == (kBTerminateDischargeAlarmBit | kBTerminateChargeAlarmBit)) {
            BM_ERRLOG("Failed with permanent failure for cmd 0x%x\n", args->nextState);
            setErrorCondition((OSSymbol *)_PermanentFailureSym);

            fPermanentFailure = true;

            /* We want to display the battery as present & completely discharged, not charging */
            fBatteryPresent = true;
            setBatteryInstalled(true);
            setIsCharging(false);
        } else {
            fPermanentFailure = false;
        }

        setPSProperty(_FullyChargedSym, (fFullyCharged ? kOSBooleanTrue :kOSBooleanFalse ));

        /* If the battery is present, we continue with our state machine
           and read battery state below.
           Otherwise, if the battery is not present, we zero out all
           the settings that would have been set in a connected battery.
        */
        if (!fBatteryPresent) {
            // Clean-up battery state for absent battery; do no further
            // battery work until messaged that another battery has
            // arrived.

            // zero out battery state with argument (do_update == true)
            // clearing out the battery statud here since battery is not found!!!
            clearBatteryState(true);
            return kIOReturnAborted;
        }

        break;

    case kBRemainingCapacityCmd:
        if (!fCapacityOverride) {
            setCurrentCapacity(val);
        }
        else {
            BM_LOG1("Capacity override is true\n");
        }

#if TARGET_OS_OSX_X86
        SET_INTEGER_IN_PROPERTIES(_RawCurrentCapacitySym, val, 2);
#endif // TARGET_OS_OSX_X86

        if (!fPermanentFailure && (0 == val)) {
            // RemainingCapacity == 0 is an absurd value.
            // We have already retried several times, so we accept this value and move on.
            BM_ERRLOG("Battery remaining capacity is set to 0\n");
        }
        break;

    case kBFullChargeCapacityCmd:
        if (!fCapacityOverride) {
            setMaxCapacity(val);
        }
        else {
            BM_LOG1("Capacity override is true\n");
        }

#if TARGET_OS_OSX_X86
        SET_INTEGER_IN_PROPERTIES(_RawMaxCapacitySym, val, 2);
#endif // TARGET_OS_OSX_X86
        break;

    /* *Instant* current */
    case kBCurrentCmd:
        SET_INTEGER_IN_PROPERTIES(_InstantAmperageSym, (int32_t)((int16_t)(val & 0xffff)), 4);
        fInstantCurrent = (int)(int16_t)val;
        break;

    /* Average current */
    case kBAverageCurrentCmd:
        setAmperage((int16_t)val);
        if (!val) {
            // Battery not present, or fully charged, or general error
            setTimeRemaining(0);
        }
        break;

    case kBAverageTimeToEmptyCmd:
        SET_INTEGER_IN_PROPERTIES(_AvgTimeToEmptySym, val, inCount);

        // if there is no AC power then we publish Time to Empty
        // The value from GG already takes into account the AVG current
        if (!fACConnected) {
            setTimeRemaining(val);
        }
        break;

    case kBAverageTimeToFullCmd:
        SET_INTEGER_IN_PROPERTIES(_AvgTimeToFullSym, val, inCount);
        // if there is AC power then we publish Time to Full
        // The value from GG already takes into account the AVG current
        if (fACConnected) {
            setTimeRemaining(val);
        }
        break;

    case kBExtendedPFStatusCmd:
    case kBExtendedOperationStatusCmd:
        // 2 stage commands, first stage SMBus write completed.
        // Do nothing other than to prevent the error log in the default case.
        break;

    case kBTemperatureCmd:
        // OSX historically uses SmartBattery format directly. On other
        // platforms we publish in centi C.
#if !TARGET_OS_OSX
        val = (uint32_t)((val64 * 100) >> 16);
#endif // !TARGET_OS_OSX
        SET_INTEGER_IN_PROPERTIES(_TemperatureSym, val, 4);
        break;

#if TARGET_OS_IPHONE || TARGET_OS_OSX_AS
    case kBVirtualTemperatureCmd:
        val = (uint32_t)((val64 * 100) >> 16);
        SET_INTEGER_IN_PROPERTIES(_VirtualTemperatureSym, val, 4);
        break;
    case kKioskModeCmd:
    case kCarrierModeCmd:
        // Nothing to do on this
        break;

    case kNominalChargeCapacityCmd:
        SET_INTEGER_IN_PROPERTIES(_NominalChargeCapacitySym, val, inCount);
        updateChannelValue(fReporterBatteryData, kIOReportBatteryNominalChargeCapacityID, val);
        break;

    case kExternalConnectedCmd:
        // disconnect
        if (fACConnected && !val) {
            // debounce disconnects to avoid multi-chimes on connect events
            fExternalConnectedTimer->setTimeoutMS(kExternalConnectedDebounceMs);
            break;
        } else if (!fACConnected && val) {
            // connect
#if TARGET_OS_OSX
            IOPMrootDomain *rd = getPMRootDomain();
            if (rd) {
                rd->receivePowerNotification(kIOPMSetACAdaptorConnected | kIOPMSetValue);
            }
#endif // TARGET_OS_OSX
        }

        if (fACConnected != val) {
            setExternalConnectedToIOPMPowerSource(val);
            fACConnected = val;
        }
        break;

    case kExternalChargeCapableCmd:
        if (fACChargeCapable && !val) {
            fExternalConnectedTimer->setTimeoutMS(kExternalConnectedDebounceMs);
            break;
        }
        setExternalChargeCapable(val);
        fACChargeCapable = val;
        break;

    case kErrorConditionCmd:
        if (val & NOT_CHARGING_REASON_PERMANENT_FAULT_MASK) {
            const OSSymbol *error;

            if (val & NOT_CHARGING_REASON_VBAT_VFAULT) {
                error = OSSymbol::withCString("Vbatt Fault");
            } else if (val & NOT_CHARGING_REASON_IBAT_MINFAULT) {
                error = OSSymbol::withCString("Ibatt MinFault");
            } else if (val & NOT_CHARGING_REASON_CHARGER_COMMUNICATION_FAILED) {
                error = OSSymbol::withCString("Charger Communication Failure");
            } else if (val & NOT_CHARGING_REASON_CELL_CHECK_FAULT) {
                error = OSSymbol::withCString("Cell Check Fault");
            } else if (val & NOT_CHARGING_REASON_BATT_CHARGED_TOO_LONG) {
                error = OSSymbol::withCString("Charged Too Long");
            } else {
                error = OSSymbol::withCString(kIOPMPSPermanentFailureKey);
            }

            setErrorCondition((OSSymbol *)error);
            OSSafeReleaseNULL(error);
            fPermanentFailure = true;

            /* We want to display the battery as present & completely discharged, not charging */
            fBatteryPresent = true;
            setBatteryInstalled(true);
            setIsCharging(false);
        } else {
            properties->removeObject(errorConditionKey);
            removeProperty(errorConditionKey);
            fPermanentFailure = false;
        }
        break;

    case kAtCriticalLevelCmd:
        if (_useAcForCritical) {
            // set critical level based on AC status
            BM_LOG2("Use AC state for critical battery\n");
            val = !fACConnected;
        }

        if (val == 1) {
            clock_sec_t secs;
            clock_usec_t microsecs;
            SMCKey key = 'UB0T';

            clock_get_calendar_microtime(&secs, &microsecs);
            SMCResult rc = _smcWriteKey(key, sizeof(secs), &secs);
            if (rc != kSMCSuccess) {
                BM_ERRLOG("Failed to set battery shutdown timestamp. rc:0x%x=%s\n", rc, printSMCResult(rc));
            }
#if TARGET_OS_OSX
            rd = getPMRootDomain();
            if (rd) {
                BM_LOG1("Sending Low battery notification to rootDomain\n");
                rd->receivePowerNotification(kIOPMPowerEmergency);
            }
            else {
                BM_ERRLOG("Failed to get rootDomain pointer\n");
            }
#endif // TARGET_OS_OSX
        }
        setAtCriticalLevel(val);
        break;
#endif // TARGET_OS_IPHONE || TARGET_OS_OSX_AS

    default:
        BM_ERRLOG("SmartBattery: Error state %x not expected\n", args->nextState);
    }

exit:
    if (txnFailures > 5) {
        BM_ERRLOG("Too many transaction errors, abort poll\n");
        return kIOReturnAborted;
    }

    return kIOReturnSuccess;
}

void AppleSmartBattery::transactionCompletion(void *ref, IOReturn status, IOByteCount inCount, uint8_t *inData)
{
    bool cancelPolling;

    IOReturn ret;
    IORWLockRead(_pollCtrlLock);
    cancelPolling = _cancelPolling;
    IORWLockUnlock(_pollCtrlLock);
    struct transactionCompletionGatedArgs args = { .cs = (CommandStruct *)ref, .status = status, .inCount = inCount, .inData = inData, .nextState = kTransactionRestart, };

    if (cancelPolling) {
        goto abort;
    }

    ret = fWorkLoop->runAction(OSMemberFunctionCast(IOWorkLoop::Action, this, &AppleSmartBattery::transactionCompletionGated),
                                        this, &args);
    if (ret != kIOReturnSuccess) {
        goto abort;
    }

    /* Kick off the next transaction */
    if (kFinishPolling != args.nextState) {
        this->initiateNextTransaction(args.nextState);
    }

    return;

abort:
    handlePollingFinished(false);
    return;
}

void AppleSmartBattery::clearBatteryStateGated(bool do_update)
{
    // Only clear out battery state; don't clear manager state like AC Power.
    // We just zero out the int and bool values, but remove the OSType values.

    fFullyDischarged        = false;
    fFullyCharged           = false;
    fBatteryPresent         = false;
    fACConnected            = -1;

    setBatteryInstalled(false);
    setIsCharging(false);
    setCurrentCapacity(0);
    setMaxCapacity(0);
    setTimeRemaining(0);
    setAmperage(0);
    setVoltage(0);
    setCycleCount(0);
    setAdapterInfo(0);
    setLocation(0);

    properties->removeObject(manufacturerKey);
    removeProperty(manufacturerKey);

    // Let rebuildLegacyIOBatteryInfoGated() update batteryInfoKey and detect
    // if any value in the dictionary has changed. Removing batteryInfoKey
    // from properties will always dirty the battery state and will cause
    // IOPMPowerSource::updateStatus() to message clients unnecessarily
    // when battery is not present.
    // properties->removeObject(batteryInfoKey);

    removeProperty(batteryInfoKey);
    properties->removeObject(errorConditionKey);
    removeProperty(errorConditionKey);
    properties->removeObject(_PFStatusSym);
    removeProperty(_PFStatusSym);

    rebuildLegacyIOBatteryInfoGated();

    BM_ERRLOG("Clearing out battery data\n");

    if (do_update) {
        updateStatus();
    }
}

void AppleSmartBattery::clearBatteryState(bool do_update)
{
    fWorkLoop->runAction(OSMemberFunctionCast(IOWorkLoop::Action, this, &AppleSmartBattery::clearBatteryStateGated),
                         this, VOIDPTR(do_update));
}

/******************************************************************************
 *  Package battery data in "legacy battery info" format, readable by
 *  any applications using the not-so-friendly IOPMCopyBatteryInfo()
 ******************************************************************************/

void AppleSmartBattery::rebuildLegacyIOBatteryInfoGated(void)
{
#if TARGET_OS_OSX_X86
    OSDictionary        *legacyDict = OSDictionary::withCapacity(5);
    uint32_t            flags = 0;
    OSNumber            *flags_num = NULL;

    if (externalConnected()) flags |= kIOPMACInstalled;
    if (batteryInstalled()) flags |= kIOPMBatteryInstalled;
    if (isCharging()) flags |= kIOPMBatteryCharging;

    flags_num = OSNumber::withNumber((unsigned long long)flags, 32);
    legacyDict->setObject(kIOBatteryFlagsKey, flags_num);
    flags_num->release();

    legacyDict->setObject(kIOBatteryCurrentChargeKey, properties->getObject(kIOPMPSCurrentCapacityKey));
    legacyDict->setObject(kIOBatteryCapacityKey, properties->getObject(kIOPMPSMaxCapacityKey));
    legacyDict->setObject(kIOBatteryVoltageKey, properties->getObject(kIOPMPSVoltageKey));
    legacyDict->setObject(kIOBatteryAmperageKey, properties->getObject(kIOPMPSAmperageKey));
    legacyDict->setObject(kIOBatteryCycleCountKey, properties->getObject(kIOPMPSCycleCountKey));

    setLegacyIOBatteryInfo(legacyDict);

    legacyDict->release();
#endif // TARGET_OS_OSX_X86
}

void AppleSmartBattery::rebuildLegacyIOBatteryInfo(void)
{
#if TARGET_OS_OSX_X86
    fWorkLoop->runAction(OSMemberFunctionCast(IOWorkLoop::Action, this, &AppleSmartBattery::rebuildLegacyIOBatteryInfoGated),
                         this);
#endif // TARGET_OS_OSX_X86
}


void AppleSmartBattery::setExternalConnectedToIOPMPowerSource(bool externalConnected)
{
    setExternalConnected(externalConnected);
}


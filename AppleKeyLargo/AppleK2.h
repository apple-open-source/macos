 /*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1998-2002 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: William Gulland
 *
 */

#ifndef _IOKIT_K2_H
#define _IOKIT_K2_H

#include <IOKit/IOLocks.h>

#include <IOKit/platform/AppleMacIO.h>

#include "USBKeyLargo.h"
#include "KeyLargoWatchDogTimer.h"
#include "KeyLargo.h"

enum {
    kK2DeviceId41		= 0x41,				// K2
	kK2Version0			= 0,
    kShastaDeviceId4f	= 0x4f,
};

enum {
    // K2 registers different from or in addition to Keylargo
    kK2FCRCount			= 11,
    kK2FCRBase			= 0x00024,			// FCRs 10 to 6 are before 0 to 5!
    kK2FCR10			= 0x00024,
    kK2FCR9				= 0x00028,
    kK2FCR8				= 0x0002c,
    kK2FCR7				= 0x00030,
    kK2FCR6				= 0x00034,
    
    	// Feature Control Register 0 Definitions new in K2
    kK2FCR0USB0SWReset					= 1 << 21,			// USB0_SW_Reset_h
    kK2FCR0USB1SWReset					= 1 << 25,			// USB1_SW_Reset_h
	kK2FCR0RingPMEDisable				= 1 << 27,			// Ring_PME_disable_h

    	// Feature Control Register 1 Definitions new in K2
    kK2FCR1PCI1BusReset					= 1 << 8,			// PCI1_BusReset_L
    kK2FCR1PCI1SleepResetEn				= 1 << 9,			// PCI1_SleepResetEn
    kK2FCR1PCI1ClkEnable				= 1 << 14,			// PCI1_ClkEnable
    kK2FCR1FWClkEnable					= 1 << 15,			// FW_ClkEnable
    kK2FCR1FWReset						= 1 << 16,			// FW_Reset_L
    kK2FCR1I2S1SWReset					= 1 << 18,			// I2S1_SW_Reset
    kK2FCR1GBClkEnable					= 1 << 22,			// GB_ClkEnable
    kK2FCR1GBPwrDown					= 1 << 23,			// GB_PwrDown
    kK2FCR1GBReset						= 1 << 24,			// GB_Reset_L
    kK2FCR1SATAClkEnable				= 1 << 25,			// SATA_ClkEnable
    kK2FCR1SATAPwrDown					= 1 << 26,			// SATA_PwrDown
    kK2FCR1SATAReset					= 1 << 27,			// SATA_Reset_L
    kK2FCR1UATAClkEnable				= 1 << 28,			// UATA_ClkEnable
    kK2FCR1UATAReset					= 1 << 30,			// UATA_Reset_L
    kK2FCR1UATAChooseClk66				= 1 << 31,			// UATA_ChooseClk66
        // Feature Control Register 1 Definitions new in Shasta
    kShastaFCR1I2S2CellEnable			= 1 << 4,			// I2S2_Cell_En_H
    kShastaFCR1I2S2ClkEnable			= 1 << 6,			// I2S2_ClkEnBit_H
    kShastaFCR1I2S2Enable				= 1 << 7,			// I2S2Enable_H
    
        // Feature Control Register 2 is all different in K2
    kK2FCR2PWM0AutoStopEn				= 1 << 4,			// PWM0AutoStopEn
    kK2FCR2PWM1AutoStopEn				= 1 << 5,			// PWM1AutoStopEn
    kK2FCR2PWM2AutoStopEn				= 1 << 6,			// PWM2AutoStopEn
    kK2FCR2PWM3AutoStopEn				= 1 << 7,			// PWM3AutoStopEn
    kK2FCR2PWM0OverTempEn				= 1 << 8,			// PWM0_OverTempEn
    kK2FCR2PWM1OverTempEn				= 1 << 9,			// PWM1_OverTempEn
    kK2FCR2PWM2OverTempEn				= 1 << 10,			// PWM2_OverTempEn
    kK2FCR2PWM3OverTempEn				= 1 << 11,			// PWM3_OverTempEn
    kK2FCR2HTEnableInterrupts			= 1 << 15,			// HT_EnableInterrupts
    kK2FCR2SBMPICEnableOutputs			= 1 << 16,			// SB_MPIC_EnableOutputs
    kK2FCR2SBMPICReset					= 1 << 17,			// SB_MPIC_Reset_L
    kK2FCR2FWLinkOnIntEn				= 1 << 18,			// FW_LinkOnIntEn
    kK2FCR2FWAltLinkOnSel				= 1 << 19,			// FW_AltLinkOnSel
    kK2FCR2PWMsEn						= 1 << 20,			// PWMs_EN_h
    kK2FCR2GBWakeIntEn					= 1 << 21,			// GB_WakeIntEn
    kK2FCR2GBEnergyIntEn				= 1 << 22,			// GB_EnergyIntEn
    kK2FCR2BlockExtGPIO1				= 1 << 23,			// BlockExtGPIO1
    kK2FCR2PCI0BridgeInt				= 1 << 24,			// PCI0_BridgeInt
    kK2FCR2PCI1BridgeInt				= 1 << 25,			// PCI1_BridgeInt
    kK2FCR2PCI2BridgeInt				= 1 << 26,			// PCI2_BridgeInt
    kK2FCR2PCI3BridgeInt				= 1 << 27,			// PCI3_BridgeInt
    kK2FCR2PCI4BridgeInt				= 1 << 28,			// PCI4_BridgeInt
    kK2FCR2HTNonFatalError				= 1 << 30,			// HT_NonFatalError_L
    kK2FCR2HTFatalError					= 1 << 31,			// HT_FatalError_L
    
        	// Feature Control Register 3 Definitions new in K2
    kK2FCR3EnableOsc25Shutdown			= 1 << 0,			// EnableOsc25Shutdown
    kK2FCR3EnableFWPadPwrdown			= 1 << 1, 			// EnableFWpadPwrdown
    kK2FCR3EnableGBpadPwrdown			= 1 << 2,			// EnableGBpadPwrdown
    kK2FCR3EnablePLL0Shutdown			= 1 << 7,			// EnablePLL0Shutdown
    kK2FCR3EnablePLL6Shutdown			= 1 << 8, 			// EnablePLL6Shutdown
    kK2FCR3DynClkStopEnable				= 1 << 11,			// DynClkStopEnable
        	// Feature Control Register 3 Definitions new in Shasta
    kShastaFCR3I2S2Clk18Enable			= 1 << 15,			// I2S2_Clk18_EN_h
    
            // Feature Control Register 9 definitions
    kK2FCR9PCI1Clk66isStopped			= 1 << 0,
    kK2FCR9PCI2Clk66isStopped			= 1 << 1,
    kK2FCR9FWClk66isStopped				= 1 << 2,
    kK2FCR9UATAClk66isStopped			= 1 << 3,
    kK2FCR9UATAClk100isStopped			= 1 << 4,
    kK2FCR9PCI3Clk66isStopped			= 1 << 5,
    kK2FCR9GBClk66isStopped				= 1 << 6,
    kK2FCR9PCI4Clk66isStopped			= 1 << 7,
    kK2FCR9SATAClk66isStopped			= 1 << 8,
    kK2FCR9USB0Clk48isStopped			= 1 << 9,
    kK2FCR9USB1Clk48isStopped			= 1 << 10,
    kK2FCR9Clk45isStopped				= 1 << 11,
    kK2FCR9Clk49isStopped				= 1 << 12,
    kK2FCR9Osc25Shutdown				= 1 << 15,
    kK2FCR9ClkStopDelayShift			= 29,
    kK2FCR9ClkStopDelayMask				= 7 << 29
    
};


// desired state of K2 FCR registers when sleeping
enum {
	// Feature Control Register 0 Sleep Settings for K2
    kK2FCR0SleepBitsSet			=	0,
	
    kK2FCR0SleepBitsClear		=	kKeyLargoFCR0USB1CellEnable |
										kKeyLargoFCR0USB0CellEnable,

	// Feature Control Register 0 Sleep Settings for SCC
    kK2FCR0SCCSleepBitsSet		=	0,
	
    kK2FCR0SCCSleepBitsClear	=	kKeyLargoFCR0SccCellEnable |
										kKeyLargoFCR0SccBEnable |
										kKeyLargoFCR0SccAEnable,

    // Feature Control Register 1 Sleep Settings
    kK2FCR1SleepBitsSet			=	0,
	
    kK2FCR1SleepBitsClear		=	kKeyLargoFCR1I2S1Enable |
										kKeyLargoFCR1I2S1ClkEnable |
										kKeyLargoFCR1I2S1CellEnable |
										kKeyLargoFCR1I2S0Enable |
										kKeyLargoFCR1I2S0ClkEnable |
										kKeyLargoFCR1I2S0CellEnable |
										kK2FCR1GBClkEnable |
										kK2FCR1SATAClkEnable |
										kK2FCR1UATAClkEnable,
                                                
    // Feature Control Register 2 Sleep Settings
    kK2FCR2SleepBitsSet			=	0,
	
    // Stop interrupts when going to sleep
    kK2FCR2SleepBitsClear		=	kK2FCR2SBMPICEnableOutputs,

    // Feature Control Register 3 Sleep and Restart Settings
    // Keep Osc25 running to keep Ethernet PHY powered.
    kK2FCR3SleepBitsSet			=	0,
										
    kK2FCR3SleepBitsClear		=	kK2FCR3EnableOsc25Shutdown,	

    // do not turn off SPI interface when restarting
	
    kK2FCR3RestartBitsSet		=	0,
										
    kK2FCR3RestartBitsClear		=	0,
                                                
    // Feature Control Register 4 Sleep Settings    
    kK2FCR4SleepBitsSet			=	0,
	kK2FCR4SleepBitsClear		=	0

};

// Just one kind of GPIO in K2
enum {
    kK2GPIOBase					= 	0x58,
    kK2GPIOCount				= 	51
};

// HyperTransport Link Capabilites Block offset (these are the only ones we care about)
enum {
	kHTLinkCapLDTCapOffset		= 0x0,		// Capabilities is in low byte
	kHTLinkCapLinkCtrlOffset	= 0x4,		// Link width is in high 16 bits
	kHTLinkCapLDTOffset			= 0xC		// Link frequency is in bits 8-11
};

class AppleK2Device : public AppleMacIODevice
{
    OSDeclareDefaultStructors(AppleK2Device);
    
public:
    bool compareName( OSString * name, OSString ** matched = 0 ) const;
    IOReturn getResources( void );
};

class AppleK2 : public KeyLargo
{
	OSDeclareDefaultStructors(AppleK2);
  
private:
	UInt32				k2CPUVCoreSelectGPIO;
 
	// remember if we need to keep the SCC enabled during sleep
	bool			keepSCCenabledInSleep;
	
	void			EnableSCC(bool state, UInt8 device, bool type);
	void			PowerModem(bool state);
	void 			ModemResetLow();
	void 			ModemResetHigh();
	void			PowerI2S (bool powerOn, UInt32 cellNum);
	IOReturn		SetPowerSupply (bool powerHi);
	void			AdjustBusSpeeds ( void );
  
	KeyLargoWatchDogTimer	*watchDogTimer;
    IOService		*fProvider;
    UInt32			fPHandle;
	
	// callPlatformFunction symbols
	const OSSymbol 	*keyLargo_resetUniNEthernetPhy;
	const OSSymbol 	*keyLargo_restoreRegisterState;
	const OSSymbol 	*keyLargo_syncTimeBase;
	const OSSymbol 	*keyLargo_recalibrateBusSpeeds;
	const OSSymbol 	*keyLargo_saveRegisterState;
	const OSSymbol 	*keyLargo_turnOffIO;
	const OSSymbol 	*keyLargo_writeRegUInt8;
	const OSSymbol 	*keyLargo_safeWriteRegUInt8;
	const OSSymbol 	*keyLargo_safeReadRegUInt8;
	const OSSymbol 	*keyLargo_safeWriteRegUInt32;
	const OSSymbol 	*keyLargo_safeReadRegUInt32;
	const OSSymbol 	*keyLargo_getHostKeyLargo;
	const OSSymbol 	*keyLargo_powerI2S;
	const OSSymbol 	*keyLargo_setPowerSupply;
    const OSSymbol	*keyLargo_EnableI2SModem;
    const OSSymbol	*mac_io_publishChildren;
    const OSSymbol	*mac_io_publishChild;
    const OSSymbol	*k2_enableFireWireClock;
    const OSSymbol	*k2_enableEthernetClock;
    const OSSymbol	*k2_getHTLinkFrequency;
    const OSSymbol	*k2_setHTLinkFrequency;
    const OSSymbol	*k2_getHTLinkWidth;
    const OSSymbol	*k2_setHTLinkWidth;
    
	// Power Management support functions and data structures:
	// These come (almost) unchanged from the MacOS9 Power
	// Manager plug-in (p99powerplugin.c)

	struct MPICTimers {
		UInt32			currentCountRegister;
		UInt32			baseCountRegister;
		UInt32			vectorPriorityRegister;
		UInt32			destinationRegister;
	};
	typedef struct MPICTimers MPICTimers;
	typedef volatile MPICTimers *MPICTimersPtr;

	struct K2MPICState {
        UInt32			mpicGlobal0;
		UInt32 			mpicIPI[kKeyLargoMPICIPICount];
		UInt32 			mpicSpuriousVector;
		UInt32 			mpicTimerFrequencyReporting;
		MPICTimers 		mpicTimers[kKeyLargoMPICTimerCount];
		UInt32 			mpicInterruptSourceVectorPriority[kKeyLargoMPICVectorsCount];
		UInt32 			mpicInterruptSourceDestination[kKeyLargoMPICVectorsCount];
		UInt32			mpicCurrentTaskPriorities[kKeyLargoMPICTaskPriorityCount];
	};
	typedef struct K2MPICState K2MPICState;
	typedef volatile K2MPICState *k2MPICStatePtr;

	struct K2GPIOState {
		UInt32 			gpioLevels[2];
		UInt8 			gpio[kK2GPIOCount];
	};
	typedef struct K2GPIOState K2GPIOState;
	typedef volatile K2GPIOState *k2GPIOStatePtr;

	struct K2ConfigRegistersState {
		UInt32 			mediaBay;
		UInt32 			featureControl[kK2FCRCount];
	};
	typedef struct K2ConfigRegistersState K2ConfigRegistersState;
	typedef volatile K2ConfigRegistersState *k2ConfigRegistersStatePtr;

	// This is a short version of the IODBDMAChannelRegisters which includes only
	// the registers we actually mean to save
	struct DBDMAChannelRegisters {
		UInt32			commandPtrLo;
		UInt32			interruptSelect;
		UInt32			branchSelect;
		UInt32			waitSelect;
	};
	typedef struct DBDMAChannelRegisters DBDMAChannelRegisters;
	typedef volatile DBDMAChannelRegisters *DBDMAChannelRegistersPtr;

	struct K2DBDMAState {
		DBDMAChannelRegisters 	dmaChannel[kKeyLargoDBDMAChannelCount];
	};
	typedef struct K2DBDMAState K2DBDMAState;
	typedef volatile K2DBDMAState *k2DBDMAStatePtr;

	struct K2I2SState {
		UInt32					i2s[kKeyLargoI2SRegisterCount * kKeyLargoI2SChannelCount];
	};
	typedef struct K2I2SState K2I2SState;
	typedef volatile K2I2SState *k2I2SStateStatePtr;

	struct K2State {
		bool							thisStateIsValid;
		K2MPICState				savedMPICState;
		K2GPIOState				savedGPIOState;
		K2ConfigRegistersState	savedConfigRegistersState;
		K2DBDMAState				savedDBDMAState;
		K2I2SState				savedI2SState;
		UInt8							savedVIAState[9];
	};
	typedef struct K2State K2State;
	
	// Base offset to HyperTransport link capabilities block
	UInt32 htLinkCapabilitiesBase;

	// These are actually the buffers where we save k2's state (above there
	// are only definitions).
	K2State savedK2State;
  
	// Methods to save and restore the state:
	void saveK2State();
	void restoreK2State();

	// this is to ensure mutual exclusive access to
	// the k2 registers:
	IOSimpleLock *mutex;

	// Remember the bus speed:
	long long busSpeed;
  
	// Reference counts for shared hardware
	long clk45RefCount;			// 45.1 MHz clock - Audio, I2S & SCC
	long clk49RefCount;			// 49.1 MHz clock - Audio & I2S
  
    OSArray *fPlatformFuncArray;	// The array of IOPlatformFunction objects

	void resetUniNEthernetPhy(void);
    void enablePCIDeviceClock(UInt32 mask, bool enable, IOService *nub);
    void EnableI2SModem(bool enable);
    bool performFunction(IOPlatformFunction *func, void *pfParam1 = 0,
			void *pfParam2 = 0, void *pfParam3 = 0, void *pfParam4 = 0);
    void logClockState();
         
public:
	virtual bool      init(OSDictionary *);
	virtual bool      start(IOService *provider);
	virtual void      stop(IOService *provider);
 
    // Override to publish just immediate children
    virtual void publishBelow( IORegistryEntry * root );
    
    // Override to remove 'k2-' frame nub name
    virtual void processNub( IOService * nub );

    // Override to create AppleK2Device nubs
    virtual IOService * createNub( IORegistryEntry * from );

	virtual IOReturn callPlatformFunction(const OSSymbol *functionName,
					bool waitForFunction, void *param1, void *param2,
					void *param3, void *param4);
  
	virtual long long syncTimeBase(void);
	virtual void	  recalibrateBusSpeeds(void);

	virtual void      turnOffK2IO(bool restart);

	virtual void	  setReferenceCounts (void);
	virtual void      saveRegisterState(void);
	virtual void      restoreRegisterState(void);
	virtual void      enableCells();
  
	// share register access:
	void safeWriteRegUInt32(unsigned long offset, UInt32 mask, UInt32 data);

	// Power handling methods:
	void initForPM (IOService *provider);
	IOReturn setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice);
  
	virtual IOReturn setAggressiveness( unsigned long selector, unsigned long newLevel );

	virtual bool getHTLinkFrequency (UInt32 *freqResult);
	virtual bool setHTLinkFrequency (UInt32 newFreq);
	virtual bool getHTLinkWidth (UInt32 *linkOutWidthResult, UInt32 *linkInWidthResult);
	virtual bool setHTLinkWidth (UInt32 newLinkOutWidth, UInt32 newLinkInWidth);

};

#endif /* ! _IOKIT_K2_H */

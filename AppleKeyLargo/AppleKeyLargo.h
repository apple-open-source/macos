 /*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/*
 * Copyright (c) 1998-2001 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Dave Radcliffe
 *
 */

#ifndef _APPLEKEYLARGO_H
#define _APPLEKEYLARGO_H

#include <IOKit/IOLocks.h>

#include <IOKit/platform/AppleMacIO.h>

#include "USBKeyLargo.h"
#include "KeyLargoWatchDogTimer.h"
#include "KeyLargo.h"

class AppleKeyLargo : public KeyLargo
{
	OSDeclareDefaultStructors(AppleKeyLargo);
  
private:
	UInt32				keyLargoCPUVCoreSelectGPIO;
 
	// Remember if the media bay needs to be turnedOn:
	bool		   	mediaIsOn;
	
	// remember if we need to keep the SCC enabled during sleep
	bool			keepSCCenabledInSleep;
	
	void			EnableSCC(bool state, UInt8 device, bool type);
	void			PowerModem(bool state);
	void 			ModemResetLow();
	void 			ModemResetHigh();
	void			PowerI2S (bool powerOn, UInt32 cellNum);
	IOReturn		SetPowerSupply (bool powerHi);
  
	// ***Added for outputting the FCR values to the IORegistry
	const OSSymbol	*keyLargo_FCRNode;
	const OSObject	*fcrs[kPangeaFCRCount];  
	const OSArray	*fcrArray;
  
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
	const OSSymbol 	*keyLargo_powerMediaBay;
	const OSSymbol 	*keyLargo_getHostKeyLargo;
	const OSSymbol 	*keyLargo_powerI2S;
	const OSSymbol 	*keyLargo_setPowerSupply;
  
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

	struct KeyLargoMPICState {
		UInt32 			mpicIPI[kKeyLargoMPICIPICount];
		UInt32 			mpicSpuriousVector;
		UInt32 			mpicTimerFrequencyReporting;
		MPICTimers 		mpicTimers[kKeyLargoMPICTimerCount];
		UInt32 			mpicInterruptSourceVectorPriority[kKeyLargoMPICVectorsCount];
		UInt32 			mpicInterruptSourceDestination[kKeyLargoMPICVectorsCount];
		UInt32			mpicCurrentTaskPriorities[kKeyLargoMPICTaskPriorityCount];
	};
	typedef struct KeyLargoMPICState KeyLargoMPICState;
	typedef volatile KeyLargoMPICState *KeyLargoMPICStatePtr;

	struct KeyLargoGPIOState {
		UInt32 			gpioLevels[2];
		UInt8 			extIntGPIO[18];
		UInt8 			gpio[17];
	};
	typedef struct KeyLargoGPIOState KeyLargoGPIOState;
	typedef volatile KeyLargoGPIOState *KeyLargoGPIOStatePtr;

	struct KeyLargoConfigRegistersState {
		UInt32 			mediaBay;
		UInt32 			featureControl[kPangeaFCRCount];		// Allow for worst case - Pangea
	};
	typedef struct KeyLargoConfigRegistersState KeyLargoConfigRegistersState;
	typedef volatile KeyLargoConfigRegistersState *KeyLargoConfigRegistersStatePtr;

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

	struct KeyLargoDBDMAState {
		DBDMAChannelRegisters 	dmaChannel[kKeyLargoDBDMAChannelCount];
	};
	typedef struct KeyLargoDBDMAState KeyLargoDBDMAState;
	typedef volatile KeyLargoDBDMAState *KeyLargoDBDMAStatePtr;


	struct KeyLargoAudioState {
		UInt32					audio[kKeyLargoAudioRegisterCount];
	};
	typedef struct KeyLargoAudioState KeyLargoAudioState;
	typedef volatile KeyLargoAudioState *KeyLargoAudioStateStatePtr;


	struct KeyLargoI2SState {
		UInt32					i2s[kKeyLargoI2SRegisterCount * kKeyLargoI2SChannelCount];
	};
	typedef struct KeyLargoI2SState KeyLargoI2SState;
	typedef volatile KeyLargoI2SState *KeyLargoI2SStateStatePtr;

	struct KeyLargoState {
		bool							thisStateIsValid;
		KeyLargoMPICState				savedMPICState;
		KeyLargoGPIOState				savedGPIOState;
		KeyLargoConfigRegistersState	savedConfigRegistersState;
		KeyLargoDBDMAState				savedDBDMAState;
		KeyLargoAudioState				savedAudioState;
		KeyLargoI2SState				savedI2SState;
		UInt8							savedVIAState[9];
	};
	typedef struct KeyLargoState KeyLargoState;

	// These are actually the buffers where we save keylargo's state (above there
	// are only definitions).
	KeyLargoState savedKeyLargoState;
  
	// This is instead only for the wireless slot:
	typedef struct WirelessPower {
		bool		cardPower;				// Keeps track of the power in the wireless card.
		UInt8		wirelessCardReg[5];		// A backup of the registers we are overwriting.
	} WirelessPower;
	WirelessPower cardStatus;
	


	// Methods to save and restore the state:
	void saveKeyLargoState();
	void restoreKeyLargoState();

	// Reference counts for shared hardware
	long clk31RefCount;			// 31.3 MHz clock - SCC & VIA
	long clk45RefCount;			// 45.1 MHz clock - Audio, I2S & SCC
	long clk49RefCount;			// 49.1 MHz clock - Audio & I2S
	long clk32RefCount;			// 32.0 MHz clock - SCC & VIA (Pangea only)
    bool fI2SState[2];			// Power state of I2S
	void resetUniNEthernetPhy(void);
  
public:
	virtual bool      init(OSDictionary *);
	virtual bool      start(IOService *provider);
	virtual void      stop(IOService *provider);
    virtual void	  processNub(IOService * nub);
    
	virtual IOReturn callPlatformFunction(const OSSymbol *functionName,
					bool waitForFunction, void *param1, void *param2,
					void *param3, void *param4);
  
	virtual IOReturn callPlatformFunction( const char * functionName,
					  bool waitForFunction, void *param1, void *param2,
					  void *param3, void *param4);
					  
	virtual long long syncTimeBase(void);
	virtual void	  recalibrateBusSpeeds(void);

	virtual void      turnOffKeyLargoIO(bool restart);
	virtual void      turnOffPangeaIO(bool restart);
	virtual void      turnOffIntrepidIO(bool restart);

	virtual void      powerWireless(bool powerOn);

	virtual void	  setReferenceCounts (void);
	virtual void      saveRegisterState(void);
	virtual void      restoreRegisterState(void);
	virtual void      enableCells();
  
	// Remember if the media bay needs to be turnedOn:
	virtual void      powerMediaBay(bool powerOn, UInt8 whichDevice);  

	// share register access:
	void safeWriteRegUInt32(unsigned long offset, UInt32 mask, UInt32 data);

	// Power handling methods:
	void initForPM (IOService *provider);
	IOReturn setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice);
  
};

#endif /* ! _APPLEKEYLARGO_H */

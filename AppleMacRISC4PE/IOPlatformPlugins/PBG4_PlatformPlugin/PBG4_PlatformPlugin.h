/*
 * Copyright (c) 2002-2004 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 2002-2004 Apple Computer, Inc.  All rights reserved.
 *
 *
 */


#ifndef _PBG4_PLATFORMPLUGIN_H_
#define _PBG4_PLATFORMPLUGIN_H_

#include "IOPlatformPlugin.h"
#include <IOKit/IOTimerEventSource.h>


// Environmental interrupt bits.  Other bits we care about in kPMUEnvIntMask are defined 
// as power events in pwr_mgt/IOPM.h
enum {
	kPMUEnvironmentIntBit				= 0x0040,
	kPMUBatteryOvercurrentIntMask		= 0x0400,
	kPMUEnvIntMask						= (kClamshellClosedEventMask | kACPlugEventMask |
		kBatteryStatusEventMask | kPMUBatteryOvercurrentIntMask)
};

#define kCtrlLoopIsStateDrivenKey				"is-state-driven"
#define kCtrlLoopPowerAdapterBaseline			"power-adapter-baseline"
#define kCtrlLoopStepperDataArray				"StepDataArray"
#define kCtrlLoopStepperData					"stepper-data"
#define kIOPluginEnvStepperDataLoadRequest		"stepper-data-load-request"
#define kIOPluginEnvStepControlState			"step-control-state"

enum {
	kMaxCtrlLoops = 16
};

class PBG4_PlatformPlugin : public IOPlatformPlugin
{

        OSDeclareDefaultStructors(PBG4_PlatformPlugin)

private:
				IOService									*nub;
				bool										fCtrlLoopStateFlagsArray[kMaxCtrlLoops];
				UInt32										fLastPMUEnvIntData;

				task_t										fTask;					// Userclient owning task

protected:
				IOPlatformPluginThermalProfile*				thermalProfile;
				IOService									*thermalNub;


				virtual UInt8 probeConfig( void );
			
				virtual void environmentChanged( void );
			
				virtual void initSymbols( void );
				
				virtual bool initCtrlLoops( const OSArray * ctrlLoopDicts );
			
				static void handleEnvironmentalInterruptEvent(IOService *client, UInt8 interruptMask, UInt32 length, UInt8 *buffer);
				static IOReturn environmentalIntSyncHandler ( void * p1, void * p2, void * p3 );

public:
				bool										fEnvDataIsValid,
															fUsePowerPlay;

				virtual bool start( IOService * provider );
				virtual IOReturn setAggressiveness(unsigned long selector, unsigned long newLevel);

				virtual void delEnv (const OSSymbol *aKey);

};

// Global pointer to our plugin for reference by sensors and control loops
extern PBG4_PlatformPlugin*			gPlatformPlugin;


#endif // _PBG4_PLATFORMPLUGIN_H_
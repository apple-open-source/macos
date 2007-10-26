/*
 * Copyright (c) 2002-2007 Apple Inc.  All rights reserved.
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

#ifndef _APPLEGPIO_H
#define _APPLEGPIO_H

#include <IOKit/IOService.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include "GPIOParent.h"
#include "IOPlatformFunction.h"

/*
 * If defined, this will maintain backwards compatibility with AppleKiwiRoot,
 * which is dependent on the publishing of register-xxx, enable-xxx etc.
 * functions.  With the IOPlatformFunction abstraction, these functions are
 * no longer published, but instead replaced by calling the associated
 * platform-xxx command with an appropriate OSSymbol as param4.
 */
#define OLD_STYLE_COMPAT

#ifdef DLOG
#undef DLOG
#endif

// Uncomment to enable debug output
// #define APPLEGPIO_DEBUG 1

#ifdef APPLEGPIO_DEBUG
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

#ifdef OLD_STYLE_COMPAT
// Macro to add an object to an OSSet, even if the OSSet is not yet allocated
// obj is an OSObject* (or pointer to instance of OSObject-derived class)
// set is an OSSet*

#define ADD_OBJ_TO_SET(obj, set)										\
{	if (set)															\
	{																	\
		if (!(set)->setObject(obj))										\
		{																\
			DLOG("AppleGPIO error adding to set\n");					\
			return(-1);													\
		}																\
	}																	\
	else																\
	{																	\
		(set) = OSSet::withObjects( (const OSObject **) &(obj), 1, 1);	\
		if (!(set))														\
		{																\
			DLOG("AppleGPIO error creating set\n");						\
			return(-1);													\
		}																\
	}																	\
}

// Device tree property keys, platform function prefixes
#define kFunctionRegisterPrefix		"register-"
#define kFunctionUnregisterPrefix	"unregister-"
#define kFunctionEvtEnablePrefix	"enable-"
#define kFunctionEvtDisablePrefix	"disable-"

#endif // OLD_STYLE_COMPAT

enum {
	// power states
	kAppleGPIOPowerOff = 0,
	kAppleGPIOPowerOn = 1,
	kAppleGPIONumPowerStates = 2,

	// Used to denote invalid GPIO ID
	kGPIOIDInvalid	= 0xFFFFFFFF
};

// Structure to store callback information
typedef struct AppleGPIOCallbackInfo_
{
	GPIOEventHandler handler;
	void *param1;
	void *param2;
	bool isEnabled;
	AppleGPIOCallbackInfo_ *next;
} AppleGPIOCallbackInfo;

class AppleGPIO : public IOService
{
    OSDeclareDefaultStructors(AppleGPIO)
    
    private:
    	IOService	*fParent;			// provider of readGPIO and writeGPIO platform functions
    	UInt32		fGPIOID;			// identifier for this GPIO

		OSArray		*fPlatformFuncArray;	// The array of IOPlatformFunction objects

		bool fIntGen;	// true if we can handle interrupts

		// Symbols for interrupt registration and enable/disable support
		const OSSymbol		*fSymIntRegister;
		const OSSymbol		*fSymIntUnRegister;
		const OSSymbol		*fSymIntEnable;
		const OSSymbol		*fSymIntDisable;
		
		// Symbols for calling our parent driver

		const OSSymbol		*fSymGPIOParentWriteGPIO;
		const OSSymbol		*fSymGPIOParentReadGPIO;

		AppleGPIOCallbackInfo	*fClients;	// holds list of interrupt notification clients
		IOSimpleLock			*fClientsLock;
		IOLock					*fPFLock;

		bool performFunction(IOPlatformFunction *func, void *pfParam1,
			void *pfParam2, void *pfParam3, void *pfParam4);

		// Parent registration state
		bool fAmRegistered;
		IOLock *fAmRegisteredLock;
		
		bool registerWithParent(void);
		void unregisterWithParent(void);
		bool amRegistered(void);

		// Parent enable/disable state
		bool fAmEnabled;
		IOSimpleLock *fAmEnabledLock;

		void enableWithParent(void);
		void disableWithParent(void);
		bool amEnabled(void);
		bool setAmEnabled(bool enabled);

		// Client registration and enable/disable
		bool registerClient(void *param1, void *param2,
    			void *param3);

		bool unregisterClient(void *param1, void *param2,
    			void *param3);

		bool enableClient(void *param1, void *param2,
    			void *param3);

		bool disableClient(void *param1, void *param2,
    			void *param3);

		bool areRegisteredClients(void);
		bool areEnabledClients(void);

		// GPIO Parent event callback
		void handleEvent(void *newData, void *z1, void *z2);

		// Power Management routines
		IOPMrootDomain *pmRootDomain;
		void doSleep(void);
		void doWake(void);

#ifdef OLD_STYLE_COMPAT

		// interrupt support routines
		OSSet *fRegisterStrings;	// OSSymbol for each valid register- platform function
		OSSet *fUnregisterStrings;	// OSSymbol for each valid unregister- platform function
		OSSet *fEnableStrings;		// OSSymbol for each valid enable- platform function
		OSSet *fDisableStrings;		// OSSymbol for each valid disable- platform function
		
		// helper functions
		void publishStrings(OSCollection *strings);
		void releaseStrings(void);
#endif
    	
    public:
        virtual bool init(OSDictionary *dict);
        virtual void free(void);
        virtual IOService *probe(IOService *provider, SInt32 *score);
        virtual bool start(IOService *provider);
        virtual void stop(IOService *provider);

		virtual IOReturn callPlatformFunction( const OSSymbol * functionName,
					  bool waitForFunction,
					  void *param1, void *param2,
					  void *param3, void *param4 );

		static void sGPIOEventOccured(void *param1, void *param2,
				void *param3, void *param4);

		virtual IOReturn powerStateWillChangeTo (IOPMPowerFlags, unsigned long, IOService*);
};

#endif // _APPLEGPIO_H
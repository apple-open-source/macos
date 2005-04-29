/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
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

#ifndef _MACIOGPIO_H
#define _MACIOGPIO_H

#include <IOKit/IOService.h>
#include <IOKit/IOInterruptEventSource.h>
#include "GPIOParent.h"

#ifdef DLOG
#undef DLOG
#endif

// Uncomment to enable debug output
// #define MACIOGPIO_DEBUG 1

#ifdef MACIOGPIO_DEBUG
#define DLOG(fmt, args...)  kprintf(fmt, ## args)
#else
#define DLOG(fmt, args...)
#endif

// callPlatformFunction symbols to access key largo registers
#define kKeyLargoSafeWriteRegUInt8 	"keyLargo_safeWriteRegUInt8"
#define kKeyLargoSafeReadRegUInt8	"keyLargo_safeReadRegUInt8"

// First 8 bytes of GPIO reg space is occupied by levels registers
#define kGPIOLevelsRegLen		8

// There are 18 extint_gpioX and 17 gpioX registers
#define kGPIONumGPIOs			(18+17)

// structure to store info about AppleGPIO interrupt clients
typedef struct MacIOGPIOCallbackInfo_
{
	IOInterruptEventSource	*eventSource;
	GPIOEventHandler		handler;
	void					*self;
	MacIOGPIOCallbackInfo_	*next;
} MacIOGPIOCallbackInfo;

class MacIOGPIO : public IOService
{
	OSDeclareDefaultStructors(MacIOGPIO)

	private:
		// key largo gives us register access
		IOService *fKeyLargoDrv;

		// GPIO register space offset relative to key largo base address
		UInt32 fGPIOBaseAddress;
		
		// interrupt support
		IOWorkLoop				*fWorkLoop;
		MacIOGPIOCallbackInfo	*fClients;
		IOLock					*fClientLock;

		// Symbols used to call the KeyLargo register reader/writer

		const OSSymbol			*fSymKeyLargoSafeWriteRegUInt8;
		const OSSymbol			*fSymKeyLargoSafeReadRegUInt8;

		bool registerClient(void *param1, void *param2, void *param3,
				void *param4);

		bool unregisterClient(void *param1, void *param2, void *param3,
				void *param4);

		bool enableClient(void *param1, void *param2, void *param3,
				void *param4);

		bool disableClient(void *param1, void *param2, void *param3,
				void *param4);

		void handleInterrupt(IOInterruptEventSource *source, int count);

	public:
        virtual bool init(OSDictionary *dict);
        virtual void free(void);
        virtual IOService *probe(IOService *provider, SInt32 *score);
        virtual bool start(IOService *provider);
        virtual void stop(IOService *provider);
        
		// GPIO reads and writes are services through the callPlatformFunction
		// mechanism
		virtual IOReturn callPlatformFunction( const char *functionName,
					  bool waitForFunction,
					  void *param1, void *param2,
					  void *param3, void *param4 );

		virtual IOReturn callPlatformFunction( const OSSymbol *functionName,
					  bool waitForFunction,
					  void *param1, void *param2,
					  void *param3, void *param4 );

		IOService *createNub( IORegistryEntry * from );
		void processNub(IOService *myNub);
		void publishBelow(IOService *root);

		static void interruptOccurred(OSObject *me, IOInterruptEventSource *source,
				int count);
};

/* --- this is the gpio device nub class that AppleGPIO attaches to --- */

class MacIOGPIODevice : public IOService
{
	OSDeclareDefaultStructors(MacIOGPIODevice)
	virtual bool compareName(OSString *name, OSString **matched) const;

};

#endif // _MACIOGPIO_H
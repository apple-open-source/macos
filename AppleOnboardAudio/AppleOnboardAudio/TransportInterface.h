/*
 *	TransportInterface.h
 *
 *	Interface class for audio data transport
 *
 *  Created by Ray Montagne on Mon Mar 12 2003.
 *  Copyright (c) 2003 AppleComputer. All rights reserved.
 *
 *	
 *
 */

#include <IOKit/IOService.h>
#include "PlatformInterface.h"

#ifndef	__TRANSPORT_INTERFACE
#define	__TRANSPORT_INTERFACE

enum TRANSPORT_CLOCK_SELECTOR {
	kTRANSPORT_MASTER_CLOCK = 0,
	kTRANSPORT_SLAVE_CLOCK
};

typedef enum {
	kTransportInterfaceType_Unknown	= 0,
	kTransportInterfaceType_I2S,
	kTransportInterfaceType_DAV,
	kTransportInterfaceType_AC97,
	kTransportInterfaceType_I2S_Slave_Only
} TransportInterfaceType;

typedef struct {
	UInt32					transportInterfaceType;
	UInt32					transportSampleRate;
	UInt32					transportSampleDepth;
	UInt32					transportDMAWidth;
	UInt32					clockSource;
	UInt32					reserved_5;
	UInt32					reserved_6;
	UInt32					reserved_7;
	UInt32					reserved_8;
	UInt32					reserved_9;
	UInt32					reserved_10;
	UInt32					reserved_11;
	UInt32					reserved_12;
	UInt32					reserved_13;
	UInt32					reserved_14;
	UInt32					reserved_15;
	UInt32					reserved_16;
	UInt32					reserved_17;
	UInt32					reserved_18;
	UInt32					reserved_19;
	UInt32					reserved_20;
	UInt32					reserved_21;
	UInt32					reserved_22;
	UInt32					reserved_23;
	UInt32					reserved_24;
	UInt32					reserved_25;
	UInt32					reserved_26;
	UInt32					reserved_27;
	UInt32					reserved_28;
	UInt32					reserved_29;
	UInt32					reserved_30;
	UInt32					reserved_31;
	UInt32					instanceState[32];
} TransportStateStruct;
typedef TransportStateStruct * TransportStateStructPtr;

class TransportInterface : public OSObject {

    OSDeclareAbstractStructors ( TransportInterface );

public:

	virtual bool			init (PlatformInterface * inPlatformInterface);
	virtual void			free ( void );

	virtual IOReturn		transportSetSampleRate ( UInt32 sampleRate );
	virtual IOReturn		transportSetSampleWidth ( UInt32 sampleDepth, UInt32 dmaWidth );
	virtual IOReturn		transportBreakClockSelect ( UInt32 clockSource );
	virtual	IOReturn		transportMakeClockSelect ( UInt32 clockSource );
	
	virtual IOReturn		performTransportSleep ( void ) = 0;
	virtual IOReturn		performTransportWake ( void ) = 0;

	virtual bool			transportCanClockSelect ( UInt32 clockSource ) {return false;}
	
	virtual UInt32			transportGetSampleRate ( void ) { return mTransportState.transportSampleRate; }
	UInt32					transportGetSampleWidth ( void ) { return mTransportState.transportSampleDepth; }
	UInt32					transportGetDMAWidth ( void ) { return mTransportState.transportDMAWidth; }
	UInt32					transportGetClockSelect ( void ) { return mTransportState.clockSource; }	
	
	virtual void			poll ( void ) { return; }
	
	void					transportSetTransportInterfaceType ( UInt32 transportType );
	
	virtual IOReturn		transportSetPeakLevel ( UInt32 channelTarget, UInt32 levelMeterValue ) { return kIOReturnError; }
	virtual UInt32			transportGetPeakLevel ( UInt32 channelTarget ) { return 0; }

	//	------------------------------
	//	USER CLIENT
	//	------------------------------
	virtual	IOReturn		getTransportInterfaceState ( TransportStateStructPtr outState );
	virtual IOReturn		setTransportInterfaceState ( TransportStateStructPtr inState );
	
protected:

	static UInt32			sInstanceCount;
	UInt32					mInstanceIndex;

	PlatformInterface *		mPlatformObject;
	TransportStateStruct	mTransportState;
};

#endif


/*
 *  I2SOpaqueSlaveOnlyTransportInterface.h
 *  AppleOnboardAudio
 *
 *  Created by AudioSW Team on Friday 14 May 2004.
 *  Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 */

#include "I2STransportInterface.h"

#ifndef	__I2S_OPAQUE_SLAVE_ONLY_TRANSPORT_INTERFACE
#define	__I2S_OPAQUE_SLAVE_ONLY_TRANSPORT_INTERFACE


class I2SOpaqueSlaveOnlyTransportInterface : public I2STransportInterface {

    OSDeclareDefaultStructors ( I2SOpaqueSlaveOnlyTransportInterface );

public:

	virtual bool		init (PlatformInterface * inPlatformInterface);
	virtual void		free ( void );
	
	virtual IOReturn	transportSetSampleRate ( UInt32 sampleRate );
	virtual IOReturn	transportSetSampleWidth ( UInt32 sampleWidth, UInt32 dmaWidth );
	
	virtual IOReturn	performTransportSleep ( void );
	virtual IOReturn	performTransportWake ( void );
	
	virtual IOReturn	transportBreakClockSelect ( UInt32 clockSource );
	virtual	IOReturn	transportMakeClockSelect ( UInt32 clockSource );

	virtual bool		transportCanClockSelect ( UInt32 clockSource ) {return false;}
	
	virtual UInt32		transportGetSampleRate ( void );

protected:

private:

};


#endif


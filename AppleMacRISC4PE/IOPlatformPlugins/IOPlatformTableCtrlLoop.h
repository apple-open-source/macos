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


#ifndef _IOPLATFORMTABLECTRLLOOP_H
#define _IOPLATFORMTABLECTRLLOOP_H

#include "IOPlatformCtrlLoop.h"

// This key holds our lookup table array in the metastate description
#define kIOPTCLLookupTableKey	"LookupTable"

class IOPlatformStateSensor;
class IOPlatformControl;

class IOPlatformTableCtrlLoop : public IOPlatformCtrlLoop
{

	OSDeclareDefaultStructors(IOPlatformTableCtrlLoop)

protected:

	IOPlatformStateSensor * inputSensor;
	IOPlatformControl * outputControl;

	OSArray * lookupTable;

	// overrides from OSObject superclass
	virtual bool init( void );
	virtual void free( void );

public:

	virtual IOReturn initPlatformCtrlLoop(const OSDictionary *dict);

	virtual void sensorRegistered( IOPlatformSensor * aSensor );
	virtual void controlRegistered( IOPlatformControl * aControl );

	virtual bool updateMetaState( void );
	virtual void adjustControls( void );

};

#endif //_IOPLATFORMTABLECTRLLOOP_H

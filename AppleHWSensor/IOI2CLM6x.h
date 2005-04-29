/*
 * Copyright (c) 2004-2005 Apple Computer, Inc.  All rights reserved.
 *
 *  File: $Id: IOI2CLM6x.h,v 1.1 2005/01/12 00:50:30 dirty Exp $
 *
 *  DRI: Cameron Esfahani
 *
 *		$Log: IOI2CLM6x.h,v $
 *		Revision 1.1  2005/01/12 00:50:30  dirty
 *		First checked in.
 *		
 *		
 */

#ifndef _IOI2CLM6x_H
#define _IOI2CLM6x_H

#include <IOI2C/IOI2CDevice.h>

#define LM6x_DEBUG 0

#if LM6x_DEBUG
	#define DEBUG_ASSERT_PRODUCTION_CODE 0
#endif	// LM6x_DEBUG

#define DEBUG_ASSERT_MESSAGE( componentNameString, assertionString, exceptionLabelString, errorString, fileName, lineNumber, error ) \
	IOI2CLM6xDebugAssert( componentNameString, assertionString, exceptionLabelString, errorString, fileName, lineNumber, error )

#include "AppleHWSensorDebug.h"

class IOI2CLM6x : public IOI2CDevice
{
	OSDeclareDefaultStructors( IOI2CLM6x )

private:

	enum // LM6x register definitions
	{
		kLM6xReg_LocalTemperature			= 0x00,
		kLM6xReg_RemoteTemperatureMSB		= 0x01,
		kLM6xReg_RemoteTemperatureLSB		= 0x10
	};


	bool						fRegistersAreSaved;
	bool						fInitHWFailed; // flag used to indicate hardware is not responding.

	// Variables
	const OSSymbol*				sGetSensorValueSym;

				IOReturn				createChildNubs	(
															IOService*				nub
														);

				IOReturn	 			initHW	(
													void
												);

				IOReturn				saveRegisters	(
															void
														);

				IOReturn				restoreRegisters	(
																void
															);

				IOReturn 				getLocalTemperature	(
																SInt32*			temperature
															);

				IOReturn 				getRemoteTemperature	(
																	SInt32*			temperature
																);

public:

	virtual		bool					start	(
													IOService*			provider
												);

	virtual		void					free	(
													void
												);

	using IOService::callPlatformFunction;
	virtual		IOReturn				callPlatformFunction	(
																	const OSSymbol*			functionName,
																	bool					waitForFunction,
																	void*					param1,
																	void*					param2,
																	void*					param3,
																	void*					param4
																);

	// Power handling methods:
	virtual		void					processPowerEvent	(
																UInt32				eventType
															);
};


#endif // _IOI2CLM6x_H

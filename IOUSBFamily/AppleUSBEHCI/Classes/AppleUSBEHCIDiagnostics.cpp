/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
#include "AppleUSBEHCIDiagnostics.h"
#include "USBTracepoints.h"

OSDefineMetaClassAndStructors(AppleUSBEHCIDiagnostics, OSObject)

OSObject * AppleUSBEHCIDiagnostics::createDiagnostics( AppleUSBEHCI* obj )
{
	AppleUSBEHCIDiagnostics *	diagnostics;
	
	diagnostics = new AppleUSBEHCIDiagnostics;
	if( diagnostics && !diagnostics->init() )
	{
		diagnostics->release();
		diagnostics = NULL;
	}
	
	diagnostics->_UIM		= obj;
	
	bzero(&obj->_UIMDiagnostics, sizeof(AppleUSBEHCI::UIMDiagnostics));
	
	return diagnostics;
}

bool AppleUSBEHCIDiagnostics::serialize( OSSerialize * s ) const
{
	OSDictionary *	dictionary;
	bool			ok;
	UInt64			currms;
	UInt32			deltams;
	AbsoluteTime	now;
	
	dictionary = OSDictionary::withCapacity( 4 );
	if( !dictionary )
		return false;

	USBLog(6, "AppleUSBEHCIDiagnostics[%p]::serialize", this);
	
	_UIM->_UIMDiagnostics.acessCount++;
	UpdateNumberEntry( dictionary, _UIM->_UIMDiagnostics.acessCount, "Access Count");

	
	UpdateNumberEntry( dictionary, _UIM->_UIMDiagnostics.totalErrors, "Errors (Total)");
	UpdateNumberEntry( dictionary, _UIM->_UIMDiagnostics.totalErrors-_UIM->_UIMDiagnostics.prevErrors, "Errors (New)");
	_UIM->_UIMDiagnostics.prevErrors = _UIM->_UIMDiagnostics.totalErrors;

	UpdateNumberEntry( dictionary, gUSBStackDebugFlags, "Debug Flags");
	if( (gUSBStackDebugFlags & kUSBEnableErrorLogMask) != 0)
	{
		UpdateNumberEntry( dictionary, _UIM->_UIMDiagnostics.recoveredErrors, "Recovered Errors");
		UpdateNumberEntry( dictionary, _UIM->_UIMDiagnostics.recoveredErrors-_UIM->_UIMDiagnostics.prevRecoveredErrors, "Recovered Errors (New)");
		_UIM->_UIMDiagnostics.prevRecoveredErrors = _UIM->_UIMDiagnostics.recoveredErrors;
		
		UpdateNumberEntry( dictionary, _UIM->_UIMDiagnostics.errors2Strikes, "Recovered 2 strike errors");
		UpdateNumberEntry( dictionary, _UIM->_UIMDiagnostics.errors2Strikes-_UIM->_UIMDiagnostics.prevErrors2Strikes, "Recovered 2 strike errors (New)");
		_UIM->_UIMDiagnostics.prevErrors2Strikes = _UIM->_UIMDiagnostics.errors2Strikes;
		
		UpdateNumberEntry( dictionary, _UIM->_UIMDiagnostics.errors3Strikes, "Fatal 3 strike errors");
		UpdateNumberEntry( dictionary, _UIM->_UIMDiagnostics.errors3Strikes-_UIM->_UIMDiagnostics.prevErrors3Strikes, "Fatal 3 strike errors (New)");
		_UIM->_UIMDiagnostics.prevErrors3Strikes = _UIM->_UIMDiagnostics.errors3Strikes;
	}
	
	clock_get_uptime(&now);
	absolutetime_to_nanoseconds(now, &currms);
	
	deltams = ((currms-_UIM->_UIMDiagnostics.lastNanosec)/1000000)+1;	// +1 so this is never zero, makes little difference if delta ms is large
	UpdateNumberEntry( dictionary, currms/1000000, "ms (Current)");
	UpdateNumberEntry( dictionary, deltams, "ms (since last read)");
	_UIM->_UIMDiagnostics.lastNanosec = currms;
	
	UpdateNumberEntry( dictionary, _UIM->_UIMDiagnostics.totalBytes, "Bytes");
	UpdateNumberEntry( dictionary, _UIM->_UIMDiagnostics.totalBytes-_UIM->_UIMDiagnostics.prevBytes, "Bytes (New)");
	UpdateNumberEntry( dictionary, (_UIM->_UIMDiagnostics.totalBytes-_UIM->_UIMDiagnostics.prevBytes)/deltams, "Bytes (New)/ms");

	_UIM->_UIMDiagnostics.prevBytes = _UIM->_UIMDiagnostics.totalBytes;

	UpdateNumberEntry( dictionary, _UIM->_UIMDiagnostics.timeouts, "Timeouts");
	UpdateNumberEntry( dictionary, _UIM->_UIMDiagnostics.timeouts-_UIM->_UIMDiagnostics.prevTimeouts, "Timeouts (New)");
	_UIM->_UIMDiagnostics.prevTimeouts = _UIM->_UIMDiagnostics.timeouts;
	
	UpdateNumberEntry( dictionary, _UIM->_UIMDiagnostics.resets, "Resets");
	UpdateNumberEntry( dictionary, _UIM->_UIMDiagnostics.resets-_UIM->_UIMDiagnostics.prevResets, "Resets (New)");
	_UIM->_UIMDiagnostics.prevResets = _UIM->_UIMDiagnostics.resets;
	

	_UIM->_UIMDiagnostics.controlBulkTxOut = _UIM->_controlBulkTransactionsOut;
	UpdateNumberEntry( dictionary, _UIM->_UIMDiagnostics.controlBulkTxOut, "ControlBulkTxOut");
	
	ok = dictionary->serialize(s);
	dictionary->release();
	
	return ok;
}
	
void AppleUSBEHCIDiagnostics::UpdateNumberEntry( OSDictionary * dictionary, UInt32 value, const char * name ) const
{
	OSNumber *	number;
	
	number = OSNumber::withNumber( value, 32 );
	if( !number )
		return;
		
	dictionary->setObject( name, number );
	number->release();
}

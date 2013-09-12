/*
 * Copyright © 1998-2013 Apple Inc. All rights reserved.
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

#include "AppleUSBDiagnostics.h"
#include "USBTracepoints.h"

OSDefineMetaClassAndStructors(AppleUSBDiagnostics, OSObject)

OSObject * AppleUSBDiagnostics::createDiagnostics( UIMDiagnostics* obj, UInt32 *controlBulkTransactionsOut, IOService *controller)
{
	AppleUSBDiagnostics *	diagnostics;
	
	diagnostics = new AppleUSBDiagnostics;
    return diagnostics->initDiagnostics(diagnostics, obj, controlBulkTransactionsOut, controller);
}

OSObject * AppleUSBDiagnostics::initDiagnostics(AppleUSBDiagnostics *diagnostics, UIMDiagnostics* obj, UInt32 *controlBulkTransactionsOut, IOService *controller)
{
	if( diagnostics && !diagnostics->init() )
	{
		diagnostics->release();
		diagnostics = NULL;
	}
	
	diagnostics->_UIMDiagnostics		= obj;
	diagnostics->_controlBulkTransactionsOut		= controlBulkTransactionsOut;
	diagnostics->_controller		= controller;
	
	bzero(obj, sizeof(UIMDiagnostics));
	
	return diagnostics;
}

void AppleUSBDiagnostics::serializePort(OSDictionary *dictionary, int port, UIMPortDiagnostics *counts, IOService *controller) const
{
#pragma unused(controller, port)
    
    UpdateNumberEntry( dictionary, counts->errorCount, "Port errors");

    if( (gUSBStackDebugFlags & kUSBEnableErrorLogMask) != 0)
    {
        UpdateNumberEntry( dictionary, counts->totalBytes, "Bytes");
        UpdateNumberEntry( dictionary, counts->totalBytes-counts->prevBytes, "Bytes (New)");
        
        counts->prevBytes = counts->totalBytes;
        
        UpdateNumberEntry( dictionary, counts->timeouts, "Timeouts");
        UpdateNumberEntry( dictionary, counts->timeouts-counts->prevTimeouts, "Timeouts (New)");
        counts->prevTimeouts = counts->timeouts;
    }
        
    UpdateNumberEntry( dictionary, counts->resets, "Resets");
    UpdateNumberEntry( dictionary, counts->resets-counts->prevResets, "Resets (New)");
    counts->prevResets = counts->resets;
    
    UpdateNumberEntry( dictionary, counts->enable, "enable");
    UpdateNumberEntry( dictionary, counts->suspend, "suspend");
    UpdateNumberEntry( dictionary, counts->resume, "resume");
    UpdateNumberEntry( dictionary, counts->warmReset, "warmReset");
    UpdateNumberEntry( dictionary, counts->power, "power");
    UpdateNumberEntry( dictionary, counts->u1Timeout, "u1Timeout");
    UpdateNumberEntry( dictionary, counts->u2Timeout, "u2Timeout");
    UpdateNumberEntry( dictionary, counts->remoteWakeMask, "remoteWakeMask");

    OSArray     * errorArray = OSArray::withCapacity(kXHCIMaxCompletionCodes);
    for(int i=0; i<kXHCIMaxCompletionCodes; i++)
    {
        OSNumber * number = OSNumber::withNumber( counts->xhciErrorCode[i], 32 );
        errorArray->setObject( i, number );
        number->release();
    }
    dictionary->setObject( "XHCI Completion Codes", errorArray );
    errorArray->release();
    
    OSArray     * linkStateArray = OSArray::withCapacity(kXHCILinkStates);
    for(int i=0; i<kXHCILinkStates; i++)
    {
        OSNumber * number = OSNumber::withNumber( counts->linkState[i], 32 );
        linkStateArray->setObject( i, number );
        number->release();
    }
    dictionary->setObject( "LinkStates", linkStateArray );
    linkStateArray->release();
    
}


bool AppleUSBDiagnostics::serialize( OSSerialize * s ) const
{
	OSDictionary *	dictionary;
	bool			ok;
	UInt64			currms;
	UInt32			deltams;
	AbsoluteTime	now;
	
	dictionary = OSDictionary::withCapacity( 4 );
	if( !dictionary )
		return false;

	USBLog(6, "AppleUSBDiagnostics[%p]::serialize", this);
	
	_UIMDiagnostics->acessCount++;
	UpdateNumberEntry( dictionary, _UIMDiagnostics->acessCount, "Access Count");

	
	UpdateNumberEntry( dictionary, _UIMDiagnostics->totalErrors, "Errors (Total)");
	UpdateNumberEntry( dictionary, _UIMDiagnostics->totalErrors-_UIMDiagnostics->prevErrors, "Errors (New)");
	_UIMDiagnostics->prevErrors = _UIMDiagnostics->totalErrors;

	UpdateNumberEntry( dictionary, gUSBStackDebugFlags, "Debug Flags");
	if( (gUSBStackDebugFlags & kUSBEnableErrorLogMask) != 0)
	{
		UpdateNumberEntry( dictionary, _UIMDiagnostics->recoveredErrors, "Recovered Errors");
		UpdateNumberEntry( dictionary, _UIMDiagnostics->recoveredErrors-_UIMDiagnostics->prevRecoveredErrors, "Recovered Errors (New)");
		_UIMDiagnostics->prevRecoveredErrors = _UIMDiagnostics->recoveredErrors;
		
		UpdateNumberEntry( dictionary, _UIMDiagnostics->errors2Strikes, "Recovered 2 strike errors");
		UpdateNumberEntry( dictionary, _UIMDiagnostics->errors2Strikes-_UIMDiagnostics->prevErrors2Strikes, "Recovered 2 strike errors (New)");
		_UIMDiagnostics->prevErrors2Strikes = _UIMDiagnostics->errors2Strikes;
		
		UpdateNumberEntry( dictionary, _UIMDiagnostics->errors3Strikes, "Fatal 3 strike errors");
		UpdateNumberEntry( dictionary, _UIMDiagnostics->errors3Strikes-_UIMDiagnostics->prevErrors3Strikes, "Fatal 3 strike errors (New)");
		_UIMDiagnostics->prevErrors3Strikes = _UIMDiagnostics->errors3Strikes;
        
        if(_UIMDiagnostics->numPorts)
        {
            UpdateNumberEntry( dictionary, _UIMDiagnostics->numPorts, "Number of ports");
            for(int i=0; i<_UIMDiagnostics->numPorts; i++)
            {
                char buf[64];
                OSDictionary * portDictionary = OSDictionary::withCapacity(1);
                serializePort(portDictionary, i, &_UIMDiagnostics->portCounts[i], _controller);
                snprintf(buf, 63, "Port %2d", i+1);
                dictionary->setObject( buf, portDictionary );
                portDictionary->release();
            }
        }
        
	}
	
	clock_get_uptime(&now);
	absolutetime_to_nanoseconds(now, &currms);
	
	deltams = ((currms-_UIMDiagnostics->lastNanosec)/1000000)+1;	// +1 so this is never zero, makes little difference if delta ms is large
	UpdateNumberEntry( dictionary, currms/1000000, "ms (Current)");
	UpdateNumberEntry( dictionary, deltams, "ms (since last read)");
	_UIMDiagnostics->lastNanosec = currms;
	
	UpdateNumberEntry( dictionary, _UIMDiagnostics->totalBytes, "Bytes");
	UpdateNumberEntry( dictionary, _UIMDiagnostics->totalBytes-_UIMDiagnostics->prevBytes, "Bytes (New)");
	UpdateNumberEntry( dictionary, (_UIMDiagnostics->totalBytes-_UIMDiagnostics->prevBytes)/deltams, "Bytes (New)/ms");

	_UIMDiagnostics->prevBytes = _UIMDiagnostics->totalBytes;

	UpdateNumberEntry( dictionary, _UIMDiagnostics->timeouts, "Timeouts");
	UpdateNumberEntry( dictionary, _UIMDiagnostics->timeouts-_UIMDiagnostics->prevTimeouts, "Timeouts (New)");
	_UIMDiagnostics->prevTimeouts = _UIMDiagnostics->timeouts;
	
	UpdateNumberEntry( dictionary, _UIMDiagnostics->resets, "Resets");
	UpdateNumberEntry( dictionary, _UIMDiagnostics->resets-_UIMDiagnostics->prevResets, "Resets (New)");
	_UIMDiagnostics->prevResets = _UIMDiagnostics->resets;

    if(_controlBulkTransactionsOut)
    {   // EHCI keeps a note of this separately, maybe it should be in the diagnostics struct
        _UIMDiagnostics->controlBulkTxOut = *_controlBulkTransactionsOut;
    }
    else
    {
        _UIMDiagnostics->controlBulkTxOut = 0;
    }
	UpdateNumberEntry( dictionary, _UIMDiagnostics->controlBulkTxOut, "ControlBulkTxOut");
	
	ok = dictionary->serialize(s);
	dictionary->release();
	
	return ok;
}
	
void AppleUSBDiagnostics::UpdateNumberEntry( OSDictionary * dictionary, UInt32 value, const char * name ) const
{
	OSNumber *	number;
	
	number = OSNumber::withNumber( value, 32 );
	if( !number )
		return;
		
	dictionary->setObject( name, number );
	number->release();
}

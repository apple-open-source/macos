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
#include "IOFireWireIP.h"
#include "IOFireWireIPDiagnostics.h"

OSDefineMetaClassAndStructors(IOFireWireIPDiagnostics, OSObject)

OSObject * IOFireWireIPDiagnostics::createDiagnostics( IOFireWireIP* obj )
{
	IOFireWireIPDiagnostics *	diagnostics;
	
	diagnostics = new IOFireWireIPDiagnostics;
	if( diagnostics && !diagnostics->init() )
	{
		diagnostics->release();
		diagnostics = NULL;
	}
	
	diagnostics->fIPObj		= obj;
	
	bzero(&obj->fIPoFWDiagnostics, sizeof(IOFireWireIP::IPoFWDiagnostics));
	
	return diagnostics;
}

bool IOFireWireIPDiagnostics::serialize( OSSerialize * s ) const
{
	OSDictionary *	dictionary;
	bool			ok;
	
	dictionary = OSDictionary::withCapacity( 4 );
	if( !dictionary )
		return false;
		
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fTxBcast, "TxB");
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fRxBcast, "RxB");
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fTxUni, "TxU");
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fRxUni, "RxU");
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fRxFragmentPkts, "RxF");
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fTxFragmentPkts, "TxF");

	if( fIPObj->transmitQueue )
	{
		updateNumberEntry( dictionary, fIPObj->transmitQueue->getState(), "tqState");
		updateNumberEntry( dictionary, fIPObj->transmitQueue->getStallCount(), "tqStall");
		updateNumberEntry( dictionary, fIPObj->transmitQueue->getRetryCount(), "tqRetries");
		updateNumberEntry( dictionary, fIPObj->transmitQueue->getSize(), "tqSize");
	}
	
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fActiveBcastCmds, "fwActiveBCastCmds" );
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fInActiveBcastCmds, "fwInActiveBCastCmds" );

	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fActiveCmds, "fwActiveCmds" );
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fNoCommands, "fwNoCommands" );
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fNoBCastCommands, "fwNoBcastCommands" );
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fInActiveCmds, "fwInActiveCmds" );
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fDoubleCompletes, "fwAttemptedDC" );
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fSubmitErrs, "fwSubmitErrs" );
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fCallErrs, "fwCompletionErrs" );
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fNoResources, "fwIPNoResources");
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fMaxQueueSize, "fwMaxQueueSize");
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fServiceInOutput, "fwServiceInOP");
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fServiceInCallback, "fwServiceInCB");
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fLastStarted, "fwLastStarted");
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fMaxPacketSize, "fwMaxPacketSize");
	
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fGaspTagError, "fwGASPTagError");
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fGaspHeaderError, "fwGASPHeaderError");
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fNonRFC2734Gasp, "fwNonRFC2734Error");
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fRemoteGaspError, "fwRemoteGaspError");
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fEncapsulationHeaderError, "fwRxBHeaderError");	

	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.inActiveMbufs, "fwInActiveMbufs");
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.activeMbufs, "fwActiveMbufs");	
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fNoMbufs, "fwNoMbufs");	
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fBusyAcks, "fwBusyAcks");	
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fFastRetryBusyAcks, "fwFastRetryBusyAcks");	
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fDoFastRetry, "fwFastRetryOn");	

	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fInCorrectMCAPDesc, "fwInCorrectMCAPDesc");
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fUnknownMCAPDesc, "fwUnknownMCAPDesc");	
	updateNumberEntry( dictionary, fIPObj->fIPoFWDiagnostics.fUnknownGroupAddress, "fwUnknownGroupAddress");	

	ok = dictionary->serialize(s);
	dictionary->release();
	
	return ok;
}
	
void IOFireWireIPDiagnostics::updateNumberEntry( OSDictionary * dictionary, UInt32 value, const char * name )
{
	OSNumber *	number;
	
	number = OSNumber::withNumber( value, 32 );
	if( !number )
		return;
		
	dictionary->setObject( name, number );
	number->release();
}

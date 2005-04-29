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
	
	diagnostics->fIPObj = obj;
	
	return diagnostics;
}

bool IOFireWireIPDiagnostics::serialize( OSSerialize * s ) const
{
	OSDictionary *	dictionary;
	bool			ok;
	
	dictionary = OSDictionary::withCapacity( 4 );
	if( !dictionary )
		return false;
		
	/////////
	updateNumberEntry( dictionary, fIPObj->fTxBcast, "TxB");
	updateNumberEntry( dictionary, fIPObj->fRxBcast, "RxB");
	updateNumberEntry( dictionary, fIPObj->fTxUni, "TxU");
	updateNumberEntry( dictionary, fIPObj->fRxUni, "RxU");
	updateNumberEntry( dictionary, fIPObj->fRxFragmentPkts, "RxF");
	updateNumberEntry( dictionary, fIPObj->fTxFragmentPkts, "TxF");

    updateNumberEntry( dictionary, fIPObj->transmitQueue->getState(), "tqState");
    updateNumberEntry( dictionary, fIPObj->transmitQueue->getStallCount(), "tqStall");
    updateNumberEntry( dictionary, fIPObj->transmitQueue->getRetryCount(), "tqRetries");
    updateNumberEntry( dictionary, fIPObj->transmitQueue->getSize(), "tqSize");
	updateNumberEntry( dictionary, fIPObj->fMissedQRestarts, "tqMissedRestarts");

	updateNumberEntry( dictionary, fIPObj->fActiveCmds, "activeCmds" );
	updateNumberEntry( dictionary, fIPObj->fNoCommands, "NoCommands" );
	updateNumberEntry( dictionary, fIPObj->fNoBCastCommands, "NoBcastCommands" );
	updateNumberEntry( dictionary, fIPObj->fInActiveCmds, "inActiveCmds" );
	updateNumberEntry( dictionary, fIPObj->fDoubleCompletes, "attemptedDC" );
	updateNumberEntry( dictionary, fIPObj->fSubmitErrs, "submitErrs" );
	updateNumberEntry( dictionary, fIPObj->fCallErrs, "completionErrs" );
	updateNumberEntry( dictionary, fIPObj->fStalls, "fwStalls");
	updateNumberEntry( dictionary, fIPObj->fNoResources, "fwIPNoResources");
	
#ifdef IPFIREWIRE_DIAGNOSTICS
	fIPObj->fDumpLog = true;
   	updateNumberEntry( dictionary, fIPObj->fMaxInputCount, "MaxInputCount");
	updateNumberEntry( dictionary, fIPObj->fMaxPktSize, "MaxPktSize");
	updateNumberEntry( dictionary, fIPObj->fLcb->maxBroadcastPayload, "maxBroadcastPayload");
	updateNumberEntry( dictionary, fIPObj->fLcb->maxBroadcastSpeed, "currBroadcastSpeed");
	updateNumberEntry( dictionary, fIPObj->fPrevBroadcastSpeed, "prevBroadcastSpeed");
#endif	
	/////////
	
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

///////////////////////////

void IOFireWireIPDiagnostics::incrementExecutedORBCount( void )
{
	fExecutedORBCount++;
}



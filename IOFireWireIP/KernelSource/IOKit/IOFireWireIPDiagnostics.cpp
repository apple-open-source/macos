/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
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
	fIPObj->updateStatistics();
	updateNumberEntry( dictionary, fIPObj->fTxBcast, "TxBCAST");
	updateNumberEntry( dictionary, fIPObj->fRxBcast, "RxBCAST");
	updateNumberEntry( dictionary, fIPObj->fTxUni, "TxUNI");
	updateNumberEntry( dictionary, fIPObj->fRxUni, "RxUNI");
	updateNumberEntry( dictionary, fIPObj->fMaxPktSize, "MaxPktSize");
	updateNumberEntry( dictionary, fIPObj->fMaxInputCount, "MaxInputCount");
	updateNumberEntry( dictionary, fIPObj->fIsoRxOverrun, "IsoRxOverrun");
	updateNumberEntry( dictionary, fIPObj->fUsedCmds, "UsedCmds" );
	updateNumberEntry( dictionary, fIPObj->fSubmitErrs, "SubmitErrs" );
	updateNumberEntry( dictionary, fIPObj->fCallErrs, "CompletionErrs" );
	updateNumberEntry( dictionary, fIPObj->fStalls, "Stalls");
	updateNumberEntry( dictionary, fIPObj->fRxFragmentPkts, "RxFragmentPkts");
	updateNumberEntry( dictionary, fIPObj->fTxFragmentPkts, "TxFragmentPkts");
	
#ifdef IPFIREWIRE_DIAGNOSTICS
	fIPObj->fDumpLog = true;
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



/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
/*
    File:       IrLMP.h

    Contains:   Methods for implementing IrLMP

*/


#ifndef __IRLMP_H
#define __IRLMP_H

#include "IrStream.h"
#include "IrEvent.h"

// Forward reference
class TIrGlue;
class TIrLAPConn;
class TIrLAP;
class CIrDiscovery;
class CList;
class CBufferSegment;

// Constants

#define kMaxReturnedAddrs       16      // Max slots => max addr
#define kMaxAddrConflicts       8       // I'm only dealing w/8 at most

enum IrLMPStates
{
    kIrLMPReady,
    kIrLMPDiscover,
    kIrLMPResolveAddress
};


// Classes

// --------------------------------------------------------------------------------------------------------------------
//                      TIrLMP
// --------------------------------------------------------------------------------------------------------------------

class TIrLMP : public TIrStream
{
	    OSDeclareDefaultStructors(TIrLMP);
    
    public:
    
	    static TIrLMP * tIrLMP(TIrGlue* irda);
	    void    free(void);

	    Boolean         Init(TIrGlue* irda);
	    void            Reset();

	    void            Demultiplexor(CBufferSegment* inputBuffer);
	    ULong           FillInLMPDUHeader(TIrPutRequest* putRequest, UByte* buffer);

	    void            StartOneSecTicker();
	    void            StopOneSecTicker();
	    void            TimerComplete(ULong refCon);

    private:

	    // TIrStream override
	    void            NextState(ULong event);

	    void            HandleReadyStateEvent(ULong event);
	    void            HandleDiscoverStateEvent(ULong event);
	    void            HandleResolveAddressStateEvent(ULong event);

	    // Helper methods

	    Boolean         AddrConflicts(CList* discoveredDevices, Boolean setAddrConflicts);

	    // Fieldsä

	    UByte           fState;
	    UByte           fTimerClients;

	    // Addr conflict resolution goop
	    ULong           fNumAddrConflicts;
	    ULong           fAddrConflicts[kMaxAddrConflicts];
	    
	    // deferred requests
	    CList*          fPendingRequests;       // requests waiting for ready state

};

#endif // __IRLMP_H

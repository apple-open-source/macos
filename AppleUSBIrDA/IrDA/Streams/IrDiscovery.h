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
    File:       IrDiscovery.h

    Contains:   Class definitions for discovery module

*/

#ifndef __IRDISCOVERY__
#define __IRDISCOVERY__

#include "IrDATypes.h"
#include "IrStream.h"
#include "IrEvent.h"
//  #include "IrExec.h"
//#include "IrDscInfo.h"


enum DiscoverEvent {
    kMaxDiscoverSlots           = 16,
    kDiscoverDefaultSlotCount   = 8
};


enum DiscoverState {
    kDiscoverIdle,
    kDiscoverActive
};
    

class TIrLMP;
class TIrGlue;
class TIrDscInfo;

class CIrDiscovery : public TIrStream
{
    OSDeclareDefaultStructors(CIrDiscovery);
    
public:
		    
    static CIrDiscovery *cIrDiscovery(TIrGlue * glue);
    void    free();
    
    Boolean Init(TIrGlue * glue);
	
    void            PassiveDiscovery    (TIrDscInfo * dscInfo);
    void            GetRemoteDeviceName (UInt32 lapAddr, UInt8 * name);
    TIrDscInfo      *GetDiscoveryInfo(void);

	//***************** TESTING
	//Boolean           ExtDiscoveryAvailable(void);            // true if not busy (testing?)
	
    IrDAErr         ExtDiscoverStart(UInt32  numSlots); // ,
					    //ExtDiscoveryUserCallBackUPP       callback,
					    //ExtDiscoveryBlock             *userData);
	
    private:

	void        NextState(ULong event);             // TIrStream override
	void        DiscoverStart(void);                // start a discover
	void        HandleDiscoverComplete(void);       // discover finished
	void        DeleteDiscoveredDevicesList(void);
	
	UInt32      fState;                             // idle or discovering
	
	CList       *fPendingDiscoverList;              // list of pending discover request events
	CList       *fDiscoveredDevices;                // list of discovered devices (IrDscInfo)
	TIrDscInfo  *fMyDscInfo;                        // discovery info for this host
	
	void        HandleExtDiscoverComplete(TIrDiscoverReply * reply);        // TESTING ONLY
};

inline TIrDscInfo * CIrDiscovery::GetDiscoveryInfo( void ) { return fMyDscInfo; }
//inline Boolean CIrDiscovery::ExtDiscoveryAvailable( void ) { return fExtDiscRequest == nil; }

#endif // __IRDISCOVERY__


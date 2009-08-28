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
    void            GetRemoteDeviceName (UInt32 lapAddr, UInt8 * name, int maxnamelen);
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


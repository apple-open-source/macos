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

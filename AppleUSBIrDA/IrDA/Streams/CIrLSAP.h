/*
    File:       CIrLSAP.h

*/

#ifndef __CIRLSAP__
#define __CIRLSAP__

#include "IrStream.h"
#include "IrDiscovery.h"
#include "IrDscInfo.h"
#include "IrEvent.h"

enum CIrLSAPStates
{
    kIrLSAPDisconnected,        // nothing going on
    
    kIrLSAPDisconnectStart,     // these all transition to disconnected
    kIrLSAPDiscoverStart,       // when the request is done
    kIrLSAPLookupStart,
    
    kIrLSAPConnectStart,        // connect goes to connected or disconnected
    kIrLSAPConnected,
	
    kIrLSAPListenStart,         // listen goes to listencomplete or disconnected
    kIrLSAPListenComplete,
    kIrLSAPAcceptStart          // transitions to connected when finished
};

typedef struct DiscoveryInfo
{
    UInt8               name[kMaxNicknameLen+1];
    UInt32              addr,
			serviceHints;
} DiscoveryInfo;

class TIASClient;
class TIrGlue;
class TIrLMP;
//class CIrDiscovery;
class TIrDscInfo;
class CBufferSegment;

class CIrLSAP : public TIrStream
{
			    OSDeclareAbstractStructors(CIrLSAP);
public:
    Boolean                 Init(TIrGlue *irda, UInt32 desiredLSAPId, UInt8 * className, UInt8 * attributeName, ULong hints=0);
    void                    free();
    
    void                    SetPeerLAPAddr( UInt32 addr );      // For those who already know
    UInt32                  GetPeerLAPAddr( void );             // For info only
    UInt32                  GetMyLSAPId(void);
    UInt32                  GetState (void);
    
    IrDAErr                 Discover    ( UInt32 slots );
    IrDAErr                 LSAPLookup  ( UInt8 * className, UInt8 * attributeName, UInt32 remoteAddr );
    IrDAErr                 Connect     ( CBufferSegment *connectData );
    IrDAErr                 Connect     ( UInt32 lsapID, CBufferSegment *connectData );
    IrDAErr                 Connect     ( UInt32 remoteAddr, UInt32 lsapID, CBufferSegment *connectData );
    IrDAErr                 DataPut     ( CBufferSegment * data );
    IrDAErr                 DataGet     ( CBufferSegment * data );
    IrDAErr                 Listen      ( CBufferSegment *connectData );
    IrDAErr                 Accept      ( CBufferSegment *connectData );
    IrDAErr                 CancelGets  ( void );               // abort all pending gets
    IrDAErr                 CancelPuts  ( void );               // abort all pending puts
    void                    Disconnect  ( void );

    void                    NextState( UInt32 event );          // called with new event (by IrStream/Glue)
    
    Boolean                 Connected( void ) { return fConnected; };
    
						// client needs to subclass and provide these
    virtual void DiscoverComplete (             // discovery has finished callback
		    UInt32  numFound,           // number of peers discovered
		    IrDAErr result  ) = 0 ;     // Any error returned

    virtual void LSAPLookupComplete (           // an IAS query has finished
		    IrDAErr result,             // result of the lookup
		    UInt32  peerLSAPId) = 0 ;   // peer's LSAP id of the service
    
    virtual void ConnectComplete (              // a connect request has completed
		    IrDAErr result,             // result of the connect request
		    TIrQOS *myQOS,              // my qos ... (requested?)
		    TIrQOS *peerQOS,            // peer's qos .. (result?)
		    CBufferSegment *data) = 0 ; // data payload from connect msg
    
    virtual void DisconnectComplete ( void ) = 0 ;  // you've been disconnected
						    // hmmph.  where's the reason code?
		
    virtual void DataPutComplete (
		    IrDAErr result,                 // result code
		    CBufferSegment *data) = 0 ;     // data that was sent
    
    virtual void DataGetComplete (
		    IrDAErr result,                 // result code
		    CBufferSegment *data) = 0 ;     // data
		
    virtual void ListenComplete (                   // check me
		    IrDAErr result,
		    UInt32  peerAddr,               // address of connecting peer
		    UInt32  peerLSAPId,             // LSAP id of connecting peer
		    TIrQOS  *myQOS,                 // my qos ... (requested?)
		    TIrQOS  *peerQOS,               // peer's qos .. (result?)
		    CBufferSegment *data) = 0 ;     // data payload from connect msg
		
    virtual void AcceptComplete (                   // check me
		    IrDAErr result,
		    CBufferSegment *data) = 0 ;     // data payload in connect msg
		
    virtual void CancelGetsComplete (               // all pending gets have been canceled
		    IrDAErr result) = 0;
		    
    virtual void CancelPutsComplete (               // all pending puts have been canceled
		    IrDAErr result) = 0;
		


protected:
    TIrDscInfo          GetDiscoveryInfo                ( void );
    void                SetState                        (UInt32 newState);

    void                HandleDisconnectComplete        ( void );       // These are the internal routines called by NextState()
    void                HandleDiscoverComplete          ( void );

    void                HandleLSAPLookupComplete        ( void );
    void                HandleNameServerConnectComplete ( void );
    void                HandleNameServerLookupComplete  ( void );
    void                HandleNameServerReleaseComplete ( void );

    void                HandleConnectComplete           ( void );
    void                HandleListenComplete            ( void );
    void                HandleAcceptComplete            ( void );
    void                HandleDataGetComplete           ( void );
    void                HandleDataPutComplete           ( void );
    void                HandleCancelPutComplete         ( void );
    void                HandleCancelGetComplete         ( void );

    UInt32              fState;
    UInt32              fPeerAddr;                  // peer address, saved by LSAPLookup and set by SetPeerLAPAddr()
    UInt32              fMyLSAPId;                  // local lsap id (port number)
    UInt32              fPeerLSAPId;                // remote lsap id (port number)
    
    UInt8               fClassName[64];             // IAS class name for this LSAP
    UInt8               fAttrName[64];              // IAS attribute name for this LSAP
    UInt8               fConnectClassName[64];      // IAS Lookup class name
    UInt8               fAttributeName[64];         // IAS Lookup attribute name
    ULong               fHints;                     // discovery hints set/cleared by this LSAP
	
    Boolean             fConnected;
    
    UInt32              fDiscoverCount;
    DiscoveryInfo       fDiscoverInfo[kMaxDiscoverSlots];
    TIrDscInfo      *   fDscInfo;                   // our own discovery info, from glue
    
private:
    TIASClient      *   fNameClient;                // we alloc and free this IAS stream client
    TLSAPConn       *   fLSAP;                      // our lsap connection.  glue makes this, we free it
    CIrDiscovery    *   fDiscovery;                 // discovery client, from glue
    Boolean             fPendingDisconnect;         // disconnect request has been deferred
    TIrDisconnectRequest    *fDisconnectRequest;    // disconnect request event (for free check)
    
    CBufferSegment *    fLastListenBuffer;      // temp debugging
    CBufferSegment *    fLastPutBuffer;         // temp debugging
};

inline UInt32   CIrLSAP::GetState() { return fState; };             
inline void     CIrLSAP::SetState(UInt32 newState) {fState = newState;};
inline UInt32   CIrLSAP::GetPeerLAPAddr() { return fPeerAddr; };    
inline UInt32   CIrLSAP::GetMyLSAPId() { return fMyLSAPId; };       // may not be valid yet

#endif  // __CIRLSAP__




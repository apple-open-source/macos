/*
    File:       IrEvent.h

    Contains:   Defines and classes for event messages
    
*/


#ifndef __IREVENT_H
#define __IREVENT_H

#include "IrDATypes.h"


// Forward reference
class TIrStream;
class TIrClient;
class TLSAPConn;
class TIASAttribute;
class TIrQOS;
class CBuffer;
class CList;

// Constants

// names for these events are in IrStream.cpp, change there too.
// keep replies = requests + 1

enum IrEvents
{
    // Non-timer events
    kIrOutputCompleteEvent = 1,     //  1:  output has completed
    kIrInputCompleteEvent,          //  2:  input has completed

    // Requests/replies
    kIrDiscoverRequestEvent,                                    //  3
    kIrDiscoverReplyEvent       = kIrDiscoverRequestEvent+1,    //  4
    
    kIrConnectRequestEvent,                                     //  5
    kIrConnectReplyEvent        = kIrConnectRequestEvent+1,     //  6
    
    kIrListenRequestEvent,                                      //  7
    kIrListenReplyEvent         = kIrListenRequestEvent+1,      //  8
    
    kIrAcceptRequestEvent,                                      //  9
    kIrAcceptReplyEvent         = kIrAcceptRequestEvent+1,      //  a
    
    kIrGetDataRequestEvent,                                     //  b
    kIrGetDataReplyEvent        = kIrGetDataRequestEvent+1,     //  c
    
    kIrPutDataRequestEvent,                                     //  d
    kIrPutDataReplyEvent        = kIrPutDataRequestEvent+1,     //  e
    
    kIrLookupRequestEvent,                                      //  f
    kIrLookupReplyEvent         = kIrLookupRequestEvent+1,      //  10
    
    kIrCancelGetRequestEvent,                                   //  11
    kIrCancelGetReplyEvent      = kIrCancelGetRequestEvent+1,   //  12
    
    kIrCancelPutRequestEvent,                                   //  13
    kIrCancelPutReplyEvent      = kIrCancelPutRequestEvent+1,   //  14
    
    kIrReleaseRequestEvent,                                     //  15
    kIrReleaseReplyEvent        = kIrReleaseRequestEvent+1,     //  16
    
    kIrDisconnectRequestEvent,                                  //  17
    kIrDisconnectReplyEvent     = kIrDisconnectRequestEvent+1,  //  18
    

    // LAP level events
    kIrLocalBusyEvent,              //  19 - all buffers in recv buffer pool in use
    kIrLocalBusyClearedEvent,       //  1a - recv buffer available again

    // Timer events - LAP
    kIrFirstIrLAPTimerEvent,        //  1b
    kIrMediaBusyTimerExpiredEvent,  //  1c
    kIrSlotTimerExpiredEvent,       //  1d - discovery slot timer for primary
    kIrQueryTimerExpiredEvent,      //  1e - discovery query timer for secondary
    kIrBackoffTimerExpiredEvent,    //  1f - random delay before retrying connect
    kIrFinalTimerExpiredEvent,      //  20
    kIrPollTimerExpiredEvent,       //  21
    kIrWatchdogTimerExpiredEvent,   //  22
    kIrTurnaroundTimerExpiredEvent, //  23
    kIrLastIrLAPTimerEvent,         //  24

    // Timer events - LMP/LSAPConn
    kIrFirstIrLMPTimerEvent,        //  25
    kIrConnWatchdogExpiredEvent,    //  26
    kIdleDisconnectEvent,           //  27 .. jdg added for LapConn disconnect
    kIrLastIrLMPTimerEvent,         //  28
    
    kIrChangeSpeedCompleteEvent,    //  29 .. jdg added 6/26/2000
    
    kIrMaxEventNumber = kIrChangeSpeedCompleteEvent     // 29 - for sanity checks
    
};


// Classes

// ***NOTE: All of the Event messages based on TIrEvent can have no more additional (ULong) fields than kMaxSubClassFields
//#define kMaxSubClassFields 7
#define kMaxSubClassFields 10   // jdg, append get/puts to conn/listen

// Comment will have one of three chars, '>', '<', or 'X'.  A '>' indicates
// that parameter is required for request.  A '<' indicates that parameter
// is valid on reply.  A 'X' indicates that parameter is required for request
// and is also valid (meaningful) on reply.  In the case of connect/listen
// requests the 'X' indicates that the parameters is required for connect,
// but returned for listen.  Sometimes a parameter may still have the values
// passed in the request, but don't depend on it if it is not marked with an 'X'.


class TIrEvent : public OSObject
{
    OSDeclareDefaultStructors(TIrEvent);
    
    public:

    static  IrDAErr     InitEventLists(void);
    static  void        DeleteEventLists(void);
    
    static  TIrEvent *  GrabEventBlock(ULong event, ULong size);
    static  void        ReleaseEventBlock(TIrEvent * eventBlock);


	    UByte           fEvent;             // X
	    UByte           fPendEvent;         // X LSAPConn saves original request here.
	    UByte           fAllocated;         // debugging
	    UByte           fPadByte2;
	    IrDAErr         fResult;            // <
	    
	    TIrStream       *fClient;           // Client who initiated this task
	    TIrStream       *fDest;             // Sream where this event is posted
};


class TIrLargestEvent : public TIrEvent
{
    OSDeclareDefaultStructors(TIrLargestEvent);
    
    public:

	static TIrLargestEvent * tIrLargestEvent();     // allocate a TIrEvent with room to morph (ugh!)
    //ULong           fSubClassFields[kMaxSubClassFields];
    uintptr_t           fSubClassFields[kMaxSubClassFields];	// LP64 test
};


class TIrDiscoverEvent : public TIrEvent
{
    OSDeclareDefaultStructors(TIrDiscoverEvent);

    public:

	    ULong           fNumSlots;          // >
	    ULong           fConflictDevAddr;   // > Set by IrLMP
	    CList*          fDiscoveredDevices; // >
	    Boolean         fPassiveDiscovery;  // < Did peer discover me?
};

class TIrExtDiscoverEvent : public TIrDiscoverEvent
{
    OSDeclareDefaultStructors(TIrExtDiscoverEvent);

    public:
	    void        *   fCallBack;
	    void        *   fUserData;
};

// Discover requests and replies
typedef TIrDiscoverEvent TIrDiscoverRequest;
typedef TIrDiscoverEvent TIrDiscoverReply;

typedef TIrExtDiscoverEvent TIrExtDiscoverRequest;
typedef TIrExtDiscoverEvent TIrExtDiscoverReply;

// The LSAP request/reply base class
class TIrLSAPConnEvent : public TIrEvent
{
    OSDeclareDefaultStructors(TIrLSAPConnEvent);

    public:

	    TLSAPConn*      fLSAPConn;          // X Client making the request, set by LSAPConn
};

// Disconnect, release requests and replies
typedef TIrLSAPConnEvent TIrDisconnectEvent;
typedef TIrDisconnectEvent TIrDisconnectRequest;
typedef TIrDisconnectEvent TIrDisconnectReply;


// The connect/listen/accept base class
class TIrConnectEvent : public TIrLSAPConnEvent
{
    OSDeclareDefaultStructors(TIrConnectEvent);

    public:

	    ULong           fDevAddr;           // > Addr of device to connect to (for connect)
	    UByte           fLSAPId;            // X Addr of LSAP to connect to (for connect)
	    Boolean         fPassiveConnect;    // < Did my connect become a secondary citizen?
	    TIrQOS*         fMyQOS;             // > Preferred QOS
	    TIrQOS*         fPeerQOS;           // > Peer device's QOS
	    CBuffer*        fData;              // > Connect data to send w/conn or recv w/listen
};

// Connect/listen/accept requests and replies
typedef TIrConnectEvent TIrConnLstnRequest;
typedef TIrConnectEvent TIrConnLstnReply;


// The get/put base class

// JDG: append get/put requests on end of a conn/listen so that when lsapconn turns
// a listen into a get it doesn't smash the original record (ahem).

//class TIrDataXferEvent : public TIrLSAPConnEvent
class TIrDataXferEvent : public TIrConnectEvent
{
    OSDeclareDefaultStructors(TIrDataXferEvent);

    public:

	    //CBuffer*      fData;              // > Buffer to receive data
	    ULong           fOffset;            // X Offset in buffer to get the data
	    ULong           fLength;            // X Space available for received data
	    UByte           fDstLSAPId;         // X Destination LSAP selector id
	    UByte           fSrcLSAPId;         // X Source LSAP selector id
	    UByte           fCtrlOpCode;        // X Data frame or control frame indicator
	    UByte           fCtrlInfo;          // X Control frame results, status
};

// Put requests and replies
typedef TIrDataXferEvent TIrPutEvent;
typedef TIrPutEvent TIrPutRequest;
typedef TIrPutEvent TIrPutReply;

// Get requests and replies
typedef TIrDataXferEvent TIrGetEvent;
typedef TIrGetEvent TIrGetRequest;
typedef TIrGetEvent TIrGetReply;

// Cancel get requests and replies
typedef TIrLSAPConnEvent TIrCancelGetEvent;
typedef TIrCancelGetEvent TIrCancelGetRequest;
typedef TIrCancelGetEvent TIrCancelGetReply;

// Cancel put requests and replies
typedef TIrLSAPConnEvent TIrCancelPutEvent;
typedef TIrCancelPutEvent TIrCancelPutRequest;
typedef TIrCancelPutEvent TIrCancelPutReply;


class TIrLookupEvent : public TIrEvent
{
    OSDeclareDefaultStructors(TIrLookupEvent);

    public:

	    UChar*          fClassName;         // > Class name of attr entry being searched for
	    UChar*          fAttrName;          // > Attr name of attr entry being searched for
	    TIASAttribute*  fAttribute;         // > Returned in reply
	    ULong           fPeerLSAPId;            // LSAP of requested Attribute
};

// Lookup requests and replies
typedef TIrLookupEvent TIrLookupRequest;
typedef TIrLookupEvent TIrLookupReply;


//----------------------------------------------------------------------------------------------
//      IrEvent inlines
//----------------------------------------------------------------------------------------------

inline ULong RequestIdToReplyId(ULong requestId)
{
    return (ULong) (requestId + 1);
}


#endif // __IREVENT_H

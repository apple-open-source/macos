/*
    File:       ttp.h

    Contains:   Tiny TP

*/

#ifndef _TTP_
#define _TTP_

#include "CIrLSAP.h"
#include "IrQOS.h"

class TIrGlue;

//********** Things probably still need fixing!!
#define kMaxTTPRequests 50      // size of TTP event request pool
typedef UInt32 TTPSAP;      // TinyTP Service Access Point (lsap id)
#define TTPBuf CBufferSegment   // this might change, hide CBufferSegment
#define BufUsed(x) (x->Position())      // mark - base 
#define BufSize(x) (x->GetSize())       // logical size of buffer (end-base)
#define BufBase(x) (x->GetBufferPtr())
#define BufHideStart(x, n) (x->Hide(n, kPosBeg))
#define BufHideEnd(x, n) (x->Hide(n, kPosEnd))
#define BufSeekStart(x) (x->Seek(0, kPosBeg))
#define BufGet(x)  (x->Get())
#define BufPut(x, y) (x->Put(y))
#define BufAlloc(x) (CBufferSegment::New(x))
#define BufFree(x)  (x->Delete())
//************************************************************

enum FlowOnOff { FlowOn, FlowOff};
typedef FlowOnOff TFlowOnOff;
enum TTP_Read_Status {TTP_Data_Ok, TTP_Data_Truncated};
typedef TTP_Read_Status TReadStatus;


//**//**//**//**//**//**//**//**//**//**//**//**//**//**//**//**//**//**
//  these don't belong here ... move 'em out!
struct SDU {
    TTPBuf *sarbuf;         // the current reassembly buffer
    Boolean busy;           // controls consumption of rcv queue.
};

enum TTPMsg {               // actions for stuff on my queue    
    TTP_Disconnect=10,      // Disconnect action
    TTP_Segment=11,         // first/middle data segment    (M=1)
    TTP_Segment_Last=12};   // first/last data segment      (M=0)
    
struct TTPq {               // silly TTP queue structure, match QElem pls
    TTPq        *qLink;     // link to next
    short       qType;      // TTPMsg
    short       reason;     // usually reason, could be anything
    TTPBuf      *buf;       // the buffer (or zero if none)
};
typedef TTPq TTPq;
//**//**//**//**//**//**//**//**//**//**//**//**//**//**//**//**//**//**

// From OSUtils.h
struct QElem {
    struct QElem *                  qLink;
    short                           qType;
    short                           qData[1];
};
typedef struct QElem QElem;

typedef QElem * QElemPtr;
struct QHdr {
	short           qFlags;
	QElemPtr        qHead;
	QElemPtr        qTail;
};
typedef struct QHdr QHdr;

typedef QHdr *QHdrPtr;
//**//**//**//**//**//**//**//**//**//**//**//**//**//**//**//**//**//**

class TTinyTP  : public CIrLSAP // hook up to IrGlue/IrStream/IrEtAl
{               
    OSDeclareAbstractStructors(TTinyTP);

public:
    void        free();
				

    Boolean TTPInitialize(              // allocate a bunch-o-buffers and initialize
	TIrGlue *irda,                  // irda glue
	UInt32 credit,                  // initial advertised credit
	UInt32 desiredLSAPId,           // use kAssignDynamicLSAPId if notlisten
	UInt8 * className,              // client's LSAP name to register w/IAS
	ULong   hints = 0);             // hint bits (if any) to set
    
					//******** TTP Client-callable primitives

    void    DoDiscoverRequest (             // look for peer
	    int     slots);                 // use 8
	
    void    DoLookupRequest (               // do an IAS lookup
	    unsigned char   *classname,     // name to look for
	    UInt32          remoteAddr );   // device to query
	
    void    DoListenRequest(                // new --- hang a listen
	    TTPBuf  *userdata);
	    
    void    DoConnectRequest (              // connect to a remote peer
	    UInt32  remoteAddr,             // peer address
	    TTPSAP  SAP,                    // remote LSAP id
	    TIrQOS *QoS,                    // requested QoS (unimpld)
	    int     MaxSduSize,             // max reassembly space
	    TTPBuf  *UserData);             // user-level connect payload (max 50 bytes or so)
	
    //void  DoConnectRequest (              // connect to a remote peer
    //      TTPSAP  SAP,                    // remote LSAP id
    //      TIrQOS *QoS,                    // requested QoS (unimpld)
    //      int     MaxSduSize,             // max reassembly space
    //      TTPBuf  *UserData);             // user-level connect payload (max 50 bytes or so)
	
    void    DoConnectResponse (             // reply to a connect indication (i.e. accept call)
	    TTPSAP  SAP,                    // calling TTP SAP
	    int     MaxSduSize,             // called MaxSduSize
	    TTPBuf  *UserData);             // called UserData

    void    DoDisconnectRequest (           // hangup
	    TTPBuf  *UserData);             // optional userdata (unimpl'd)
	
    void    DoDataRequest (                 // put
	    TTPBuf  *UserData);             // data to send

    void    DoUdataRequest (                // udata put
	    TTPBuf  *UserData);             // data to send

    void    SetLocalFlow (                  // flow control 
	    TFlowOnOff      onOff);         // start/stop flow
	    
    int     TTPXmitQueueSize(int maxPacketSize);    // Flow control - number of xmits we'll currently accept
    void    TTPRxDone();                    // Flow control - let TTP know client is done w/buffer
    void    TTPDiscardPendingPuts(void);    // discard any pending puts (zombie)

    UInt32  GetPeerAddr(void);              // Get peer address and SAP
    UInt32  GetPeerSAP(void);
	
					    //*******
					    //******* TTP Client supplied virtual callbacks
					    //*******

    virtual void TTPDiscoverComplete (      // Discover has completed
	int     numFound,                   // number of peers discovered
	IrDAErr result      ) = 0;          // result of discovery 
	
    virtual void TTPLookupComplete (        // Lookup completed
	IrDAErr result,
	UInt32  peerLSAPId) = 0;

    virtual void TTPConnectIndication (     // Listen complete
	IrDAErr result,
	TTPSAP  SAP,                        // calling TTP SAP
	TIrQOS *ourQOS,                     // our QoS (post negotiation)
	TIrQOS *peerQOS,                    // peer QoS (post negotiation)
	int     MaxSduSize,                 // calling MaxSduSize
	TTPBuf  *UserData) = 0;             // calling UserData

    virtual void TTPConnectConfirm (        // Connect complete
	TTPSAP  SAP,                        // called TTP SAP
	TIrQOS *ourQOS,                     // our QoS (post negotiation)
	TIrQOS *peerQOS,                    // peer QoS (post negotiation)
	int     MaxSduSize,                 // called MaxSduSize
	TTPBuf  *UserData) = 0;             // called UserData
    
    virtual void TTPDisconnectIndication (  // Disconnect complete
	int     reason,                     // passed up from IrLMP (not)
	TTPBuf  *UserData) = 0;

    virtual void TTPDataIndication (        // Read complete
	TTPBuf  *UserData,                  // data read
	TTP_Read_Status status) = 0;        // Ok or Truncated

    virtual void TTPUDataIndication (       // UData Read complete (unimpld)
	TTPBuf  *UserData) = 0;                 // data read
    
    virtual void TTPAcceptDoneIndication(   // accept done (we're really open now)
	IrDAErr result) = 0;                    // just result code, buffer copied during accept call

    virtual void TTPBackEnable(void) = 0;       // Called when more TTP buffers available


private:                        // TinyTP Internal interfaces

				// These are named funny so they closely match the TinyTP spec
    int AvailCredit;            // credit available to advance to peer TTP entity
    int RemoteCredit;           // credit held by peer TTP entity
    int SendCredit;             // credit held by local TTP entity
    Boolean Connected;          // reflects state of LM-MUX connection
    QHdr AvailQueue;            // Queue of available TTPq elements (ahem)
    QHdr TxQueue;               // FIFO queue for TTP_Segments and TTP_Disconnect requests
    QHdr RxQueue;               // FIFO queue for inbound TTP_Segments and TTP_Disconnect requests
    int MaxSegSize;             // Max size of segment in an outbound TTP-PDU
    int TxMaxSduSize;           // Received from peer
    int RxMaxSduSize;           // Sent to peer.  Used to police size of inbound SDUs
    struct SDU RxSdu;           // The current incoming SDU
    Boolean discoverPending;    // true if have a discover request pending already
    int txQDepth;                   // jdg, keep count of entries on tx queue (avoid race condition)
    TTPq ttpqpool[kMaxTTPRequests]; // memory pool for ttq 
    //Boolean putPending;           // jdg test: let a put/put complete finish before new put (needed?)
    //TTinyTP *nextTTP;             // testing: keep a list of the ttp's
    //mblk_t        *fPendingGet;   // allocated msg for pending reads
    /////////////////////////////////////////////////////////////////////////////////////
					// Support data and methods
    UInt32  fPeerAddr;                  // address of our peer
    UInt32  fPeerSAP;                   // lsap id of our peer

    int initial_credit;                 // number of packet buffers to alloc at init time
    void AppendTail(QHdr *, TTPMsg, int, TTPBuf *); // add event to end of my list
    void FlushQueue(QHdr *);                        // remove events from my queue
    TTPBuf *GetSegment(int, TTPBuf *);              // extract segment
    void CheckTheQueues();              // do pending work
    Boolean CheckTxQueue();
    Boolean CheckRxQueue();
    void Reassemble(TTPBuf *dest, TTPBuf *src); // append TTPBuf to SAR buffer
    void SendDataless();                // send off a dataless data PDU
    ////////////////////////////////////////////////////////////////////////////////////
    

	
					//*****
					// TTP is supplying these virtual functions to CIrLSAP
					//*****
    void DiscoverComplete (                 // discovery has finished callback
		UInt32  numFound,           // number of peers discovered
		IrDAErr result );           // result of discovery

    void LSAPLookupComplete (           // an IAS query has finished
		IrDAErr result,             // result of the lookup
		UInt32  peerLSAPId);        // peer's LSAP id of the service
    
    void ConnectComplete (              // a connect request has completed
		IrDAErr result,             // result of the connect request
		TIrQOS *myQOS,              // my qos ... (requested?)
		TIrQOS *peerQOS,            // peer's qos .. (result?)
		CBufferSegment *data);      // data payload from connect msg
    
    void DisconnectComplete (void );        // you've been disconnected
		
    void DataPutComplete (
		IrDAErr result,             // result code
		CBufferSegment *data);      // data that was sent

    void DataGetComplete (
		IrDAErr result,             // result code
		CBufferSegment *data);      // data
		
    void ListenComplete (                   // check me
		IrDAErr result,
		UInt32  peerAddr,           // address of connecting peer
		UInt32  peerLSAPId,         // LSAP id of connecting peer
		TIrQOS  *myQOS,             // my qos ... (requested?)
		TIrQOS  *peerQOS,           // peer's qos .. (result?)
		CBufferSegment *data);      // data payload from connect msg
		
    void AcceptComplete (                   // check me
		IrDAErr result,
		CBufferSegment *data);      // data payload in connect msg

    void CancelGetsComplete (               // all pending gets have been canceled
		    IrDAErr result);
		    
    void CancelPutsComplete (               // all pending puts have been canceled
		    IrDAErr result);


typedef TTPSAP  LMSAP;                      // just a little leftover callback glue 
    void TTPHandleConnectIndication (       // between the above LSAP callbacks and TTP
	IrDAErr result,
	LMSAP   sap,
	TIrQOS  *ourQOS,                    // our QoS
	TIrQOS  *peerQOS,                   // peer QoS
	TTPBuf  *UserData);
	
    void TTPHandleConnectConfirm (
	LMSAP   sap,
	TIrQOS  *ourQOS,                    // our QoS
	TIrQOS  *peerQOS,                   // peer QoS
	TTPBuf  *UserData);
	
    void TTPHandleDisconnectIndication (
	int     reason,
	TTPBuf  *UserData);

    void TTPHandleDataIndication (
	TTPBuf  *userData);

    void TTPHandleUDataIndication (
	TTPBuf  *userData); 

    void TTPHandleAcceptComplete (
	IrDAErr result,
	TTPBuf  *userdata);                 // buffer send in with accept call

};  // class TTinyTP

// Inlines
inline UInt32 TTinyTP::GetPeerAddr() { return fPeerAddr; };
inline UInt32 TTinyTP::GetPeerSAP() { return fPeerSAP; };

#endif  // _TTP_





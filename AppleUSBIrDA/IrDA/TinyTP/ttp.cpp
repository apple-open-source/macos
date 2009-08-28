/*
    File:       ttp.cpp

    Contains:   Client interfaces to Tiny TP


*/

#include "ttp.h"
#include "ttppdu.h"
#include "CBufferSegment.h"
#include "IrDALog.h"

#if (hasTracing > 0 && hasTTPTracing > 0)

enum IrTinyTPTraceCodes
{
    kLogFree = 1,
    kLogInit,
    kDiscoverRequest,
    kLookupRequest,
    kListenRequest,
    kConnectRequest,
    kConnectResponse,
    kDisconnectRequest,
    kDataRequest,
    kFlowControl,
    kDataRequestDropped,
    kXmitQueueSize,
    kXmitQueueSize2,
    kXmitQueueSize3,
    kXmitIrDABufsLow,
    kTTPRxDone,
    kDiscardPendingPuts
};

EventTraceCauseDesc gTraceEvents[] = {
    {kLogFree,                      "TinyTP: free obj="},
    {kLogInit,                      "TinyTP: initializing, obj=, credit=, lsap="},
    {kDiscoverRequest,              "TinyTP: discover request"},
    {kLookupRequest,                "TinyTP: lookup request"},
    {kListenRequest,                "TinyTP: listen request"},
    {kConnectRequest,               "TinyTP: connect request"},
    {kConnectResponse,              "TinyTP: connect response"},
    {kDisconnectRequest,            "TinyTP: disconnect request"},
    {kDataRequest,                  "TinyTP: put data request"},
    {kFlowControl,                  "TinyTP: flow control request"},
    {kDataRequestDropped,           "TinyTP: ** Data request dropped ***"},
    {kXmitQueueSize,                "TinyTP: XMIT Queue Size, sendcredit=, depth="},
    {kXmitQueueSize2,               "TinyTP: XMIT Queue Size2, explode_factor, MaxSegSize="},
    {kXmitQueueSize3,               "TinyTP: XMIT Queue Size3, txQDepth=, depth="},
    {kXmitIrDABufsLow,              "TinyTP: XMIT - global buffers low, SendCredit=, CBufCnt="},
    {kTTPRxDone,                    "TinyTP: Client says Rx is done"},
    {kDiscardPendingPuts,           "TinyTP: discard pending puts"}
};

#define XTRACE(x, y, z)  IrDALogAdd( x, y, (uintptr_t)z & 0xffff, gTraceEvents, true ) 

#else
    #define XTRACE(x, y, z) ((void)0)
#endif

const UInt8 kIASTinyTPLSAPSelAttrStr[]        = "IrDA:TinyTP:LsapSel";

#define super CIrLSAP
	OSDefineMetaClassAndAbstractStructors(TTinyTP, CIrLSAP);

void
TTinyTP::free()
{
    XTRACE(kLogFree, 0, this);
    
    super::free();  
}

Boolean 
TTinyTP::TTPInitialize(             // do real initialization
	TIrGlue *irda,                  // glue
	UInt32  credit,                 // number of concurrent read buffers (hmm?)
	UInt32  desiredLSAPId,          // port to get (kAssignDynamicLSAPId if not listening)
	UInt8 * className,              // name to register with IAS
	ULong   hints)
{
    int i;

    XTRACE(kLogInit, 0, this);
    XTRACE(kLogInit, credit, desiredLSAPId);
    
    
    TxQueue.qFlags = 0;
    TxQueue.qHead = TxQueue.qTail = nil;
    RxQueue.qFlags = 0;
    RxQueue.qHead = RxQueue.qTail = nil;
    AvailQueue.qFlags = 0;
    AvailQueue.qHead = AvailQueue.qTail = nil;


    AvailCredit     = 0;            // lets zero out some fields for sanity's sake
    RemoteCredit    = 0;
    SendCredit      = 0;
    MaxSegSize      = 0;
    TxMaxSduSize    = 0;
    RxMaxSduSize    = 0;
    RxSdu.sarbuf = nil;
    RxSdu.busy = false;

    txQDepth = 0;           // nothing on tx queue yet
    Connected = false;
    discoverPending = false;    // no discover request pending yet.
    
    initial_credit = credit;
    
    // have number of buffers allocated compiled into IrDA "for now"
    // allocate "credit" buffers and hand to Ir?
    //for (i = 0 ; i < (10*credit) ; i++) {
    for (i = 0 ; i < kMaxTTPRequests ; i++) {
	TTPq *tq;
	//tq = (TTPq *)NewPtr(sizeof(TTPq));
	//check(tq);                        // these should work ...
	tq = &ttpqpool[i];          // grab out of our own pool instead of using NewPtr
	tq->qLink = nil;
	tq->qType = 1;
	Enqueue((QElem *)tq, &(this->AvailQueue));  // Prep my free list
    }
    
    return super::Init(irda, desiredLSAPId, className, (UInt8 *) kIASTinyTPLSAPSelAttrStr, hints);      // Init LSAP
}

//
// thin layer over LMP (turn into inlines after debugged)
//
void
TTinyTP::DoDiscoverRequest (int slots)
{
    XTRACE(kDiscoverRequest, 0, this);
    if (!discoverPending) {
	discoverPending = true;
	this->Discover(slots);          // just call LSAP
    }
}

void
TTinyTP::DoLookupRequest (
	unsigned char   *classname,
	UInt32          remoteAddr  )       // name and device to query
{
    XTRACE(kLookupRequest, 0, this);
    
    //fPeerAddr = remoteAddr;       // FOO.  CIrLSAP sets peer addr during lookup
    
    this->LSAPLookup(classname, ( UInt8 * )kIASTinyTPLSAPSelAttrStr, remoteAddr );
}

void
TTinyTP::DoListenRequest(
    TTPBuf  *userData)          // read buffer
{
    XTRACE(kListenRequest, 0, this);
    Listen(userData);           // just call CIrLSAP to start the listen
}

//  ************
//  ************ Connect Request
//  ************   client can release userData upon return
//  ************

/*
void
TTinyTP::DoConnectRequest (
    TTPSAP  sap,                // peer's lsap
    TIrQOS  *qos,               // requested QoS
    int     maxSduSize,
    TTPBuf  *userData)          // size of buffer is how much to send
{
    this->DoConnectRequest(
	fPeerAddr,              // supply the missing remote address (ahem)
	sap, qos, maxSduSize, userData);
}
*/
void
TTinyTP::DoConnectRequest (
    UInt32  remoteAddr,         // peer's address
    TTPSAP  sap,                // peer's lsap
    TIrQOS  *qos,               // requested QoS
    int     maxSduSize,
    TTPBuf  *userData)          // size of buffer is how much to send
{
#pragma unused (qos)
    int n;
    TTPBuf *pduBuf;                 // the buffer for it

    XTRACE(kConnectRequest, 0, this);
    fPeerAddr = remoteAddr;             // save peer's address
    fPeerSAP = sap;                     // save peer's sap address (not used yet)
    
    this->Connected = false;                // Not connected to anything yet
    this->AvailCredit = 0;                  // (needed?)
    this->RxMaxSduSize = maxSduSize;        // Set max inbound SDU size
    //this->RxSdu.size = 0;
    //if (maxSduSize > 0)
    //  this->RxSdu.sarbuf = BufAlloc(maxSduSize);  // alloc a reassembly buffer
    //else
	this->RxSdu.sarbuf = nil;       // else make sure it's nil
    this->RxSdu.busy = false;           // ??
    
    n = this->initial_credit;       // default buffering
    this->SendCredit = 0;
    if (n > 127) {
	AvailCredit = n - 127;
	n = 127;
    }
    RemoteCredit = n;
    pduBuf = ttp_pdu_connect (              // return filled in connect TTP PDU
	    (maxSduSize == 0) ? 0 : 1,      // P flag (1 if sends maxSduSize)
	    n, maxSduSize, userData);
    require(pduBuf, NoMem);

    this->Connect(fPeerAddr, sap, pduBuf);  // ask LSAP to do a connect
    this->CheckTheQueues();                 // see if we have more work to do

NoMem:
    return;
}

//  ************
//  ************ Connect Response (Accept call)
//  ************   client can release userData upon return
//  ************

void
TTinyTP::DoConnectResponse (
    TTPSAP  sap,                // calling TTP SAP
    int     maxSduSize,         // called MaxSduSize
    TTPBuf  *userData)          // called UserData
{
#pragma unused (sap)
    int n;
    TTPBuf *pduBuf;                 // the buf for it
    
    XTRACE(kConnectResponse, 0, this);
    XTRACE(kConnectResponse, sap, maxSduSize);
    XTRACE(kConnectResponse, maxSduSize >> 16, maxSduSize);
    
    this->AvailCredit = 0;
    this->RxMaxSduSize = maxSduSize;
    //this->RxSdu.size = 0;
    //if (maxSduSize > 0)
    //  this->RxSdu.sarbuf = BufAlloc(maxSduSize);  // alloc a reassembly buffer
    //else
	this->RxSdu.sarbuf = nil;           // else make sure it's nil
    this->RxSdu.busy = false;
    
    n = this->initial_credit;       // default buffering
    if (n > 127) {
	this->AvailCredit = n - 127;
	n = 127;
    }
    this->RemoteCredit = n;         // we're extending these many buffers to peer
    pduBuf = ttp_pdu_connect(
	(maxSduSize == 0) ? 0 : 1, n, maxSduSize, userData);
    require(pduBuf, NoMem);
	
    this->Accept(pduBuf);           // tell LSAP to accept (sanity check request parms first?)
    this->Connected = true;
    this->CheckTheQueues();         // see if we have more work to do
NoMem:
    return;
}

//  ************
//  ************ Disconnect Request
//  ************   client can release userData upon return
//  ************

void
TTinyTP::DoDisconnectRequest (
	TTPBuf *userData)           // userdata doesn't make it
{
    XTRACE(kDisconnectRequest, 0, this);
    
    if (Connected == false) {       // if already disconnected
	//TTPDisconnectIndication(0, nil);      // just tell 'em so (again?)
	Disconnect();                           // tell cirlsap to disconnect
    }
    else {
	this->AppendTail(&(this->TxQueue), TTP_Disconnect, 0, userData);
	txQDepth++;
	this->CheckTheQueues();         // see if we have more work to do
    }
}

//  ************
//  ************ Data Request
//  ************   client CAN release userData upon return (until we rewrite it again)
//  ************
extern int queueDepth(QHdr *qh);        // debugging ... return depth of a queue

void
TTinyTP::DoDataRequest (
	TTPBuf *userData)           // data to send
{
    TTPBuf *newbuf;                     // get rid of this soon ....
    //static int dropCount = 0;         // debugging.  count number of drops in a row ...
    
    XTRACE(kDataRequest, 0, this);
    
    check (userData);                   // error if nil buffer ptr
    if (!userData) return;
    //check(this->Connected == true);       // error if we're not connected
    check(BufSize(userData));           // error, if zero size send (ahem, why?)
    if (Connected == false) return;     // error, but don't crash in this version ....
    
    /*************************************************************************
    if (SendCredit == 0) {              // drop if our credit has hit bottom
	XTRACE(kDataRequestDropped, dropCount, queueDepth(&TxQueue));
	dropCount++;
	if (dropCount > 20) {           // 100 was hit
	    DebugStr("\pTTP just dropped 20 sends in a row due to congestion");
	    dropCount = 0;      // may want to keep trying ....
	}
	this->CheckTheQueues();         // this shouldn't be needed ...
	return;                 // return (drop the request)
    }
    else
	dropCount = 0;          // hey, we're still alive!
    *************************************************************************/
    
    if (this->TxMaxSduSize == 0) {              // SAR Disabled
	if (BufSize(userData) > this->MaxSegSize)
	    return;                             // todo: return error: too big & SAR disabled
	newbuf = ttp_pdu_data(false, 0, userData);
	require(newbuf, NoMem);
	this->AppendTail(&(this->TxQueue), TTP_Segment_Last, 0, newbuf);
	txQDepth++;
    }
    else {                                  // SAR enabled
	int i, numsegs;
	if (BufSize(userData) > this->TxMaxSduSize)     // error ...
	    return;                         // todo: return error: too big, even for SAR
	    
	numsegs = ((BufSize(userData) + this->MaxSegSize -1) / this->MaxSegSize);
	if (numsegs == 1) {                 // if fits w/out SAR ... (jdg added)
	    newbuf = ttp_pdu_data(false, 0, userData);          // copy to data pdu
	    require(newbuf, NoMem);
	    this->AppendTail(&(this->TxQueue), TTP_Segment_Last, 0, newbuf);
	    txQDepth++;
	}
	else {                              // else ... gee let's do SAR and split into lots of packets
	    TTPBuf *seg;                        // Get segment just returns pointers into the orig buffer
	    for (i = 1 ; i < numsegs; i++) {    // first N-1 segments have the more bit set
		seg = GetSegment(i, userData);              // get ptrs to the initial segments
		require(seg, NoMem);
		newbuf = ttp_pdu_data(true, 0, seg);        // copy to new buffer w/the ttp header byte
		require(newbuf, NoMem);
		BufFree(seg);                               // this just frees the wrapper (I hope)
		this->AppendTail(&(this->TxQueue), TTP_Segment, 0, newbuf);
		txQDepth++;
	    }
	    seg = GetSegment(numsegs, userData);            // get the last segment
	    require(seg, NoMem);
	    newbuf = ttp_pdu_data(false, 0, seg);           // copy the data out
	    require(newbuf, NoMem);
	    BufFree(seg);                                   // free the wrapper
	    this->AppendTail(&(this->TxQueue), TTP_Segment_Last, 0, newbuf);
	    txQDepth++;
	}
    }
    this->CheckTheQueues();         // see if we have work to do
NoMem:
    return;
}

// we're a zombie, discard any queued up data
void
TTinyTP::TTPDiscardPendingPuts(void)
{
    TTPq *tq;
    IrDAErr err;
    int counter = 0;        // debugging only

    XTRACE(kDiscardPendingPuts, 0x1111, 0);
    // Could do a flushqueue, but I don't want to flush a 
    // disconnect request, just data puts.  oh well.
    //FlushQueue(&TxQueue);     // should be empty anyway

    while ((tq = (TTPq *)TxQueue.qHead) != 0) {
	if (tq->qType != TTP_Segment &&             // if not a segment put
	    tq->qType != TTP_Segment_Last) break;   // then prob a disconnect, stop
	err = Dequeue((QElem *)tq, &TxQueue);
	ncheck(err);                                // really shouldn't happen
	if (tq->buf != 0) BufFree(tq->buf);         // be careful ...
	tq->buf = nil;
	tq->qLink = nil;
	Enqueue((QElem *)tq, &AvailQueue);
	counter++;
    }
    XTRACE(kDiscardPendingPuts, 0xffff, counter);
}

void
TTinyTP::DoUdataRequest (               // not really impl'd
	TTPBuf *userData)               // data to send
{
#pragma unused (userData)
    if (this->Connected == false) return;   // error
    //this->UDataPut(userData);         // send off UData
}

void
TTinyTP::SetLocalFlow (
	TFlowOnOff onOff)       // start/stop flow
{
    XTRACE(kFlowControl, 0, 0);
    
    if (onOff == FlowOn) this->RxSdu.busy = false;
    else                 this->RxSdu.busy = true;
    
    this->CheckTheQueues();         // see if we have work to do
}

//
// Return number of packets the client can send to us w/out worrying
// about running out of send credits (or CBuffers).  Note that this
// is intended to be called from non-def task clients, so there's been
// some work done to avoid race conditions.
//
int
TTinyTP::TTPXmitQueueSize(int maxPacketSize)
{   int depth;
    int explode_factor;             // maxPacketSize could turn into this many frags
    //int REVIEW_Unlimited_Buffer_Allocation;
    
    // Check against 8 buffers, one for a new write, and 7 for new get's in case
    // we get a 7-window full set of data packets all at once.
    /**************************
    if (CountFreeCBuffers() <= 8) { // first -- is IrDA running out of buffers?
	XTRACE(kXmitIrDABufsLow, SendCredit, CountFreeCBuffers());
	return 0;                   // if so, then pretend we can't send
    }
    *****************************/
	
    depth = SendCredit;                 // first start with currently available SendCredit

// avoid queueDepth so that we can call this from non-dt tasks!
//  depth -= queueDepth(&TxQueue);      // now subtract out pending writes
//                                      // now have actual packets count available to send
    depth -= txQDepth;                  // subtract out depth of tx queue

    XTRACE(kXmitQueueSize3, txQDepth, depth);
    
    check(depth >= 0);                  // sanity check ...
    if (depth <= 0) return 0;
    
    if (this->TxMaxSduSize == 0) {      // if SAR Disabled
	XTRACE(kXmitQueueSize, SendCredit, depth);
	return depth;                   //  then we're done
    }

    // if we got this far, we have some credit available, let's see what the
    // worst-case scenario looks like
    
    explode_factor = (maxPacketSize + this->MaxSegSize -1) / this->MaxSegSize;
					// else each send could fragment into lots of packets
    depth = depth / explode_factor;     // assuming max packetsize on all packets ...
    
    if (depth == 0) {           // ahem, special case of hitting the limit, let it!
				// we know depth started out >= 1 due to previous test
	depth = 1;              // let it send one more and then go busy
    }

    XTRACE(kXmitQueueSize, SendCredit, depth);
    XTRACE(kXmitQueueSize2, explode_factor, MaxSegSize);
    return depth;       // could be fancier ...
}

void
TTinyTP::TTPRxDone(void)            // move this ...
{
    AvailCredit++;              // new style credit
    XTRACE(kTTPRxDone, RemoteCredit, AvailCredit);
    CheckTheQueues();           // see if we have work to do (eg a dataless credit extension) 
}

/*
    File:       IrComm.cpp

    Contains:   IrCOMM protocol layer

*/

#include "IrComm.h"
#include "IrDAComm.h"
#include "IrGlue.h"
#include "CBufferSegment.h"
#include "ttppdu.h"
#include "IrDALog.h"

#pragma mark -- Globals

#if (hasTracing > 0 && hasIrCommTracing > 0)

enum TraceCodes
{
    kLogNew = 1,
    kLogFree,
    kLogInit,
    kLogTxAvail,
    kLogWrite,
    kLogReturnCredit,
    kLogBackEnable,
    
    kLogTryConnect,
    kLogListen,
    kLogDisconnect,
    kLogDiscoverComplete,
    kLogLkupDone,
    kLogConnectConfirm,
    kLogDisconnected,
    
    kLogConnectIndication,
    kLogAcceptDone,
    
    kLogTTPDataRead,
    kLogTTPDataRead2,
    
    kLogDataWrite,
    kLogDataRead
};

static
EventTraceCauseDesc gTraceEvents[] = {
    {kLogNew,               "IrComm: new, obj="},
    {kLogFree,              "IrComm: free, obj="},
    {kLogInit,              "IrComm: init, obj="},
    {kLogTxAvail,           "IrComm: tx available, returning="},
    {kLogWrite,             "IrComm: write, count="},
    {kLogReturnCredit,      "IrComm: return credit. bytes consumed, count="},
    {kLogBackEnable,        "IrComm: back eable"},
    
    {kLogTryConnect,        "IrComm: try to connect"},
    {kLogListen,            "IrComm: listen"},
    {kLogDisconnect,        "IrComm: disconnect request"},
    {kLogDiscoverComplete,  "IrComm: discover complete, numfound=, result="},
    {kLogLkupDone,          "IrComm: lookup done, result=, peerid="},
    {kLogConnectConfirm,    "IrComm: connect confirm, result="},
    {kLogDisconnected,      "IrComm: disconnected.  reason="},
    
    {kLogConnectIndication, "IrComm: listen complete"},
    {kLogAcceptDone,        "IrComm: accept done"},
    
    {kLogTTPDataRead,           "IrComm: read complete, result=, len="},
    {kLogTTPDataRead2,          "IrComm: read ctlLen=, dataLen="},

    {kLogDataWrite,         "IrComm: write data"},
    {kLogDataRead,          "IrComm: read data"}
};

#define XTRACE(x, y, z)  IrDALogAdd( x, y, (uintptr_t)z & 0xffff, gTraceEvents, true )  
#else
#define XTRACE(x, y, z) ((void)0)
#endif

#pragma mark -- Prototypes

void    InitSizeQueue(void);                    // init the silly size queue
void    AddPacketSize(short length);            // add a packet's size to the fifo
short   FirstPacketSize(void);                  // return size (remaining) from the first packet in the q
void    ShrinkFirstPacketSize(short length);    // mark a few bytes as consumed from the first packet in q
void    NukeFirstPacketSize(void);              // first packet in the q has been consumed

extern IrDAErr Dequeue(QElemPtr qElement, QHdrPtr qHeader);     // MOVE these too **************
extern IrDAErr Enqueue(QElemPtr qElement, QHdrPtr qHeader);

#if (hasTracing > 0 && hasIrCommTracing > 1)
void IrDALogData(int msg, UInt8 *buf, int count);       // log data into irdalog
#define LOGDATA(msg, buf, count)    IrDALogData(msg, buf, count)
#else
#define LOGDATA(msg, buf, count) (void)0)
#endif


#define super TTinyTP
    OSDefineMetaClassAndStructors(IrComm, TTinyTP);

/*static*/
IrComm *
IrComm::irComm(TIrGlue *irda, IrDAComm *irdacomm)
{
    IrComm *obj = new IrComm;
    
    XTRACE(kLogNew, 0, obj);
    
    if (obj && !obj->Init(irda, irdacomm)) {
	obj->release();
	obj = nil;
    }
    
    return obj;
}

void
IrComm::free(void)
{
    XTRACE(kLogFree, 0, this);
    
    ///xxx;

    super::free();
}

Boolean
IrComm::Init(TIrGlue *irda, IrDAComm *irdacomm)
{
    XTRACE(kLogInit, 0, this);
    UInt8 *classname = (UInt8 *)"IrDA:IrCOMM";  // same lsap for client and server!
    
    fIrDAComm = irdacomm;
    fConnected = false;
    fMaxPacketSize = 2048;      // set for real when connected
    
    InitSizeQueue();
    
    if (!super::TTPInitialize(
		    irda,
		    (UInt32)kTinyTPCredit,                  // credits (in packets)
		    (UInt32)kAssignDynamicLSAPId,           // any old lsap id for now (will register it later?)
		    classname,                              // unused classname (real one done later)
		    (ULong)kDevInfoHintIrCOMM)) return false;       // init tinytp, bail if fails
    
    ////
    return true;
}


// Return the number of bytes we can transmit without starting
// to worry about flow control.

UInt32
IrComm::TxBufferAvailable(void)
{
    if (fConnected) {
	UInt32  count;
	count = TTPXmitQueueSize(fMaxPacketSize) * fMaxPacketSize;
	XTRACE(kLogTxAvail, count >> 16, count);
	return count;
    }
    XTRACE(kLogTxAvail, 0xffff, 0xffff);
    return 30*1024;         // anything big, we just going to flush it (race condition?)
}

//
// break the write up into packet size chucks
// and send them off to tinytp for writing
//
UInt32
IrComm::Write(UInt8 *buf, UInt32 length)
{
    UInt32 written = 0;
    
    XTRACE(kLogWrite, length >> 16, length);
    
    require(buf, Done);
    require(length > 0, Done);

    while (length > 0) {
	CBufferSegment *pkt;
	int rc, len;
	
	require(TTPXmitQueueSize(fMaxPacketSize) > 0, Done);    // should have at least room for one more write
	
	len = Min(length, fMaxPacketSize);
	require(len > 0, Done);
	require(len < 2047, Done);  // sanity
	
	pkt = BufAlloc(len + 1);
	require(pkt, Done);
		
	rc = pkt->Put(0);           // no ircomm control info in this packet
	check(rc == 0);             // should return the byte we're putting
	
	rc = pkt->Putn(buf, len);
	require(rc == len, Done);   // should return count of bytes moved (will truncate to fit in buffer)
	
	BufHideRest(pkt);           // hide rest of the buffer (i.e. set the write length)
	
	DoDataRequest(pkt);         // send to tinytp for xmit
	
	BufFree(pkt);               // tinytp copies the data from us, free our copy (keep the cbuf?)
	
	buf     += len;
	written += len;
	length  -= len;
    }
    //IOLog("IrComm: wrote %ld bytes\n", written);
    
Done:
    return written;
}

//
// our tty client has consumed some bytes, convert the count into
// a packet count and extend that many credits back to our ttp peer.
//
void
IrComm::ReturnCredit(UInt32 bytecount)
{
    XTRACE(kLogReturnCredit, bytecount >> 16, bytecount);
    short x;
    
    while (bytecount > 0) {         // loop until we've "processed" what's been returned
	x = FirstPacketSize();      // get size of first packet in the fifo (what's left anyway)
	require(x > 0, Fail);       // sanity
	
	if (bytecount >= (UInt32)x) {       // if we've consumed all of the first packet
	    NukeFirstPacketSize();          // take it out of the fifo
	    bytecount -= x;                 // adjust amount consumed
	    TTPRxDone();                    // the flow control work: extend tinytp credit by one -- we've consumed a pkt!
	}
	else {                          // first packet size is more than we've consumed
	    ShrinkFirstPacketSize(bytecount);
	    bytecount = 0;              // and we're done
	}
    }
Fail:
    return;
}

void
IrComm::TryConnect(int slots)
{
    XTRACE(kLogTryConnect, 0, fConnected);
    
    if (fConnected == false)
	DoDiscoverRequest(slots);       // look for peer (1,6,8,16 legal slot counts)
}

void
IrComm::Listen(void)
{
    CBufferSegment *buf;

    XTRACE(kLogListen, 0, fConnected);
    require(fConnected == false, Fail);
    
    buf = BufAlloc(1000);       // for initial connect message data
    require(buf, Fail);
    
    DoListenRequest(buf);
    
Fail:
    return;
}

void
IrComm::Disconnect(void)
{
    //int review_disconnect_logic;
    
    XTRACE(kLogDisconnect, 0, 0);
    
    // review ... 
    DoDisconnectRequest(nil);
}

#pragma mark ==== TinyTP Callbacks ====
    
void IrComm::TTPDiscoverComplete(int numFound, IrDAErr result)
{
    int peer_index;                     // init -1 to keep from connnecting to non-ircomm hint devices, else init to 0
    Boolean found_ircomm_peer = false;
    
    XTRACE(kLogDiscoverComplete, numFound, result);
    
#if (hasTracing > 0 && hasIrCommTracing > 1)
    peer_index = 0;                         // if debug, allow connect attempt to first device if no ircomm 
    DebugLog("numFound=%d, result=%ld", numFound, result);
#else
    peer_index = -1;                        // if normal build, insist on an ircomm device before connecting
#endif
    if (numFound > 0) {
	int i;
#if (hasTracing > 0 && hasIrCommTracing > 1)
	for (i = 0 ; i < numFound; i++) {       // log all the discovered peers -- debugging only
	    DebugLog("discovered at %ld nicname '%s' hints 0x%lx",
			fDiscoverInfo[i].addr,
			fDiscoverInfo[i].name,
			fDiscoverInfo[i].serviceHints);
	}
#endif
	for (i = 0 ; i < numFound; i++) {                       // find a peer with the ircomm hint bit
	    if (fDiscoverInfo[i].serviceHints & kDevInfoHintIrCOMM) {   // ircomm hint!
		peer_index = i;
		found_ircomm_peer = true;
		break;
	    }
	}
	
	/* now try and do an lsap lookup if our peer had the ircomm hint bit set */
	/* or if debug build, then maybe try to connect to an hp netbeamir */
	if (peer_index >= 0) {          // if we found an ircomm peer, do ias query
	    IrDAErr err;
	    UInt8 *classname = (UInt8 *)"IrDA:IrCOMM";
	    UInt8 *attrname = (UInt8 *)"IrDA:TinyTP:LsapSel";
#if (hasTracing > 0 && hasIrCommTracing > 0)
	    if (found_ircomm_peer == false) classname = (UInt8 *)"HP-BDP";          // jdg's debugging
#endif            
	    fPeerAddress = fDiscoverInfo[peer_index].addr;
	    err = LSAPLookup(classname, attrname, fDiscoverInfo[peer_index].addr);
	    ncheck(err);
	    if (err == noErr)       // success
		return;
	}
    }
    
    // Fall through with any error
    if (fIrDAComm)
	fIrDAComm->ConnectionStatus(false);     // we're disconnected
    
}

void IrComm::TTPLookupComplete(IrDAErr result,UInt32 peerLSAPId)
{
    XTRACE(kLogLkupDone, result, peerLSAPId);

#if (hasTracing > 0 && hasIrCommTracing > 0)
    DebugLog("lookup complete, result=%ld, lsap=%ld", (long int)result, (long int)peerLSAPId);
#endif
    if (result == noErr && peerLSAPId > 0) {
	DoConnectRequest (              // connect to a remote peer
	    fPeerAddress,                   // peer address
	    peerLSAPId,                     // remote LSAP id
	    nil, 0, nil);                   // requested QoS (unimpld), no reassembly, no user level data
    }
    else {
	fConnected = false;
	if (fIrDAComm)
	    fIrDAComm->ConnectionStatus(false);     // we're disconnected
    }
}

void
IrComm::TTPConnectIndication (      // Listen complete
	IrDAErr result,
	TTPSAP  SAP,                        // calling TTP SAP
	TIrQOS *ourQOS,                     // our QoS (post negotiation)
	TIrQOS *peerQOS,                    // peer QoS (post negotiation)
	int     MaxSduSize,                 // calling MaxSduSize
	TTPBuf  *UserData)                  // calling UserData
{
    XTRACE(kLogConnectIndication, 0, result);
    
    require(UserData, Fail);            // sanity, we allocated this in listen, should always be there
    BufFree(UserData);                  // returned with data from connect msg.  should look at it here.
    
    if (result == noErr) {
	if (peerQOS)    fMaxPacketSize = peerQOS->GetDataSize() - 5;
	else            fMaxPacketSize = 2048;  // never happens I hope
	
	DoConnectResponse (             // reply to a connect indication (i.e. accept call)
		SAP,                    // calling TTP SAP
		0,                      // ircomm doesn't do tinytp fragmentation (silly)
		nil);                   // called UserData
    }
    else if (fIrDAComm)
	    fIrDAComm->ConnectionStatus(false);
Fail:
    return;
}


void
IrComm::TTPConnectConfirm (     // Connect complete
	TTPSAP  SAP,                        // called TTP SAP
	TIrQOS *ourQOS,                     // our QoS (post negotiation)
	TIrQOS *peerQOS,                    // peer QoS (post negotiation)
	int     MaxSduSize,                 // called MaxSduSize
	TTPBuf  *UserData)                  // called UserData
{
    XTRACE(kLogConnectConfirm, 0, 0);
    
    check(ourQOS);
    check(peerQOS);
    if (peerQOS) {
	fMaxPacketSize = peerQOS->GetDataSize() - 5;
	//IOLog("ircomm - max packet size read as %d\n", fMaxPacketSize);
    }
    else {
	fMaxPacketSize = 2048;  // never happens I hope
	//IOLog("ircomm - qos nil, max packet set to %d\n", fMaxPacketSize);
    }
    
    if (fIrDAComm) {
	fConnected = true;
	fIrDAComm->ConnectionStatus(true);
    }
}
    

void
IrComm::TTPDisconnectIndication (       // Disconnect complete
	int     reason,
	TTPBuf  *UserData)
{
    XTRACE(kLogDisconnected, 0, reason);
    fConnected = false;
    if (fIrDAComm)
	fIrDAComm->ConnectionStatus(false);     // we're disconnected
}


void
IrComm::TTPDataIndication(TTPBuf *userdata, TTP_Read_Status status)
{
    UInt32 ctlLen, dataLen, count;
    UInt8 *ctlBuffer, *dataBuffer, *buf;

    require(userdata, Fail);
    nrequire(status, Fail);
    
    XTRACE(kLogTTPDataRead, status, BufSize(userdata));

    count = BufSize(userdata);
    buf   = BufBase(userdata);
    
    //IOLog("IrComm got %ld bytes\n", count);

//  LOGDATA(kLogDataRead, buf, count);      // send all data to irdalog if tracing > 1
    
    ctlLen    = *buf;                       // first byte is length of control data
    ctlBuffer = buf + 1;                    // control data (if any) after ctl length

    dataLen     = count - ctlLen - 1;       // data is packet - ctlLen byte - ctl buffer
    dataBuffer  = ctlBuffer + ctlLen;       // data follows control buffer
    
    XTRACE(kLogTTPDataRead2, ctlLen, dataLen);
    //
    // parse control buffer here (eventually)
    //
    if (dataLen) {                                  // if any data to pass up
	AddPacketSize(dataLen);                             // keep track of packet lens for flow control
	if (fIrDAComm)
	    fIrDAComm->IrCommDataRead(dataBuffer, dataLen);     // send data to IrDAComm and pseudo tty
    } 
	else TTPRxDone();
    
    BufFree(userdata);                      // release the cbuffer
    
Fail:
    return;
}

void
IrComm::TTPUDataIndication(TTPBuf *UserData)
{
    // crash, this isn't impld
}
    

void 
IrComm::TTPAcceptDoneIndication(IrDAErr result)     // connection fully opened
{
    XTRACE(kLogAcceptDone, 0, result);
    
    fConnected = (result == noErr);

    if (fIrDAComm)
	fIrDAComm->ConnectionStatus(result == noErr);
}           


void
IrComm::TTPBackEnable(void)         // more tinytp buffers are available
{
    XTRACE(kLogBackEnable, 0, 0);
    if (fIrDAComm)
	fIrDAComm->BackEnable();
}

#pragma mark ==== Really silly packet size FIFO ====

//
// This is really silly, but we need to keep track
// of packet sizes in a race-condition safe manner.
// at least if we want to try and do flow control
// in a moderately sane manner (todo: explore insanity)
//
#define kSizeQueueLength    (kTinyTPCredit+2)       // if more, then should never ever run out
QElem   gSizeQueueArray[kSizeQueueLength];          // our queue elements fit in std QElem
QHdr    gAvailSizeQueue;            // the free list
QHdr    gSizeQueue;                 // the "packet size" fifo

void
InitSizeQueue(void)
{
    int i;
    
    gAvailSizeQueue.qFlags = gSizeQueue.qFlags = 0;
    gAvailSizeQueue.qHead  = gSizeQueue.qHead = nil;
    gAvailSizeQueue.qTail  = gSizeQueue.qTail = nil;
    for (i = 0 ; i < kSizeQueueLength; i++) {
	Enqueue(&gSizeQueueArray[i], &gAvailSizeQueue);
    }
}

//
// caller has a new packet, the size of which we
// want to save in a fifo for flow control.
// Can you say "excessively complex"?
//
void
AddPacketSize(short length)
{
    QElemPtr    q;
    IrDAErr     err;
    
    q = gAvailSizeQueue.qHead;
    require(q, Fail);                   // really shouldn't happen
    
    err = Dequeue(q, &gAvailSizeQueue);
    nrequire(err, Fail);
	
    q->qLink = nil;
    q->qType = 123;
    q->qData[0] = length;
    Enqueue(q, &gSizeQueue);

Fail:
    return;
}

//
// caller wants to know the size of the first packet 
//
short
FirstPacketSize(void)
{
    QElemPtr    q;
    q = gSizeQueue.qHead;
    if (q == nil) return 0;
    return q->qData[0];
}

//
// caller has consumed some, but not all, of the 
// data in the first packet.  adjust the length
//
void
ShrinkFirstPacketSize(short length)
{
    QElemPtr    q;
    q = gSizeQueue.qHead;
    require(q, Fail);
    q->qData[0] -= length;
    
Fail:
    return;
}

// caller has consumed first packet (at least)
// so pull it off the size queue and add it 
// back to the free list
void
NukeFirstPacketSize(void)
{
    QElemPtr    q;
    IrDAErr     err;
    
    q = gSizeQueue.qHead;
    require(q, Fail);
    
    err = Dequeue(q, &gSizeQueue);
    nrequire(err, Fail);
    
    Enqueue(q, &gAvailSizeQueue);
    
Fail:   
    return;
}


#if (hasTracing > 0 && hasIrCommTracing > 1)

void IrDALogData(int msg, UInt8 *buf, int count)    // log data into irdalog
{
    UInt32  x = 0;
    int     i = 0;      // count of bytes in x
    
    while (count-- > 0) {
	x = x << 8;
	x |= *buf++;
	if (++i == 4) {
	    XTRACE(msg, x >> 16, x);
	    i = 0;
	    x = 0;
	}
    }
    if (i) XTRACE(msg, x >> 16, x);
}
    
#endif  
    

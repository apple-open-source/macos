/*
    File:       ttp3.c

    Contains:   Most of the real work of TinyTP.  Checking rd/wr queues et al.

*/

#include "ttp.h"
#include "ttppdu.h"
#include "CBufferSegment.h"

int queueDepth(QHdr *qh);       // debugging ... return depth of a queue

#if (hasTracing > 0 && hasTTP3Tracing > 0)

enum IrTinyTP3TraceCodes
{
    kAppendTail = 1,
    kFlushQueue,
    kGetSegment,
    kCheckTheQs,
    kCheckTheQs2,
    kSendDataless,
    kCheckTxQ,
    kCheckRxQ,
    kReassemble,
    kGotSmallPacket,
    kPendingPut,
    kCurrentSendCredit
};

static
EventTraceCauseDesc gTraceEvents[] = {
    {kAppendTail,       "TinyTP3: append tail"},
    {kFlushQueue,       "TinyTP3: flush queue"},
    {kGetSegment,       "TinyTP3: Get segment"},
    {kCheckTheQs,       "TinyTP3: Check Qs"},
    {kCheckTheQs2,      "TinyTP3: Qs. Remote/Avail credit"},
    {kSendDataless,     "TinyTP3: Send Dataless"},
    {kCheckTxQ,         "TinyTP3: Check TxQ"},
    {kCheckRxQ,         "TinyTP3: Check RxQ"},
    {kReassemble,       "TinyTP3: Reassemble"},
    {kGotSmallPacket,   "TinyTP3: Passing up small packet"},
    {kPendingPut,       "TinyTP3: put delay pending complete"},
    {kCurrentSendCredit, "TinyTP3: current SendCredit"}
};

#define XTRACE(x, y, z)  IrDALogAdd( x, y, z, gTraceEvents, true ) 

#else
    #define XTRACE(x, y, z) ((void)0)
#endif


void
TTinyTP::AppendTail(QHdr *qhdr, TTPMsg msg, int r, TTPBuf *buf) 
{   TTPq    *tq;
    IrDAErr err;

    require(qhdr, Fail);
    /***
    if (0) {
	if (qhdr == &RxQueue) XTRACE(kAppendTail, 1, msg); 
	else if (qhdr == &TxQueue) XTRACE(kAppendTail, 2, msg);
	else {
	    DebugStr("\pLogic err. TTinyTP::AppendTail called w/unknown queue hdr");
	    return;
	}
    }
    ***/
    
    tq = (TTPq *)AvailQueue.qHead;      // get available
    require(tq, Fail);                  // what to do if run out?

    err = Dequeue((QElem *)tq, &AvailQueue);
    nrequire(err, Fail);
    
    tq->qLink = nil;
    tq->qType = msg;
    tq->reason = r;
    tq->buf = buf;
    Enqueue((QElem *)tq, qhdr);     // put it on the real queue
    
    XTRACE(kAppendTail, 3, queueDepth(qhdr));
Fail:
    return;
}

void
TTinyTP::FlushQueue(QHdr *q)
{   TTPq    *tq;

    check(q);
    if (q == &RxQueue) {XTRACE(kFlushQueue, 1, queueDepth(q));}
    else if (q == &TxQueue) {XTRACE(kFlushQueue, 2, queueDepth(q));}
    else DebugLog("Logic err. TTinyTP::FlushQueue called w/unknown queue hdr");

    while ((tq = (TTPq *)q->qHead) != 0) {
	IrDAErr err;
	err = Dequeue((QElem *)tq, q);
	ncheck(err);
	if (tq->buf != 0) BufFree(tq->buf); // be careful ...
	tq->buf = nil;
	tq->qLink = nil;
	Enqueue((QElem *)tq, &AvailQueue);
    }
}

// Get the N'th segment out of the large packet that's about to
// be sent.  Index is 1 thru Number packets.  All but the last
// packet will be MaxSegSize long.
//                                      Rewrite
TTPBuf *
TTinyTP::GetSegment(int i, TTPBuf *ttpbuf)      // extract segment
{
    TTPBuf *seg;        // the segment wrapper
    unsigned char *newBase;     // pointer to start of segment
    int length;                 // length of the segment

    check(ttpbuf);
    XTRACE(kGetSegment, i, ttpbuf->GetSize());
    
    i = i - 1;                      // turn to zero-based index
    length = BufSize(ttpbuf) - (i * MaxSegSize);        // amount left
    if (length > MaxSegSize)        // if too much for one packet
	length = MaxSegSize;        // max length of a segment
    check(length);
    
    newBase = BufBase(ttpbuf) + (i * MaxSegSize);       // starting point
    seg = CBufferSegment::New(newBase, length);     // wrap a cbuffer around the block
    check(seg);
    
    return seg;
}

//
//************** Queue checkers
//
#define QueueEmpty(x) (x.qHead==nil)
#define QueueNotEmpty(x) (x.qHead != nil)
#define LowThreshold  2         // Check this *********
void
TTinyTP::CheckTheQueues()
{
    TTinyTP *curTTP;
    
    curTTP = this;
    {
	// start of 'original' CheckTheQueues'
	Boolean check = true;       // true until no changes after queue checks
	int rxDepth, txDepth;               // debug only
	rxDepth = queueDepth(&curTTP->RxQueue);     // debug only
	txDepth = queueDepth(&curTTP->TxQueue);     // debug only
	//if (txDepth > 10)     // shouldn't happen now
	//  DebugStr("\pTx Queue wedged");
	
	XTRACE(kCheckTheQs, rxDepth, txDepth);
	XTRACE(kCheckTheQs2, curTTP->RemoteCredit, curTTP->AvailCredit);
	XTRACE(kCurrentSendCredit, curTTP->initial_credit, curTTP->SendCredit);
	
	while (check) {         // loop until no state changes
	    check = false;
		    // if nothing to send or can't send due to lack of credit AND
	    if ((QueueEmpty(curTTP->TxQueue) || curTTP->SendCredit == 0) &&
		    // remote is almost dry or we have "lots" to extend AND
		(curTTP->RemoteCredit <= LowThreshold || curTTP->AvailCredit > 3) &&
		    // we have something to extend AND we're connnected
		curTTP->AvailCredit > 0 && curTTP->Connected) {
		    curTTP->SendDataless();     // send dataless flow PDU
		    // check = true;    // goes right on out, not q'd
		}
	    // hmm, if lots is q'd, should I empty one before going
	    // to the next?
	    if (QueueNotEmpty(curTTP->TxQueue) &&
		    curTTP->CheckTxQueue()) check = true;
    
	    if (QueueNotEmpty(curTTP->RxQueue) &&
		    curTTP->CheckRxQueue()) check = true;
	}
    }
}

// Send a dataless data PDU
// Used to advance credit in the absence of any userdata to send
void
TTinyTP::SendDataless()
{   TTPBuf *data;
    int n;
    
    XTRACE(kSendDataless, RemoteCredit, AvailCredit);
    
    n = AvailCredit;
    AvailCredit = 0;
    if (n > 127) { AvailCredit = n - 127; n = 127; }
    RemoteCredit += n;
    data = ttp_pdu_data(false, n, NULL);
    require(data, NoMem);
    this->DataPut(data);            // tell LSAP to send it off
    return;
NoMem:
    RemoteCredit -= n;      // didn't make it, back off unsent credit
    AvailCredit  += n;
    return;
}

Boolean
TTinyTP::CheckTxQueue()
{   TTPq *tq;
    IrDAErr err;

    XTRACE(kCheckTxQ, 0, 0);
    
    tq = (TTPq *)TxQueue.qHead;     // get first off the Xmit list
    if (tq == 0) return false;      // nothing there, nothing changed

    // First case -- disconnect request was queued (on the
    // transmit queue, allowing pending puts to complete first
    if (tq->qType == TTP_Disconnect) {
	Connected = false;      // no longer connected state
	err = Dequeue((QElem *)tq, &TxQueue);   // remove current elem
	ncheck(err);
	FlushQueue(&TxQueue);       // should be empty anyway
	FlushQueue(&RxQueue);
	txQDepth = 0;
	
	// send off disconnect request, with reason=UserRequested
	//do_lmp_disconnect_request(this, kIrUserRequestedDisconnect, tq->buf);
	this->Disconnect();         // tell LSAP to hangup
	
	tq->qLink = nil;
	Enqueue((QElem *)tq, &AvailQueue);  // put back on avail list
	return true;            // something changed
    }
    
    // Second case -- data queued and available send credit
    if ((tq->qType == TTP_Segment || tq->qType == TTP_Segment_Last) &&
	SendCredit > 0) {
	    int n;
	    Boolean m;
			
	    n = AvailCredit;
	    if (n > 127) {
		AvailCredit = n - 127;
		n = 127;
	    } else AvailCredit = 0;
	    RemoteCredit += n;
	    err = Dequeue((QElem *)tq, &TxQueue);   // remove current elem
	    ncheck(err);
	    txQDepth--;
	    SendCredit--;           // we're consuming a transmit credit
	    
	    //*** DO lmp_data_request( data ttp pdu, deltacredit = n, userdata = data)
	    m = (tq->qType == TTP_Segment);     // set "More" flag
	    ttp_pdu_data_setbyte(m, n, tq->buf);    // update the ttp flags 
	    this->DataPut(tq->buf);                 // send off the data PDU
				// we free it when we get back put complete
	    tq->buf = nil;
	    tq->qLink = nil;
	    Enqueue((QElem *)tq, &AvailQueue);  // put back on avail list
	    return true;
    }
    return false;           // didn't do anything this time through
}

Boolean
TTinyTP::CheckRxQueue()
{   TTPq *tq;
    IrDAErr err;

    XTRACE(kCheckRxQ, 0, 0);

    tq = (TTPq *)RxQueue.qHead;     // get first off the Input list
    if (tq == 0) return false;      // nothing there, nothing changed
    if (RxSdu.busy) return false;   // factored out, can't do anything if blocked
    
    if (RxMaxSduSize == 0 &&        // If SAR is *not* being used
	(tq->qType == TTP_Segment ||        // and M=1 or M=0
	 tq->qType == TTP_Segment_Last)) {  // (spec says to ignore M bit if !SAR)
					    // *** send off tq->buf directly
    
	err = Dequeue((QElem *)tq, &RxQueue);   // remove current elem
	ncheck(err);
	
	TTPDataIndication(tq->buf, TTP_Data_Ok);    // virtual callback
						    // CLIENT frees the packet buffer
	tq->buf = nil;
	tq->qLink = nil;
	Enqueue((QElem *)tq, &AvailQueue);      // put back on avail list
	//AvailCredit++;                            // ready for some more (CHECK)
	return true;
    }
    
    // If it's a data packet, we know SAR is in force below here
    
    // jdg added: check to see if SAR is in effect, but not needed!
    // if last segment && no reassembly buffer ....
    if ((tq->qType == TTP_Segment_Last) &&      // if "last" buffer of a sequence
	(RxSdu.sarbuf == nil)) {                //  and no previous packet
	    check (tq->buf);
	    XTRACE(kGotSmallPacket, 0, BufSize(tq->buf));
	    
	    err = Dequeue((QElem *)tq, &RxQueue);   // remove current elem
	    ncheck(err);
	    
	    TTPDataIndication(tq->buf, TTP_Data_Ok);    // just pass it up!
	    tq->buf = nil;                              // CLIENT must free it
	    tq->qLink = nil;                        // finally, done w/rx q element
	    Enqueue((QElem *)tq, &AvailQueue);      // put back on avail list
	    //AvailCredit++;                            // ready for some more
	    return true;
    }
     
     // Ok, if we have data here, then we really are doing SAR, should
     // have a big packet ...
				    // Data (terminal or not)
    if ((tq->qType == TTP_Segment) ||       // data w/more to follow
	(tq->qType == TTP_Segment_Last)) {  // data w/out more to follow
	
	    err = Dequeue((QElem *)tq, &RxQueue);   // remove current elem
	    ncheck(err);

	    if (RxSdu.sarbuf == nil) {      // first time we've needed an SAR?
		check( RxMaxSduSize );
		RxSdu.sarbuf = BufAlloc(RxMaxSduSize);
		require(RxSdu.sarbuf, NoMem);       // drop packet
	    }
	    // ok, double check that the thing will fit
	    if ((BufUsed(RxSdu.sarbuf) + BufSize(tq->buf)) <= RxMaxSduSize) {
		Reassemble(RxSdu.sarbuf, tq->buf);
		BufFree(tq->buf);               // done with the partial buffer
		AvailCredit++;                  // new style //** check this.
	    }
	    // else drop it on the floor!  spec error here????
	    else DebugLog("TinyTP3: Very confused in check RxBuffer");
	    
	    if (tq->qType == TTP_Segment_Last) {    // if last buffer in segment
						//**** send it up, status = ok
		BufHideRest(RxSdu.sarbuf);          // ok, set buffer size & rewind
		TTPDataIndication(RxSdu.sarbuf, TTP_Data_Ok);   // virtual callback
							// CLIENT must free buffer
		RxSdu.sarbuf = nil;                 // no longer have sar buffer
						    //   am giving it to the client
	    }
    NoMem:              
	    tq->buf = nil;
	    tq->qLink = nil;                        // finally, done w/rx q element
	    Enqueue((QElem *)tq, &AvailQueue);      // put back on avail list
	    //AvailCredit++;                            // ready for some more
	    return true;
	}

    
    if (tq->qType == TTP_Disconnect) {
	    err = Dequeue((QElem *)tq, &RxQueue);   // remove current elem
	    ncheck(err);
	    FlushQueue(&RxQueue);                   // should be empty already
	    ///*** do disconnect indication, reason=r, data=data
	    TTPDisconnectIndication(tq->reason, tq->buf);   // virtual callback
	    tq->buf = nil;                              // CLIENT frees buffer
	    tq->qLink = nil;
	    Enqueue((QElem *)tq, &AvailQueue);      // put back on avail list
	    return true;
    }
    
    return false;           // fell though, nothing done
}

void
TTinyTP::Reassemble(TTPBuf *dest, TTPBuf *src)
{
    check(dest);
    check(src);
    XTRACE(kReassemble, BufSize(src), BufUsed(dest));
    
    // copy from src to end of dest -- REWRITE!!
    int len;
    len = BufSize(src);
    while (len--) {
	BufPut(dest, BufGet(src));
    }
}

int
queueDepth(QHdr *qh)
{
    int count = 0;
    QElemPtr   link;
    link = qh->qHead;
    while (link) {
	count++;
	link = link->qLink;
    }
    return count;
}

//
// Temp until something better is found
//
// Note I don't need the atomic stuff that I did under OS-9
//
IrDAErr Enqueue(QElemPtr qElement, QHdrPtr qHeader)
{
    require(qHeader, Fail);
    require(qElement, Fail);
    
    // first, check and verify it's not already on the list
    if (1) {
	QElemPtr t;
	for (t = qHeader->qHead ; t != nil; t = t->qLink)
	    require(t != qElement, Fail);
    }
    
    qElement->qLink = qHeader->qHead;       // put new one at front
    qHeader->qHead = qElement;
    if (qHeader->qTail == nil)              // if only one, then new tail
	qHeader->qTail = qElement;
    
    return noErr;
    
Fail:
    return kIrDAErrGeneric;
}

IrDAErr Dequeue(QElemPtr qElement, QHdrPtr qHeader)
{
    QElemPtr t, prev;
    int old_depth, new_depth;   // debug

    require(qHeader, Fail);
    require(qElement, Fail);
    prev = nil;
    
    old_depth = queueDepth(qHeader);
    
    for (t = qHeader->qHead; t != nil; t = t->qLink) {
	if (t == qElement) {        // found it
	    if (prev)               // if not first
		prev->qLink = t->qLink; // remove from chain
	    else                    // else has new head
		qHeader->qHead = t->qLink;
	    
	    if (qHeader->qTail == t)        // if we were the old tail
		qHeader->qTail = prev;      // then there's a new tail
	    
	    {
		new_depth = queueDepth(qHeader);
		check(new_depth == old_depth -1);
	    }
	    return noErr;
	}
	prev = t;
    }

Fail:
    return kIrDAErrGeneric; 
}
















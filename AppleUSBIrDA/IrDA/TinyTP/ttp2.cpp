/*
    File:       ttp2.cpp

    Contains:   Tiny TP Implementations of callbacks by CIrLSAP


*/

#include "ttp.h"
#include "ttppdu.h"
#include "CBufferSegment.h"

// Contents:   this module contains all of the TinyTP callbacks that
//              are invoked (one way or another) by CIrLSAP when something
//              happens worth telling us about

#if (hasTracing > 0 && hasTTP2Tracing > 0)

enum IrTinyTP2TraceCodes
{
    kHandleConnectIndication = 1,
    kHandleConnectConfirm,
    kHandleDisconnectIndication,
    kHandleDataIndication,
    kDataLessPacket
};

static
EventTraceCauseDesc gTraceEvents[] = {
    {kHandleConnectIndication,      "TinyTP2: connect indication"},
    {kHandleConnectConfirm,         "TinyTP2: connect confirm"},
    {kHandleDisconnectIndication,   "TinyTP2: disconnect indication"},
    {kHandleDataIndication,         "TinyTP2: data indication"},
    {kDataLessPacket,               "TinyTP2: dataless indication"}
};

#define XTRACE(x, y, z)  IrDALogAdd( x, y, z, gTraceEvents, true ) 

#else
    #define XTRACE(x, y, z) ((void)0)
#endif


void
TTinyTP::TTPHandleConnectIndication (
	IrDAErr result,
	TTPSAP  sap,                        // calling TTP SAP
	TIrQOS  *ourQOS,                    // our QoS
	TIrQOS  *peerQOS,                   // peer QoS
	TTPBuf  *userData)                  // calling UserData
{
    unsigned char plist[100];       // enough for me ...
    int     p;                      // parameters?
    int     n;                      // initial credit
    
    XTRACE(kHandleConnectIndication, 0, result);
    if (result != noErr) {
	TTPConnectIndication(result, sap, ourQOS, peerQOS, this->TxMaxSduSize, userData);   // virtual callback
	return;
    }
    
    ttp_pdu_connect_parse(userData, &p, &n, &plist[0]); // strips out ttp pdu params
    this->SendCredit = n;
    this->TxMaxSduSize = 0;
    //this->MaxSegSize = MaxTxIrLapDataSize - 3;
    this->MaxSegSize = peerQOS->GetDataSize() - 3;      // get max send pkt size
    
    if (p == 1) {
	UInt32 value;
	if (ttp_pdu_connect_get_max_sdu_size(plist, &value))
	    this->TxMaxSduSize = value;
    }
    
    TTPConnectIndication(result, sap, ourQOS, peerQOS, this->TxMaxSduSize, userData);   // virtual callback
    //BufFree(userData);                                // we'll free up the userData now ...
    // nope -- this buffer was passed to us in a listen request, let our client free it too.
}

void
TTinyTP::TTPHandleConnectConfirm (
	TTPSAP  sap,                        // calling TTP SAP
	TIrQOS  *ourQOS,                    // our QoS
	TIrQOS  *peerQOS,                   // peer QoS
	TTPBuf  *userData)                  // calling UserData
{
    unsigned char plist[100];       // enough for me ...
    int     p;                      // parameters?
    int     n;                      // initial credit
    
    XTRACE(kHandleConnectConfirm, 0, 0);
    
    ttp_pdu_connect_parse(userData, &p, &n, &plist[0]); // strips out ttp pdu params
    this->SendCredit = n;
    this->TxMaxSduSize = 0;
    //this->MaxSegSize = MaxTxIrLapDataSize - 3;
    this->MaxSegSize = peerQOS->GetDataSize() - 3;      // get max send pkt size

    if (p == 1) {
	UInt32 value;
	if (ttp_pdu_connect_get_max_sdu_size(plist, &value))
	    this->TxMaxSduSize = value;
	XTRACE(kHandleConnectConfirm, 1, value);
    }
    
    this->Connected = true;         // change state before my callback does more work!


    // this is a rather different ... as soon as the TTP session is open, hang a read
    // on the connection.  every time a read completes, pass the buffer up, allocate
    // a new one, and hang another read out.   TTP Client is responsible for eventually
    // freeing the buffers passed to it via DataIndication().
    {   CBufferSegment *getBuf;
	getBuf = BufAlloc(2048+5);      // need up to max lap size
	require(getBuf, NoMem);         // not a lot I can do w/out a read buffer
	this->DataGet(getBuf);          // start the read
    }

    // Now that the read is pending, tell the client .... (was in other order)
    TTPConnectConfirm(sap, ourQOS, peerQOS, this->TxMaxSduSize, userData);  // virtual callback to ttp client
    BufFree(userData);          // free the connect buffer allocated by PDU Connect
NoMem:
    return;
}

void
TTinyTP::TTPHandleDisconnectIndication (
	int     reason,                     // passed up from IrLMP
	TTPBuf  *userData)
{
    XTRACE(kHandleDisconnectIndication, 0, 0);
    
    this->Connected = false;
    this->FlushQueue(&(this->TxQueue));
    txQDepth = 0;
    this->AppendTail(&(this->RxQueue), TTP_Disconnect, reason, userData);
}

void
TTinyTP::TTPHandleDataIndication (
    TTPBuf  *userData)                  // data read
{
    int credit;
    Boolean m;

    check(userData);
    {   int position, bufsize;
	position = BufUsed(userData);       // should be zero
	bufsize  = BufSize(userData);       // amount read
	XTRACE(kHandleDataIndication, position, bufsize);
    }
    
    ttp_pdu_data_parse(userData, &m, &credit);  // strips out ttp overhead byte
    this->SendCredit += credit;
    if (SendCredit > 30) SendCredit = 30;       // FIXME: temp workaround for HP runaway credits
					// WARNING: HP tells us that Microsoft believes in
					// extending lots and lots of credits, and relies on
					// lap-layer flow control to survive.
    
    if (credit > 0)                     // if we've been given more credits from IrLan box
	TTPBackEnable();                // then tell clients flow-control state has changed

    if (BufSize(userData) > 0) {
	XTRACE(kHandleDataIndication, m, credit);           // log what we parsed
	this->RemoteCredit--;
	if (m == false)         // if no More data
	    this->AppendTail(&(this->RxQueue), TTP_Segment_Last, 0, userData);
	else                    // else have More data
	    this->AppendTail(&(this->RxQueue), TTP_Segment, 0, userData);
    }
    else {
	XTRACE(kDataLessPacket, m, credit);     // log we received a dataless packet
	BufFree(userData);                      // we're done with it now...
    }
    
    // Make sure a read is pending at all times .... this could
    // use a better model.  Only post if connected.
    if( this->Connected )
    {   CBufferSegment *getBuf;
	getBuf = BufAlloc(2048+5);      // need up to max lap size
	require(getBuf, NoMem);
	this->DataGet(getBuf);
    }
NoMem:
    return;
}

void
TTinyTP::TTPHandleUDataIndication (
    TTPBuf  *userData)                  // data read
{
    //DebugLog("How'd I get here?  TTinyTP::HandleUDataIndication");
    TTPUDataIndication(userData);
}

void
TTinyTP::TTPHandleAcceptComplete(IrDAErr result, TTPBuf *userdata)
{
    check(userdata);
    BufFree(userdata);          // free buffer alloc'd by ttp.c in accept routine
    TTPAcceptDoneIndication(result);    // just result, no buffer, since we copied clients during accept
}


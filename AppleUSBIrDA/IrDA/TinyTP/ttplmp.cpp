/*
    File:       ttplmp.cpp

    Contains:   TinyTP's impl of the virtual callbacks in CIrLSAP

*/

#include "ttp.h"
#include "ttppdu.h"
#include "CBufferSegment.h"
#include "IrDALog.h"

#if (hasTracing > 0 && hasTTPLMPTracing > 0)

enum TTPLmpTraceCodes
{
    kDiscoverComplete = 1,
    kLSAPLookupComplete,
    kConnected,
    kPutComplete,
    kGetComplete,
    kDisconnected,
    kConnectError,
    kListenComplete,
    kAcceptComplete,
    
    kQOSrate,
    kQOStimeout,
    kQOSdatasize, 
    kQOSwindowsize,
    kQOSbofs,
    kQOSminturn,
    kQOSlinkdisc
    
    
}TTPLMPTraceCodes;

static
EventTraceCauseDesc gTraceEvents[] = {
    {kDiscoverComplete,             "ttplmp: discovery complete"},
    {kLSAPLookupComplete,           "ttplmp: LSAP lookup complete"},
    {kConnected,                    "ttplmp: Connect complete, buf posn=, size="},
    {kPutComplete,                  "ttplmp: put complete, result="},
    {kGetComplete,                  "ttplmp: get complete, result=, size="},
    {kDisconnected,                 "ttplmp: disconnect event"},
    {kConnectError,                 "ttplmp: connect failed, result="},
    {kListenComplete,               "ttplmp: listen complete, peer sap=, size="},
    {kAcceptComplete,               "ttplmp: accept complete, result="},

    {kQOSrate,                      "ttplmp: qos bitrate"},
    {kQOStimeout,                   "ttplmp: qos timeout"},
    {kQOSdatasize,                  "ttplmp: qos datasize"},
    {kQOSwindowsize,                "ttplmp: qos windowsize"},
    {kQOSbofs,                      "ttplmp: qos bofs"},
    {kQOSminturn,                   "ttplmp: qos min turn"},
    {kQOSlinkdisc,                  "ttplmp: qos link disc"}


};

#define XTRACE(x, y, z)  IrDALogAdd( x, y, (uintptr_t)z & 0xffff, gTraceEvents, true )  

#else
#define XTRACE(x, y, z) ((void)0)
#endif

unsigned const char *className = (unsigned char *)"IrLan";

#if (hasTracing > 0 && hasTTPLMPTracing > 1)
void    LogOneQOS(int id, TIrQOS *q);
void    LogQOSData(TIrQOS *myQOS, TIrQOS *peerQOS);
#else
#define LogQOSData(x, y) ((void)0);
#endif

//Boolean putPending = false;           // one per entire system!

//////////////////////////////////////////////////////////////////////////////////
// the following are called by CIrLSAP via virtual function overloading ...
//////////////////////////////////////////////////////////////////////////////////


void 
TTinyTP::DiscoverComplete(                  // discovery has finished
	    UInt32  numFound,
	    IrDAErr result      )
{   
    XTRACE( kDiscoverComplete, numFound, 0 );
    discoverPending = false;                // allow another discover request
    
    // TinyTP doesn't do anything with this, just pass along to our client
    TTPDiscoverComplete(numFound, result);
    this->CheckTheQueues();                 // see if we have more work to do
}


void
TTinyTP::LSAPLookupComplete (       // an IAS query has finished
	IrDAErr result,
	UInt32  peerLSAPId)
{
    XTRACE (kLSAPLookupComplete, result, peerLSAPId);

    TTPLookupComplete(result, peerLSAPId);  // virtual callback
    this->CheckTheQueues();                 // see if we have more work to do
}


void
TTinyTP::ConnectComplete (
    IrDAErr result,
    TIrQOS *myQOS,
    TIrQOS *peerQOS,
    CBufferSegment *data)           // up to my client to free this data
{
#pragma unused (myQOS, peerQOS)
    
    //check(data);              // might be nil if connect failed
    
    if (result == noErr) {      // if the connect worked
	check(data);                    // should always have data on a TTP connect
	LogQOSData(myQOS, peerQOS);     // only if tracing > 1
	XTRACE (kConnected, data->Position(), data->GetSize());
	BufHideRest(data);          // set eof pointer, rewind
	TTPHandleConnectConfirm(fPeerLSAPId, myQOS, peerQOS, data);     // let TTP know we have connect
    }
    else {
	XTRACE (kConnectError, result >> 16, result);
	//Disconnect();                         // tell CIrLSAP to disconnect (this is a nop and vanishes)
	TTPHandleDisconnectIndication(0, nil);      // foo, fake a disconnect
    }
    this->CheckTheQueues();                 // see if we have more work to do
}

void
TTinyTP::DisconnectComplete (void)
{
    XTRACE (kDisconnected, 0, 0);
    TTPHandleDisconnectIndication(0, nil);      // sigh, where's my reason?
    this->CheckTheQueues();                 // see if we have more work to do
}   
		
void
TTinyTP::DataPutComplete (
		IrDAErr result,
		CBufferSegment *data)       // TTP "copy" of data ... foo
{
    check(data);
    XTRACE (kPutComplete, 0, result);
    
    if (result) {                   //// Try and handle hangups
	if (Connected)                  // if we think we're connected
	    DoDisconnectRequest(nil);       // force a disconnect on error
    }
    
    BufFree(data);                          // put done, don't need the buffer anymore
    this->CheckTheQueues();                 // see if we have more work to do
}

void
TTinyTP::DataGetComplete (
		IrDAErr result,
		CBufferSegment *data)
{
    XTRACE (kGetComplete, result, data->GetSize());

    if (result == noErr) {          // if the read worked
	check(data);                    // then, ah, there should be data
	BufHideRest(data);              // set buffer size and rewind
	TTPHandleDataIndication(data);      // pass up to TTP & TTP Client
    }
    else {
	if (Connected) DoDisconnectRequest(nil);    // treat err as disconnect request
	BufFree(data);                              // get failed, don't need the buffer anymore
    }
    
    this->CheckTheQueues();             // see if we have more work to do
}


void
TTinyTP::ListenComplete (
		IrDAErr result,
		UInt32  peerAddr,
		UInt32  peerLSAPId, 
		TIrQOS  *myQOS,                 // my qos
		TIrQOS  *peerQOS,               // peer's qos
		CBufferSegment *data)
{
    XTRACE (kListenComplete, 0, this);
    XTRACE (kListenComplete, peerLSAPId, result);
    check(data);                            // should always have data on a listen complete

    fPeerAddr = peerAddr;                   // hang out to the address of our caller
    fPeerSAP  = peerLSAPId;
    
    if (result == noErr) {                  // if the listen is ok
	BufHideRest(data);                  // set eof pointer, rewind
    }
    TTPHandleConnectIndication(result, peerLSAPId, myQOS, peerQOS, data);   // let TTP know listen finished
	
    this->CheckTheQueues();                 // see if we have more work to do
}

//
// BOGUS: there isn't an accept complete return to our caller, so their
// buffer has to be freed by us instead of by them.  BOGUS.  Pass it up.
//
void
TTinyTP::AcceptComplete (
		IrDAErr result,
		CBufferSegment *data)
{
    XTRACE(kAcceptComplete, 0, result);
    check(data);                            // should always have a buffer, it's allocd by DoConnectResponse()
    //BufFree(data);                        // free the connect buffer allocated by PDU Connect
    
    TTPHandleAcceptComplete(result, data);      // pass up the result code and buffer ptr (client frees it now)
    
    if (result == noErr) {              // if we're fully open now
	CBufferSegment *getBuf;
	
	Connected = true;               // change state before my callback does more work!
	getBuf = BufAlloc(2048+5);      // need up to max lap size
	require(getBuf, NoMem);         // not a lot I can do w/out a read buffer
	DataGet(getBuf);                // start the read
    }
NoMem:
    this->CheckTheQueues();                 // see if we have more work to do
}

void
TTinyTP::CancelGetsComplete(IrDAErr result)
{
#pragma unused(result)
    // never gets here
}

void
TTinyTP::CancelPutsComplete(IrDAErr result)
{
#pragma unused(result)
    // never gets here
}


#pragma mark ---- QoS debug logging

#if (hasTracing > 0 && hasTTPLMPTracing > 1)
	
void
LogOneQOS(int id, TIrQOS *q)
{
    check(q);

    BitRate     rate = q->GetBaudRate();
    XTRACE(kQOSrate, id, rate);

    TTimeout    timeout = q->GetMaxTurnAroundTime();
    XTRACE(kQOStimeout, id, timeout);

    ULong       datasize = q->GetDataSize();
    XTRACE(kQOSdatasize, id, datasize);

    ULong       windowsize = q->GetWindowSize();
    XTRACE(kQOSwindowsize, id, windowsize);

    ULong       bofs = q->GetExtraBOFs();
    XTRACE(kQOSbofs, id, bofs);

    TTimeout    minturn = q->GetMinTurnAroundTime();
    XTRACE(kQOSminturn, id, minturn);

    TTimeout    linkdisc = q->GetLinkDiscThresholdTime();
    XTRACE(kQOSlinkdisc, id, linkdisc);
    
}

void
LogQOSData(TIrQOS *myQOS, TIrQOS *peerQOS)
{
    LogOneQOS(1, myQOS);
    LogOneQOS(2, peerQOS);
}

#endif // hasTTPLMPTracing > 1

/*
    File:       IrLAP.cpp

    Contains:   Implementation of IrLAP


*/

#define IRDA_TEST_FRAME_SUPPORTED   1           // required I think


#include "IrLAP.h"
#include "IrLMP.h"
#include "CList.h"
#include "IrGlue.h"
#include "CListIterator.h"
#include "IrDscInfo.h"
#include "IrQOS.h"
#include "IrDiscovery.h"
#include "IrLSAPConn.h"
#include "CIrDevice.h"

#define forMac 1    // TEMP TEMP TEMP -- clean up the code

const UByte IrSlotCounts[4] = {1, 6, 8, 16};        // valid discovery slots


#if (hasTracing > 0 && hasLAPTracing > 0)

enum IrLAPTraceCodes
{
    kLogCreate = 1,
    kLogFree,
    kLogInit,
    kLogInit2,

    kUnexpectedEvent,
    kLogStateEvent,

    kNDMTimeoutEvent,
    kNDMRecdInputEvent,
    kNDMDiscoveryEvent,
    kNDMConnectEvent,
    kNDMListenEvent,
    kNDMDisconnectEvent,

    kQueryTimeoutEvent,
    kQueryRecdInputEvent,
    kQueryOutputDoneEvent,
    kQueryDisconnectEvent,

    kConnectBackoffTimeoutEvent,
    kConnectFinalTimeoutEvent,
    kConnectRecdInputEvent,
    kConnectOutputDoneEvent,
    kConnectDisconnectEvent,
    kConnectChangeSpeed,            // jdg

    kListenRecdInputEvent,
    kListenOutputDoneEvent,
    kListenDisconnectEvent,
    kListenChangeSpeed,             // jdg
    kOutputUACommandEvent,

    kReplyTimeoutEvent,
    kReplyRecdInputEvent,
    kReplyOutputDoneEvent,
    kReplyDisconnectEvent,
    kReplyDiscoverRequestEvent,     // jdg
    kReplyListenRequestEvent,       // jdg

    kPriReceiveFinalTimeoutEvent,
    kPriReceiveRecdInputEvent,
    kPriReceiveOutputDoneEvent,
    kPriReceivePutDataEvent,
    kPriReceiveDisconnectEvent,
    kPriReceiveClrLocBsyPendEvent,
    kPriReceiveFRMREvent,

    kPriTransmitEnterEvent,
    kPriTransmitPollTimeoutEvent,
    kPriTransmitOutputDoneEvent,
    kPriTransmitPutDataEvent,
    kPriTransmitDisconnectEvent,
    kPriTransmitClrLocBsyPendEvent,

    kPriCloseFinalTimeoutEvent,
    kPriCloseRecdInputEvent,
    kPriCloseOutputDoneEvent,
    kPriCloseDisconnectRequest,

    kSecReceiveWDTimeoutEvent,
    kSecReceiveRecdInputEvent,
    kSecReceiveOutputDoneEvent,
    kSecReceivePutDataEvent,
    kSecReceiveDisconnectEvent,
    kSecReceiveClrLocBsyPendEvent,
    kSecReceiveConnLstnRequest,         // jdg

    kSecTransmitEnterEvent,
    kSecTransmitOutputDoneEvent,
    kSecTransmitPutDataEvent,
    kSecTransmitDisconnectEvent,
    kSecTransmitClrLocBsyPendEvent,
    kSecTransmitConnLstnRequest,        // jdg

    kSecCloseWDTimeoutEvent,
    kSecCloseRecdInputEvent,
    kSecCloseOutputDoneEvent,
    kSecCloseDisconnectRequest,     // jdg

    kDiscoveredDevice,
    kDiscoverXIDCmdEvent,
    kDiscoverXIDRspEvent,

    kQOSSetBaudRateEvent,
    kQOSBufferInfoEvent,
    kQOSLeadInCountEvent,
    kQOSMinTurnAroundEvent,
    kQOSMaxTurnAroundEvent,

    kSetLocalBusyPendingEvent,
    kSetLocalBusyEvent,
    kAbortLocalBusyPendingEvent,
    kClrLocalBusyPendingEvent,
    kClrLocalBusyEvent,

    kNDMDisconnectFwdReply,
    kNDMDisconnectReply,
    kNRMDisconnectReply,
    kNRMDisconnectAsyncReply,
    kNRMDisconnectRequeue,

    kTestFrameReceivedEvent,
    kTestFrameOutputDoneEvent,
    
    kValidPacketReceived,
    kLogPacketDropped,
    kPacketOutput,
    
    kUnexpectedNr,              // jdg
    kInvalidNr,
    kUnexpectedNs,
    kInvalidNs,
    kIgnoringInvalidNrNs,
    kOutputControlFrame,
    kOutputControlFrameRR,
    kOutputDataFrame,
    kOutputDataFrameNotFinal,
    
    kInputControlFrame,
    kInputControlFrameRR,
    kInputControlFrameNotFinal,
    kInputDataFrame,
    kInputDataFrameNotFinal,
    kResendRejectedFrames,
    kProcessISFrame,
    kUpdateNrReceived,
    
    kEnqueueEvent,              // cpd
    kDequeueEventStart,
    kDequeueEventEnd,
    
    kReusingBuffer,
    kUsingDefaultBuffer,
    kLAPAddr,
    
    kInputAborted,              // jdg performance hack
    kConnLstnComplete,
    kRejectRequest,             // jdg - send back a request,
    
    kLogEnteringCloseState,
    
    kLogStartTimer,
    kLogStopTimer,
    kLogTimerComplete,
    
    kLogSuspend,
    kLogResume,
    kLogChangeSpeedCompleteIgnored,
    kLogMyAddr,
    kLogGotAddr,
    kLogStartInput,
    kLogStartInput2,
    kLogStartInput3,
    
    kLogGotData,
    kLogFailedFirst,
    kLogFailedSecond,
    kLogFailedThird,
    
    kLogReleaseInputBuffer,
    kLogReleaseInputBuffer2,
    kLogStartDataRcv1,
    kLogStartDataRcv2,
    kLogReset1,
    kLogReset2,
    kLogFreeGetBuffers1,
    kLogFreeGetBuffers2,
    kLogFreeGetBuffers3,
    kLogFreeGetBuffers4
};

static
EventTraceCauseDesc TraceEvents[] = {
    {kLogCreate,                    "irlap: create obj="},
    {kLogFree,                      "irlap: free obj="},
    {kLogInit,                      "irlap: init, obj="},
    {kLogInit2,                     "irlap: init, my dev addr="},

    {kUnexpectedEvent,              "irlap: unexpected event"},
    {kLogStateEvent,                "irlap: NextState, state=, event="},

    {kNDMTimeoutEvent,              "irlap: NDM media busy timer"},
    {kNDMRecdInputEvent,            "irlap: NDM recvd input"},
    {kNDMDiscoveryEvent,            "irlap: NDM discovery"},
    {kNDMConnectEvent,              "irlap: NDM connect"},
    {kNDMListenEvent,               "irlap: NDM listen"},
    {kNDMDisconnectEvent,           "irlap: NDM disconnect"},

    {kQueryTimeoutEvent,            "irlap: Probe query timer"},
    {kQueryRecdInputEvent,          "irlap: Probe query recvd input"},
    {kQueryOutputDoneEvent,         "irlap: Probe query output done"},
    {kQueryDisconnectEvent,         "irlap: Probe query disconnect"},

    {kConnectBackoffTimeoutEvent,   "irlap: Connect backoff timer"},
    {kConnectFinalTimeoutEvent,     "irlap: Connect F-timer"},
    {kConnectRecdInputEvent,        "irlap: Connect recvd input"},
    {kConnectOutputDoneEvent,       "irlap: Connect output done"},
    {kConnectDisconnectEvent,       "irlap: Connect disconnect"},
    {kConnectChangeSpeed,           "irlap: Connect changed speed"},

    {kListenRecdInputEvent,         "irlap: Listen recvd input"},
    {kListenOutputDoneEvent,        "irlap: Listen output done"},
    {kListenDisconnectEvent,        "irlap: Listen disconnect"},
    {kListenChangeSpeed,            "irlap: Listen changed spped"},
    {kOutputUACommandEvent,         "irlap: OutputUAPacket"},

    {kReplyTimeoutEvent,            "irlap: Probe reply timer"},
    {kReplyRecdInputEvent,          "irlap: Probe reply recvd input"},
    {kReplyOutputDoneEvent,         "irlap: Probe reply output done"},
    {kReplyDisconnectEvent,         "irlap: Probe reply disconnect"},
    {kReplyDiscoverRequestEvent,    "irlap: Probe reply, got discover request"},
    {kReplyListenRequestEvent,      "irlap: Probe reply, got listen request"},

    {kPriReceiveFinalTimeoutEvent,  "irlap: PRecv final timer"},
    {kPriReceiveRecdInputEvent,     "irlap: PRecv recvd input, cmd, Ns|Nr"},
    {kPriReceiveOutputDoneEvent,    "irlap: PRecv output done"},
    {kPriReceivePutDataEvent,       "irlap: PRecv put data"},
    {kPriReceiveDisconnectEvent,    "irlap: PRecv disconnect"},
    {kPriReceiveClrLocBsyPendEvent, "irlap: PRecv clr local busy"},
    {kPriReceiveFRMREvent,          "irlap: PRecv recvd FRMR"},

    {kPriTransmitEnterEvent,        "irlap: PXmit enter"},
    {kPriTransmitPollTimeoutEvent,  "irlap: PXmit poll timer"},
    {kPriTransmitOutputDoneEvent,   "irlap: PXmit output done"},
    {kPriTransmitPutDataEvent,      "irlap: PXmit put data"},
    {kPriTransmitDisconnectEvent,   "irlap: PXmit disconnect"},
    {kPriTransmitClrLocBsyPendEvent,"irlap: PXmit clr local busy"},

    {kPriCloseFinalTimeoutEvent,    "irlap: PClose final timer"},
    {kPriCloseRecdInputEvent,       "irlap: PClose recvd input"},
    {kPriCloseOutputDoneEvent,      "irlap: PClose output done"},
    {kPriCloseDisconnectRequest,    "irlap: PClose deferring disconnect request"},

    {kSecReceiveWDTimeoutEvent,     "irlap: SRecv WD timer"},
    {kSecReceiveRecdInputEvent,     "irlap: SRecv recvd input"},
    {kSecReceiveOutputDoneEvent,    "irlap: SRecv output done"},
    {kSecReceivePutDataEvent,       "irlap: SRecv put data"},
    {kSecReceiveDisconnectEvent,    "irlap: SRecv disconnect"},
    {kSecReceiveClrLocBsyPendEvent, "irlap: SRecv clr local busy"},
    {kSecReceiveConnLstnRequest,    "irlap: SRecv conn/listen request"},

    {kSecTransmitEnterEvent,        "irlap: SXmit enter"},
    {kSecTransmitOutputDoneEvent,   "irlap: SXmit output done"},
    {kSecTransmitPutDataEvent,      "irlap: SXmit put data"},
    {kSecTransmitDisconnectEvent,   "irlap: SXmit disconnect"},
    {kSecTransmitClrLocBsyPendEvent,"irlap: SXmit clr local busy"},
    {kSecTransmitConnLstnRequest,   "irlap: SXmit conn/listen request"},

    {kSecCloseWDTimeoutEvent,       "irlap: SClose WD timer"},
    {kSecCloseRecdInputEvent,       "irlap: SClose recvd input"},
    {kSecCloseOutputDoneEvent,      "irlap: SClose output done"},
    {kSecCloseDisconnectRequest,    "irlap: SClose deferring disconnect request"},

    {kDiscoveredDevice,             "irlap: discovered"},
    {kDiscoverXIDCmdEvent,          "irlap: put XID command"},
    {kDiscoverXIDRspEvent,          "irlap: put XID response"},

    {kQOSSetBaudRateEvent,          "irlap: new baud rate"},
    {kQOSBufferInfoEvent,           "irlap: buf cnt/size"},
    {kQOSLeadInCountEvent,          "irlap: lead in count"},
    {kQOSMinTurnAroundEvent,        "irlap: min turnaround"},
    {kQOSMaxTurnAroundEvent,        "irlap: max turnaround"},

    {kSetLocalBusyPendingEvent,     "irlap: set local busy pending"},
    {kSetLocalBusyEvent,            "irlap: set local busy"},
    {kAbortLocalBusyPendingEvent,   "irlap: set local busy pending aborted"},
    {kClrLocalBusyPendingEvent,     "irlap: clr local busy pending"},
    {kClrLocalBusyEvent,            "irlap: clr local busy"},

    {kNDMDisconnectFwdReply,        "irlap: NDM disconnect forward reply"},
    {kNDMDisconnectReply,           "irlap: NDM disconnect reply"},
    {kNRMDisconnectReply,           "irlap: NRM disconnect reply"},
    {kNRMDisconnectAsyncReply,      "irlap: NRM disconnect async reply"},
    {kNRMDisconnectRequeue,         "irlap: NRM disconnect requeue event"},

    {kTestFrameReceivedEvent,       "irlap: test frame received"},
    {kTestFrameOutputDoneEvent,     "irlap: test frame response sent"},
    
    {kValidPacketReceived,          "irlap: valid packet received"},
    {kLogPacketDropped,             "irlap: packet dropped ** no read pending!"},
    {kPacketOutput,                 "irlap: packet output"},
    
    
    {kUnexpectedNr,                 "irlap: Unexpected NR"},        // jdg
    {kInvalidNr,                    "irlap: Invalid NR"},
    {kUnexpectedNs,                 "irlap: Unexpected NS"},
    {kInvalidNs,                    "irlap: Invalid NS"},
    {kIgnoringInvalidNrNs,          "irlap: IGNORING*** invalid NrNs"},
    {kOutputControlFrame,           "irlap: output non-RR control frame, Cmd, Nr"},
    {kOutputControlFrameRR,         "irlap: output RR control frame, 0, Nr"},
    {kOutputDataFrame,              "irlap: output data frame (Ns, Nr)"},
    {kOutputDataFrameNotFinal,      "irlap: output data frame, not final (Ns,Nr)"},

    {kInputControlFrame,            "irlap: input NON-RR control frame, (CMD,Nr)"},
    {kInputControlFrameRR,          "irlap: input RR control frame, (0, Nr)"},
    {kInputControlFrameNotFinal,    "irlap: input control frame, NOT FINAL (cfield,Nr)"},
    {kInputDataFrame,               "irlap: input data frame (Ns, Nr)"},
    {kInputDataFrameNotFinal,       "irlap: input data frame, non-final (Ns, Nr)"},
    {kResendRejectedFrames,         "irlap: inside resend rejected frames"},
    {kProcessISFrame,               "irlap: process Info/Super frame"},
    {kUpdateNrReceived,             "irlap: update Nr received"},

    {kEnqueueEvent,                 "irlap: Event Queued"},
    {kDequeueEventStart,            "irlap: Event Start"},
    {kDequeueEventEnd,              "irlap: Event End"},

    {kReusingBuffer,                "irlap: start data recv, reusing input buffer"},
    {kUsingDefaultBuffer,           "irlap: start data recv, using DEFAULT input buffer"},

    {kLAPAddr,                      "irlap: Peer Addr: My Addr:"},
    {kInputAborted,                 "irlap: input abort - fast link turnaround after read err"},
    {kConnLstnComplete,             "irlap: conn/lstn complete, peer dev addr="},
    {kRejectRequest,                "irlap: rejecting request"},
    
    {kLogEnteringCloseState,        "irlap: entering close state at #"},
    
    {kLogStartTimer,                "irlap: start timer, (delay, event)"},
    {kLogStopTimer,                 "irlap: stop timer"},
    {kLogTimerComplete,             "irlap: timer complete, 0, event"},
    
    {kLogSuspend,                   "irlap: sleep"},
    {kLogResume,                    "irlap: wakeup"},
    {kLogChangeSpeedCompleteIgnored,    "irlap: change speed complete ignored"},
    {kLogMyAddr,                    "irlap: my addr"},
    {kLogGotAddr,                   "irlap: rcvd addr"},
    {kLogStartInput,                "irlap: start input, buf="},
    {kLogStartInput2,               "irlap: start input, old buf="},
    {kLogStartInput3,               "irlap: start input - keeping memory from leaking"},
    
    {kLogGotData,                   "irlap: gotdata, wanted=, got="},
    {kLogFailedFirst,               "irlap: ** failed ** 1st test"},
    {kLogFailedSecond,              "irlap: ** failed ** second test"},
    {kLogFailedThird,               "irlap: ** failed ** third test"},
    
    {kLogReleaseInputBuffer,        "irlap: release input buffer, buffer="},
    {kLogReleaseInputBuffer2,       "irlap: release input buffer, flags, index"},
    {kLogStartDataRcv1,             "irlap: start data rcv alloc, buffer="},
    {kLogStartDataRcv2,             "irlap: start data rcv alloc, flags, index"},
    {kLogReset1,                    "irlap: reset, buffer alloc mask"},
    {kLogReset2,                    "irlap: reset, states"},
    {kLogFreeGetBuffers1,           "irlap: free get buffers, buffer="},
    {kLogFreeGetBuffers2,           "irlap: free get buffers, flag, index"},
    {kLogFreeGetBuffers3,           "irlap: free get buffers, mask="},
    {kLogFreeGetBuffers4,           "irlap: free get buffers, found fInputBuffer"}

};

    #define XTRACE(x, y, z) IrDALogAdd ( x, y, (uintptr_t)z & 0xffff, TraceEvents, true )
#else
    #define XTRACE(x, y, z) ((void)0)
#endif  // hasTracing && hasLAPTracing

#define GetLMP          (fIrDA->GetLMP())
#define GetDiscovery    (fIrDA->GetDiscovery())
#define GetIrDevice     (fIrDA->GetIrDevice())

void LAPTimerNotifier( UInt32 refCon, UInt32 sig );
Boolean gLastWasFRMR = false;               // TESTING.  Try (once) to recover from rcvd FRMR (shouldn't happen)

//============================ Initialization stuff ==============================

#define super TIrStream
    OSDefineMetaClassAndStructors(TIrLAP, TIrStream);

//--------------------------------------------------------------------------------
//      TIrLAP
//--------------------------------------------------------------------------------
/*static*/
TIrLAP *
TIrLAP::tIrLAP(TIrGlue *irda, TIrQOS *myQOS, TIrQOS* peerQOS)
{
    TIrLAP *obj = new TIrLAP;
    XTRACE(kLogCreate, 0, obj);
    
    if (obj && !obj->Init(irda, myQOS, peerQOS)) {
	obj->release();
	obj = nil;
    }
    return obj;
}

Boolean
TIrLAP::Init(TIrGlue *irda, TIrQOS *myQOS, TIrQOS* peerQOS)
{
    XTRACE(kLogInit, 0, this);
    
    fState = kIrLAPDisconnectedState;       // maybe should have an invalid state here
    fConnAddr = 0;
    fMyDevAddr = 0;
    fDiscoverMaxSlots = 0;
    fDiscoverSlot = 0;
    fDiscoverFlags =0;
    fDiscoverReplied = fDiscoverEnteredReplyState = false;
    fConflictDevAddr = fReplacementDevAddr = fPeerDevAddr= 0;
    
    fCurrentRequest = nil;
    fPendingDisconnect = nil;
    fPendingRequests = nil;
    
    //fMyQOS = fPeerQOS = nil;
    fMyQOS = myQOS;
    fPeerQOS = peerQOS;

    fVr = fVs = fNextToAck = fWindow = 0;
    fConnected = fLocalBusy = fRemoteBusy = false;
    fSetLocalBusyPending = fClrLocalBusyPending = fEnteringCloseState = fRespondingToDisconnect = false;
    
    fWaitingForPollTimer = fHandlingTestFrame = false;
    bzero(&fTestHeader, sizeof(fTestHeader));
    
    fFRMRPending = false;
    fFRMRRejCtrlField = fFRMRMyNrAndNs = fFRMRReasonFlags = 0;
    fRetryCount = fDisconnectWarningLimit = fDisconnectLinkLimit = 0;
    fInitialRetryTime = fDisconnectLinkLimitTime = fBusyCounter = 0;
    fDataRetries = fProtocolErrs = 0;

    fLocalBusyClearedEvent = nil;
    

    fLeadInCount = fMyWindowSize = fPeerWindowSize  = 0;
    fPollTimerTimeout = fFinalTimerTimeout = fWatchdogTimeout = fMinTurnAroundTimeout = 0;  
    fPrimary = fPutReqsPending  = false;
    fNextCmdRspToSend = fLastCmdRsp = 0;


    fRecdCtrl = fRecdCR = fRecdAddr  = 0;
    fRecdPF = fRecdNr = fRecdNs = fRecdCmdRsp = 0;  
    fValidRecdNr = fValidRecdNs = fNrNsFlags  = 0;

    fIOBufferItem = nil;
    
    fInputInProgress = fOutputInProgress = fInBrokenBeam  = false;
    
    fGetBufferAvail = fNumGetBuffers = 0;
    bzero(fGetBuffers, sizeof(fGetBuffers));
    fInputBuffer = nil;
    
    bzero(fNickName, sizeof(fNickName));
    fPendingPutRequests = nil;
    fPutBuffer = nil;
    
    bzero(fPutRequests, sizeof(fPutRequests));


#if (hasTracing > 0 && hasLAPTracing > 0)
	if (!super::Init(irda, TraceEvents, kEnqueueEvent)) return false;
#else
	if (!super::Init(irda)) return false;
#endif
    
    fLeadInCount = kIrDefaultLeadinCount;
    //fNeedNewInputBuffer = true;
	    
    // Get a random device address for my address (range is 1 - FFFFFFFE)
    fMyDevAddr = ( random() % 0xFFFFFFFE + 1 );

    // Make it something different if it happens to be the sniff or probe addresses
    if ((fMyDevAddr == kIrLAPSnifferDevAddr) || (fMyDevAddr == kIrLAPSniffeeDevAddr) || (fMyDevAddr == kIrLAPProbeDevAddr)) {
	fMyDevAddr++;
    }
    XTRACE(kLogInit2, fMyDevAddr>>16, fMyDevAddr);
    require(fMyDevAddr, Fail);          // should not be zero
    
    fIOBufferItem = CBufferSegment::New(3000);  // Fix (size is ignored for now)
    require(fIOBufferItem, Fail);

    fPendingPutRequests = CList::cList();
    require(fPendingPutRequests, Fail);

    fPendingRequests = CList::cList();
    require(fPendingRequests, Fail);
    
    fLocalBusyClearedEvent = fIrDA->GrabEventBlock(kIrLocalBusyClearedEvent, sizeof(TIrEvent)); // review, needed?
    require(fLocalBusyClearedEvent, Fail);
    
    fPutBuffer = TIrLAPPutBuffer::tIrLAPPutBuffer();
    require(fPutBuffer, Fail);

    ResetStats();               // reset stats counters (needed?)
	
    return true;
    
Fail:

    return false;

} // TIrLAP::Init


//--------------------------------------------------------------------------------
//      free
//--------------------------------------------------------------------------------
void
TIrLAP::free()
{
    XTRACE(kLogFree, 0, this);
    
#define FREE(x) { if (x) { (x)->release(); x = nil; } }
	
    // Free any buffer lists that have been allocated
    FreeGetBuffers();
    
    FREE(fPendingPutRequests);
    FREE(fPendingRequests);
    FREE(fIOBufferItem);
    FREE(fPutBuffer);
    
    
    if (fIrDA) {
	//int Review_Event_Free;      // may be able to delete this
	//if (fLocalBusyEvent) {
	//  fIrDA->ReleaseEventBlock(fLocalBusyEvent);
	//  fLocalBusyEvent = nil;
	//}
	if (fLocalBusyClearedEvent) {
	    fIrDA->ReleaseEventBlock(fLocalBusyClearedEvent);
	    fLocalBusyClearedEvent = nil;
	}
    }
	    
    super::free();
    
} // TIrLAP::free



//--------------------------------------------------------------------------------
//      Reset
//--------------------------------------------------------------------------------
void TIrLAP::Reset()
{
    // called by our disconnect complete, and via the glue async disconnect
    // and also by glue's stop code (so state can be other than disconnected)
    XTRACE(kLogReset1, fGetBufferAvail >> 16, fGetBufferAvail);
    XTRACE(kLogReset2, kIrLAPDisconnectedState, fState);
    //check(fState == kIrLAPDisconnectedState);     // also kIrLAPPriCloseState and ?? 
    fState = kIrLAPDisconnectedState;
    FreeGetBuffers();

} // TIrLAP::Reset

// Sleep/wakeup support
void
TIrLAP::Suspend(void)
{
    XTRACE(kLogSuspend, 0, 0);
    fRetryCount = fDisconnectLinkLimit;         // force disconnect on next timeout
}

void
TIrLAP::Resume(void)
{
    XTRACE(kLogResume, 0, 0);
}


//--------------------------------------------------------------------------------
//      GetNickName
//--------------------------------------------------------------------------------
void TIrLAP::GetNickName( UInt8 * nickName, int maxlen )
{
    if( fNickName[0] != 0 )
	strlcpy( ( char * )nickName, ( const char * )fNickName, maxlen);
    else
	nickName[0] = 0;
}


//--------------------------------------------------------------------------------
//      FreeGetBuffers
//--------------------------------------------------------------------------------
void TIrLAP::FreeGetBuffers()
{
    UInt32  flag, index;
    
    XTRACE(kLogFreeGetBuffers3, fGetBufferAvail >> 16, fGetBufferAvail);
	
    // Free the input buffers (allocated during connect with peer)
    for (index = 0, flag=1; index < fNumGetBuffers; index++, flag <<= 1) {
	XTRACE(kLogFreeGetBuffers1, 0, fGetBuffers[index]);
	XTRACE(kLogFreeGetBuffers2, flag, index);
	
	check(fGetBuffers[index]);
	check((fGetBufferAvail & flag) ||                   // should be "available" OR
		(fInputBuffer == fGetBuffers[index]));      // current fInputBuffer
		
	if (fGetBuffers[index] && (fGetBufferAvail & flag)) {   // if ok to delete
	    fGetBuffers[index]->Delete();                       // release the buffer
	}
	else {
	    // if it's in use as fInputBuffer then it won't have it's avail flag set
	    // but we can free it anyway since we own it (wasn't sent up the stack) 
	    if (fInputBuffer == fGetBuffers[index]) {
		XTRACE(kLogFreeGetBuffers4, 0, fInputBuffer);
		fInputBuffer = nil;
		fGetBuffers[index]->Delete();               // release the buffer
	    }
	    else {      // memory leak, but let's not free it since it may be in use
		DebugLog("memory leak! fInputBuffer=0x%lx\n", (uintptr_t)fInputBuffer);
		DebugLog("buffer problem in free get buffers, index %ld, buffer 0x%lx\n",
				(long int)index, (uintptr_t) fGetBuffers[index]);
		//IrDALogTracingOff();
	    }
	}
	fGetBuffers[index] =  nil;          // sanity
    }
    fNumGetBuffers = 0;         // no longer have any buffers available
    fGetBufferAvail = 0;        // bitmask of available buffers is zero available now

} // TIrLAP::FreeGetBuffers

//
// JDG: lapconn needs a way to abort a listen if a connect request comes in
// while a listen is pending.  This will do it if we're still in NDM state
// Could avoid this if we didn't use client event records for lapconn/lap interaction
//
TIrEvent *
TIrLAP::CancelPendingListenRequest(void)        // return event if listen canceled
{
    TIrEvent *request = GetCurrentEvent();
    
    StopInput();                                    // stop input, lapconn wants to stop the listen
    
    if (fState == kIrLAPDisconnectedState && request && request->fEvent == kIrListenRequestEvent) {
	//RejectRequest(request, kIRErrRetry);          // no, don't send it back
	fCurrentRequest = nil;                          // no pending request now
	return request;                                 // return 'ok'
    }
    return nil;
}   // TIrLAP::CancelPendingListenRequest


//--------------------------------------------------------------------------------
//      NextState
//--------------------------------------------------------------------------------
void TIrLAP::NextState(ULong event)
{
    XTRACE(kLogStateEvent, fState, event);
    
    switch (fState) {
	case kIrLAPDisconnectedState:
	    HandleDisconnectedStateEvent(event);
	    break;

	case kIrLAPQueryState:
	    HandleQueryStateEvent(event);
	    break;

	case kIrLAPConnectState:
	    HandleConnectStateEvent(event);
	    break;

	case kIrLAPListenState:
	    HandleListenStateEvent(event);
	    break;

	case kIrLAPReplyState:
	    HandleReplyStateEvent(event);
	    break;

	case kIrLAPPriReceiveState:
	    HandlePriReceiveStateEvent(event);
	    break;

	case kIrLAPPriTransmitState:
	    HandlePriTransmitStateEvent(event);
	    break;

	case kIrLAPPriCloseState:
	    HandlePriCloseStateEvent(event);
	    break;

	case kIrLAPSecReceiveState:
	    HandleSecReceiveStateEvent(event);
	    break;

	case kIrLAPSecTransmitState:
	    HandleSecTransmitStateEvent(event);
	    break;

	case kIrLAPSecCloseState:
	    HandleSecCloseStateEvent(event);
	    break;

	default:
	    DebugLog("TIrLAP::NextState: bad fState");
	    break;
    }
}


//--------------------------------------------------------------------------------
//      HandleDisconnectedStateEvent (fState == kIrLAPDisconnectedState) NDM
//--------------------------------------------------------------------------------
void TIrLAP::HandleDisconnectedStateEvent(ULong event)
{
#if forMac                          // Using a hammer for the problem, but if I am
    fConnected = false;             // here, then I am disconnected.  Simple logic
    fDiscoverActive = false;        // If I am discovering, then it will get set to
    //gConnectionBroken = false;        // true before I exit.
    fNickName[0] = 0;               // Clear out the connect name.
	
#endif

    switch (event) {
	case kIrDiscoverRequestEvent:
	    {
		XTRACE(kNDMDiscoveryEvent, 0, 0);

		fDiscoverActive = true;             // flag for status routines that we're discovering
		// Save the request block - use it as the reply block later
		//XASSERT(fCurrentRequest == nil);                  // jdg: maybe on the newton
		if (fCurrentRequest != nil) {                       // JDG ADDED, can we do better?
		    RejectRequest(fCurrentRequest, kIrDAErrRetry);  // dispose of previous request
		}
		fCurrentRequest = GetCurrentEvent();
					    
		// Start up the media busy timeout timer
		GetIrDevice->ResetMediaBusy();
		StartTimer(kIrMediaBusyTimeout, kIrMediaBusyTimerExpiredEvent);
		fBusyCounter = 0;       // JDG: Try N times to discover
    //          StartMediaBusyTimer( kIrMediaBusyTimeout, kIrMediaBusyTimerExpiredEvent );
		// Wait for possible xid request from peer device
		StartInput(fIOBufferItem);
	    }
	    break;

	case kIrConnectRequestEvent:
	    {
		XTRACE(kNDMConnectEvent, 0, 0);

		TIrConnLstnRequest* connectRequest = (TIrConnLstnRequest*)GetCurrentEvent();

		// Save the request block - use it as the reply block later
		//XASSERT(fCurrentRequest == nil);                  // jdg - maybe on a newton
		if (fCurrentRequest != nil) {                       // JDG ADDED, can we do better?
		    fDiscoverActive = false;                        // we're not discovering anymore
		    StopTimer();                                    // stop media timer (if discovery req)
		    RejectRequest(fCurrentRequest, kIrDAErrRetry);  // dispose of previous request
		}
		fCurrentRequest = GetCurrentEvent();
		
		// Save myQOS parms and place to receive peerQOS parms
		fMyQOS = connectRequest->fMyQOS;
		fPeerQOS = connectRequest->fPeerQOS;
		
		/*** TEMP DEBUGGING ***/
		if (1) {
		    // First my info
		    XTRACE(kQOSBufferInfoEvent, fMyQOS->GetWindowSize(), fMyQOS->GetDataSize());
		    XTRACE(kQOSLeadInCountEvent, 0, fMyQOS->GetExtraBOFs());
		    XTRACE(kQOSMinTurnAroundEvent, 0, fMyQOS->GetMinTurnAroundTime());
		    XTRACE(kQOSMaxTurnAroundEvent, 0, fMyQOS->GetMaxTurnAroundTime());

		    // Then the peer device's info
		    XTRACE(kQOSBufferInfoEvent, fPeerQOS->GetWindowSize(), fPeerQOS->GetDataSize());
		    XTRACE(kQOSLeadInCountEvent, 1, fPeerQOS->GetExtraBOFs());
		    XTRACE(kQOSMinTurnAroundEvent, 1, fPeerQOS->GetMinTurnAroundTime());
		    XTRACE(kQOSMaxTurnAroundEvent, 1, fPeerQOS->GetMaxTurnAroundTime());
		}
		/**** TEMP DEBUGGING ****/

		// Address of device to connect to
		fPeerDevAddr = connectRequest->fDevAddr;

		// Get a random connection address for the conn address to be used while in NRM mode
		// The IrLAP spec reserves values 0 and 7F, but some errata have also implied
		// that 1 and 7E are also invalid, so I'm leaving them out too to be safe.
		fConnAddr = (UByte)random() % 0x7C + 2; // 0-7B plus 2 => 2-7D

		// For efficiency, avoid conn addrs of 0x60 (which become 0xC0/0xC1 in A field)
		// and 0x3E (which become 0x7D in A field).  Otherwise the A field would always
		// have to be escaped.  This avoids it up front, since we have control in this case.
		// NOTE: This is a SIR only issue, but doesn't hurt if also done for FIR.
		if ((fConnAddr == 0x60) || (fConnAddr == 0x3E)) {
		    // Now 0x60 => 0x61 (0xC2/0xC3) and 0x3E => 0x3F (0x7E); and no escaping needed.
		    fConnAddr++;
		}

		XTRACE( kLAPAddr, fPeerDevAddr, fConnAddr );
		check(fConnAddr);
		GetIrDevice->SetLAPAddress( fConnAddr );

		// Init some state
		fRetryCount = 0;
		fInitialRetryTime = 0;

		// Change state to connect(ing)
		fState = kIrLAPConnectState;

		// Begin by sending the SNRM command
		OutputSNRMCommand();
	    }
	    break;

	case kIrListenRequestEvent:
	    {
		XTRACE(kNDMListenEvent, 0, 0);

		TIrConnLstnRequest* listenRequest = (TIrConnLstnRequest*)GetCurrentEvent();

		// Save the request block - use it as the reply block later
		//XASSERT(fCurrentRequest == nil);                  // jdg: maybe on a newton
		// Listen requests defer to pending discover, reject this one if we're not idle
		if (fCurrentRequest != nil) {                       // Can we do better?
		    //StopTimer();                                  // don't stop media timer
		    //RejectRequest(fCurrentRequest, kIrDAErrCancel);   // dispose of previous request
		    RejectRequest(GetCurrentEvent(), kIrDAErrRetry);    // reject the new LISTEN (discover in progress)
		    return;                                         // and return!                                  
		}
		fCurrentRequest = GetCurrentEvent();
		
		// Save myQOS parms and place to receive peerQOS parms
		fMyQOS = listenRequest->fMyQOS;
		fPeerQOS = listenRequest->fPeerQOS;

		///////////////////////////////////////////////////////////////////////
		// TESTING ABORT ASYNC I/O HACK
		///fDiscoverActive = false;     // we are not discovering, thank you
		///StartTimer(5*1000, kIrMediaBusyTimerExpiredEvent);       // 5 seconds
		///////////////////////////////////////////////////////////////////////
		// Begin by waiting for initial XID command
		// (to be followed by (optional addr resolutions) then a snrm)
		StartInput(fIOBufferItem);
	    }
	    break;

	case kIrInputCompleteEvent:
	    {
		XTRACE(kNDMRecdInputEvent, fRecdCmdRsp, fRecdAddr);

		// Stop the media busy timer (if we are discovering)
		StopTimer();
		
		// Check for xid (peer device is discovering)
		if (RecdPollCmd(kIrLAPCmdXID)) {
		    fDiscoverEnteredReplyState = true;
		    HandleReplyStateEvent(kIrInputCompleteEvent);
		    // Note: input was reposted in HandleReplyStateEvent (if didn't change state)
		}
		// Check for snrm (peer device is connecting)
		else if (RecdPollCmd(kIrLAPCmdSNRM)) {
		    HandleListenStateEvent(kIrInputCompleteEvent);
		    // Note: input was reposted by HandleListenStateEvent (if didn't change state)
		}
		// It was something else.  Ignore it, but repost the input
		else {
		    StartInput(fIOBufferItem);
		}

		// If we are still in the disconnected state (because the XID wasn't
		// for us or the snrm wasn't for us) then restart/continue the requests
		if (fState == kIrLAPDisconnectedState) {
		    //XASSERT(fCurrentRequest != nil);
		    // Note: If listen almost completed but failed with an out of memory
		    // error then fCurrentRequest could legitimately be nil at this point.
		    if (fCurrentRequest && fCurrentRequest->fEvent == kIrDiscoverRequestEvent) {
			// Start up the media busy timeout timer
			GetIrDevice->ResetMediaBusy();
			StartTimer(kIrMediaBusyTimeout, kIrMediaBusyTimerExpiredEvent);
    //                  StartMediaBusyTimer( kIrMediaBusyTimeout, kIrMediaBusyTimerExpiredEvent );
		    }
		}
	    }
	    break;

	case kIrMediaBusyTimerExpiredEvent:
	    {
    //          DebugLog("LAP NDM kIrMediaBusyTimerExpiredEvent" );
		XTRACE(kNDMTimeoutEvent, 0, 0);
		//XASSERT((fCurrentRequest != nil) && (fCurrentRequest->fEvent == kIrDiscoverRequestEvent));


		TIrDiscoverRequest* discoverRequest = (TIrDiscoverRequest*)fCurrentRequest;
		fDiscoverActive = true;             // this should already be set!

		// Check mediaBusy flag to really make sure that there was no "traffic"
		if (GetIrDevice->GetMediaBusy()) {
		    // Media is busy, wait some more before trying again
		    // NOTE: This implementation currently tries forever.  If that is not the correct
		    // model then need to use a retry count here and return error when retry is reached.
		    /*** JDG: Let's stop the discover after one try. ... nope, now N tries ****/
		    if (fBusyCounter++ < kMaxDiscoverRetries) {     // if ok to try again
			//StopInput();                              // jdg: at least reset the busy flag!
			//StartInput(fIOBufferItem);                    // jdg: by stopping & restarting read
			GetIrDevice->ResetMediaBusy();              // reset busy flag directly now
			StartTimer(kIrMediaBusyTimeout, kIrMediaBusyTimerExpiredEvent);
			// StartMediaBusyTimer( kIrMediaBusyTimeout, kIrMediaBusyTimerExpiredEvent );
		    } else {                                        // give up and reject the discover request
			fDiscoverActive = false;                            // jdg: don't try again
			discoverRequest->fPassiveDiscovery = false;         // probably not needed, but ...
			RejectRequest(fCurrentRequest, kIrDAErrLinkBusy);       // return the request
			fCurrentRequest = nil;
		    }
		}

		// No other traffic - okay to begin a discover dialog
		else {
		    StopInput();

		    // Default to 1 slot if invalid numSlots is passed in
		    fDiscoverFlags = kIrLAPDiscoverFlags1Slot;
		    fDiscoverMaxSlots = 0;  // Initially 1 - 1

		    // Assign appropriate slot bit field
		    UByte index;
		    for (index = 0; index < sizeof(IrSlotCounts); index++) {
			if (IrSlotCounts[index] == discoverRequest->fNumSlots) {
			    fDiscoverFlags = index;
			    fDiscoverMaxSlots = (UByte)discoverRequest->fNumSlots - 1;
			    break;
			}
		    }

		    // Set fConflictDevAddr (which may be kIrLAPBroadcastDevAddr) if discover vs addr conflict
		    fConflictDevAddr = discoverRequest->fConflictDevAddr;
		    if (fConflictDevAddr != kIrLAPBroadcastDevAddr) {
			fDiscoverFlags |= kIrLAPDiscoverFlagsNewAddr;
		    }

		    // Start from the beginning
		    fDiscoverSlot = 0;

		    // Change state to discover(ing)
		    fState = kIrLAPQueryState;

		    // Begin by sending the initial XID command
		    OutputXIDCommand();
		}
	    }
	    break;

	case kIrCancelPutRequestEvent:
	    CancelPutRequest();
	    break;

	case kIrDisconnectRequestEvent:
	    XTRACE(kNDMDisconnectEvent, 0, 0);
	    HandleNDMDisconnectRequest();
	    break;

	case kIrPutDataRequestEvent:
	    NotConnectedCompletion();                                   // jdg: yes!  send back to CIrLSAP
	    //RejectRequest(GetCurrentEvent(), kCommErrNotConnected);   // jdg: send to lmp, not directly to lsapconn
	    break;

	case kIrLocalBusyClearedEvent:
	    // Don't need to do anything here.
	    // This is a side effect of releasing buffers at disconnect cleanup (see IrLAPConn).
	    break;

	case kIrOutputCompleteEvent:        // jdg added.  this can be safely ignored, we're on the way
	    break;                          //   out ... I hope!
	    
	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TIrLAP::HandleDisconnectedStateEvent: bad event");
	    break;
    }

}   // TIrLAP::HandleDisconnectedStateEvent


//--------------------------------------------------------------------------------
//      HandleQueryStateEvent (fState == kIrLAPQueryState)
//--------------------------------------------------------------------------------
void TIrLAP::HandleQueryStateEvent(ULong event)
{   
    fDiscoverActive = true;             // Always update date the globals.

    switch (event) {
	case kIrInputCompleteEvent:
	    XTRACE(kQueryRecdInputEvent, fRecdCmdRsp, fRecdAddr);
	    // Only accept broadcast XID responses
	    if ((fRecdAddr == kIrLAPBroadcastAddr) && RecdFinalRsp(kIrLAPRspXID)) {
		TXIDPacket  xidRsp;

		// Parse the received info
		if (GotData(&xidRsp.fFormatId, kTXIDPacketSize - 2)) {
		    // Only log it if dest address is mine
		    if (xidRsp.fDstDevAddr == fMyDevAddr) {
			XTRACE(kDiscoveredDevice, 0, (xidRsp.fSrcDevAddr >> 16));
			XTRACE(kDiscoveredDevice, 1, (xidRsp.fSrcDevAddr));
			// Log the discovered info (rev/addr/info)
			TIrDscInfo* discoveryInfo = TIrDscInfo::tIrDscInfo();
			if (discoveryInfo != nil) {
			    TIrDiscoverReply* discoverReply = (TIrDiscoverReply*)fCurrentRequest;
			    XASSERT(discoverReply != nil);

			    // Fill in, extract info for discover info
			    discoveryInfo->SetVersion(xidRsp.fVersion);
			    discoveryInfo->SetDeviceAddr(xidRsp.fSrcDevAddr);
			    discoveryInfo->ExtractDevInfoFromBuffer(fInputBuffer);

			    // Add it to the list of discovered devices
			    discoverReply->fDiscoveredDevices->InsertLast(discoveryInfo);
			}
		    }
		    else {
			XTRACE(kLogMyAddr, fMyDevAddr >> 16, fMyDevAddr);
			XTRACE(kLogGotAddr, xidRsp.fDstDevAddr >> 16, xidRsp.fDstDevAddr);
			XTRACE(kLogFailedThird, 0, 0);
			DebugLog(" failed 3rd");
		    }
		}
		else {
		    DebugLog(" failed 2nd");
		    XTRACE(kLogFailedSecond, 0, 0);
		}
	    }
	    else {
		XTRACE(kLogFailedFirst, 0, 0);
		DebugLog(" failed 1st");
	    }
	    
	    // Wait for input
	    StartInput(fIOBufferItem);
	    break;

	case kIrOutputCompleteEvent:
	    XTRACE(kQueryOutputDoneEvent, fDiscoverSlot, fDiscoverMaxSlots);
	    XASSERT(fNextCmdRspToSend == kIrLAPCmdXID);
	    if (fDiscoverSlot != kIrLAPFinalSlot) {
		// Start the slot timer and wait for a reply
		StartTimer(kIrDiscoverSlotTimeout, kIrSlotTimerExpiredEvent);
//#if forMac
		StartInput(fIOBufferItem);          // issue read only if we're maybe expecting something
//#endif
	    }
	    else {
		// Let the initiator know that the discovery phase has completed
		TIrDiscoverReply* discoverReply = (TIrDiscoverReply*)fCurrentRequest;
		fCurrentRequest = nil;
		discoverReply->fEvent = kIrDiscoverReplyEvent;
		discoverReply->fPassiveDiscovery = false;
#if forMac
		fDiscoverActive = false;                // Update the globals.
		///////////////////////////////////////////////////////////////////////
		// TESTING ABORT ASYNC I/O HACK
		// no we're hanging a read here before going back to disconnect state
		//StartTimer(5*1000, kIrMediaBusyTimerExpiredEvent);        // 5 seconds
		///////////////////////////////////////////////////////////////////////
#endif              
		// Discovery process complete
		fState = kIrLAPDisconnectedState;
		GetLMP->EnqueueEvent(discoverReply);
	    }
//#if forMac
//          StartInput(fIOBufferItem);      // don't do a read after the last XID, no reply coming
//#endif
	    break;

	case kIrSlotTimerExpiredEvent:
	    XTRACE(kQueryTimeoutEvent, fDiscoverSlot, fDiscoverMaxSlots);
	    StopInput();
	    if (fDiscoverSlot < fDiscoverMaxSlots) {
		// Send out XID for next slot
		fDiscoverSlot++;
	    }
	    else {
		// Send out final XID
		fDiscoverSlot = kIrLAPFinalSlot;
	    }
	    OutputXIDCommand();
	    break;

	case kIrDisconnectRequestEvent:
	    XTRACE(kQueryDisconnectEvent, 0, 0);
	    XASSERT(fCurrentRequest != nil);
#if forMac
	    fDiscoverActive = false;                // Update the globals.
#endif              
	    HandleNDMDisconnectRequest();
	    break;

	case kIrDiscoverRequestEvent:
	case kIrListenRequestEvent:
	case kIrConnectRequestEvent:
	    XTRACE(kUnexpectedEvent, fState, event);
	    RejectRequest(GetCurrentEvent(), kIrDAErrWrongState);   // don't do it, don't crash
	    break;

	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TIrLAP::HandleQueryStateEvent: bad event");
	    break;
    }

} // TIrLAP::HandleQueryStateEvent


//--------------------------------------------------------------------------------
//      HandleConnectStateEvent (fState == kIrLAPConnectState)
//--------------------------------------------------------------------------------
void TIrLAP::HandleConnectStateEvent(ULong event)
{
    switch (event) {
	case kIrChangeSpeedCompleteEvent:
	    XTRACE(kConnectChangeSpeed, 0, 0);  // log it
	    OutputControlFrame(kIrLAPFrameRR);  // Note: OutputControlFrame sets poll bit
	    break;
	    
	case kIrInputCompleteEvent:
	    {
		XTRACE(kConnectRecdInputEvent, fRecdCmdRsp, 0);
		Boolean repostInput = true;

		if (RecdFinalRsp(kIrLAPRspUA)) {
		    TUAPacket uaRsp;

		    // Parse the received ua response
		    if (GotData((UByte*)&uaRsp.fSrcDevAddr, kTUAPacketSize - 2)) {
			if ((uaRsp.fSrcDevAddr == fPeerDevAddr) && (uaRsp.fDstDevAddr == fMyDevAddr)) {
			    IrDAErr result;

			    StopTimer();
			    // Parse, negotiate and init conn state (and apply time consuming QOS parms)
			    result = ParseNegotiateAndInitConnState(true /*primary*/);
			    // NOTE: If error, ConnLstnComplete has already been called and gone to NDM
			    if (result != noErr) break;

			    // Change baud rate to the negotiated speed
			    GetIrDevice->ChangeSpeed(fMyQOS->GetBaudRate());

			    // Note that leadInCount is not changed yet on purpose
			    // so there is a minimum turnaround before sending the RR
			    // MOVED next line to after change speed completes
			    //OutputControlFrame(kIrLAPFrameRR);    // Note: OutputControlFrame sets poll bit
			    repostInput = false;
    #if forMac                                                  // Report I are connected, though not
			    fConnected = true;                  // really until I receive an RR from
								// the other size.
			    GetDiscovery->GetRemoteDeviceName( fPeerDevAddr, fNickName, sizeof(fNickName));
    #endif
			}
		    }
		}
		else if (RecdPollCmd(kIrLAPCmdSNRM)) {
		    TSNRMPacket snrmCmd;

		    // Parse the received snrm command
		    if (GotData((UByte*)&snrmCmd.fSrcDevAddr, kTSNRMPacketSize - 2)) {
			// NOTE: Only agree to be secondary if myDevAddr < peerDevAddr
			if ((snrmCmd.fDstDevAddr == fMyDevAddr) && (fMyDevAddr < snrmCmd.fSrcDevAddr)) {
			    IrDAErr result;
			    UByte connAddr = snrmCmd.fConnAddr >> 1;

			    // Don't accept any wooden nickels (whatever that means)
			    if ((connAddr > 0) && (connAddr < kIrLAPBroadcastAddr)) {
				// Recd snrm while connecting and myDevAddr < peerDevAddr - so enter listen state
				fState = kIrLAPListenState;

				// Save relevent info
				fConnAddr = connAddr;
				fPeerDevAddr = snrmCmd.fSrcDevAddr;

				StopTimer();
				
				//check(fPeerDevAddr);
				//fIrSIR->SetLAPAddress( fPeerDevAddr );
				check(connAddr);                    // JDG: let's use the right
				GetIrDevice->SetLAPAddress(connAddr);   // field, shall we?
				
				// Parse, init conn state and apply time consuming QOS parms
				result = ParseNegotiateAndInitConnState(false /*primary*/);
				// NOTE: If error, ConnLstnComplete has already been called and gone to NDM
				if (result != noErr) break;

				OutputUAResponse();
				repostInput = false;
			    }
			}
		    }
		}
		else if (RecdCmd(kIrLAPCmdDISC) || RecdRsp(kIrLAPRspDM)) {
		    // Let the initiator know that the connect failed - (was stopped by peer)
		    ConnLstnComplete(kIrDAErrCancel);
		    repostInput = false;
		}
		// Resume input if nothing recognizable was received
		if (repostInput) {
		    StartInput(fIOBufferItem);
		}
	    }
	    break;

	case kIrOutputCompleteEvent:
	    XTRACE(kConnectOutputDoneEvent, fNextCmdRspToSend, fConnAddr);
	    switch (fNextCmdRspToSend) {
		case kIrLAPCmdSNRM:
		    fRetryCount++;
		    StartTimer(kIrConnectFinalTimerTimeout, kIrFinalTimerExpiredEvent);
		    StartInput(fIOBufferItem);
		    break;

		case kIrLAPFrameRR:
		    // Start receiving at earliest possible point
		    //StartDataReceive();           // jdg: can't do this since read can call me back w/data

		    // Set additional number of BOFs required
		    // NOTE: Done after sending RR so there is a min turnaround delay (w/BOFs)
		    fLeadInCount = (UByte)fPeerQOS->GetExtraBOFs();

		    // Let the initiator know that the connect succeeded
		    ConnLstnComplete(noErr);

		    StartTimer(fFinalTimerTimeout, kIrFinalTimerExpiredEvent);

		    fState = kIrLAPPriReceiveState;
		    StartDataReceive();         // now that state's changed, can do a read
		    break;

		default:
		    DebugLog("TIrLAP::HandleConnectStateEvent: bad output state");
		    break;
	    }
	    break;

	case kIrFinalTimerExpiredEvent:
	    XTRACE(kConnectFinalTimeoutEvent, 0, 0);
	    if (fRetryCount < kMaxConnectRetries) {
		// Delay random time before resending SNRM
		// Random delay x should be between .5 and 1.5 time to send SNRM request
		// Time to send SNRM is appx 50 mSecs, range of x is 25 <= x <= 75 mSecs
		StartTimer((25 + (random() % 50)) * kMilliseconds, kIrBackoffTimerExpiredEvent);
	    }
	    else {
		StopInput();

		// Let the initiator know that the connect failed - no responder
		ConnLstnComplete(kIrDAErrTimeout);
	    }
	    break;

	case kIrBackoffTimerExpiredEvent:
	    XTRACE(kConnectBackoffTimeoutEvent, 0, 0);
	    StopInput();
	    OutputSNRMCommand();
	    break;

	case kIrDisconnectRequestEvent:
	    XTRACE(kConnectDisconnectEvent, 0, 0);
	    XASSERT(fCurrentRequest != nil);
	    HandleNDMDisconnectRequest();
	    break;

	case kIrDiscoverRequestEvent:
	case kIrListenRequestEvent:
	case kIrConnectRequestEvent:
	    XTRACE(kUnexpectedEvent, fState, event);
	    RejectRequest(GetCurrentEvent(), kIrDAErrWrongState);   // don't do it, don't crash
	    break;

	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TIrLAP::HandleConnectStateEvent: bad event");
	    break;
    }

} // TIrLAP::HandleConnectStateEvent


//--------------------------------------------------------------------------------
//      HandleListenStateEvent (fState == kIrLAPListenState)
//--------------------------------------------------------------------------------
void TIrLAP::HandleListenStateEvent(ULong event)
{
    switch (event) {
	case kIrInputCompleteEvent:
	    {
		XTRACE(kListenRecdInputEvent, fRecdCmdRsp, 0);
		Boolean repostInput = true;

		if (RecdPollCmd(kIrLAPCmdSNRM)) {
		    TSNRMPacket snrmCmd;

		    // Parse the received snrm command
		    if (GotData((UByte*)&snrmCmd.fSrcDevAddr, kTSNRMPacketSize - 2)) {
			if (snrmCmd.fDstDevAddr == fMyDevAddr) {
			    IrDAErr result;
			    UByte connAddr = snrmCmd.fConnAddr >> 1;

			    // Don't accept any wooden nickels (whatever that means)
			    if ((connAddr > 0) && (connAddr < kIrLAPBroadcastAddr)) {
				// Recd valid snrm for me - so enter listen state
				fState = kIrLAPListenState;

				// Save relevent info
				fConnAddr = connAddr;
				fPeerDevAddr = snrmCmd.fSrcDevAddr;

				//check(fPeerDevAddr);
				//fIrSIR->SetLAPAddress( fPeerDevAddr );
				check(connAddr);                    // JDG: let's use the right
				GetIrDevice->SetLAPAddress(connAddr);   // field, shall we?

				// Parse, init conn state and apply time consuming QOS parms
				result = ParseNegotiateAndInitConnState(false /*primary*/);
				// NOTE: If error, ConnLstnComplete has already been called and gone to NDM
				if (result != noErr) break;

				OutputUAResponse();
				repostInput = false;
			    }
			}
		    }
		}
		// Resume input if nothing recognizable was received
		if (repostInput) {
		    StartInput(fIOBufferItem);
		}
	    }
	    break;

	case kIrOutputCompleteEvent:
	    XTRACE(kListenOutputDoneEvent, fNextCmdRspToSend, 0);
	    XASSERT(fNextCmdRspToSend == kIrLAPRspUA);

	    // Change baud rate to the negotiated speed
	    GetIrDevice->ChangeSpeed(fMyQOS->GetBaudRate());

	    // "falls through" after ChangeSpeed completes .. jdg 6/26/2000
	    break;
	    
	case kIrChangeSpeedCompleteEvent:
	    XTRACE(kListenChangeSpeed, 0, 0);

	    // Start receiving at earliest possible point
	    //StartDataReceive();           // jdg: moved below

	    // Set additional number of BOFs required
	    fLeadInCount = (UByte)fPeerQOS->GetExtraBOFs();

	    // Let the initiator know that the listen has completed
	    ConnLstnComplete(noErr);

	    // ***FIXME: Is this the right amount of time. Errata had some mention of this
	    StartTimer(fWatchdogTimeout, kIrWatchdogTimerExpiredEvent);
	    fState = kIrLAPSecReceiveState;
	    StartDataReceive();         // jdg: wait until new state before issuing read
	    break;

	case kIrDisconnectRequestEvent:
	    XTRACE(kListenDisconnectEvent, 0, 0);
	    XASSERT(fCurrentRequest != nil);
	    HandleNDMDisconnectRequest();
	    break;

	case kIrDiscoverRequestEvent:
	case kIrListenRequestEvent:             // REVIEW THIS ONE.  Unsolicited connect, listen?
	case kIrConnectRequestEvent:
	    XTRACE(kUnexpectedEvent, fState, event);
	    RejectRequest(GetCurrentEvent(), kIrDAErrWrongState);   // don't do it, don't crash
	    break;

	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TIrLAP::HandleListenStateEvent: bad event");
	    break;
    }

} // TIrLAP::HandleListenStateEvent


//--------------------------------------------------------------------------------
//      HandleReplyStateEvent (fState == kIrLAPReplyState)
//--------------------------------------------------------------------------------
void TIrLAP::HandleReplyStateEvent(ULong event)
{
    switch (event) {
	case kIrInputCompleteEvent:
	    {
		XTRACE(kReplyRecdInputEvent, fRecdCmdRsp, 0);
		Boolean repostInput = true;

		if (RecdPollCmd(kIrLAPCmdXID)) {
		    TXIDPacket  xidCmd;

		    // Parse the received XID command
		    if (GotData(&xidCmd.fFormatId, kTXIDPacketSize - 2)) {
			if (((xidCmd.fFormatId == kIrLAPDiscoveryXIDFormat)) &&
			    ((xidCmd.fSrcDevAddr != 0) && (xidCmd.fSrcDevAddr != kIrLAPBroadcastDevAddr)) &&
			    ((xidCmd.fDstDevAddr == kIrLAPBroadcastDevAddr) || ((xidCmd.fDstDevAddr == fMyDevAddr) && (xidCmd.fFlags & kIrLAPDiscoverFlagsNewAddr)))) {
			    if (xidCmd.fSlotNum != kIrLAPFinalSlot) {
				// Init some state first time through
				if (fDiscoverEnteredReplyState) {
				    // Recd bcast xid or xid for me and not just the final - so enter reply state
				    fState = kIrLAPReplyState;

				    fDiscoverMaxSlots = IrSlotCounts[xidCmd.fFlags & kIrLAPDiscoverFlagsSlotMask];
				    fDiscoverSlot = random() % fDiscoverMaxSlots;
				    fDiscoverReplied = false;
				    fReplacementDevAddr = 0;
    #if forMac  // FIXME - XID slots are too long (w/ write completion proc).  I had to increase the time period
		//          for it to work.  
				    StartTimer(fDiscoverMaxSlots * 140 * kMilliseconds, kIrQueryTimerExpiredEvent);
    #else
				    StartTimer(fDiscoverMaxSlots * 100 * kMilliseconds, kIrQueryTimerExpiredEvent);
    #endif
				    fDiscoverEnteredReplyState = false;
				}

				// Respond to the XID query if haven't yet and its my slot
				if (!fDiscoverReplied && (xidCmd.fSlotNum >= fDiscoverSlot)) {
				    OutputXIDResponse(xidCmd);
				    fDiscoverReplied = true;
				    repostInput = false;
				}
			    }
			    else {
				// Final slot - all done with the XID game for now
				StopTimer();
				
				if (fReplacementDevAddr) fMyDevAddr = fReplacementDevAddr;

				// Discovery reply complete
				fState = kIrLAPDisconnectedState;

				// ***MINIMUM: If this were a full implementation then the
				// listener (one who made a listen request) would receive some
				// sort of interim discovery indication.  But for now only the
				// discoverer (one who made a discover request) is notified.
				
				//XASSERT(fCurrentRequest != nil);
				// JDG -- it *can* be nil when we're not doing much and get probed.
				
				if (fCurrentRequest && fCurrentRequest->fEvent == kIrDiscoverRequestEvent) {
				    // Let the initiator know that the discovery phase has completed
				    TIrDscInfo* discoveryInfo = TIrDscInfo::tIrDscInfo();
				    TIrDiscoverReply* discoverReply = (TIrDiscoverReply*)fCurrentRequest;
				    fCurrentRequest = nil;
				    discoverReply->fEvent = kIrDiscoverReplyEvent;
				    if (discoveryInfo == nil) {
					discoverReply->fResult = kIrDAErrNoMemory;
				    }
				    else {
					// Fill in, extract info for discover info
					discoveryInfo->SetVersion(xidCmd.fVersion);
					discoveryInfo->SetDeviceAddr(xidCmd.fSrcDevAddr);
					discoveryInfo->ExtractDevInfoFromBuffer(fInputBuffer);

					// Add it to the list of discovered devices
					discoverReply->fDiscoveredDevices->InsertLast(discoveryInfo);
					discoverReply->fResult = noErr;
					discoverReply->fPassiveDiscovery = true;
				    }
				    // In the case where we are replying to an XID cmd while
				    // discovering and are now finished with the discovering
				    // then we do not want to input again.
				    repostInput = false;

				    GetLMP->EnqueueEvent(discoverReply);
				}
				else {
				    CIrDiscovery *  discObj = GetDiscovery;
				    
				    if( discObj ) {
					TIrDscInfo* discoveryInfo = TIrDscInfo::tIrDscInfo();
					//TIrDiscoverReply* discoverReply = (TIrDiscoverReply*)fCurrentRequest;   // ?
					if (discoveryInfo ) {
					    // Fill in, extract info for discover info
					    discoveryInfo->SetVersion(xidCmd.fVersion);
					    discoveryInfo->SetDeviceAddr(xidCmd.fSrcDevAddr);
					    discoveryInfo->ExtractDevInfoFromBuffer(fInputBuffer);

					    discObj->PassiveDiscovery( discoveryInfo );
					}
				    }
				}
			    }
			}
		    }
		}
		// If we sent an XID response (or perhaps we were responding because we were
		// discovered while discovering) then don't fall thru, wait for output to complete
		if (!repostInput) break;
	    }
	    // Fall thru to OutputComplete event to re-post input

	case kIrOutputCompleteEvent:
	    XTRACE(kReplyOutputDoneEvent, fNextCmdRspToSend, fDiscoverSlot);
	    StartInput(fIOBufferItem);
	    break;

	case kIrQueryTimerExpiredEvent:
	    XTRACE(kReplyTimeoutEvent, 0, 0);
	    // ***FIXME: I got here because I missed the final slot query - if I responded
	    // to the query for my slot then everything should be ok; however, if I did not
	    // respond to a query for my slot then the other side doesn't know about me and
	    // listen should probably terminate with some error.
	    // ***I think that StopInput needs to be called here too.
	    if (fReplacementDevAddr) fMyDevAddr = fReplacementDevAddr;
	    fState = kIrLAPDisconnectedState;
	    DebugLog("Missed final packet of peer discovery;g");
	    //StopInput();                      // scc (?) was hanging here, let's reset it
	    //StartTimer(5*1000, kIrMediaBusyTimerExpiredEvent);        // 5 seconds abort read hack
	    //StartInput(fIOBufferItem);            // hang a read out
	    // TODO - review this again
	    break;

	case kIrDisconnectRequestEvent:
	    XTRACE(kReplyDisconnectEvent, 0, 0);
	    XASSERT(fCurrentRequest != nil);
	    HandleNDMDisconnectRequest();
	    break;

	case kIrListenRequestEvent:     // jdg hacking - just send the listen back to caller w/error
	    {
		XTRACE(kReplyListenRequestEvent, 0, 0);
		TIrConnLstnReply* connLstnReply = (TIrConnLstnReply*)GetCurrentEvent();
		connLstnReply->fEvent = kIrListenReplyEvent;
		connLstnReply->fResult = kIrDAErrToolBusy;
		GetLMP->EnqueueEvent(connLstnReply);
	    }
	    break;
	    
	case kIrDiscoverRequestEvent:
	    XTRACE(kUnexpectedEvent, fState, event);
	    RejectRequest(GetCurrentEvent(), kIrDAErrWrongState);   // don't do it, don't crash
	    break;

	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TIrLAP::HandleReplyStateEvent: bad event");
	    break;
    }

} // TIrLAP:: HandleReplyStateEvent


//--------------------------------------------------------------------------------
//      HandleNDMDisconnectRequest
//--------------------------------------------------------------------------------
void TIrLAP::HandleNDMDisconnectRequest()
{
    // Stop any pending input, output and timers
    StopInput();
    StopOutput();
    StopTimer();

    fDiscoverActive = false;        // we are not discovering anymore


    // If a request is pending (discover, listen, connect) reply to it.
    if (fCurrentRequest != nil) {
	TIrEvent* reply = (TIrEvent*)fCurrentRequest;
	fCurrentRequest = nil;
	reply->fEvent = (UByte)RequestIdToReplyId(reply->fEvent);
	reply->fResult = kIrDAErrCancel;
	XTRACE(kNDMDisconnectFwdReply, fState, reply->fEvent);
	GetLMP->EnqueueEvent(reply);
    }

    // Reply to the disconnect request
    TIrDisconnectReply* discReply = (TIrDisconnectReply*)GetCurrentEvent();
    discReply->fEvent = kIrDisconnectReplyEvent;
    discReply->fResult = kIrDAErrCancel;
    XTRACE(kNDMDisconnectReply, fState, 0);

    // Change state to disconnected
    fState = kIrLAPDisconnectedState;

    GetLMP->EnqueueEvent(discReply);

} // HandleNDMDisconnectRequest


//--------------------------------------------------------------------------------
//      HandlePriReceiveStateEvent (fState == kIrLAPPriReceiveState)
//--------------------------------------------------------------------------------
void TIrLAP::HandlePriReceiveStateEvent(ULong event)
{
    
    switch (event) {
	case kIrFinalTimerExpiredEvent:
	    GetIrDevice->Stats_ReceiveTimeout();
	    XTRACE(kPriReceiveFinalTimeoutEvent, 0, 0);
	    //DebugLog(" precv final timer");
	    StopInput();

	    if (fRetryCount >= fDisconnectLinkLimit) {
		// Apply default conn parms, send disconnect indication, state = NDM
		DisconnectComplete(kIrDAErrTimeout);
	    }
	    else {
		if (fRetryCount == fDisconnectWarningLimit) {
		    fInBrokenBeam = true;
		    //gConnectionBroken = true;
		}

	//      if( fRetryCount == 0 ) {                // Remember time of first retry
	//          fInitialRetryTime = TickCount();    // and smack the hardware
	//      }

		fRetryCount++;
		// Output RR/RNR depending on whether local is busy or not
		OutputControlFrame(fLocalBusy ? kIrLAPFrameRNR : kIrLAPFrameRR);
	    }
	    break;

	case kIrTurnaroundTimerExpiredEvent:
	    // Check for pending disconnect
	    if (fEnteringCloseState) {
		fNextCmdRspToSend = kIrLAPCmdDISC;
		fState = kIrLAPPriCloseState;
	    }

	    // Check for set localBusy pending and set it and notify peer device
	    else if (fSetLocalBusyPending) {
		XTRACE(kSetLocalBusyEvent, 0, 0);
		fNextCmdRspToSend = kIrLAPFrameRNR;
		fLocalBusy = true;
		fSetLocalBusyPending = false;
	    }

	    // Check for clr localBusy pending and clear it and notify peer device
	    else if (fClrLocalBusyPending) {
		XTRACE(kClrLocalBusyEvent, 0, 0);
		fNextCmdRspToSend = kIrLAPFrameRR;
		fLocalBusy = false;
		fClrLocalBusyPending = false;
	    }

	    // Can we begin transmitting now?
	    else if (fNextCmdRspToSend == kIrLAPFrameINFO) {
		// NOTE: This is different from the secondary and also different
		// from the IrLAP spec.  The secondary will only enter XMIT if both
		// remoteBusy is false and there are pending requests.  The IrLAP spec
		// allows primary to enter XMIT no matter what.  But, this doesn't make
		// sense if the secondary is busy (i.e. remoteBusy is true), because
		// then there will be an entire P-timer timeout before the primary
		// receives again to find out if the secondary is no longer busy.  So,
		// in this case I only enter XMIT if fRemoteBusy is false.
//              if (fRemoteBusy) {
//                  fNextCmdRspToSend = fLocalBusy ? kIrLAPFrameRNR : kIrLAPFrameRR;
//              }
//              else {                                                          //CPD moved into Pri
//                  StartTimer(fPollTimerTimeout, kIrPollTimerExpiredEvent);    // XMIT state
		    fValidRecdNr = 0;
		    fState = kIrLAPPriTransmitState;
		    NextState(kIrTurnaroundTimerExpiredEvent);
		    break;
//              }
	    }
	    OutputControlFrame(fNextCmdRspToSend);  // Note: OutputControlFrame sets poll bit
	    break;

	case kIrOutputCompleteEvent:
	    XTRACE(kPriReceiveOutputDoneEvent, fNextCmdRspToSend, 0);
	    switch (fNextCmdRspToSend) {
		case kIrLAPFrameRR:
		case kIrLAPFrameRNR:
		    // Finish up after sending RR/RNR
		    StartDataReceive();
		    StartTimer(fFinalTimerTimeout, kIrFinalTimerExpiredEvent);
		    break;

		default:
		    DebugLog("TIrLAP::HandlePriReceiveStateEvent: bad output state");
		    break;
	    }
	    break;

	case kIrInputCompleteEvent:
	    XTRACE(kPriReceiveRecdInputEvent, fRecdCmdRsp, (fRecdNs << 8) | fRecdNr);

	    // Stop the F-timer if received frame with Final bit set
	    if (RecdFinal()) {
		StopTimer();
	    }

	    // We received some input so reset the retry count that is used for the
	    // disconnect warning or disconnect link actions in final timer timeout.
	    fRetryCount = 0;
#if forMac
	    fInitialRetryTime   = 0;
	    fInBrokenBeam       = false;
	    //gConnectionBroken     = false;
#endif
	    // Init most common cases for responses
	    fNextCmdRspToSend = kIrLAPFrameINFO;

	    // TESTING
	    if (!RecdFinalRsp(kIrLAPRspFRMR))   // if anything other than FRMR
		    gLastWasFRMR = false;       // then reset the flag

	    // Handle received data frames and supervisor commands
	    if (!RecdUFrame() || RecdRsp(kIrLAPRspUI)) {
		if (RecdInvalidNrOrNs()) {
		    // Record this as a protocol error
		    fProtocolErrs++;
		    // Disconnect if received invalid Nr or Ns
		    // NOTE: The upper layers do not support reset so just disconnect now
		    if (0) {            // jdg.  hit with hammer
			fEnteringCloseState = true;
			//DebugLog("fEnteringCloseState setting true #1");  // jdg
		    }
		    XTRACE(kIgnoringInvalidNrNs, 0, 0);     // jdg
		    /**if (1) {     // TEMP TEMP TEMP TEMP
			extern struct IrDALogHdr gIrDALog;
			gIrDALog.fTracingOn = false;        // HACK turn off tracing so we can dump w/tool
			DebugLog("lap: invalid ns/nr. logging stopped");    // temp
			fEnteringCloseState = true;         // TEMP TEMP TEMP TEMP
		    }***/
		}
		else {
		    ProcessRecdInfoOrSuperFrame();
		}
	    }
	    //
	    else if (RecdFinalRsp(kIrLAPRspFRMR)) {
		UInt8   frmrInfo[3];
		fInputBuffer->Getn( ( UInt8 * )&frmrInfo, 3 );  // format XA, BC
		XTRACE(kPriReceiveFRMREvent, frmrInfo[0], *( UInt16 *)&frmrInfo[1] );
    /*******/   GetIrDevice->Stats_TransmitTimeout();   /**** TEMP *** LOG FRMR RCVD *****/
		// Close down the link
		// JDG: but only if we've gotten more than one of these in a row!!!
		if (gLastWasFRMR) {
		    fEnteringCloseState = true;
	//          DebugLog("fEnteringCloseState setting true #2");
		    XTRACE(kLogEnteringCloseState, 2, 2);
		}
		else
		    gLastWasFRMR = true;        // if get another one, shut down the link
	    }
	    //
	    else if (RecdFinalRsp(kIrLAPRspRD) || RecdFinalRsp(kIrLAPRspRNRM)) {
		// Close down the link
		fEnteringCloseState = true;
		XTRACE(kLogEnteringCloseState, 3, 3);
		//DebugLog("fEnteringCloseState setting true #3");
	    }
	    // Received some other response w/final bit on - go to xmit state
	    else if (RecdFinal()) {
		// Record this as a protocol error
		fProtocolErrs++;
		// Handled below
	    }
	    // Received some other response w/final bit off - continue receiving
	    else {
		// Record this as a protocol error
		fProtocolErrs++;
		// Handled below
	    }

	    // If didn't disconnect for some reason above, receive again or xmit after delay
	    if (fState == kIrLAPPriReceiveState) {
		if (RecdFinal()) {
		    // If received final, transmitting next (after min turnaround delay)
		    if( fRecdCmdRsp == kIrLAPFrameRNR ) {
			StartTimer(fPollTimerTimeout>>2, kIrTurnaroundTimerExpiredEvent);
		    }
		    else
			StartTimer(fMinTurnAroundTimeout, kIrTurnaroundTimerExpiredEvent);
//                  StartMinTurnAroundTimer( fMinTurnAroundTimeout, kIrTurnaroundTimerExpiredEvent);
		}
		else {
		    // If didn't receive final, receive again
		    StartDataReceive();
		}
	    }
	    break;

	case kIrPutDataRequestEvent:
	    XTRACE(kPriReceivePutDataEvent, 0, 0);
	    PostponePutRequest();
	    break;

	case kIrCancelPutRequestEvent:
	    CancelPutRequest();
	    break;

	case kIrDisconnectRequestEvent:
	    XTRACE(kPriReceiveDisconnectEvent, 0, 0);
	    XASSERT(fPendingDisconnect == nil);
	    fPendingDisconnect = GetCurrentEvent();
	    fEnteringCloseState = true;
	    XTRACE(kLogEnteringCloseState, 4, 4);
	    //DebugLog("fEnteringCloseState setting true #4");
	    break;

	case kIrLocalBusyClearedEvent:
	    // Don't need to do anything special here - handled above
	    XTRACE(kPriReceiveClrLocBsyPendEvent, 0, 0);
	    // XASSERT(fClrLocalBusyPending);   -- now bogus ... jdg
	    break;

	case kIrDiscoverRequestEvent:
	case kIrListenRequestEvent:
	case kIrConnectRequestEvent:
	    XTRACE(kUnexpectedEvent, fState, event);
	    RejectRequest(GetCurrentEvent(), kIrDAErrWrongState);   // don't do it, don't crash
	    break;

	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TIrLAP::HandlePriReceiveStateEvent: bad event");
	    break;
    }

} // TIrLAP::HandlePriReceiveStateEvent


//--------------------------------------------------------------------------------
//      HandlePriTransmitStateEvent (fState == kIrLAPPriTransmitState)
//--------------------------------------------------------------------------------
void TIrLAP::HandlePriTransmitStateEvent(ULong event)
{   
    switch (event) {
	case kIrPollTimerExpiredEvent:
#if forMac
	    //fIrSIR->TransmitTimeout();        // jdg - bogus, normal turnaround, not timeout
#endif
	    XTRACE(kPriTransmitPollTimeoutEvent, 0, 0);
	    fWaitingForPollTimer = false;
	    StopInput();
	    OutputControlFrame(kIrLAPFrameRR);  // Note: OutputControlFrame sets poll bit
	    break;

	case kIrTurnaroundTimerExpiredEvent:
	    XTRACE(kPriTransmitEnterEvent, 0, 0);
	    if (!fPendingPutRequests->IsEmpty() && ! fRemoteBusy) { 
		// There is something waiting to be sent.
		TIrPutRequest* putRequest = (TIrPutRequest*)fPendingPutRequests->Last();
		XASSERT(putRequest != nil);
		fPendingPutRequests->RemoveLast();
		
		// Check if there are more put pending.  If not, then I can set p/f and turn the link
		fPutReqsPending = !fPendingPutRequests->IsEmpty();  
		
		// Only start the poll timer if there are multiple frames to send
		if( ( fWindow > 1 ) && fPutReqsPending ) {                  
		    StartTimer(fPollTimerTimeout, kIrPollTimerExpiredEvent);
		}                                                           
		
		OutputDataFrame(putRequest, !fLocalBusy && ((fWindow == 1) || !fPutReqsPending));
	    }
	    else {
/*              // We have nothing to send and the remote is still busy or we are trading RR's.
		// Delay a bit to let the remote process its buffers (1/4 turn around time).  If 
		// something else turned the link then just turn it back with a RR.
		if(     fRemoteBusy || 
		    (   fLastCmdRsp == kIrLAPFrameRR && fRecdCmdRsp == kIrLAPFrameRR ) ) {
		    fWaitingForPollTimer = true;
		    StartTimer( fPollTimerTimeout >> 2, kIrPollTimerExpiredEvent );
		}
		else {
*/                  NextState( kIrPollTimerExpiredEvent );  // so exit state with a RR

/*                  // I received a full window of data last time, so chances are there is more
		    // to come.  Release the link immediately. 
		    if( fRecdCmdRsp == kIrLAPFrameINFO )
			NextState( kIrPollTimerExpiredEvent );  // so exit state with a RR
		    else
		    // This gives the client time to process any data received, instead of
		    // spending all the time trading RRs back and forth.
			StartTimer( fPollTimerTimeout >> 2, kIrPollTimerExpiredEvent );
		}
*/          }
	    break;

	case kIrOutputCompleteEvent:
	    XTRACE(kPriTransmitOutputDoneEvent, fNextCmdRspToSend, 0);
	    switch (fNextCmdRspToSend) {    // this is really the last cmd sent!
		case kIrLAPFrameINFO:
		    if (--fWindow > 0 && fPutReqsPending) {
			// Continue sending requests (if any and until P-timer expires)
			if (!fPendingPutRequests->IsEmpty()) {
			    TIrPutRequest* putRequest = (TIrPutRequest*)fPendingPutRequests->Last();
			    XASSERT(putRequest != nil);
			    fPendingPutRequests->RemoveLast();
			    
			    // Take note of whether additional put requests are pending now
			    fPutReqsPending = !fPendingPutRequests->IsEmpty();

			    // Stop the timer if this is the last frame to send
			    if ( ( fWindow == 1 ) || !fPutReqsPending )  {
				StopTimer();
			    }
			    OutputDataFrame(putRequest, !fLocalBusy && ((fWindow == 1) || !fPutReqsPending));
			}
			break;
		    }
		    else {
			// Reset window
			fWindow = fPeerWindowSize;

			if (fLocalBusy) {
			    // Send RNR if local busy
			    OutputControlFrame(kIrLAPFrameRNR); // Note: OutputControlFrame sets poll bit
			    break;
			}

			else {
			    // fall thru and switch to RECV
			}
		    }

		case kIrLAPFrameRR:
		case kIrLAPFrameRNR:
		    //StartDataReceive();                           // Post the read as soon as possible
		    StartTimer(fFinalTimerTimeout, kIrFinalTimerExpiredEvent);
		    fState = kIrLAPPriReceiveState;
		    StartDataReceive();             // jdg: wait until state change to do read
		    break;

		default:
		    DebugLog("TIrLAP::HandlePriTransmitStateEvent: bad output state");
		    break;
	    }
	    break;

	case kIrPutDataRequestEvent:
	    XTRACE(kPriTransmitPutDataEvent, 0, 0);
	    // Only one choice for primary (now):
	    // If output in progress, queue the request.
	    // If other requests in queue, queue the request.
	    // And if no requests pending we already gave control back to primary, queue the request.
	    // If the p/f isn't set (fPutReqsPending), it may make it out this time around 
	    if (fWaitingForPollTimer && !fRemoteBusy) {     // performance hack
		fWaitingForPollTimer = false;               // don't send out an RR
		StopTimer();
		OutputDataFrame((TIrPutRequest*)GetCurrentEvent(), true);       // always set final bit
	    }
	    else
	    PostponePutRequest();

/*          // Queue up the request if outputting now or other requests in queue (to maintain order)
	    if (fOutputInProgress || !fPendingPutRequests->IsEmpty()) {
		XTRACE(kPriTransmitPutDataEvent, 0, 0);
		PostponePutRequest();
	    }
	    // No requests currently active or pending.  Send this one out.
	    else {
		XTRACE(kPriTransmitPutDataEvent, 1, 0);
		if (fWindow == 1) {
		    StopTimer();
		}
		OutputDataFrame((TIrPutRequest*)GetCurrentEvent(), !fLocalBusy && (fWindow == 1));
	    }
*/          break;

	case kIrCancelPutRequestEvent:
	    CancelPutRequest();
	    break;

	case kIrDisconnectRequestEvent:
	    XTRACE(kPriTransmitDisconnectEvent, 0, 0);
	    XASSERT(fPendingDisconnect == nil);
	    fPendingDisconnect = GetCurrentEvent();
	    fEnteringCloseState = true;
	    XTRACE(kLogEnteringCloseState, 5, 5);
	    //DebugLog("fEnteringCloseState setting true #5");
	    break;

	case kIrLocalBusyClearedEvent:
	    // Don't need to do anything special here - handled in secondary receive
	    // ***FIXME: Could fLocalBusy be cleared here sometimes?  Would it improve performance any?
	    XTRACE(kPriReceiveClrLocBsyPendEvent, 0, 0);
	    XASSERT(fClrLocalBusyPending);
	    break;

	case kIrDiscoverRequestEvent:
	case kIrListenRequestEvent:
	case kIrConnectRequestEvent:
	    XTRACE(kUnexpectedEvent, fState, event);
	    RejectRequest(GetCurrentEvent(), kIrDAErrWrongState);   // don't do it, don't crash
	    break;

	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    //DebugLog("TIrLAP::HandlePriTransmitStateEvent: bad event");
//          fIrDA->ReleaseEventBlock(GetCurrentEvent());    // jdg: free the event
	    break;
    }

} // TIrLAP::HandlePriTransmitStateEvent


//--------------------------------------------------------------------------------
//      HandlePriCloseStateEvent (fState == kIrLAPPriCloseState)
//--------------------------------------------------------------------------------
void TIrLAP::HandlePriCloseStateEvent(ULong event)
{
    switch (event) {
	case kIrFinalTimerExpiredEvent:
	    XTRACE(kPriCloseFinalTimeoutEvent, 0, 0);
	    StopInput();
	    if (++fRetryCount >= kMaxDisconnectRetries) {
		// Apply default conn parms, send disconnect indication, state = NDM
		DisconnectComplete(kIrDAErrTimeout);
	    }
	    else {
		// Resend the disconnect
		OutputControlFrame(kIrLAPCmdDISC);  // Note: OutputControlFrame sets poll bit
	    }
	    break;

	case kIrOutputCompleteEvent:
	    XTRACE(kPriCloseOutputDoneEvent, fNextCmdRspToSend, 0);
	    switch (fNextCmdRspToSend) {
		case kIrLAPCmdDISC:
		    if (fEnteringCloseState) {
			fEnteringCloseState = false;
			fRetryCount = 0;
		    }
		    StartTimer(fFinalTimerTimeout, kIrFinalTimerExpiredEvent);
		    StartInput(fIOBufferItem);
		    break;

		default:
		    DebugLog("TIrLAP::HandlePriCloseStateEvent: bad output state");
		    break;
	    }
	    break;

	case kIrInputCompleteEvent:
	    XTRACE(kPriCloseRecdInputEvent, fRecdCmdRsp, (fRecdNs << 8) | fRecdNr);
	    if (RecdFinalRsp(kIrLAPRspUA) || RecdFinalRsp(kIrLAPRspDM)) {
		// Stop timer, apply default conn parms, send disconnect indication, state = NDM
		DisconnectComplete(kIrDAErrCancel);
	    }
	    else {
		// Continue waiting for the UA/DM response
		StartInput(fIOBufferItem);
	    }
	    break;

	case kIrCancelPutRequestEvent:
	    CancelPutRequest();
	    break;

	case kIrPutDataRequestEvent:
	    NotConnectedCompletion();                               // jdg: Yes, send to CIrLSAP, lmp doesn't handle this
	    //RejectRequest(GetCurrentEvent(), kCommErrNotConnected);   // jdg: send to lmp, not directly to lsapconn
	    break;

	case kIrDiscoverRequestEvent:
	case kIrListenRequestEvent:
	case kIrConnectRequestEvent:
	    XTRACE(kUnexpectedEvent, fState, event);
	    RejectRequest(GetCurrentEvent(), kIrDAErrWrongState);   // don't do it, don't crash
	    break;

	case kIrDisconnectRequestEvent:                     // jdg: hold this until close is done
	    XTRACE(kPriCloseDisconnectRequest, 0, 0);
	    fPendingRequests->InsertLast(GetCurrentEvent());
	    break;
	    
	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TIrLAP::HandlePriCloseStateEvent: bad event");
	    break;
    }

} // TIrLAP::HandlePriCloseStateEvent


//--------------------------------------------------------------------------------
//      HandleSecReceiveStateEvent (fState == kIrLAPSecReceiveState)
//--------------------------------------------------------------------------------
void TIrLAP::HandleSecReceiveStateEvent(ULong event)
{
    switch (event) {
	case kIrWatchdogTimerExpiredEvent:
	    GetIrDevice->Stats_ReceiveTimeout();
	    XTRACE(kSecReceiveWDTimeoutEvent, 0, 0);
	    if (fRetryCount >= fDisconnectLinkLimit) {
		// Apply default conn parms, send disconnect indication, state = NDM
		StopInput();
		DisconnectComplete(kIrDAErrTimeout);
	    }
	    else {
		if (fRetryCount == fDisconnectWarningLimit) {
		    fInBrokenBeam = true;
		    //gConnectionBroken = true;
		    
		    //if( gIrDAPrefs.DoBrokenBeamWarning() )
		    //  BrokenBeamNotification( this );
		}
		//if( fRetryCount == 0 ) {              // Remember time of first retry
		//  fInitialRetryTime = TickCount();    // and smack the hardware
		//  fNextKickScc = fInitialRetryTime + 60 * 4;  // in 4 seconds if still deaf
		//}
		fRetryCount++;
		StartTimer(fWatchdogTimeout, kIrWatchdogTimerExpiredEvent);
	    }
	    break;

	case kIrTurnaroundTimerExpiredEvent:
	    // Check for pending disconnect
	    if (fEnteringCloseState) {
		fNextCmdRspToSend = fRespondingToDisconnect ? kIrLAPRspUA : kIrLAPRspRD;
		fState = kIrLAPSecCloseState;
	    }

	    // Check for pending FRMR response
	    else if (fFRMRPending) {
		OutputFRMRResponse();
		fFRMRPending = false;
		break;
	    }

	    // Check for set localBusy pending and set it and notify peer device
	    else if (fSetLocalBusyPending) {
		XTRACE(kSetLocalBusyEvent, 0, 0);
		fNextCmdRspToSend = kIrLAPFrameRNR;
		fLocalBusy = true;
		fSetLocalBusyPending = false;
	    }

	    // Check for clr localBusy pending and clear it and notify peer device
	    else if (fClrLocalBusyPending) {
		XTRACE(kClrLocalBusyEvent, 0, 0);
		fNextCmdRspToSend = kIrLAPFrameRR;
		fLocalBusy = false;
		fClrLocalBusyPending = false;
	    }

	    // Can we begin transmitting now?
	    else if (fNextCmdRspToSend == kIrLAPFrameINFO) {
		if (fRemoteBusy || fPendingPutRequests->IsEmpty()) {
		    fNextCmdRspToSend = fLocalBusy ? kIrLAPFrameRNR : kIrLAPFrameRR;
		}
		else {
		    fValidRecdNr = 0;
		    fState = kIrLAPSecTransmitState;
		    NextState(kIrTurnaroundTimerExpiredEvent);
		    break;
		}
	    }
	    OutputControlFrame(fNextCmdRspToSend);  // Note: OutputControlFrame sets final bit
	    break;

	case kIrOutputCompleteEvent:
	    XTRACE(kSecReceiveOutputDoneEvent, fNextCmdRspToSend, 0);
	    switch (fNextCmdRspToSend) {
		case kIrLAPFrameRR:
		case kIrLAPFrameRNR:
		case kIrLAPRspFRMR:
		    // Finish up after sending RR/RNR/FRMR
		    StartDataReceive();
		    StartTimer(fWatchdogTimeout, kIrWatchdogTimerExpiredEvent);
		    break;

		default:
		    DebugLog("TIrLAP::HandleSecReceiveStateEvent: bad output state");
		    break;
	    }
	    break;

	case kIrInputCompleteEvent:
	    XTRACE(kSecReceiveRecdInputEvent, fRecdCmdRsp, (fRecdNs << 8) | fRecdNr);

	    // Stop the Watchdog-timer if received frame with Poll bit set
	    if (RecdPoll()) {
		StopTimer();
	    }

	    // We received some input so reset the retry count that is used for the
	    // disconnect warning or disconnect link actions in watchdog timeout.
	    fRetryCount = 0;

#if forMac
	    fInitialRetryTime   = 0;
	    //fNextKickScc      = 0;
	    fInBrokenBeam       = false;
	    //gConnectionBroken     = false;
#endif          
	    // Init most common cases for responses
	    fNextCmdRspToSend = kIrLAPFrameINFO;

	    // Ignore any non-poll bit set frames if in "error state" (waiting to send FRMR)
	    if (fFRMRPending && !RecdPoll()) {
		// Just ignore it
	    }

	    // Handle received data frames and supervisor commands
	    else if (!RecdUFrame() || RecdCmd(kIrLAPRspUI)) {
		if (RecdInvalidNrOrNs()) {
		    // Record this as a protocol error
		    fProtocolErrs++;
		    // Prepare FRMR response - send it when we receive poll bit
		    PrepareFRMRResponse();
		}
		else {
		    ProcessRecdInfoOrSuperFrame();
		}
	    }
	    //
	    else if (RecdPollCmd(kIrLAPCmdDISC)) {
		// Reply to the disconnect
		fEnteringCloseState = true;
		XTRACE(kLogEnteringCloseState, 6, 6);
		//DebugLog("fEnteringCloseState setting true #6");
		fRespondingToDisconnect = true;
	    }
	    //
	    else if (RecdPollCmd(kIrLAPCmdSNRM)) {
		if (fRecdAddr == fConnAddr) {
		    // Reject the SNRM (reset not supported by IrLAPConn)
		    fEnteringCloseState = true;
		    XTRACE(kLogEnteringCloseState, 7, 7);
		    //DebugLog("fEnteringCloseState setting true #7");
		    fRespondingToDisconnect = false;
		}
		else {
		    // Ignore connection SNRMs from other sources
		    // Kind of a hack forcing poll bit off so StartDataReceive is called below
		    fRecdPF = 0;
		}
	    }
	    //
	    else if (RecdFinalRsp(kIrLAPRspDM)) {
		// Apply default conn parms, send disconnect indication, state = NDM
		DisconnectComplete(kIrDAErrCancel);
	    }
	    // Received some other unrecognized command - send FRMR
	    else {
		// Record this as a protocol error
		fProtocolErrs++;
		// Send FRMR
		PrepareFRMRResponse();
	    }

	    // If didn't disconnect for some reason above, receive again or xmit after delay
	    if (fState == kIrLAPSecReceiveState) {
		if (RecdPoll()) {
		    // If received poll, transmitting next (after min turnaround delay)
		    StartTimer(fMinTurnAroundTimeout, kIrTurnaroundTimerExpiredEvent);
//                  StartMinTurnAroundTimer( fMinTurnAroundTimeout, kIrTurnaroundTimerExpiredEvent);
		}
		else {
		    // If didn't receive poll, receive again
		    StartDataReceive();
		}
	    }
	    break;

	case kIrPutDataRequestEvent:
	    XTRACE(kSecReceivePutDataEvent, 0, 0);
	    PostponePutRequest();
	    break;

	case kIrCancelPutRequestEvent:
	    CancelPutRequest();
	    break;

	case kIrDisconnectRequestEvent:
	    XTRACE(kSecReceiveDisconnectEvent, 0, 0);
	    XASSERT(fPendingDisconnect == nil);
	    fPendingDisconnect = GetCurrentEvent();
	    fEnteringCloseState = true;
	    //DebugLog("fEnteringCloseState setting true #8");
	    XTRACE(kLogEnteringCloseState, 8, 8);
	    fRespondingToDisconnect = false;
	    break;

	case kIrLocalBusyClearedEvent:
	    // Don't need to do anything special here - handled above
	    XTRACE(kSecReceiveClrLocBsyPendEvent, 0, 0);
	    XASSERT(fClrLocalBusyPending);
	    break;

	case kIrConnectRequestEvent:                                    // jdg added
	case kIrListenRequestEvent:                                     // jdg added
	    XTRACE(kSecReceiveConnLstnRequest, 0, 0);
	    check(fCurrentRequest == nil);
	    fCurrentRequest = GetCurrentEvent();                    // hack so we can use ConnLstnComplete
	    ConnLstnComplete(noErr);
	    break;

	case kIrDiscoverRequestEvent:
	    XTRACE(kUnexpectedEvent, fState, event);
	    RejectRequest(GetCurrentEvent(), kIrDAErrWrongState);   // don't do it, don't crash
	    break;

	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TIrLAP::HandleSecReceiveStateEvent: bad event");
	    break;
    }

} // TIrLAP::HandleSecReceiveStateEvent


//--------------------------------------------------------------------------------
//      HandleSecTransmitStateEvent (fState == kIrLAPSecTransmitState)
//--------------------------------------------------------------------------------
void TIrLAP::HandleSecTransmitStateEvent(ULong event)
{
    switch (event) {
	case kIrTurnaroundTimerExpiredEvent:
	    {
		XTRACE(kSecTransmitEnterEvent, 0, 0);
		// We just entered the transmit state
		// Should only be here if there are pending put requests and remote is not busy
		XASSERT(!fPendingPutRequests->IsEmpty() && !fRemoteBusy);
		TIrPutRequest* putRequest = (TIrPutRequest*)fPendingPutRequests->Last();
		XASSERT(putRequest != nil);
		fPendingPutRequests->RemoveLast();
		// Take note of whether additional put requests are pending now, because there
		// may not be any now, but a request might come in while the data is being
		// output, but we're locked in now and need to finish what we started after the
		// output completes (in other words, if we set poll bit here because no more
		// reqs pending (vs window == 1), then we have to give control to secondary after
		// the output whether another request comes in or not).
		fPutReqsPending = !fPendingPutRequests->IsEmpty();
		OutputDataFrame(putRequest, !fLocalBusy && ((fWindow == 1) || !fPutReqsPending));
	    }
	    break;

	case kIrOutputCompleteEvent:
	    XTRACE(kSecTransmitOutputDoneEvent, fNextCmdRspToSend, 0);
	    switch (fNextCmdRspToSend) {
		case kIrLAPFrameINFO:
		    if ((--fWindow > 0) && fPutReqsPending) {
			// Continue sending requests (if pending and peer device has room)
			TIrPutRequest* putRequest = (TIrPutRequest*)fPendingPutRequests->Last();
			XASSERT(putRequest != nil);
			fPendingPutRequests->RemoveLast();
			// Take note of whether additional put requests are pending now (see note above)
			fPutReqsPending = !fPendingPutRequests->IsEmpty();
			OutputDataFrame(putRequest, !fLocalBusy && ((fWindow == 1) || !fPutReqsPending));
			break;
		    }
		    else {
			// Reset window
			fWindow = fPeerWindowSize;

			if (fLocalBusy) {
			    // Send RNR if local busy
			    OutputControlFrame(kIrLAPFrameRNR); // Note: OutputControlFrame sets final bit
			    break;
			}

			else {
			    // fall thru and switch to RECV
			}
		    }

		case kIrLAPFrameRR:
		case kIrLAPFrameRNR:
		    StartTimer(fWatchdogTimeout, kIrWatchdogTimerExpiredEvent);
		    fState = kIrLAPSecReceiveState;
		    StartDataReceive();         // jdg: change state before issuing read - can come right back
						// with some data ... before I get a chance to changestate!
		    break;

		default:
		    DebugLog("TIrLAP::HandleSecTransmitStateEvent: bad output state");
		    break;
	    }
	    break;

	case kIrPutDataRequestEvent:
	    XTRACE(kSecTransmitPutDataEvent, 0, 0);
	    // Only one choice for secondary:
	    // If output in progress, queue the request.
	    // If other requests in queue, queue the request.
	    // And if no requests pending we already gave control back to primary, queue the request.
	    PostponePutRequest();
	    break;

	case kIrCancelPutRequestEvent:
	    CancelPutRequest();
	    break;

	case kIrDisconnectRequestEvent:
	    XTRACE(kSecTransmitDisconnectEvent, 0, 0);
	    XASSERT(fPendingDisconnect == nil);
	    fPendingDisconnect = GetCurrentEvent();
	    fEnteringCloseState = true;
	    //DebugLog("fEnteringCloseState setting true #9");
	    XTRACE(kLogEnteringCloseState, 9, 9);
	    break;

	case kIrLocalBusyClearedEvent:
	    // Don't need to do anything special here - handled in secondary receive
	    // ***FIXME: Could fLocalBusy be cleared here sometimes?  Would it improve performance any?
	    XTRACE(kSecTransmitClrLocBsyPendEvent, 0, 0);
	    XASSERT(fClrLocalBusyPending);
	    break;

	case kIrConnectRequestEvent:                                    // jdg added
	case kIrListenRequestEvent:                                     // jdg added
	    XTRACE(kSecTransmitConnLstnRequest, 0, 0);
	    check(fCurrentRequest == nil);
	    fCurrentRequest = GetCurrentEvent();                    // hack so we can use ConnLstnComplete
	    ConnLstnComplete(noErr);
	    break;

	case kIrDiscoverRequestEvent:
	    XTRACE(kUnexpectedEvent, fState, event);
	    RejectRequest(GetCurrentEvent(), kIrDAErrWrongState);   // don't do it, don't crash
	    break;

	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TIrLAP::HandleSecTransmitStateEvent: bad event");
	    break;
    }

} // TIrLAP::HandleSecTransmitStateEvent


//--------------------------------------------------------------------------------
//      HandleSecCloseStateEvent (fState == kIrLAPSecCloseState)
//--------------------------------------------------------------------------------
void TIrLAP::HandleSecCloseStateEvent(ULong event)
{
    switch (event) {
	case kIrWatchdogTimerExpiredEvent:
	    XTRACE(kSecCloseWDTimeoutEvent, 0, 0);
	    StopInput();
	    // Apply default conn parms, send disconnect indication, state = NDM
	    DisconnectComplete(kIrDAErrTimeout);
	    break;

	case kIrTurnaroundTimerExpiredEvent:
	    OutputControlFrame(fNextCmdRspToSend);  // Note: OutputControlFrame sets final bit
	    break;

	case kIrOutputCompleteEvent:
	    XTRACE(kSecCloseOutputDoneEvent, fNextCmdRspToSend, 0);
	    switch (fNextCmdRspToSend) {
		case kIrLAPRspRD:
		    fEnteringCloseState = false;
		    StartTimer(fWatchdogTimeout, kIrWatchdogTimerExpiredEvent);
		    StartInput(fIOBufferItem);
		    break;

		case kIrLAPRspUA:
		    // Apply default conn parms, send disconnect indication, state = NDM
		    DisconnectComplete(kIrDAErrCancel);
		    break;

		default:
		    DebugLog("TIrLAP::HandleSecCloseStateEvent: bad output state");
		    break;
	    }
	    break;

	case kIrInputCompleteEvent:
	    XTRACE(kSecCloseRecdInputEvent, fRecdCmdRsp, 0);
	    // Stop the watchdog timer
	    StopTimer();

	    if (RecdPollCmd(kIrLAPCmdDISC)) {
		// Acknowledge the disconnect (after min turnaround delay)
		fNextCmdRspToSend = kIrLAPRspUA;
		StartTimer(fMinTurnAroundTimeout, kIrTurnaroundTimerExpiredEvent);
//              StartMinTurnAroundTimer( fMinTurnAroundTimeout, kIrTurnaroundTimerExpiredEvent);
	    }
	    else if (RecdFinalRsp(kIrLAPRspDM)) {
		// Stop timer, apply default conn parms, send disconnect indication, state = NDM
		DisconnectComplete(kIrDAErrCancel);
	    }
	    else if (RecdPoll()) {
		// Resend the RD (after min turnaround delay)
		fNextCmdRspToSend = kIrLAPRspRD;
		StartTimer(fMinTurnAroundTimeout, kIrTurnaroundTimerExpiredEvent);
//              StartMinTurnAroundTimer( fMinTurnAroundTimeout, kIrTurnaroundTimerExpiredEvent);
	    }
	    else {
		// Continue waiting for the DISC/DM response
		StartTimer(fWatchdogTimeout, kIrWatchdogTimerExpiredEvent);
		StartInput(fIOBufferItem);
	    }
	    break;

	case kIrCancelPutRequestEvent:
	    CancelPutRequest();
	    break;

	case kIrPutDataRequestEvent:
	    NotConnectedCompletion();               // jdg - yes, pls do send back to CIrLSAP, not to lmp which drops it
	    //RejectRequest(GetCurrentEvent(), kCommErrNotConnected);   // jdg: send to lmp, not directly to lsapconn
	    break;

	case kIrDiscoverRequestEvent:
	case kIrListenRequestEvent:
	case kIrConnectRequestEvent:
	    XTRACE(kUnexpectedEvent, fState, event);
	    RejectRequest(GetCurrentEvent(), kIrDAErrWrongState);   // don't do it, don't crash
	    break;

	case kIrDisconnectRequestEvent:                     // jdg: hold this until close is done
	    XTRACE(kSecCloseDisconnectRequest, 0, 0);
	    fPendingRequests->InsertLast(GetCurrentEvent());
	    break;
	    
	default:
	    XTRACE(kUnexpectedEvent, fState, event);
	    DebugLog("TIrLAP::HandleSecCloseStateEvent: bad event");
	    break;
    }

} // TIrLAP::HandleSecCloseStateEvent


//--------------------------------------------------------------------------------
//      UpdateNrReceived
//--------------------------------------------------------------------------------
void TIrLAP::UpdateNrReceived()
{
    XTRACE(kUpdateNrReceived, fNextToAck, fRecdNr);

    while (fNextToAck != fRecdNr) {
	TIrPutReply* putReply = (TIrPutReply*)fPutRequests[fNextToAck];
	fPutRequests[fNextToAck] = nil;

	// Send back reply (or in some cases free up the request block)
	PutComplete(putReply, noErr);

	// Clear the fValidRecdNr bit that corresponds to the "ack'd"
	fValidRecdNr &= ~(1 << fNextToAck);

	// Check next to ack (taking wrapping into account)
	fNextToAck = (fNextToAck + 1) & 0x7;
    }

} // TIrLAP::UpdateNrReceived


//--------------------------------------------------------------------------------
//      ResendRejectedFrames
//--------------------------------------------------------------------------------
void TIrLAP::ResendRejectedFrames()
{
    // ***FIXME: Should the fValidRecdNr bits be "reduced" by the amount resent
    // And if not, should the fPutRequests be nil'd out or left alone?

    ULong index = (fVs - 1) & 0x7;

    XTRACE(kResendRejectedFrames, 0, index);        // debug
    
    // Go thru the list backwards so frames are resent in the correct order
    while (true) {
	TIrPutRequest* putRequest = fPutRequests[index];
	XASSERT(putRequest != nil);
	if (putRequest == nil) {
	    // Client must have cancelled put and peer device is asking for resend
	    // Just get out of here, because things will be shut down soon anyway.
	    break;
	}
	XTRACE(kResendRejectedFrames, 1, index);        // jdg
	
	fPendingPutRequests->InsertLast(putRequest);
							
	fPutRequests[index] = nil;
	fDataRetries++;
#if forMac
	GetIrDevice->Stats_PacketResent();
	// testing TEMP TEMP TEMP TEMP TEMP
	//if (fIrSIR->fIRStats.resent == 100) { // if hit 100 resends
	//  extern struct IrDALogHdr gIrDALog;
	//  gIrDALog.fTracingOn = false;        // HACK turn off tracing so we can dump w/tool
	//  DebugLog("Ok, 100 resends. logging turned off, dump the log");
	//}
#endif
	if (index == fRecdNr) {
	    break;
	} else {
	    index = (index - 1) & 0x7;
	}
    }

    // Back up Vs, ready for the first resend
    fVs = fRecdNr;

    XTRACE(kResendRejectedFrames, 2, fVs);          // jdg

} // TIrLAP::ResendRejectedFrames


//--------------------------------------------------------------------------------
//      ProcessRecdInfoOrSuperFrame
//--------------------------------------------------------------------------------
void TIrLAP::ProcessRecdInfoOrSuperFrame()
{
    // This is a common routine used by both primary and secondary receive states
    // It should only be called for information/supervisory frames or the UI frame.
    // It is assumed that the invalid Ns/Nr case has already been checked for.
    // Also if the F/P bit was received the caller should have already stopped the timer.
    // And on entry, fNextCmdRspToSend is defaulted (preset) to kIrLAPFrameINFO.
    
    XTRACE(kProcessISFrame, RecdCmd(), RecdRsp());
    
    if (!Recd(kIrLAPRspUI)) {
	// Primary should only receive responses, secondary should only receive commands
	if ((fPrimary && RecdCmd()) || (!fPrimary && RecdRsp())) {
	    // Record this as a protocol error
	    fProtocolErrs++;
	    // Stop timer, apply default conn parms, send disconnect indication, state = NDM
	    DisconnectComplete(kIrDAErrPacket);
	    return;
	}
	// Update Nr received - i.e. reply to put requests, remove buffers from list
	UpdateNrReceived();
    }

    switch(fRecdCmdRsp) {
	case kIrLAPFrameINFO:
	    // Process received data if not fLocalBusy (or local busy pending)
	    if (!fLocalBusy && !fSetLocalBusyPending) {
		if (RecdUnexpectedNs()) {
		    // Not used if not final/poll frame but doesn't hurt to set it
		    fNextCmdRspToSend = kIrLAPFrameRR;
#if forMac
		    GetIrDevice->Stats_PacketDropped();
#endif
		}
		else {
		    // This should never happen!
		    XASSERT(fInputBuffer != fIOBufferItem);
		    if (fInputBuffer == fIOBufferItem) {    // Debugging
			DebugLog("oops, fInputbuffer == fIObufferItem 0x%lx", (uintptr_t) fInputBuffer);
			//IrDALogTracingOff();              // Debugging!
		    }
		    
		    // Let the demultipexor know that a buffer has arrived
		    GetLMP->Demultiplexor(fInputBuffer);

		    // Clear the fValidRecdNs bit that corresponds to current fVr
		    fValidRecdNs &= ~(1 << fVr);

		    // Update fValidRecdNs to include next maximum allowed received Ns
		    fValidRecdNs |= 1 << ((fVr + fMyWindowSize) & 0x7);

		    // Record that I received/accepted a frame
		    fVr = (fVr + 1) & 0x7;

		    // Next receive requires new buffer
		    //fNeedNewInputBuffer = true;
		    fInputBuffer = nil;

		    // If we just passed a buffer to the demultiplexor and there was not
		    // a "get" request pending to consume the data immediately and there
		    // are no more buffers then we are entering a local busy condition.
		    if (fGetBufferAvail == 0) {
			XTRACE(kSetLocalBusyPendingEvent, 0, 0);
			fSetLocalBusyPending = true;
		    }

		    if (RecdUnexpectedNr()) {
			ResendRejectedFrames();
		    }
		}
	    }
#if forMac
	    else
		GetIrDevice->Stats_PacketDropped();         // Packet was dropped due to flow control
#endif
	    break;

	case kIrLAPFrameRR:
	    fRemoteBusy = false;
	    if (RecdUnexpectedNr() && !fLocalBusy) {
		ResendRejectedFrames();
	    }
#if forMac
	    GetIrDevice->Stats_RRRec();
#endif
	    break;

	case kIrLAPFrameREJ:
	    ResendRejectedFrames();
#if forMac
	    GetIrDevice->Stats_REJRec();
#endif
	    break;

	case kIrLAPFrameSREJ:
	    ResendRejectedFrames();
#if forMac
	    GetIrDevice->Stats_SREJRec();
#endif
	    break;

	case kIrLAPFrameRNR:
	    fRemoteBusy = true;
	    fNextCmdRspToSend = fLocalBusy ? kIrLAPFrameRNR : kIrLAPFrameRR;
#if forMac
	    GetIrDevice->Stats_RNRRec();                // Remote is in flow control
#endif
	    break;

	default:
	    XASSERT(fRecdCmdRsp == kIrLAPRspUI);
	    if (!fLocalBusy) {
		// ***FUTURE: Handle ui here

		// Next receive requires new buffer
		// ***FUTURE: Put back next line when/if actual ui receiver implemented
		// fNeedNewInputBuffer = true;
	    }
	    break;
    }

} // TIrLAP::ProcessRecdInfoOrSuperFrame


//--------------------------------------------------------------------------------
//      CopyStatsTo
//--------------------------------------------------------------------------------
void TIrLAP::CopyStatsTo(TCMOSlowIRStats* irStats)
{
#if forMac
#pragma unused(irStats)
#else
    // Just copy the stats that IrLAP modifies
    irStats->dataRetries = fDataRetries;
    irStats->protocolErrs = fProtocolErrs;
#endif

} // TIrLAP::CopyStatsTo


//--------------------------------------------------------------------------------
//      ResetStats
//--------------------------------------------------------------------------------
void TIrLAP::ResetStats()
{
    // Just clear the fields that IrLAP modifies
    fDataRetries = 0;
    fProtocolErrs = 0;

} // TIrLAP::ResetStats


//=============================== Utility methods ================================


//--------------------------------------------------------------------------------
//      ParseNegotiateAndInitConnState
//--------------------------------------------------------------------------------
IrDAErr TIrLAP::ParseNegotiateAndInitConnState(Boolean primary)
{
    // **FIXME: Should this be broken up a bit to make patching easier?

    IrDAErr result;
    ULong numBuffers;
    ULong bufferSize;
    CBufferSegment* bufferItem;
    TTimeout retryTimeUnit;
    TTimeout disconnectLinkTime;

    fPrimary = primary;

    // Parse negotiation parms passed (may not be any, don't panic - use defaults)
    result = fPeerQOS->ExtractInfoFromBuffer(fInputBuffer);
    XREQUIRENOT(result, Fail_QOS_ExtractInfo);

    // Negotiate conn parms
    result = fMyQOS->NegotiateWith(fPeerQOS);
    XREQUIRENOT(result, Fail_QOS_NegotiateMe);
    result = fPeerQOS->NegotiateWith(fMyQOS);
    XREQUIRENOT(result, Fail_QOS_NegotiatePeer);

    // Apply conn parms (that can be) ahead of time

    // Note: DON'T change baud rate yet
    XASSERT(fMyQOS->GetBaudRate() == fPeerQOS->GetBaudRate());
    XTRACE(kQOSSetBaudRateEvent, 0, fMyQOS->GetBaudRate());

    // Set various timeout values (Note: WD timeout is 1.25x time primary can hold link)
    fPollTimerTimeout = fMyQOS->GetMaxTurnAroundTime();
    fFinalTimerTimeout = fPeerQOS->GetMaxTurnAroundTime();

    // HACK -- since we don't really wait for our transmit queue to empty before
    // turning the link around, we have to bump up the time we wait for our peer's
    // response by the time we could be spending transmitting.  
    // TEST == incr finaltimer (and polltimer) by our max time
    //fPollTimerTimeout  += fMyQOS->GetMaxTurnAroundTime();
    //fFinalTimerTimeout += fMyQOS->GetMaxTurnAroundTime();
    // TEMP TEMP.  TODO: impl real output complete so the timer is correct?

    //fPollTimerTimeout = fPeerQOS->GetMaxTurnAroundTime();
    //fFinalTimerTimeout = fMyQOS->GetMaxTurnAroundTime();
    //fFinalTimerTimeout += fPollTimerTimeout;      // JDG HACKING

    fWatchdogTimeout = fPeerQOS->GetMaxTurnAroundTime() + (fPeerQOS->GetMaxTurnAroundTime() >> 2);
    fMinTurnAroundTimeout = fPeerQOS->GetMinTurnAroundTime();
    //DebugLog(" test - setting peer min ttime to 10 ms;g");
    //fMinTurnAroundTimeout = 10;       // TEST TEST TEST new hardware

    // Set link disconnect warning/limit times
    retryTimeUnit = primary ? fFinalTimerTimeout : fWatchdogTimeout;

    // Note: addition of (retryTimeUnit / 2) is to round up for better accuracy
    disconnectLinkTime = fMyQOS->GetLinkDiscThresholdTime();
    fDisconnectLinkLimit = (disconnectLinkTime + (retryTimeUnit >> 1)) / retryTimeUnit;
    // Calculate the BB duration time.  Adjust for one retry at the beginning.
    fDisconnectLinkLimitTime = fDisconnectLinkLimit * retryTimeUnit - retryTimeUnit;

    if (disconnectLinkTime == kIrDisconnectWarningTimeout) {
	// Special case: warning time is 0 seconds if link disconnect at 3 seconds
	fDisconnectWarningLimit = 0;
    }
    else {
	// Note: addition of (retryTimeUnit / 2) is to round up for better accuracy
	fDisconnectWarningLimit = ((kIrDisconnectWarningTimeout + (retryTimeUnit >> 1)) / retryTimeUnit) - 1;
    }

    // Set number of frames that can be sent to peer before giving up link
    fPeerWindowSize = (UByte)fPeerQOS->GetWindowSize();

    // Set number of frames that peer can send to me before giving up link
    fMyWindowSize = (UByte)fMyQOS->GetWindowSize();

    // Allocate receive buffers
    numBuffers = fMyQOS->GetWindowSize() * 2;   // FIXME - post alpha allocate extra to avoid assert
    bufferSize = fMyQOS->GetDataSize()+5;       // Add room for Addr, CNTL, CRC and 1 for DMA

    check(numBuffers <= (sizeof(fGetBuffers) / sizeof(fGetBuffers[0])));    // sanity
    check(numBuffers < 32);                                                 // bitmask allocation
    // Allocate buffers for receiving data
    for (fGetBufferAvail = 0, fNumGetBuffers = 0; fNumGetBuffers < numBuffers; fNumGetBuffers++) {
	// Allocate, init buffer segment and check for errors
	result = kIrDAErrNoMemory;
	bufferItem = CBufferSegment::New( bufferSize );
	XREQUIRE(bufferItem, Fail_BufferItem_New);

	// Add next buffer list
	fGetBuffers[fNumGetBuffers] = bufferItem;
	fGetBufferAvail |= (ULong) (1 << fNumGetBuffers);
    }

    // First my info
    XTRACE(kQOSBufferInfoEvent, fMyQOS->GetWindowSize(), fMyQOS->GetDataSize());
    XTRACE(kQOSLeadInCountEvent, 0, fMyQOS->GetExtraBOFs());
    XTRACE(kQOSMinTurnAroundEvent, 0, fMyQOS->GetMinTurnAroundTime() / kMicroseconds);
    XTRACE(kQOSMaxTurnAroundEvent, 0, fMyQOS->GetMaxTurnAroundTime() / kMilliseconds);

    // Then the peer device's info
    XTRACE(kQOSBufferInfoEvent, fPeerQOS->GetWindowSize(), fPeerQOS->GetDataSize());
    XTRACE(kQOSLeadInCountEvent, 1, fPeerQOS->GetExtraBOFs());
    XTRACE(kQOSMinTurnAroundEvent, 1, fPeerQOS->GetMinTurnAroundTime() / kMicroseconds);
    XTRACE(kQOSMaxTurnAroundEvent, 1, fPeerQOS->GetMaxTurnAroundTime() / kMilliseconds);

    // Initialize connection state
    fVr = 0;
    fVs = 0;
    fWindow = fPeerWindowSize;
    fValidRecdNs = 0xFF >> (8 - fMyWindowSize);

    fRetryCount = 0;
#if forMac
    fInitialRetryTime   = 0;
    //fNextKickScc      = 0;
    fInBrokenBeam       = false;
    //gConnectionBroken     = false;
#endif

    fRemoteBusy = false;
    fLocalBusy = false;
    fSetLocalBusyPending = false;
    fClrLocalBusyPending = false;

    fRecdNr = 0;
    fRecdNs = 0;
    fNextToAck = 0;

    return noErr;

    // Error exits:

//Fail_BufferItem_Init:
//  delete bufferItem;

Fail_BufferItem_New:
    FreeGetBuffers();

Fail_QOS_NegotiatePeer:
Fail_QOS_NegotiateMe:
Fail_QOS_ExtractInfo:

    ConnLstnComplete(result);
    return result;

} // TIrLAP::ParseNegotiateAndInitConnState


//--------------------------------------------------------------------------------
//      ConnLstnComplete
//--------------------------------------------------------------------------------
void TIrLAP::ConnLstnComplete(IrDAErr result)
{
    // Let the initiator know that the connect/listen has completed
    TIrConnLstnReply* connLstnReply = (TIrConnLstnReply*)fCurrentRequest;
    XTRACE(kConnLstnComplete, fPeerDevAddr >> 16, fPeerDevAddr);
    fCurrentRequest = nil;
    //XASSERT(connLstnReply != nil);    // jdg, this can happen if we're not doing much and get connected to
    if (connLstnReply) {            // jdg - if there was a current request (can be listen, connect or discover)
	//XASSERT((connLstnReply->fEvent == kIrConnectRequestEvent) || (connLstnReply->fEvent == kIrListenRequestEvent));
	if ((connLstnReply->fEvent == kIrConnectRequestEvent) || (connLstnReply->fEvent == kIrListenRequestEvent)) {
	    connLstnReply->fEvent = (UByte)RequestIdToReplyId(connLstnReply->fEvent);
	    connLstnReply->fPassiveConnect = (connLstnReply->fEvent == kIrConnectReplyEvent) && !fPrimary;
	    connLstnReply->fResult = result;
	    connLstnReply->fDevAddr = fPeerDevAddr; // Only listen needs this, already set for connect
	}
	else {      // we have a request, but it's not listen or connect (probably discover).  reject it.
	    if (connLstnReply->fEvent == kIrDiscoverRequestEvent) {
		RejectRequest(connLstnReply, kIrDAErrRetry);	// send it back with an error
		connLstnReply = nil;				// don't do it again!  nil out for below
	    }
	    else {
		XTRACE(kConnLstnComplete, (UInt16)-2, (UInt16)-2);      // bug: can have a listen complete ...
		// just ignore it for now ...
	    }
	}
    }
    else {
	XTRACE(kConnLstnComplete, (UInt16)-1, (UInt16)-1);      // jdg: what to do here?
    }
    // Connect/listen failed?
    if (result != noErr) {
	fState = kIrLAPDisconnectedState;
    }
    if (connLstnReply)      // jdg
	GetLMP->EnqueueEvent(connLstnReply);

} // TIrLAP::ConnLstnComplete


//--------------------------------------------------------------------------------
//      DisconnectComplete
//--------------------------------------------------------------------------------
void TIrLAP::DisconnectComplete(IrDAErr result)
{
    // Its not good to complete disconnect with okay
    XASSERT(result != noErr);

    // Stop the Final/Poll/Watchdog timer (if it was on - nop if it wasn't)
    StopTimer();

    // Strickly speaking this shouldn't be necessary if this has been called properly
    // But its a good idea to make sure that they're really "stopped" before shutting down
    // At least check to see if calling code has some logic errors/flaws/omissions/opportunities
    XASSERT(!fInputInProgress);
    XASSERT(!fOutputInProgress);
    StopInput();
    StopOutput();

    // Complete all in progress and pending put requests
    CancelPendingPutRequests(nil, kIrDAErrCancel);


    // Reset some state in case we are re-connected to.
#if forMac
    fInBrokenBeam = false;
    //gConnectionBroken = false;
    fConnected = false;                     // JDG.  we're no longer connected
    fNickName[0] = 0;                       // Clear out the connect name.

#endif

    fLocalBusy = false;
    fSetLocalBusyPending = false;
    fEnteringCloseState = false;
    fFRMRPending = false;
    //fNeedNewInputBuffer = true;       // jdg - this is now cleared in FreeGetBuffers

    fState = kIrLAPDisconnectedState;

    // Notify initiator of disconnect or notify clients via async event response
    if (fPendingDisconnect) {
	TIrDisconnectReply* disconnectReply = (TIrDisconnectReply*)fPendingDisconnect;
	fPendingDisconnect = nil;
	disconnectReply->fEvent = kIrDisconnectReplyEvent;
	disconnectReply->fResult = result;
	XTRACE(kNRMDisconnectReply, fState, 0);
	GetLMP->EnqueueEvent(disconnectReply);
	fIrDA->Disconnected(false);     // tell glue to reset qos only
	Reset();            // JDG: FreeGetBuffers (else memory leak on next connection)
    }
    else {
	// async disconnect
	XTRACE(kNRMDisconnectAsyncReply, fState, 0);  // FIXME - This doesn't look good for mac
	fIrDA->Disconnected(true);      // tell glue to reset qos and lap and lmp and lapconn ...
    }

    // Reset back to NDM state (9600bps, disconnected, etc.)
    ApplyDefaultConnParms();

    // jdg: now that we're back to normal, requeue any requests that came in while
    // we were waiting for the close state to exit
    if (fPendingRequests && !fPendingRequests->Empty()) {
	CListIterator *iter = CListIterator::cListIterator(fPendingRequests);
	TIrEvent *request;
	for (request = (TIrEvent*)iter->FirstItem();
	    iter->More(); request = (TIrEvent*)iter->NextItem()) {
		XTRACE(kNRMDisconnectRequeue,0, request->fEvent);
		// Send the reply to ourselves
		this->EnqueueEvent(request);
	}
	iter->release();
	while (!fPendingRequests->Empty())      // now let's purge the list
	    fPendingRequests->RemoveLast();
    }


} // TIrLAP::DisconnectComplete


//--------------------------------------------------------------------------------
//      CancelPutRequest
//--------------------------------------------------------------------------------
void TIrLAP::CancelPutRequest()
{
    TIrCancelPutRequest* cancelPutRequest = (TIrCancelPutRequest*)GetCurrentEvent();

    // Cancel all pending (or in progress) put requests for the lsapConn which made the request
    CancelPendingPutRequests(cancelPutRequest->fLSAPConn, kIrDAErrRequestCanceled);

    // Respond to the request, using the request block for the response
    cancelPutRequest->fEvent = kIrCancelPutReplyEvent;
    cancelPutRequest->fResult = noErr;
    cancelPutRequest->fLSAPConn->EnqueueEvent(cancelPutRequest);

} // TIrLAP::CancelPutRequest


//--------------------------------------------------------------------------------
//      CancelPendingPutRequests
//--------------------------------------------------------------------------------
void TIrLAP::CancelPendingPutRequests(TLSAPConn* lsapConn, IrDAErr returnCode)
{
    FastInt index;
    TIrPutReply* putReply;

    // Reply to all in-progress put requests
    for (index = 0; index < 8; index++) {
	putReply = (TIrPutReply*)fPutRequests[index];
	if ((putReply != nil) && ((lsapConn == nil) || (lsapConn == putReply->fLSAPConn))) {
	    fPutRequests[index] = nil;
	    PutComplete(putReply, returnCode);
	}
    }

    // Reply to all pending put requests (must remove from end, since holes will be created, and its faster)
    for (index = fPendingPutRequests->GetArraySize() - 1; index >= 0 ; index--) {
	putReply = (TIrPutReply*)fPendingPutRequests->At(index);
	if ((putReply != nil) && ((lsapConn == nil) || (lsapConn == putReply->fLSAPConn))) {
	    fPendingPutRequests->RemoveAt(index);
	    PutComplete(putReply, returnCode);
	}
    }

} // TIrLAP::CancelPendingPutRequests


//--------------------------------------------------------------------------------
//      PutComplete
//--------------------------------------------------------------------------------
void TIrLAP::PutComplete(TIrPutReply* putReply, IrDAErr result)
{
    // Put completes happen in a couple of places, so I consolidated them here

    // A special case: the peer device acknowledges the put after it was cancelled.
    // In this case the caller (UpdateNrReceived) would pass nil, since CancelPutRequests
    // was already called and set the pending put request to nil.  So just ignore it.
    if (putReply == nil) {
	// Nothing to do, man
    }

    // If fLSAPConn is non-nil, send back put reply
    else if (putReply->fLSAPConn != nil) {
	putReply->fEvent = kIrPutDataReplyEvent;
	putReply->fResult = result;
	putReply->fLSAPConn->EnqueueEvent(putReply);
    }
    // If fLSAPConn is nil, don't reply - just release the put request block
    else {
	fIrDA->ReleaseEventBlock(putReply);
    }

} // TIrLAP::PutComplete


//--------------------------------------------------------------------------------
//      NotConnectedCompletion
//--------------------------------------------------------------------------------
void TIrLAP::NotConnectedCompletion()
{
    // A request has been received while either before getting connected or more
    // likely some time after a disconnect has begun.  [Most likely its a put request].
    TIrLSAPConnEvent* request = (TIrLSAPConnEvent*)GetCurrentEvent();

    // Simple reply to the request with an error.
    switch (request->fEvent) {
	case kIrPutDataRequestEvent:
	    request->fEvent = kIrPutDataReplyEvent;
	    break;

	default:
	    DebugLog("TIrLAP::NotConnectedCompletion: unexpected request");
	    break;
    }
    request->fResult = kIrDAErrNotConnected;
    request->fLSAPConn->EnqueueEvent(request);

} // TIrLAP::NotConnectedCompletion


//--------------------------------------------------------------------------------
//      ApplyDefaultConnParms
//--------------------------------------------------------------------------------
void TIrLAP::ApplyDefaultConnParms()
{
    // Change baud rate back to negotiation baud rate
    GetIrDevice->ChangeSpeed(k9600bps);

    // Set lead-in count back to default
    fLeadInCount = kIrDefaultLeadinCount;

} // TIrLAP::ApplyDefaultConnParms


//--------------------------------------------------------------------------------
//      StartDataReceive
//--------------------------------------------------------------------------------
void TIrLAP::StartDataReceive()
{
    CBufferSegment* inputBuffer;

    // fIOBufferItem should be used only for control and when local busy
    // is in effect.  else we should have a GetBuffer available for a
    // normal data read.  GetBuffers are the only ones valid to be passed
    // up the stack (and returned via ReleaseInputBuffer).
    
    if (fInputBuffer == fIOBufferItem)  // if currently using control buffer (or busy)
	fInputBuffer = nil;             // reset it to switch to getbuffers

    if (fInputBuffer) {                 // if non-nil, then we already have a getbuffer
					// and reuse the current input buffer
	inputBuffer = fInputBuffer;
	XTRACE( kReusingBuffer, 0, fInputBuffer );
    }

    else {
	ULong flags;            // better not have more than 32 buffers 
	ULong index;

	// Set default in case no available buffer was found
	inputBuffer = fIOBufferItem;            // use this if "local busy" condition

	// Find and use an available buffer
	for (index = 0, flags = 1; index < fNumGetBuffers; index++, flags <<= 1) {
	    if (fGetBufferAvail & flags) {              // if this buffer free
		fGetBufferAvail &= ~flags;              // allocate the buffer
		inputBuffer = fGetBuffers[index];
		//fNeedNewInputBuffer = false;
		XTRACE(kLogStartDataRcv1, 0, inputBuffer);
		XTRACE(kLogStartDataRcv2, flags, index);
		break;
	    }
	}
    }

    if (inputBuffer == fIOBufferItem )      // debugging only
	XTRACE( kUsingDefaultBuffer, 0, fIOBufferItem );

    require(inputBuffer, Bogus);
    StartInput(inputBuffer);
    
Bogus:
    return;
} // TIrLAP::StartDataReceive

//--------------------------------------------------------------------------------
//      ReleaseInputBuffer
//--------------------------------------------------------------------------------
void TIrLAP::ReleaseInputBuffer(CBufferSegment* inputBuffer)
{
    // This is called when the multiplexor has passed the inputbuffer to a client
    // and is returning the buffer to the buffer pool.  The buffer is marked as available.

    ULong flags;
    ULong index;
    Boolean bufferFound = false;
    
    XTRACE(kLogReleaseInputBuffer, 0, inputBuffer);

    // Find the buffer
    for (index = 0, flags = 1; index < fNumGetBuffers; index++, flags <<= 1) {
	if (fGetBuffers[index] == inputBuffer) {
	    // Check for releasing a buffer twice
	    XASSERT((fGetBufferAvail & flags) == 0);
	    fGetBufferAvail |= flags;
	    bufferFound = true;
	    XTRACE(kLogReleaseInputBuffer2, flags, index);
	}
    }

    //XASSERT(bufferFound);                         // It should be one of the buffers,
    require(bufferFound, BufferNotFound);           // but don't croak if its not

    // Clear localBusy condition before it gets reported
    if (fSetLocalBusyPending) {
	XTRACE(kAbortLocalBusyPendingEvent, 0, 0);
	XASSERT(!fLocalBusy);
	fSetLocalBusyPending = false;
    }

    // Clear localBusy condition if we were in a local busy condition and just now got out of it
    else if (fLocalBusy && !fClrLocalBusyPending) {
	XTRACE(kClrLocalBusyPendingEvent, 0, 0);
	fLocalBusyClearedEvent->fEvent = kIrLocalBusyClearedEvent;
	fLocalBusyClearedEvent->fResult = noErr;
	fClrLocalBusyPending = true;
	this->EnqueueEvent(fLocalBusyClearedEvent);
    }
BufferNotFound:
    return;

} // TIrLAP::ReleaseInputBuffer


//--------------------------------------------------------------------------------
//      PostponePutRequest
//--------------------------------------------------------------------------------
void TIrLAP::PostponePutRequest()
{
    fPendingPutRequests->InsertFirst(GetCurrentEvent());

} // TIrLAP::PostponePutRequest


//--------------------------------------------------------------------------------
//      PrepareFRMRResponse
//--------------------------------------------------------------------------------
void TIrLAP::PrepareFRMRResponse()
{
    // Save info necessary when the FRMR can be sent
    fFRMRRejCtrlField = fRecdCtrl;
    fFRMRMyNrAndNs = (fVr << kIrLAPNrShift) | (fVs << kIrLAPNsShift) | (fRecdCR ? kIrFRMRCrBit : 0);
    if (RecdInvalidNr()) {
	fFRMRReasonFlags = kIrFRMRFlagInvalidNrCount;
    }
    else if (RecdInvalidNs()) {
	fFRMRReasonFlags = 0;
    }
    else {
	fFRMRReasonFlags = kIrFRMRFlagUndefinedCtrl;
    }

    // Set flag indicating that frmr is pending
    fFRMRPending = true;

} // TIrLAP::PrepareFRMRResponse


//--------------------------------------------------------------------------------
//      OutputXIDCommand
//--------------------------------------------------------------------------------
void TIrLAP::OutputXIDCommand()
{
    ULong deviceInfoLength = 0;
    TIrDscInfo* myDscInfo;
    TXIDPacket* xidCmd = (TXIDPacket*)fIOBufferItem->GetBufferPtr();

    XTRACE(kDiscoverXIDCmdEvent, fDiscoverSlot, 0);

    // Set cmd output so output complete event knows what to do next
    fNextCmdRspToSend = kIrLAPCmdXID;

    // Fill out the discover XID frame
    xidCmd->fAddress = (kIrLAPBroadcastAddr << 1) | kIrLAPCommandBit;
    xidCmd->fCmdRsp = kIrLAPCmdXID | kIrLAPPollBit;
    xidCmd->fFormatId = kIrLAPDiscoveryXIDFormat;
    xidCmd->fSrcDevAddr = fMyDevAddr;
    xidCmd->fDstDevAddr = fConflictDevAddr;
    xidCmd->fFlags = fDiscoverFlags;
    xidCmd->fSlotNum = fDiscoverSlot;
    xidCmd->fVersion = kIrLAPVersionNumber;

    // Add the caller supplied discovery info on final.
    if (fDiscoverSlot == kIrLAPFinalSlot) {
	// Get my discovery info (PDA/"Newton" unless changed by client)
#if forMac
	myDscInfo = GetDiscovery->GetDiscoveryInfo();
#else
	myDscInfo = fIrDA->GetMyDscInfo();
#endif
	deviceInfoLength = myDscInfo->AddDevInfoToBuffer(&xidCmd->fDevInfo[0],
	    kMaxUnconnectedPacketSize - kTXIDPacketSize);
    }

    // Set up the put buffer
    fPutBuffer->SetControlBuffer(&xidCmd->fAddress, kTXIDPacketSize + deviceInfoLength, true);

    // Output the discover XID frame
    check(fLeadInCount == kIrDefaultLeadinCount);       // if not, how'd that happen?
    StartOutput(fPutBuffer, kIrDefaultLeadinCount );    // XID should have 10 BOFs per spec

//#if forMac                            // Immediately post a read for the response packet.  The driver
//  StartInput(&fIOBufferItem);     // will queue it behind the XID write and it be posted to .XIn 
//#endif                                // when the XID completes.  It takes too long for the stack to
				    // process the Output complete and post the response read.
} // TIrLAP::OutputXIDCommand


//--------------------------------------------------------------------------------
//      OutputXIDResponse
//--------------------------------------------------------------------------------
void TIrLAP::OutputXIDResponse(TXIDPacket& xidCmd)
{
    ULong deviceInfoLength;
    UByte xidFlags = xidCmd.fFlags;
    TIrDscInfo* myDscInfo;
    TXIDPacket* xidRsp = (TXIDPacket*)fIOBufferItem->GetBufferPtr();

    XTRACE(kDiscoverXIDRspEvent, fDiscoverSlot, xidFlags);

    // Set cmd output so output complete event knows what to do next
    fNextCmdRspToSend = kIrLAPRspXID;

    // Give myself a new dev address if conflict resolution situation
#if forMac

    if( xidFlags & kIrLAPDiscoverFlagsNewAddr )
	fReplacementDevAddr = ( UInt32 ) ( random()  % 0xFFFFFFFE + 1 );
    else
	fReplacementDevAddr = 0;
	
#else
    fReplacementDevAddr = xidFlags & kIrLAPDiscoverFlagsNewAddr ? (ULong)rand() % 0xFFFFFFFE + 1 : 0;
#endif

    // Create a response XID frame
    xidRsp->fAddress = kIrLAPBroadcastAddr << 1;
    xidRsp->fCmdRsp = kIrLAPRspXID | kIrLAPFinalBit;
    xidRsp->fFormatId = kIrLAPDiscoveryXIDFormat;
    xidRsp->fSrcDevAddr = fReplacementDevAddr ? fReplacementDevAddr : fMyDevAddr;
    xidRsp->fDstDevAddr = xidCmd.fSrcDevAddr;
    // Response flags are essentially reflection of the flags received.
    // Mask off all but supported flags just in case some junk was received.
    xidRsp->fFlags = xidFlags & (kIrLAPDiscoverFlagsNewAddr | kIrLAPDiscoverFlagsSlotMask);
    xidRsp->fSlotNum = fDiscoverSlot;
    xidRsp->fVersion = kIrLAPVersionNumber;

    // Add the caller supplied discovery info.
#if forMac
	myDscInfo = GetDiscovery->GetDiscoveryInfo();
#else
	myDscInfo = fIrDA->GetMyDscInfo();
#endif
    deviceInfoLength = myDscInfo->AddDevInfoToBuffer(&xidRsp->fDevInfo[0],
	kMaxUnconnectedPacketSize - kTXIDPacketSize);

    // Set up the put buffer
    fPutBuffer->SetControlBuffer(&xidRsp->fAddress, kTXIDPacketSize + deviceInfoLength, true);

    // Output the discover XID frame
    StartOutput(fPutBuffer, /* fLeadInCount */ 10); // CPD-XID should have 10 BOFs per spec

} // TIrLAP::OutputXIDResponse


//--------------------------------------------------------------------------------
//      OutputSNRMCommand
//--------------------------------------------------------------------------------
void TIrLAP::OutputSNRMCommand()
{
    ULong qosDataLength;
    TSNRMPacket* snrmCmd = (TSNRMPacket*)fIOBufferItem->GetBufferPtr();

    // Set cmd output so output complete event knows what to do next
    fNextCmdRspToSend = kIrLAPCmdSNRM;

    // Fill out the SNRM frame
    snrmCmd->fAddress = (kIrLAPBroadcastAddr << 1) | kIrLAPCommandBit;
    snrmCmd->fCmdRsp = kIrLAPCmdSNRM | kIrLAPPollBit;
    snrmCmd->fSrcDevAddr = fMyDevAddr;
    snrmCmd->fDstDevAddr = fPeerDevAddr;
    snrmCmd->fConnAddr = fConnAddr << 1;

    // Add negotiation info passed down to me
    qosDataLength = fMyQOS->AddInfoToBuffer(&snrmCmd->fQOSParmData[0],
	kMaxUnconnectedPacketSize - kTSNRMPacketSize);

    // Set up the put buffer
    fPutBuffer->SetControlBuffer(&snrmCmd->fAddress, kTSNRMPacketSize + qosDataLength, true);

    // Output the connect SNRM frame
    StartOutput(fPutBuffer, fLeadInCount);

} // TIrLAP::OutputSNRMCommand


//--------------------------------------------------------------------------------
//      OutputUAResponse
//--------------------------------------------------------------------------------
void TIrLAP::OutputUAResponse()
{
    ULong qosDataLength;
    TUAPacket* uaRsp = (TUAPacket*)fIOBufferItem->GetBufferPtr();

#if forMac
    if (!fConnected) {              // calling OutputUA more than once, tell discover only once
	fConnected = true;                      // jdg flag as connected a little earlier than before
	GetDiscovery->GetRemoteDeviceName( fPeerDevAddr, fNickName, sizeof(fNickName) );
    }
#endif

    // Set cmd output so output complete event knows what to do next
    fNextCmdRspToSend = kIrLAPRspUA;

    // Fill out the UA frame
    uaRsp->fAddress = fConnAddr << 1;
    uaRsp->fCmdRsp = kIrLAPRspUA | kIrLAPFinalBit;
    uaRsp->fSrcDevAddr = fMyDevAddr;
    uaRsp->fDstDevAddr = fPeerDevAddr;

    // Add negotiation info (passed down to me and negotiated against SNRM QOS parms)
    qosDataLength = fMyQOS->AddInfoToBuffer(&uaRsp->fQOSParmData[0],
	kMaxUnconnectedPacketSize - kTUAPacketSize);

    // Set up the put buffer
    fPutBuffer->SetControlBuffer(&uaRsp->fAddress, kTUAPacketSize + qosDataLength, true);

    XTRACE(kOutputUACommandEvent, uaRsp->fCmdRsp, uaRsp->fAddress );
    XTRACE(kOutputUACommandEvent, qosDataLength >> 16, qosDataLength ); // testing
    
    // Output the connect UA response frame
    StartOutput(fPutBuffer, fLeadInCount);
    
} // TIrLAP::OutputUAResponse


//--------------------------------------------------------------------------------
//      OutputFRMRResponse
//--------------------------------------------------------------------------------
void TIrLAP::OutputFRMRResponse()
{
    TFRMRPacket* frmrRsp = (TFRMRPacket*)fIOBufferItem->GetBufferPtr();

    // Set cmd output so output complete event knows what to do next
    fNextCmdRspToSend = kIrLAPRspFRMR;

    // Fill out the FRMR frame
    frmrRsp->fAddress = fConnAddr << 1;
    frmrRsp->fCmdRsp = kIrLAPRspFRMR | kIrLAPFinalBit;
    frmrRsp->fRejCtrlField = fFRMRRejCtrlField;
    frmrRsp->fMyNrAndNs = fFRMRMyNrAndNs;
    frmrRsp->fReasonFlags = fFRMRReasonFlags;

    // Set up the put buffer
    fPutBuffer->SetControlBuffer(&frmrRsp->fAddress, kTFRMRPacketSize, true);

    // Output the FRMR response frame
    StartOutput(fPutBuffer, fLeadInCount);
} // TIrLAP::OutputFRMRResponse


//--------------------------------------------------------------------------------
//      OutputControlFrame
//--------------------------------------------------------------------------------
void TIrLAP::OutputControlFrame(UByte cmdRsp)
{
    // This can be used to output simple U frames or S frames (not intended for I frames)
    // NOTE: The final/poll bit is always set by this.

    TControlPacket* frame = (TControlPacket*)fIOBufferItem->GetBufferPtr();

    // Set cmd output so output complete event knows what to do next
    fNextCmdRspToSend = cmdRsp;

    // Fill out the control frame
    frame->fAddress = (fConnAddr << 1) | (fPrimary ? kIrLAPCommandBit : 0);
    frame->fCmdRsp = cmdRsp | kIrLAPPollBit;
    if ((cmdRsp & kIrLAPUSIMask) != kIrLAPUnnumbered) {
	frame->fCmdRsp |= fVr << kIrLAPNrShift;
    }

    if (cmdRsp == kIrLAPFrameRR) XTRACE(kOutputControlFrameRR, 0, fVr);
    else                         XTRACE(kOutputControlFrame, cmdRsp, fVr);

    // Set up the put buffer
    fPutBuffer->SetControlBuffer(&frame->fAddress, kTControlPacketSize, true);

    // Output the frame
    StartOutput(fPutBuffer, fLeadInCount);

#if forMac
    switch( cmdRsp ) {          // Log the packet type sent
	case kIrLAPFrameRR:
	    GetIrDevice->Stats_RRSent();
	    break;
	    
	case kIrLAPFrameRNR:
	    GetIrDevice->Stats_RNRSent();
	    break;
	    
	case kIrLAPFrameREJ:
	    GetIrDevice->Stats_REJSent();
	    break;
	    
	case kIrLAPFrameSREJ:
	    GetIrDevice->Stats_SREJSent();
	    break;
    }           
#endif

} // TIrLAP::OutputControlFrame


//--------------------------------------------------------------------------------
//      OutputDataFrame
//--------------------------------------------------------------------------------
void TIrLAP::OutputDataFrame(TIrPutRequest* request, Boolean finalOrPollFlag)
{
    ULong lmPDULength;
    UByte finalPollBit = finalOrPollFlag ? kIrLAPFinalBit : 0;  // Note: final/poll bit same bit
    TControlPacket* frame = (TControlPacket*)fIOBufferItem->GetBufferPtr();

    if (finalPollBit) XTRACE(kOutputDataFrame, fVs, fVr);
    else             XTRACE(kOutputDataFrameNotFinal, fVs, fVr);


    // Set cmd output so output complete event knows what to do next
    fNextCmdRspToSend = kIrLAPFrameINFO;

    // Keep track of the send buffer in the resend/ack/reply array
    fPutRequests[fVs] = request;

    // Update the valid fRecdNr bits
    fValidRecdNr |= 1 << fVs;

    // Fill out the data frame header
    frame->fAddress = (fConnAddr << 1) | (fPrimary ? kIrLAPCommandBit : 0);
    frame->fCmdRsp = kIrLAPFrameINFO | finalPollBit | (fVr << kIrLAPNrShift) | (fVs << kIrLAPNsShift);

    // Advance the send index
    fVs = (fVs + 1) & 0x7;

    // Fill in the LM-PDU header (control or data LM-PDU info)
    lmPDULength = GetLMP->FillInLMPDUHeader(request, frame->fLMPDUData);

    // Set up the put buffer
    fPutBuffer->SetControlBuffer(&frame->fAddress, kTControlPacketSize + lmPDULength, true);
    fPutBuffer->SetDataBuffer(request->fData, request->fOffset, request->fLength);

    // Output the data frame
    StartOutput(fPutBuffer, (UInt32)fLeadInCount);

#if forMac
    GetIrDevice->Stats_IFrameSent();
#endif
} // TIrLAP::OutputDataFrame


//--------------------------------------------------------------------------------
//      GotData
//--------------------------------------------------------------------------------
Boolean TIrLAP::GotData(UByte *data, ULong size)
{
    ULong   result;
    result = fInputBuffer->Getn(data, size);    // try and get size bytes
    XTRACE(kLogGotData, size, result);          // log requested/result counts
    return (result == size);
    //return (ULong)fInputBuffer->Getn(data, size) == size;
} // TIrLAP::GotData


//--------------------------------------------------------------------------------
//      RecdCmd
//--------------------------------------------------------------------------------
Boolean TIrLAP::RecdCmd(UByte cmdPattern)
{
    return fRecdCR && (fRecdCmdRsp == cmdPattern);

} // TIrLAP::RecdCmd


//--------------------------------------------------------------------------------
//      RecdPollCmd
//--------------------------------------------------------------------------------
Boolean TIrLAP::RecdPollCmd(UByte cmdPattern)
{
    return fRecdPF && RecdCmd(cmdPattern);

} // TIrLAP::RecdPollCmd


//--------------------------------------------------------------------------------
//      RecdRsp
//--------------------------------------------------------------------------------
Boolean TIrLAP::RecdRsp(UByte rspPattern)
{
    return !fRecdCR && (fRecdCmdRsp == rspPattern);

} // TIrLAP::RecdRsp


//--------------------------------------------------------------------------------
//      RecdFinalRsp
//--------------------------------------------------------------------------------
Boolean TIrLAP::RecdFinalRsp(UByte rspPattern)
{
    return fRecdPF && RecdRsp(rspPattern);

} // TIrLAP::RecdFinalRsp


//================================= Timer stuff ==================================


//--------------------------------------------------------------------------------
//      StartTimer
//--------------------------------------------------------------------------------
void TIrLAP::StartTimer(TTimeout timeDelay, int refCon)
{
    XTRACE(kLogStartTimer, timeDelay, refCon);
    
    fIrDA->StartTimer(kTimer_LAP, timeDelay, refCon);

} // TIrLAP::StartTimer


//--------------------------------------------------------------------------------
//      StopTimer
//--------------------------------------------------------------------------------
void TIrLAP::StopTimer()
{
/*  fMediaBusyTimer.        StopTimer();    // Stop them all to make sure.  This is fine
    fDiscoverSlotTime.      StopTimer();    // since the stack expects only one timer
    fFinalTimer.            StopTimer();    // to be active at any given time, which is
    fBackOffTimer.          StopTimer();    // not always the case.  Real work is done only
    fWatchDogTimer.         StopTimer();    // if the timer is active.
    fDiscoverMaxSlotTimer.  StopTimer();
    fMinTurnAroundTimer.    StopTimer();
*/
    XTRACE(kLogStopTimer, 0, 0);
    fIrDA->StopTimer(kTimer_LAP);

} // TIrLAP::StopTimer


//--------------------------------------------------------------------------------
//      TimerComplete
//--------------------------------------------------------------------------------
void TIrLAP::TimerComplete(ULong refCon)
{
    XTRACE(kLogTimerComplete, 0, refCon);
    check((refCon >= kIrFirstIrLAPTimerEvent) && (refCon <= kIrLastIrLAPTimerEvent));
    NextState(refCon);
} // TIrLAP::TimerComplete


//============================= Input/Output stuff ===============================


//--------------------------------------------------------------------------------
//      StartOutput
//--------------------------------------------------------------------------------
void TIrLAP::StartOutput(TIrLAPPutBuffer* outputBuffer, ULong leadInCount)
{
    fOutputInProgress = true;
    fLastCmdRsp = fNextCmdRspToSend;    // Stash the current packet type so it is valid
					// between sends.
#if forMac
    GetIrDevice->StartTransmit(outputBuffer, leadInCount);
#else
    fIrDA->StartTransmit(outputBuffer, leadInCount);
#endif

} // TIrLAP::StartOutput


//--------------------------------------------------------------------------------
//      StopOutput
//--------------------------------------------------------------------------------
void TIrLAP::StopOutput()
{
    fOutputInProgress = false;      
#if !forMac                         // Packet was completely written during StartOutput
    fIrDA->StopTransmit();
#endif
} // TIrLAP::StopOutput


//--------------------------------------------------------------------------------
//      StartInput
//--------------------------------------------------------------------------------
void TIrLAP::StartInput(CBufferSegment* inputBuffer)
{
    XTRACE(kLogStartInput, 0, inputBuffer);
    XTRACE(kLogStartInput2, 0, fInputBuffer);
    
    require(inputBuffer, Bogus);        // shouldn't ever happen
    //Boolean localBusy = fLocalBusy || fSetLocalBusyPending;
    
    // jdg hack #832
    // if fInputBuffer is non-nil and not fIOBufferItem and not the same as what
    // was used last time, then it's one of the get buffers and we need to "release"
    // it so it can be safely cleaned up by FreeGetBuffers.  Could skip this except for
    // the sanity checks in FreeGetBuffers.
    if (fInputBuffer && fInputBuffer != fIOBufferItem && inputBuffer != fInputBuffer) {
	XTRACE(kLogStartInput3, 0xfeed, 0xbeef);
	require(inputBuffer == fIOBufferItem, SanityLacks);
	//DebugLog("working to avoid buffer leak in StartInput, release on 0x%lx\n", (UInt32) fInputBuffer);
	ReleaseInputBuffer(fInputBuffer);   // could queue clear local busy event... should be ok
    }
SanityLacks:
    
    fInputInProgress = true;
    fInputBuffer = inputBuffer;         // remember input buffer
    inputBuffer->Reset();               // Start the ball rolling
    
    // Used to tell the i/o guy to just return the afield and cfield if we've run out
    // of buffers, but now that fIOBufferItem is a full-sized CBufferSegment we
    // go ahead and always accept a full-size packet anyway.  Shouldn't ever
    // get back more than the header anyway.
    GetIrDevice->StartReceive(inputBuffer);
Bogus:
    return;
} // TIrLAP::StartInput


//--------------------------------------------------------------------------------
//      StopInput
//--------------------------------------------------------------------------------
void TIrLAP::StopInput()
{
    fInputInProgress = false;
    GetIrDevice->StopReceive();

} // TIrLAP::StopInput

/*
//--------------------------------------------------------------------------------
//      InputHappening
//--------------------------------------------------------------------------------
Boolean TIrLAP::InputHappening()
{
    // Are we receiving data even though receipt may not be complete?
    return fIrSIR->Stats_ReceivingData();
} // TIrLAP::InputHappening
*/

//--------------------------------------------------------------------------------
//      OutputComplete
//--------------------------------------------------------------------------------
void TIrLAP::OutputComplete()
{
    fOutputInProgress = false;
#ifdef IRDA_TEST_FRAME_SUPPORTED
    if (fHandlingTestFrame) {
	TestFrameComplete();
    }
    else
#endif
    {
	XTRACE( kPacketOutput, 0, 0 );
	NextState(kIrOutputCompleteEvent);
    }
} // TIrLAP::OutputComplete

//--------------------------------------------------------------------------------
//      ChangeSpeedComplete
//--------------------------------------------------------------------------------
void TIrLAP::ChangeSpeedComplete()
{
    // only do the ChangeSpeedComplete event in the two states that handle it, otherwise
    // this routine is a nop (i.e. called from one of the paths to DisconnectComplete)
    if (fState == kIrLAPConnectState || fState == kIrLAPListenState)
	NextState(kIrChangeSpeedCompleteEvent);
    else {
	XTRACE(kLogChangeSpeedCompleteIgnored, 0, 0);
    }
} // TIrLAP::ChangeSpeedComplete



//--------------------------------------------------------------------------------
//      InputAborted -- input aborted, pretend we timed out (but faster)
//          returns true if the 'abort' worked (else the caller shoujld
//          hang another read out)
//--------------------------------------------------------------------------------
Boolean TIrLAP::InputAborted()
{
    XTRACE(kInputAborted, 0, 0);
    if (fState == kIrLAPPriReceiveState &&      // sanity
	fMyWindowSize == 1) {           // if aborted read and my windowsize == 1
	StartTimer(fMinTurnAroundTimeout, kIrFinalTimerExpiredEvent);
	return true;
    }
    return false;
}

//--------------------------------------------------------------------------------
//      InputComplete
//--------------------------------------------------------------------------------
void TIrLAP::InputComplete(UByte aField, UByte cField)
{
    // Note: The aField and cField were extracted by the low level input routine (vs being
    // added to the input buffer) because the IrLAP spec specifies that the negotiated data
    // size is the maximum bytes in the I field of a frame, which does not leave room for the
    // aField or the cField.

    //fInputInProgress = false;     // jdg - use as a read complete "valid" flag below
    // Extract the c/r bit from aField and normalize the address
    fRecdCR = aField & kIrLAPCommandBit;
    fRecdAddr = aField >> 1;

    // Extract the PF bit from cField
    fRecdPF = cField & kIrLAPPFMask;

    UByte nrMask = 0;
    UByte nsMask = 0;
    fNrNsFlags = 0;

    // Only set (change from its previous value) fRecdNr if S or I frame
    if ((cField & kIrLAPUSIMask) != kIrLAPUnnumbered) {
	nrMask = kIrLAPNrMask;
	fRecdNr = (cField & nrMask) >> kIrLAPNrShift;
	if (fRecdNr != fVs) {
	    fNrNsFlags |= kIrLAPUnexpectedNr;
	    XTRACE(kUnexpectedNr, fRecdNr, fVs);                // jdg
	    
	    if ((fValidRecdNr & (1 << fRecdNr)) == 0) {
		fNrNsFlags |= kIrLAPInvalidNr;
		XTRACE(kInvalidNr, fValidRecdNr, fRecdNr);      // jdg
	    }
	}
    }
    // Only set (change from its previous value) fRecdNs if I frame
    if ((cField & kIrLAPIMask) == kIrLAPInformation) {
	nsMask = kIrLAPNsMask;
	fRecdNs = (cField & nsMask) >> kIrLAPNsShift;
	if (fRecdNs != fVr) {
	    fNrNsFlags |= kIrLAPUnexpectedNs;
	    XTRACE(kUnexpectedNs, fRecdNs, fVr);            // jdg
	    
	    if ((fValidRecdNs & (1 << fRecdNs)) == 0) {
		fNrNsFlags |= kIrLAPInvalidNs;
		XTRACE(kInvalidNs, fValidRecdNs, fRecdNs);      // jdg
	    }
	}
    }
    // Mask off P/F, Nr and Ns from cField to get received command
    fRecdCmdRsp = cField & ~(kIrLAPPFMask | nrMask | nsMask);

    // jdg ... now that that's set, let's do some debugging
    if (RecdSFrame()) {             // supervisor, only Nr valid
	if (RecdPollOrFinal()) {
	    if (fRecdCmdRsp == kIrLAPFrameRR) XTRACE(kInputControlFrameRR, 0, fRecdNr);
	    else XTRACE(kInputControlFrame, fRecdCmdRsp, fRecdNr);
	}
	else {
	    XTRACE(kInputControlFrameNotFinal, cField, fRecdNr);        // possible??
	}
    }
    else if (RecdIFrame()) {            // data, both Ns and Nr valid
#if forMac
	GetIrDevice->Stats_IFrameRec();
#endif
	if (RecdPollOrFinal()) XTRACE(kInputDataFrame, fRecdNs, fRecdNr);
	else {
			       XTRACE(kInputDataFrameNotFinal, fRecdNs, fRecdNr);
	}
    }
    else if( RecdUFrame() ) {
	GetIrDevice->Stats_UFrameRec();
    }
    /// end jdg add
    
    // Save the control field as is for FRMR error reporting
    fRecdCtrl = cField;

    // Only accept the input if broadcast or for me
    // Should have been verified by the input routine
    XASSERT ((fRecdAddr == kIrLAPBroadcastAddr) || (fRecdAddr == fConnAddr));

#ifdef IRDA_TEST_FRAME_SUPPORTED
    if (fRecdCmdRsp == kIrLAPCmdTEST) {
	// Handle test frame as a special case (man oh man, I hate special cases)
	HandleTestFrame();
    }
    else
#endif
    if (fInputInProgress) {                 // Only if we're expecting a read complete ...
	XTRACE( kValidPacketReceived, fState, 0 );
	fInputInProgress = false;           // now we don't have a read pending anymore
	NextState(kIrInputCompleteEvent);
    }
    else {
	// on my iMac, with a USB IrDA Pod, I get this every time the disk spins
	// back *up*.  The deferred tasks are held for maybe 3 seconds, more than
	// long enough to fire the fTimer -- while a completed read is in the "queue"
	// waiting for us to handle.  Could fix this at the IrDA USB layer, but the
	// general solution is to not accept packets here unless a read is still pending.
	XTRACE(kLogPacketDropped, 0, 0);
	//DebugLog("irlap ignoring packet;g");
    }
} // TIrLAP::InputComplete


#ifdef IRDA_TEST_FRAME_SUPPORTED
//--------------------------------------------------------------------------------
//      HandleTestFrame
//--------------------------------------------------------------------------------
void TIrLAP::HandleTestFrame()
{
    XTRACE(kTestFrameReceivedEvent, fRecdAddr, 0);

    // If the frame was either sent to my connection or broadcast to me or anyone then send a response
    ULong testFrameLength = fInputBuffer->GetSize();
    ULong* testFrame = (ULong*)fInputBuffer->GetBufferPtr();
    if ((fRecdAddr == fConnAddr) ||
	((testFrameLength >= 8) && ((testFrame[1] == fMyDevAddr) || (testFrame[1] == kIrLAPSniffeeDevAddr) || (testFrame[1] == kIrLAPBroadcastDevAddr)))) {

	// By typical standards this appears to be a test frame for me, but still need
	// to check for special considerations when its for Sniffee's and I'm listening -
	// In that case do not respond, as Sniffers are only interested in non-listeners.
	if ((testFrame[1] == kIrLAPSniffeeDevAddr) &&
	    (fCurrentRequest != nil) && (fCurrentRequest->fEvent == kIrListenRequestEvent)) {
	    TestFrameComplete();
	}

	// It's a test frame for me or broadcast to anyone and not a sniff special -
	else {
	    fHandlingTestFrame = true;

	    // Build the header
	    fTestHeader.fAddress = (fRecdAddr << 1) | (fPrimary ? kIrLAPCommandBit : 0);
	    fTestHeader.fCmdRsp = kIrLAPRspTEST | kIrLAPFinalBit;

	    // Swap the source and destination addresses if broadcast
	    if (fRecdAddr == kIrLAPBroadcastAddr) {
		// Source becomes the destination
		testFrame[1] = testFrame[0];

		// And I'm now the source
		testFrame[0] = fMyDevAddr;
	    }

	    // Set up the put buffer
	    fPutBuffer->SetControlBuffer(&fTestHeader.fAddress, kTTestHdrPacketSize, true);
	    fPutBuffer->SetDataBuffer(fInputBuffer, 0, testFrameLength);

	    // Output the data frame
	    StartOutput(fPutBuffer, fLeadInCount);
	}
    }

    // Test frame wasn't for me, so continue inputting in the simplest/least code way
    else {
	TestFrameComplete();
    }

} // TIrLAP::HandleTestFrame
#endif


#ifdef IRDA_TEST_FRAME_SUPPORTED
//--------------------------------------------------------------------------------
//      TestFrameComplete
//--------------------------------------------------------------------------------
void TIrLAP::TestFrameComplete()
{
    XTRACE(kTestFrameOutputDoneEvent, 0, 0);

    // Output of the test frame response has completed.
    fHandlingTestFrame = false;

    // Restart whatever input was pending and continue on (as you were private!).
    StartInput(fInputBuffer);

} // TIrLAP::TestFrameComplete
#endif

//
// JDG hacking.  Currently used in HandleDisconnectedStateEvent to reject
// pending requests rather than croak.  Needs some cleanup.
//
void TIrLAP::RejectRequest(TIrEvent *request, IrDAErr err)
{
    XTRACE(kRejectRequest, request->fEvent, err);
    request->fEvent = (UByte)RequestIdToReplyId(request->fEvent);
    request->fResult = err;
    GetLMP->EnqueueEvent(request);
}

#if forMac
//--------------------------------------------------------------------------------
//      LAPTimerNotifier: Completion proc for both timers
//--------------------------------------------------------------------------------
void LAPTimerNotifier( UInt32 refCon, UInt32 sig )
{
    check( refCon );
    TIrLAP *    obj = ( TIrLAP * )refCon;
    obj->TimerComplete( sig );
}

//=============================== TIrLAPPutBuffer ================================

#endif

#undef super
#define super OSObject
    OSDefineMetaClassAndStructors(TIrLAPPutBuffer, OSObject);

//--------------------------------------------------------------------------------
//      TIrLAPPutBuffer
//--------------------------------------------------------------------------------
/*static*/
TIrLAPPutBuffer *
TIrLAPPutBuffer::tIrLAPPutBuffer()
{
    TIrLAPPutBuffer *obj = new TIrLAPPutBuffer;
    if (obj && !obj->init()) {
	obj->release();
	obj = nil;
    }
    return obj;
}


//--------------------------------------------------------------------------------
//      Init
//--------------------------------------------------------------------------------
bool TIrLAPPutBuffer::init()
{
    if (!super::init()) return false;

    fCtrlBuf = nil;
    fCtrlBufLength = 0;
    fCtrlBufPos = 0;

    fDataBuf = nil;
    fDataBufOffset = 0;
    fDataBufLength = 0;
    fDataBufPos = 0;

    return true;
    
} // TIrLAPPutBuffer::init

//--------------------------------------------------------------------------------
//      Free
//--------------------------------------------------------------------------------
void TIrLAPPutBuffer::free(void)
{
    super::free();
}

//--------------------------------------------------------------------------------
//      SetControlBuffer
//--------------------------------------------------------------------------------
void TIrLAPPutBuffer::SetControlBuffer(UByte* buffer, ULong length, Boolean initFirst)
{
    if (initFirst) {
	init();
    }

    fCtrlBuf = buffer;
    fCtrlBufLength = length;
    fCtrlBufPos = 0;

} // TIrLAPPutBuffer::SetControlBuffer


//--------------------------------------------------------------------------------
//      SetDataBuffer
//--------------------------------------------------------------------------------
void TIrLAPPutBuffer::SetDataBuffer(CBuffer* buffer, ULong offset, ULong length)
{
    fDataBuf = buffer;
    fDataBufOffset = offset;
    fDataBufLength = length;
    fDataBufPos = 0;

} // TIrLAPPutBuffer::SetDataBuffer


//--------------------------------------------------------------------------------
//      Get
//--------------------------------------------------------------------------------
UByte TIrLAPPutBuffer::Get(void)
{
    UByte nextChar;

    if (fCtrlBufPos < fCtrlBufLength) {
	XASSERT(fCtrlBuf);
	nextChar = fCtrlBuf[fCtrlBufPos++];
    }
    else if (fDataBufPos < fDataBufLength) {
	nextChar = fDataBuf->Get();
	fDataBufPos++;
    }
    else {
	DebugLog("TIrLAPPutBuffer::Get: getting past end of buffer(s)");
	nextChar = 0xFF;
    }

    return nextChar;

} // TIrLAPPutBuffer::Get


//--------------------------------------------------------------------------------
//      Seek
//--------------------------------------------------------------------------------
void TIrLAPPutBuffer::Seek(Long off, int dir)
{
#pragma unused(off)

    // Low-rent version only supports whats needed
    XASSERT(((dir == kPosBeg) && (off == 0)) || ((dir == kPosCur) && (off == -1)));

    // If direction is kPosBeg, then this is a seek to 0
    if (dir == kPosBeg) {
	fCtrlBufPos = 0;
	if (fDataBuf) {
	    fDataBuf->Seek(fDataBufOffset, kPosBeg);
	    fDataBufPos = 0;
	}
    }

    // Otherwise the only other supported option is to seek back to the previous character
    else {
	if (fDataBufPos) {
	    // If fDataBufPos > 0 then must be getting chars from there, back up one
	    XASSERT(fDataBuf);
	    fDataBuf->Seek(-1, kPosCur);
	    fDataBufPos--;
	}
	else if (fCtrlBufPos) {
	    fCtrlBufPos--;
	}
    }

} // TIrLAPPutBuffer::Seek


//--------------------------------------------------------------------------------
//      AtEOF
//--------------------------------------------------------------------------------
Boolean TIrLAPPutBuffer::AtEOF(void) const
{
    return (fCtrlBufPos == fCtrlBufLength) && (fDataBufPos == fDataBufLength);

} // TIrLAPPutBuffer::AtEOF















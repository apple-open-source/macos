/*
    File:       IrLAP.h

    Contains:   Methods for implementing IrLAP

*/


#ifndef __IRLAP_H
#define __IRLAP_H

#include "IrDATypes.h"
#include "IrStream.h"
#include "CBufferSegment.h"
#include "IrEvent.h"

// Forward reference
class TIrGlue;
class TIrLMP;
class TIrQOS;
class TLSAPConn;
class TCMOSlowIRStats;
class CIrDiscovery;
class CIrDevice;
class CBuffer;

// Constants

#define kIrLAPVersionNumber     0x00
#define kIrLAPBroadcastAddr     0x7F
#define kIrLAPCommandBit        0x01

#define kIrLAPBroadcastDevAddr  0xFFFFFFFF
#define kIrLAPSnifferDevAddr    0x736E6966
#define kIrLAPSniffeeDevAddr    0x726F7365
#define kIrLAPProbeDevAddr      0x70726265

#define kIrDefaultLeadinCount   10
#define kIrLAPFinalSlot         0xFF


enum IrLAPCommandsAndResponses
{
    kIrLAPUSIMask               = 0x03,
    kIrLAPIMask                 = 0x01,
    kIrLAPUnnumbered            = 0x03,
    kIrLAPSupervisory           = 0x01,
    kIrLAPInformation           = 0x00,

    kIrLAPPFMask                = 0x10,
    kIrLAPPollBit               = 0x10,
    kIrLAPFinalBit              = 0x10,

    kIrLAPNrMask                = 0xE0,
    kIrLAPNrShift               = 5,
    kIrLAPNsMask                = 0x0E,
    kIrLAPNsShift               = 1,

    kIrLAPUnexpectedNr          = 0x01,
    kIrLAPUnexpectedNs          = 0x02,
    kIrLAPInvalidNr             = 0x04,
    kIrLAPInvalidNs             = 0x08,

    // Unnumbered commands
    kIrLAPCmdSNRM               = 0x83,
    kIrLAPCmdDISC               = 0x43,
    kIrLAPCmdUI                 = 0x03,
    kIrLAPCmdXID                = 0x2F,
    kIrLAPCmdTEST               = 0xE3,

    // Unnumbered responses
    kIrLAPRspRNRM               = 0x83,
    kIrLAPRspUA                 = 0x63,
    kIrLAPRspFRMR               = 0x87,
    kIrLAPRspDM                 = 0x0F,
    kIrLAPRspRD                 = 0x43,
    kIrLAPRspUI                 = 0x03,
    kIrLAPRspXID                = 0xAF,
    kIrLAPRspTEST               = 0xE3,

    // Supervisor cmd/rsp
    kIrLAPFrameRR               = 0x01,
    kIrLAPFrameRNR              = 0x05,
    kIrLAPFrameREJ              = 0x09,
    kIrLAPFrameSREJ             = 0x0D,

    // Information
    kIrLAPFrameINFO             = 0x00
};

enum IrLAPFormatIdentifiers
{
    kIrLAPDiscoveryXIDFormat    = 0x01
};

enum IrLAPDiscoverFlags
{
    kIrLAPDiscoverFlagsSlotMask = 0x03,
    kIrLAPDiscoverFlags1Slot    = 0x00,
    kIrLAPDiscoverFlags6Slots   = 0x01,
    kIrLAPDiscoverFlags8Slots   = 0x02,
    kIrLAPDiscoverFlags16Slots  = 0x03,

    kIrLAPDiscoverFlagsNewAddr  = 0x04
};

enum IrFRMRFlags    // w,x,y,z fields of third FRMR I-field
{
    kIrFRMRFlagUndefinedCtrl    = 0x01,
    kIrFRMRFlagInvalidIField    = 0x02,
    kIrFRMRFlagSizeExceeded     = 0x04,
    kIrFRMRFlagInvalidNrCount   = 0x08,

    kIrFRMRCrBit                = 0x10
};

enum IrLAPStates
{
    kIrLAPDisconnectedState,                /* NDM */
    kIrLAPQueryState,                       /* Discover query from primary */
    kIrLAPConnectState,                     /* Connect from primary */
    kIrLAPListenState,                      /* Listen from secondary */
    kIrLAPReplyState,                       /* Discover reply from secondary */

    kIrLAPLastNDMState          = kIrLAPReplyState,

    kIrLAPPriReceiveState,                  /* Primary in NRM receive */
    kIrLAPPriTransmitState,                 /* Primary in NRM transmit */
    kIrLAPPriCloseState,                    /* Primary in NRM close */
    kIrLAPSecReceiveState,                  /* Secondary in NRM receive */
    kIrLAPSecTransmitState,                 /* Secondary in NRM transmit */
    kIrLAPSecCloseState                     /* Secondary in NRM close */
};

enum IrLAPTimings       // in milliseconds
{
    kIrMediaBusyTimeout         = 600,
    kIrDiscoverSlotTimeout      = 100,      // *** Review - should be 60 (or35 if done right)
    kIrConnectFinalTimerTimeout = 500,      // *** Review - is this right?
    kIrDisconnectWarningTimeout = 3 * 1000  // 3 seconds
};

enum IrLAPRetries
{
    kMaxConnectRetries          = 3,
    kMaxDisconnectRetries       = 3,
    kMaxDiscoverRetries         = 10        // JDG: max times mediabusy timer fires before 1st discover
};

enum  {
    kMaxUnconnectedPacketSize   = 64
};

// Classes


// --------------------------------------------------------------------------------------------------------------------
//                      TXIDPacket
// --------------------------------------------------------------------------------------------------------------------

#define kTXIDPacketSize     14                      // Can't use sizeof because of padding stuff

class TXIDPacket
{
    public:

	    UByte           fPhonyBologna1;         // Ensure alignment of the followingŠ
	    UByte           fAddress;
	    UByte           fCmdRsp;
	    UByte           fFormatId;
	    ULong           fSrcDevAddr;
	    ULong           fDstDevAddr;
	    UByte           fFlags;
	    UByte           fSlotNum;
	    UByte           fVersion;
	    UByte           fDevInfo[2];            // Device info, actually longer, just padding
};


// --------------------------------------------------------------------------------------------------------------------
//                      TSNRMPacket
// --------------------------------------------------------------------------------------------------------------------

#define kTSNRMPacketSize    11                      // Can't use sizeof because of padding stuff

class TSNRMPacket
{
    public:

	    UByte           fPhonyBologna1;         // Ensure alignment of the followingŠ
	    UByte           fPhonyBalogna2;         // Ensure alignment of the followingŠ
	    UByte           fAddress;
	    UByte           fCmdRsp;
	    ULong           fSrcDevAddr;
	    ULong           fDstDevAddr;
	    UByte           fConnAddr;
	    UByte           fQOSParmData[3];        // QOS parm info, actually longer, just padding
};


// --------------------------------------------------------------------------------------------------------------------
//                      TUAPacket
// --------------------------------------------------------------------------------------------------------------------

#define kTUAPacketSize      10                      // Can't use sizeof because of padding stuff

class TUAPacket
{
    public:

	    UByte           fPhonyBologna1;         // Ensure alignment of the followingŠ
	    UByte           fPhonyBalogna2;         // Ensure alignment of the followingŠ
	    UByte           fAddress;
	    UByte           fCmdRsp;
	    ULong           fSrcDevAddr;
	    ULong           fDstDevAddr;
	    UByte           fQOSParmData[4];        // QOS parm info, actually longer, just padding
};


// --------------------------------------------------------------------------------------------------------------------
//                      TControlPacket
// --------------------------------------------------------------------------------------------------------------------

#define kTControlPacketSize     2                   // Can't use sizeof because of padding stuff

class TControlPacket
{
    public:

	    UByte           fPhonyBologna1;         // Ensure alignment of the followingŠ
	    UByte           fPhonyBalogna2;         // Ensure alignment of the followingŠ
	    UByte           fAddress;
	    UByte           fCmdRsp;
	    UByte           fLMPDUData[4];          // Up to 4 bytes
};


// --------------------------------------------------------------------------------------------------------------------
//                      TFRMRPacket
// --------------------------------------------------------------------------------------------------------------------

#define kTFRMRPacketSize        5                   // Can't use sizeof because of padding stuff

class TFRMRPacket
{
    public:

	    UByte           fAddress;
	    UByte           fCmdRsp;
	    UByte           fRejCtrlField;
	    UByte           fMyNrAndNs;
	    UByte           fReasonFlags;
	    UByte           fPhonyBologna1;         // Pad it out (sheer paranoia)
	    UByte           fPhonyBalogna2;         // Pad it out (sheer paranoia)
	    UByte           fPhonyBalogna3;         // Pad it out (sheer paranoia)
};


// --------------------------------------------------------------------------------------------------------------------
//                      TTestHdrPacket
// --------------------------------------------------------------------------------------------------------------------

#define kTTestHdrPacketSize     2                   // Can't use sizeof because of padding stuff

class TTestHdrPacket
{
    public:

	    UByte           fAddress;
	    UByte           fCmdRsp;
};


//--------------------------------------------------------------------------------
//      TIrLAPPutBuffer
//--------------------------------------------------------------------------------
class TIrLAPPutBuffer : public OSObject
{
    OSDeclareDefaultStructors(TIrLAPPutBuffer);
    
public:
	    static TIrLAPPutBuffer * tIrLAPPutBuffer(void);    // factory constructor
	    void            free(void);
	    bool            init();
	    
	    void            SetControlBuffer(UByte* buffer, ULong length, Boolean initFirst);
	    void            SetDataBuffer(CBuffer* buffer, ULong offset, ULong length);

	    UByte           Get(void);
	    void            Seek(Long off, int dir);
	    Boolean         AtEOF(void) const;
	    ULong           GetCtrlSize     ( void );
	    UByte       *   GetCtrlBuffer   ( void );
	    ULong           GetDataSize     ( void );
	    UByte       *   GetDataBuffer   ( void );

private:
	    // FieldsŠ
	    UByte*          fCtrlBuf;
	    ULong           fCtrlBufLength;
	    ULong           fCtrlBufPos;

	    CBuffer*        fDataBuf;
	    ULong           fDataBufOffset;
	    ULong           fDataBufLength;
	    ULong           fDataBufPos;
};

inline ULong    TIrLAPPutBuffer::GetCtrlSize()      { return fCtrlBufLength; }
inline UByte *  TIrLAPPutBuffer::GetCtrlBuffer()    { return fCtrlBuf; }
inline ULong    TIrLAPPutBuffer::GetDataSize()      { return fDataBufLength; }
inline UByte *  TIrLAPPutBuffer::GetDataBuffer()    { return fDataBuf ? ((CBufferSegment *)fDataBuf)->GetBufferPtr() : nil; };


// --------------------------------------------------------------------------------------------------------------------
//                      TIrLAP
// --------------------------------------------------------------------------------------------------------------------

class TIrLAP : public TIrStream
{
    OSDeclareDefaultStructors(TIrLAP);
    
    public:

	    static TIrLAP * tIrLAP(TIrGlue *irda, TIrQOS *myQOS, TIrQOS* peerQOS);
	    Boolean         Init(TIrGlue *irda, TIrQOS *myQOS, TIrQOS* peerQOS);
	    void            free(void);
	    
	    void            Reset();

	    // Callback for timer completion

	    void            TimerComplete(ULong refCon);

	    // Callbacks for I/O completion

	    void            OutputComplete();
	    void            ChangeSpeedComplete();      // jdg
	    
	    void            InputComplete(UByte aField, UByte cField);
	    Boolean         InputAborted();         // read failed

	    // Access to IrLAP level stats

	    void            CopyStatsTo(TCMOSlowIRStats* irStats);
	    void            ResetStats();

	    // To return input buffer to the buffer pool

	    void            ReleaseInputBuffer(CBufferSegment* inputBuffer);

	    // Inline getter for my device address (for address resolution checking)

	    ULong           GetMyDevAddr()              { return fMyDevAddr;    }
	    
	    UInt32          GetDisconnectTimeoutMax     ( void );
	    UInt32          GetDisconnectTime           ( void );
	    Boolean         InBrokenBeam                ( void );
	    Boolean         Discovering                 ( void );
	    Boolean         ReaquiredConnection         ( void );
	    void            BrokenBeamDisconnect        ( void );
	    Boolean         IsConnected                 ( void );
	    void            GetNickName                 ( UInt8 * name, int maxlen);
										// jdg: stop listen if in disconnected state
	    TIrEvent *      CancelPendingListenRequest  (void);                 // return event if canceled, else nil
/*** Temp I hope ***/           
/***JDG***/ void            DisconnectComplete(IrDAErr result = errConnectionAborted);
	    TIrEvent *      GetCurrentRequest(void);        // jdg: ugly hack #2341

	    void            Suspend(void);          // jdg: going to sleep mode
	    void            Resume(void);           // jdg: waking up from sleep

    private:

	    // TIrStream override
	    void            NextState(ULong event);

	    void            DeInit();
	    void            FreeGetBuffers();

	    // NDM states
	    void            HandleDisconnectedStateEvent(ULong event);
	    void            HandleQueryStateEvent(ULong event);
	    void            HandleConnectStateEvent(ULong event);
	    void            HandleListenStateEvent(ULong event);
	    void            HandleReplyStateEvent(ULong event);
	    void            HandleNDMDisconnectRequest();

	    // NRM states
	    void            HandlePriTransmitStateEvent(ULong event);
	    void            HandlePriReceiveStateEvent(ULong event);
	    void            HandlePriCloseStateEvent(ULong event);
	    void            HandleSecTransmitStateEvent(ULong event);
	    void            HandleSecReceiveStateEvent(ULong event);
	    void            HandleSecCloseStateEvent(ULong event);

	    void            UpdateNrReceived();
	    void            ResendRejectedFrames();
	    void            ProcessRecdInfoOrSuperFrame();

	    IrDAErr         ParseNegotiateAndInitConnState(Boolean primary);
	    void            ConnLstnComplete(IrDAErr result);
	//  void            DisconnectComplete(IrDAErr result = kCommErrConnectionAborted);
	    void            CancelPutRequest();
	    void            CancelPendingPutRequests(TLSAPConn* lsapConn, IrDAErr returnCode);
	    void            PutComplete(TIrPutReply* putReply, IrDAErr result);
	    void            NotConnectedCompletion();
	    void            ApplyDefaultConnParms();
	    void            StartDataReceive();

	    void            PostponePutRequest();
	    void            PrepareFRMRResponse();

	    void            OutputXIDCommand();
	    void            OutputXIDResponse(TXIDPacket& xidCmd);
	    void            OutputSNRMCommand();
	    void            OutputUAResponse();
	    void            OutputFRMRResponse();
	    void            OutputControlFrame(UByte cmdRsp);
	    void            OutputDataFrame(TIrPutRequest* request, Boolean finalOrPollFlag);

	    Boolean         GotData(UByte *data, ULong size);
	    Boolean         RecdCmd(UByte cmdPattern);
	    Boolean         RecdPollCmd(UByte cmdPattern);
	    Boolean         RecdRsp(UByte rspPattern);
	    Boolean         RecdFinalRsp(UByte rspPattern);

	    Boolean         Recd(UByte cmdRsp)      { return fRecdCmdRsp == cmdRsp;         }
	    Boolean         RecdCmd()               { return fRecdCR == kIrLAPCommandBit;   }
	    Boolean         RecdRsp()               { return fRecdCR != kIrLAPCommandBit;   }
	    Boolean         RecdPoll()              { return fRecdPF;   }
	    Boolean         RecdFinal()             { return fRecdPF;   }
	    Boolean         RecdPollOrFinal()       { return fRecdPF;   }

	    Boolean         RecdUFrame()            { return (fRecdCmdRsp & kIrLAPUSIMask) == kIrLAPUnnumbered;     }
	    Boolean         RecdSFrame()            { return (fRecdCmdRsp & kIrLAPUSIMask) == kIrLAPSupervisory;    }
	    Boolean         RecdIFrame()            { return (fRecdCmdRsp & kIrLAPIMask) == kIrLAPInformation;      }

	    Boolean         RecdInvalidNr()         { return (fNrNsFlags & kIrLAPInvalidNr) != 0;                   }
	    Boolean         RecdInvalidNs()         { return (fNrNsFlags & kIrLAPInvalidNs) != 0;                   }
	    Boolean         RecdInvalidNrOrNs()     { return (fNrNsFlags & (kIrLAPInvalidNr | kIrLAPInvalidNs)) != 0;}
	    Boolean         RecdUnexpectedNr()      { return (fNrNsFlags & kIrLAPUnexpectedNr) != 0;                }
	    Boolean         RecdUnexpectedNs()      { return (fNrNsFlags & kIrLAPUnexpectedNs) != 0;                }

	    void            StartTimer(TTimeout timeDelay, int refCon);
	    void            StopTimer();

	    void            StartOutput(TIrLAPPutBuffer* outputBuffer, ULong leadInCount);
	    void            StopOutput();
	    void            StartInput(CBufferSegment* inputBuffer);
	    void            StopInput();

	    void            HandleTestFrame();
	    void            TestFrameComplete();

	    void            RejectRequest(TIrEvent *request, IrDAErr err);  // jdg, send back a request w/error

	    // FieldsŠ

	    UByte           fState;
	    UByte           fConnAddr;              // The 'A' field address
	    ULong           fMyDevAddr;

	    UByte           fDiscoverMaxSlots;
	    UByte           fDiscoverSlot;
	    UByte           fDiscoverFlags;
	    Boolean         fDiscoverEnteredReplyState;
	    Boolean         fDiscoverReplied;
	    ULong           fConflictDevAddr;
	    ULong           fReplacementDevAddr;

	    ULong           fPeerDevAddr;

	    TIrEvent        *fCurrentRequest;       // For discover, connect, listen
	    TIrEvent        *fPendingDisconnect;    // For pending disconnect
	    CList           *fPendingRequests;      // jdg: deferred requests 

	    TIrQOS          *fMyQOS;
	    TIrQOS          *fPeerQOS;

	    UByte           fVr;
	    UByte           fVs;
	    UByte           fNextToAck;
	    UByte           fWindow;
	    Boolean         fConnected;                 // Is LAP connected?
	    Boolean         fLocalBusy;
	    Boolean         fRemoteBusy;
	    Boolean         fSetLocalBusyPending;
	    Boolean         fClrLocalBusyPending;
	    Boolean         fEnteringCloseState;
	    Boolean         fRespondingToDisconnect;

	    Boolean         fWaitingForPollTimer;       // avoiding cycle comsumption while idle or busy

	    Boolean         fHandlingTestFrame;
	    TTestHdrPacket  fTestHeader;

	    Boolean         fFRMRPending;
	    UByte           fFRMRRejCtrlField;
	    UByte           fFRMRMyNrAndNs;
	    UByte           fFRMRReasonFlags;

	    ULong           fRetryCount;
	    ULong           fDisconnectWarningLimit;
	    ULong           fDisconnectLinkLimit;

	    UInt32          fInitialRetryTime;
	    UInt32          fDisconnectLinkLimitTime;
	    UInt32          fBusyCounter;       // JDG: number of media busy in a row

	    ULong           fDataRetries;
	    ULong           fProtocolErrs;

	    //TIrEvent      *fLocalBusyEvent;           // Review.  Why is this not referenced?
	    TIrEvent        *fLocalBusyClearedEvent;    // REVIEW.  How is this used?

	    UByte           fLeadInCount;
	    UByte           fMyWindowSize;          // Number of recv buffers - from fMyQOS->GetWindowSize()
	    UByte           fPeerWindowSize;        // Number of recv buffers - from fPeerQOS->GetWindowSize()
	    TTimeout        fPollTimerTimeout;
	    TTimeout        fFinalTimerTimeout;
	    TTimeout        fWatchdogTimeout;
	    TTimeout        fMinTurnAroundTimeout;

	    Boolean         fPrimary;
	    //Boolean       fNeedNewInputBuffer;    // if fInputBuffer is nil, then we need a new one
	    Boolean         fPutReqsPending;        // One or more addl put reqs pending for secondary?
	    UByte           fNextCmdRspToSend;      // Cmd to send after recv cmd is processed
	    UByte           fLastCmdRsp;            // The last packet type sent.

	    UByte           fRecdCtrl;              // Control field as received (only used w/FRMR)
	    UByte           fRecdCR;                // C/R bit of the A field
	    UByte           fRecdAddr;              // Addr bits of the A field
	    UByte           fRecdPF;                // PF bit of the C field
	    UByte           fRecdNr;                // Nr bits of the C field
	    UByte           fRecdNs;                // Ns bits of the C field
	    UByte           fRecdCmdRsp;            // C field (less P/F, Nr, Ns)

	    UByte           fValidRecdNr;           // Bits representing valid fRecdNr values
	    UByte           fValidRecdNs;           // Bits representing valid fRecdNs values
	    UByte           fNrNsFlags;             // Unexpected/invalid Nr/Ns flags

	    //ULong         fIOBuffer[64/4];        // Note: must be ULong for alignment!
	    CBufferSegment  *fIOBufferItem;         // used for misc i/o and when out of input buffers.

	    Boolean         fInputInProgress;       // Waiting for input (or perhaps receiving data)
	    Boolean         fOutputInProgress;      // Outputting data
	    
	    Boolean         fInBrokenBeam;          // true if the beam is broken (but connection still exists)
	    Boolean         fDiscoverActive;        // true if we're discovering

	    ULong           fGetBufferAvail;
	    ULong           fNumGetBuffers;
	    CBufferSegment* fGetBuffers[15];        // Was 8, have 2*window for
						    // a workaround

	    CBufferSegment* fInputBuffer;           // either fIOBufferItem or one of fGetBuffers
	    //CList         fPendingPuts;
	    UInt8           fNickName[22];          // Name of device connected to.
	    CList*          fPendingPutRequests;
	    TIrLAPPutBuffer *fPutBuffer;
	    TIrPutRequest   *fPutRequests[8];
	    
};

inline  UInt32  TIrLAP::GetDisconnectTimeoutMax()   { return fDisconnectLinkLimitTime; }
inline  UInt32  TIrLAP::GetDisconnectTime()         { return fInitialRetryTime; }
inline  Boolean TIrLAP::InBrokenBeam()              { return fInBrokenBeam; }
inline  Boolean TIrLAP::Discovering()               { return fDiscoverActive;   }
inline  Boolean TIrLAP::ReaquiredConnection()       { return fRetryCount < fDisconnectWarningLimit; }
inline  void    TIrLAP::BrokenBeamDisconnect()      { if( fInBrokenBeam )   
							fRetryCount = fDisconnectLinkLimit; }
inline  Boolean TIrLAP::IsConnected( void )         { return fConnected; }

inline TIrEvent * TIrLAP::GetCurrentRequest(void)   { return fCurrentRequest; };    // jdg, temp?
#endif // __IRLAP_H

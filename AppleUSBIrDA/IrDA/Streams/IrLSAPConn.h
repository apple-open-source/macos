/*
    File:       IrLSAPConn.h

    Contains:   Methods for implementing IrLSAPConn

*/


#ifndef __IRLSAPCONN_H
#define __IRLSAPCONN_H

#include "IrStream.h"
#include "IrEvent.h"

// Forward reference
class TIrGlue;
class TIrLMP;
class CBuffer;

// Constants

#define kWatchdogTimeoutCount   30      // Wait for up to 30 secs for conn response

enum LSAPConnStates
{
    kLSAPConnDisconnected,
    kLSAPConnConnectPending,            // Referred to as "SetupPending" in LMP spec
    kLSAPConnConnect,                   // Referred to as "Setup" in LMP spec
    kLSAPConnListenPending,
    kLSAPConnListen,
    kLSAPConnAccept,
    kLSAPConnDataTransferReady,
    kLSAPDisconnectPending              // jdg: new state
};

enum LMPDUElements
{
    kLMPDUControlFlag           = 0x80, // Set on the fDstLSAPId field for non-data events
    kLMPDUReplyFlag             = 0x80, // Set for connect & access mode replies

    kLMPDUDataEvent             = 0x00,

    kLMPDUConnectRequest        = 0x01,
    kLMPDUConnectReply          = 0x81,

    kLMPDUDisconnectEvent       = 0x02,

    kLMPDUAccessModeRequest     = 0x03,
    kLMPDUAccessModeReply       = 0x83
};

enum LMPDUDisconnectReasons
{
    kIrUserRequestedDisconnect  = 0x01,
    kIrLAPUnexpectedDisconnect  = 0x02,
    kIrLAPFailedConnection      = 0x03,
    kIrLAPReset                 = 0x04,
    kIrLMMuxInitiatedDisconnect = 0x05,
    kIrDataSentOnDiscLSAPConn   = 0x06,
    kIrNonResponsiveLMMuxClient = 0x07,
    kIrNoAvailableLMMuxClient   = 0x08,
    kIrHalfOpen                 = 0x09,
    kIrIllegalSourceAddress     = 0x0A
};

enum LMPDUControlStatusValues
{
    kIrLMPDUControlSuccess      = 0x00,
    kIrLMPDUControlFailure      = 0x01,
    kIrLMPDUControlUnsupported  = 0xFF
};

enum LMPDUAccessModes
{
    kIrLMPMultiplexedMode       = 0x00,
    kIrLMPExclusiveMode         = 0x01
};


// Classes

// --------------------------------------------------------------------------------------------------------------------
//                      TControlPacket
// --------------------------------------------------------------------------------------------------------------------

class TLMPDUHeader
{
    public:

	    UByte           fDstLSAPId;
	    UByte           fSrcLSAPId;
	    UByte           fOpCode;
	    UByte           fInfo;
	    UByte           fMode;
};


// --------------------------------------------------------------------------------------------------------------------
//                      TLSAPConn
// --------------------------------------------------------------------------------------------------------------------

class TLSAPConn : public TIrStream
{
	    OSDeclareDefaultStructors(TLSAPConn);
    
    public:
	    static TLSAPConn *  tLSAPConn(TIrGlue* irda, TIrStream* client);
	    Boolean             Init(TIrGlue* irda, TIrStream* client);
	    void                free(void);

	    void                AssignId(ULong id);
	    TIrEvent*           GetPendConnLstn();
	    Boolean             YourData(TLMPDUHeader& header, Boolean justChecking);
	    void                OneSecTickerComplete();
	    UByte               GetMyLSAPId();

    private:

	    // TIrStream override
	    void                NextState(ULong event);

	    void                HandleDisconnectedStateEvent(ULong event);
	    void                HandleConnectPendingStateEvent(ULong event);
	    void                HandleConnectStateEvent(ULong event);
	    void                HandleListenPendingStateEvent(ULong event);
	    void                HandleListenStateEvent(ULong event);
	    void                HandleAcceptStateEvent(ULong event);
	    void                HandleDataTransferReadyStateEvent(ULong event);
	    void                HandleDisconnectPendingStateEvent(ULong event);

	    void                SaveCurrentRequest();
	    Boolean             InternalDisconnectRequest();
	    Boolean             InternalPutRequest();
	    void                PassRequestToLMP();
	    void                DisconnectStart(IrDAErr result, TIrDisconnectRequest *discRequest = nil);

	    void                GetControlFrame();
	    void                PutControlFrame(UByte opCode, UByte info);
	    void                GetDataFrame(Boolean resend = false);
	    void                PutDataFrame();

	    void                ConnLstnComplete(IrDAErr result);

	    void                StartConnectTimer();
	    void                StopConnectTimer();

	    // Fieldsä

	    UByte               fState;
	    Boolean             fConnecting;            // vs listening

	    TIrStream*          fClient;                // Upstream client

	    IrDAErr             fResult;                // Pending result for failed connect or listen
	    TIrEvent*           fPendConnLstn;          // Pending connect, listen request
	    CBuffer*            fConnLstnUserData;      // Buffer to send/recv out of band data w/connect or listen

	    CBuffer*            fGetData;               // Save get buffer in case resend is required
	    ULong               fGetOffset;             // Save offset in buffer in case resend is required
	    ULong               fGetLength;             // Save space available in buffer in case resend is required

	    UByte               fMyLSAPId;
	    UByte               fPeerLSAPId;

						    // JDG: listen/connect event records smashed!
	    ULong               fDevAddr;           // jdg: saved fDevAddr from listen complete
	    UByte               fLSAPId;            // jdg: save  fLSAPId  from listen complete

	    CList               *fPendingRequests;  // jdg: queue requests pending during disconnect

	    Boolean             fWatchdogTimerActive;
	    UByte               fWatchdogTimerCount;

};

#endif // __IRLSAPCONN_H

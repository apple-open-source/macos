/*
    File:       IrLAPConn.h

    Contains:   Methods for implementing IrLAPConn


*/


#ifndef __IRLAPCONN_H
#define __IRLAPCONN_H

#include "IrDATypes.h"
#include "IrStream.h"
#include "IrEvent.h"


// Forward reference
class TIrGlue;
class TIrLAP;
class TLSAPConn;
class TLMPDUHeader;
class CBufferSegment;



// Constants

enum IrLAPConnStates
{
    kIrLAPConnStandby,
    kIrLAPConnConnectOrListen,
    kIrLAPConnActive
};


// --------------------------------------------------------------------------------------------------------------------
//                      TIrLAPConn
// --------------------------------------------------------------------------------------------------------------------

class TIrLAPConn : public TIrStream
{
    OSDeclareDefaultStructors(TIrLAPConn);

    public:
	    static TIrLAPConn *tIrLAPConn(TIrGlue* irda);
	    void    free(void);
	    
	    Boolean         Init(TIrGlue* irda);
	    void            Reset();
	    void            DoIdleDisconnect();         // disconnect now if idle


	    void            Demultiplexor(CBufferSegment* inputBuffer);
	    ULong           FillInLMPDUHeader(TIrPutRequest* putRequest, UByte* buffer);

	    void            TimerComplete(ULong refCon);

    private:

	    // TIrStream override
	    void            NextState(ULong event);

	    void            HandleStandbyStateEvent(ULong event);
	    void            HandleConnectOrListenStateEvent(ULong event);
	    void            HandleActiveStateEvent(ULong event);

	    void            HandleGetDataRequest();
	    void            CleanupPendingGetRequestsAndReplies(TLSAPConn* lsapConn, IrDAErr returnCode);
	    void            CancelPendingGetRequests(TLSAPConn* lsapConn, IrDAErr returnCode);
	    void            ReplyToInvalidFrame(TLMPDUHeader& header, UByte replyOpCode, UByte replyInfo);
	    Boolean         ExtractHeader(CBufferSegment* inputBuffer, TLMPDUHeader& header, ULong& length);
	    Boolean         DataDelivered(TIrGetRequest* getRequest, TLMPDUHeader& header, ULong headerLength, CBufferSegment* dataBuffer);

	    void            StartIdleDisconnectTimer(void);
	    void            StopIdleDisconnectTimer(void);

	    // Fieldsä

	    UByte           fState;
	    Boolean         fConnected;
	    ULong           fPeerDevAddr;
	    CList*          fLSAPConnList;

	    CList*          fPendingGetRequests;
	    CList*          fUnmatchedGetReplys;
	    CList*          fPendingRequests;           // requests on hold until disconnect done
	    Boolean         fDisconnectPending;         // if a lap disconnect is pending
};

#endif // __IRLAPCONN_H

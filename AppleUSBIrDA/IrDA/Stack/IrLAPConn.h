/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
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

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
    File:       IrIASClient.h

    Contains:   Methods for implementing IrIASClient

*/


#ifndef __IRIASCLIENT_H
#define __IRIASCLIENT_H

#include "IrDATypes.h"
#include "IrStream.h"
#include "IrEvent.h"


// Forward reference
class TIrGlue;
class TLSAPConn;
class CBufferSegment;
class TIASAttribute;

// Constants

enum IASClientStates
{
    kIrIASClientDisconnected,
    kIrIASClientConnected
};

enum IASClientReceiveStates
{
    kIASClientReceiveReply,
    kIASClientReceiveWaitFinal
};

#define kIASClientBufferSize        128


// Classes

// --------------------------------------------------------------------------------------------------------------------
//                      TIASClient
// --------------------------------------------------------------------------------------------------------------------

class TIASClient : public TIrStream
{
	    OSDeclareDefaultStructors(TIASClient);
	    
    public:
	    static TIASClient * tIASClient(TIrGlue* irda, TIrStream* client);
	    void                free();
	    Boolean             Init(TIrGlue* irda, TIrStream* client);

    private:

	    // TIrStream override
	    void                NextState(ULong event);

	    void                HandleDisconnectedStateEvent(ULong event);
	    void                HandleConnectedStateEvent(ULong event);

	    IrDAErr             SendRequest();
	    void                ParseInput();
	    IrDAErr             ParseReply();

	    void                GetStart();
	    void                PutStart();
	    void                LookupComplete(IrDAErr result);

	    // Fieldsä

	    UByte               fState;
	    UByte               fReceiveState;

	    TIrStream           *fClient;               // Client of IASClient
	    TIrLookupRequest    *fLookupRequest;

							// we create and free the following
	    TLSAPConn           *fLSAPConn;             // My connection "handle"
	    TIrEvent            *fRequestReply;         // Buffer for all requests/replies
	    TIASAttribute       *fAttribute;
	    CBufferSegment      *fGetPutBuffer;


};

#endif // __IRIASCLIENT_H

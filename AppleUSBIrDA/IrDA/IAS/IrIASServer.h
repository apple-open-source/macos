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
    File:       IrIASServer.h

    Contains:   Methods for implementing the irda info server

*/


#ifndef __IRIASSERVER_H
#define __IRIASSERVER_H

#include "IrDATypes.h"
#include "IrStream.h"

// Forward reference
class TIrGlue;
class TIrLMP;
class TIASService;
class TIASAttribute;
class TLSAPConn;
class CBufferSegment;

// Constants

enum IASServerReceiveStates
{
    kIASServerReceiveStart,
    kIASServerReceiveWaitFinal,
    kIASServerWaitingToDie              // jdg hacking
};

#define kIASServerBufferSize        128
#define kIASMaxClassOrAttrStrLen    60


// Classes

// --------------------------------------------------------------------------------------------------------------------
//                      TIASServer
// --------------------------------------------------------------------------------------------------------------------

class TIASServer : public TIrStream
{
	    OSDeclareDefaultStructors(TIASServer);
	    
    public:
	    static TIASServer * tIASServer(TIrGlue *irda, TIASService* nameService);
	    void                free(void);
	    
	    void                ListenStart();              // call this once to start it up

    private:

	    // TIrStream override
	    void                NextState(ULong event);

	    Boolean             Init(TIrGlue *irda, TIASService* nameService);
	    void                GetStart();
	    void                PutStart();
	    void                ParseInput();
	    TIASAttribute       *ParseRequest(UByte& iasReturnCode);
	    Boolean             GotAValidString(UChar* string);
	    void                SendResponse(UByte iasReturnCode, TIASAttribute* attrElement);

	    // Fieldsä

	    UByte               fOpCode;
	    UByte               fReceiveState;

	    TIASService         *fNameService;          // Name registry/lookup service

	    TLSAPConn           *fLSAPConn;             // My connection "handle"
	    TIrEvent            *fRequestReply;         // Our event buffer

	    CBufferSegment      *fGetPutBuffer;
};

#endif // __IRIASSERVER_H

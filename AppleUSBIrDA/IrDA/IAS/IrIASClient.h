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

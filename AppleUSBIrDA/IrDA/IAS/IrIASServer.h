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

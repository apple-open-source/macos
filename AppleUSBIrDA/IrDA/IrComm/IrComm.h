/*
    File:       IrComm.h

    Contains:   IrCOMM protocol layer

*/

#ifndef __IrComm_h__
#define __IrComm_h__

#include "ttp.h"            // subclassing off tinytp
#include "IrDscInfo.h"      // for hints

class TIrGlue;
class IrDAComm;

enum {
    kTinyTPCredit   = 2             // USB queue is 4k bytes.  2k max per irda window, so 2 packets
};

class IrComm  : public TTinyTP      // hook up to TTinyTP/CIrLSAP/TIrStream/etal
{               
    OSDeclareDefaultStructors(IrComm);

public:
    static  IrComm *irComm(TIrGlue *irda, IrDAComm *irdacomm);
    Boolean Init(TIrGlue *irda, IrDAComm *irdacomm);
    
    void        free();
    UInt32      TxBufferAvailable(void);            // max number of bytes in next write
    UInt32      Write(UInt8 *buf, UInt32 length);   // send to peer
    void        ReturnCredit(UInt32 bytecount);     // our consumer has released this many bytes
    void        TryConnect(int slots);              // try to connect
    void        Listen();                           // listen for peer connect attempt
    void        Disconnect(void);                   // disconnect existing, abort pending
    
				
private:                            // we implement these for tinytp
	
    virtual void TTPDiscoverComplete (      // Discover has completed
	int     numFound,                   // number of peers discovered
	IrDAErr result);                    // result of discovery 
	    
    virtual void TTPLookupComplete (        // Lookup completed
	IrDAErr result,
	UInt32  peerLSAPId);

    virtual void TTPConnectIndication (     // Listen complete
	IrDAErr result,
	TTPSAP  SAP,                        // calling TTP SAP
	TIrQOS *ourQOS,                     // our QoS (post negotiation)
	TIrQOS *peerQOS,                    // peer QoS (post negotiation)
	int     MaxSduSize,                 // calling MaxSduSize
	TTPBuf  *UserData);                 // calling UserData

    virtual void TTPConnectConfirm (        // Connect complete
	TTPSAP  SAP,                        // called TTP SAP
	TIrQOS *ourQOS,                     // our QoS (post negotiation)
	TIrQOS *peerQOS,                    // peer QoS (post negotiation)
	int     MaxSduSize,                 // called MaxSduSize
	TTPBuf  *UserData);                 // called UserData
    
    virtual void TTPDisconnectIndication (  // Disconnect complete
	int     reason,                     // passed up from IrLMP (not)
	TTPBuf  *UserData);

    virtual void TTPDataIndication (        // Read complete
	TTPBuf  *UserData,                  // data read
	TTP_Read_Status status);            // Ok or Truncated

    virtual void TTPUDataIndication (       // UData Read complete (unimpld)
	TTPBuf  *UserData);                 // data read
    
    virtual void TTPAcceptDoneIndication(   // accept done (we're really open now)
	IrDAErr result);                    // just result code, buffer copied during accept call

    virtual void TTPBackEnable(void);       // Called when more TTP buffers available
    
    // fields ...
    
    IrDAComm        *fIrDAComm;             // back to my manager
    UInt32          fPeerAddress;           // address of our ircomm peer
    UInt16          fMaxPacketSize;         // max size to transmit to peer in one packet
    Boolean         fConnected;             // all the state we need


};  // class IrComm


#endif  // __IrComm_h__





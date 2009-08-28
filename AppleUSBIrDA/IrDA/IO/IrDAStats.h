/*
    File:       IrDAStats.h

    Contains:   Status and statistics of interest to outside clients

*/
#ifndef __IrDAStats__
#define __IrDAStats__

#include <libkern/OSTypes.h> 

enum {                              // lap connection state
    kIrDAStatusIdle,                // idle
    kIrDAStatusDiscoverActive,      // looking for peer
    kIrDAStatusConnected,           // connected
    kIrDAStatusBrokenConnection,    // still connected, but beam blocked
    kIrDAStatusInvalid,             // Invalid Status (Use by UI)
    kIrDAStatusOff                  // We have been powered down
};

typedef struct
{
    UInt8   connectionState;    // see enum
    UInt32  connectionSpeed;    // in bps
    UInt8   nickName[22];       // Nickname of peer (from discovery).  valid if connected

    UInt32  dataPacketsIn;      // total packets read
    UInt32  dataPacketsOut;     // total packets written
    UInt32  crcErrors;          // packets read with CRC errors (if available) 
    UInt32  ioErrors;           // packets read with other errors (if available)

    UInt32  recTimeout;         // number of recv timeouts
    UInt32  xmitTimeout;        // number of transmit timeouts (if implemented)

    UInt32  iFrameRec;          // Info frames received (data carrying packets)
    UInt32  iFrameSent;         // Info frames sent (data carrying packets)

    UInt32  uFrameRec;          // U frames received
    UInt32  uFrameSent;         // U frames sent

    UInt32  dropped;            // input packet dropped for (one of several) reasons
    UInt32  resent;             // count of our packets that we have resent

    UInt32  rrRec;              // count of RR packets read
    UInt32  rrSent;             // count of RR packets written
	    
    UInt16  rnrRec;             // count of receiver-not-ready packets read
    UInt16  rnrSent;            // count of receiver-not-ready packets sent
	    
    UInt8   rejRec;             // number of reject packets received
    UInt8   rejSent;            // number of reject packets sent
	    
    UInt8   srejRec;            // ?
    UInt8   srejSent;
			
    UInt32  protcolErrs;        // ?
} IrDAStatus;


#endif  // __IrDAStats__
/*
    File:       IrGlue.h

    Contains:   Top of the IrDA stack


*/


#ifndef __IRGLUE_H
#define __IRGLUE_H

#include "IrDAStats.h"
#include "IrDATypes.h"


// Constants


enum IrLSAPIds
{
    kNameServerLSAPId           = 0x00,
    kAssignDynamicLSAPId        = 0x00,
    kLastValidLSAPId            = 0x6F,
    kInvalidLSAPId              = 0xFF,
    kPendingConnectLSAPId       = 0xFF
};


class TIrStream;
class CBuffer;
class TLSAPConn;
class TIASService;
class TIrEvent;
class CIrDevice;
class TIrLAP;
class TIrLMP;
class TIrLAPConn;
class CIrDiscovery;
class TIASService;
class TIASServer;
class CTimer;
class TIrQOS;
class CList;
class TIrDscInfo;
class AppleIrDASerial;
class AppleIrDA;
struct USBIrDAQoS;

enum {
    kNumTimers = 3,                 // we have 3 timers, id's 0,1,2
    kTimer_LAP = 0,                 // LAP's timer
    kTimer_LMP = 1,                 // LMP's 1 second timer (LSAPConn)
    kTimer_LAPConn = 2              // LAPConn's idle disconnect timer
};

class IOWorkLoop;

class TIrGlue : public OSObject
{
    OSDeclareDefaultStructors(TIrGlue);

public:
    
    static TIrGlue *tIrGlue(AppleIrDASerial *driver, AppleIrDA *appleirda, IOWorkLoop *work, USBIrDAQoS *qos);
    
    Boolean         init(AppleIrDASerial *driver, AppleIrDA *appleirda, IOWorkLoop *work, USBIrDAQoS *qos);
    void            free(void);
    
    // Packets coming in from the driver
    void            ReadComplete(UInt8 *buffer, UInt32 length);     // add return code?
    void            RunQueue();                                     // shorthand for TIrStream::RunQueue
    
    // Device transmit finished, hand back to driver
    void            TransmitComplete(Boolean worked);
    
    // Device finished changing speed
    void            SetSpeedComplete(Boolean worked);
    
    void            Start(void);    // do any runtime startup.
    void            Stop(void);     // usb driver isn't available anymore
	    
    // Client Methods
	    
	    IrDAErr         RegisterMyNameAndLSAPId( UChar* className, UChar* attrName, UInt32 * lsapId );
	    
	    IrDAErr         ConnectStart    (   TIrStream   *   client,
						UInt32          myLSAPId,
						UInt32          devAddr,
						UInt32          peerLSAPId,
						CBuffer     *   clientData,
						TLSAPConn   **  theLSAP );

	    IrDAErr         ListenStart     (   TIrStream   *   client,         // Caller
						UInt32          lsapId,         // Preallocated LSAP Id
						CBuffer     *   clientData,     // Data to pass with connect
						TLSAPConn   **  theLSAP     );  // The allocated LSAP


	    // LSAPId management routines for LSAPConn
	    IrDAErr         ObtainLSAPId    ( UInt32 & desiredLSAPId );
	    void            ReleaseLSAPId   ( UInt32 lsapId );

	    // Temp(?!) until this is cleaned up some more
	    TIASService     *GetNameService(void);      // get nameservice
	    
    // Lower Layer methods
    
	    // Terminate helper routine for IrLAP
	    void            Disconnected(Boolean reset_lap);

	    // Timer helper routines for IrLMP, IrLAP.  Id is [0..2]
	    void            StartTimer(int id, TTimeout timeDelay, UInt32 refCon);
	    void            StopTimer(int id);

	    void            TimerComplete(UInt32 refCon);
	    
	    // Event related management

	    TIrEvent        *GrabEventBlock(UInt32 event = 0, UInt32 size = 0); // size is currently ignored
	    void            ReleaseEventBlock(TIrEvent *reqBlock);

	    // Inline member getters
	    TIrLAP              *GetLAP             (void);
	    TIrLMP              *GetLMP             (void);
	    TIrLAPConn          *GetLAPConn         (void);
	    CIrDevice           *GetIrDevice        (void);
	    TIrQOS              *GetMyQOS           (void);
	    TIrQOS              *GetPeerQOS         (void);
	    CIrDiscovery        *GetDiscovery       (void);
	    
	    Boolean             IsLAPConnected(void);           // check with lap re our connection status
	    void                DoIdleDisconnect(void);         // tell lapconn to hurry up and disconnect now if idle

	    CTimer              *GetTimer(int id);              // this is really just used by glue, move to private?
	    
    // statistics
	    void            GetIrDAStatus(IrDAStatus *status);

    private:
	    Boolean         InitNameService(void);  // set initial values for our IAS service

	    CTimer          *fTimers[kNumTimers];   // our three timers
	    UInt32           fLSAPIdsInUse;         // bitmap of allocated LSAP ids (bug?  max 32 in use)

	    
	    TIrLAP          *fIrLAP;                // one lap
	    TIrLMP          *fIrLMP;                // one lmp
	    TIrLAPConn      *fIrLAPConn;            // one lap link
	    CIrDevice       *fIrDevice;             // link back to IOKit's usb/irda bridge
	    CIrDiscovery    *fIrDiscovery;          // one discovery engine
	    TIrQOS          *fMyQOS;
	    TIrQOS          *fPeerQOS;
	    
	    TIASService     *fNameService;          // The IAS name database
	    TIASServer      *fNameServer;           // Our server to access the IAS database
	    
	    AppleIrDA       *fAppleIrDA;            // link to iokit message sender
	    UInt8           fLastState;             // irda state change --> message to clients
};

#define CheckReturn(x) { check(x); return (x); }

inline CTimer       *   TIrGlue::GetTimer(int id)       { CheckReturn(fTimers[id]); }
inline TIASService  *   TIrGlue::GetNameService(void)   { CheckReturn(fNameService); }

inline TIrLAP       *   TIrGlue::GetLAP(void)           { CheckReturn(fIrLAP); }
inline TIrLMP       *   TIrGlue::GetLMP(void)           { CheckReturn(fIrLMP); }
inline TIrLAPConn   *   TIrGlue::GetLAPConn(void)       { CheckReturn(fIrLAPConn); }
inline CIrDevice    *   TIrGlue::GetIrDevice(void)      { CheckReturn(fIrDevice); }
inline TIrQOS       *   TIrGlue::GetMyQOS(void)         { CheckReturn(fMyQOS); }
inline TIrQOS       *   TIrGlue::GetPeerQOS(void)       { CheckReturn(fPeerQOS); }
inline CIrDiscovery *   TIrGlue::GetDiscovery(void)     { CheckReturn(fIrDiscovery); }

#endif // __IRGLUE_H

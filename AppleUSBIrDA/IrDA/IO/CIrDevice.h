#ifndef __IRDEVICE_H
#define __IRDEVICE_H

#include "IrDATypes.h"
#include "IrDAStats.h"

class CBufferSegment;
class TIrLAPPutBuffer;
class TIrGlue;
class AppleIrDASerial;

class CIrDevice : public  OSObject
{
    OSDeclareDefaultStructors(CIrDevice);

public:
    static CIrDevice * cIrDevice(TIrGlue *irda, AppleIrDASerial *driver);
    Boolean     Init(TIrGlue *irda, AppleIrDASerial *driver);
    void        free();
    void        Start(void);
    void        Stop(void);
    
    void        ReadComplete(UInt8 *buffer, UInt32 length);     // incoming packet from the hardware
    void        TransmitComplete(Boolean worked);               // scc/usb write finished
    void        SetSpeedComplete(Boolean worked);               // set speed finally finished
	
    void        SetLAPAddress(UInt8 addr);          // Set our LAP address
    UInt8       GetLAPAddress(void);                // return our current LAP address
    Boolean     ValidFrameAddress(UInt8 aField);    // Check address field for broadcast or our address
    
    //virtual       void    UpdatePrefData(PrefDataPtr prefs) = 0;  // override qos resources (needed??)
    
    void    StartTransmit(TIrLAPPutBuffer* outputBuffer, ULong leadInCount);
    void    StartReceive(CBufferSegment* inputBuffer);
    void    StopReceive(void);
    
    IrDAErr ChangeSpeed(unsigned long bps);
    UInt32  GetSpeed(void);
    
    Boolean GetMediaBusy(void);                 // true if any traffic since last reset
    void    ResetMediaBusy(void);               // reset media busy flag
    
    
    // Statistics
    void    ResetStats(void);
    void    GetStatus(IrDAStatus *stats);
    
    void    Stats_DataPacketIn      (void);
    void    Stats_DataPacketOut     (void);
    void    Stats_CRCError          (void);
    void    Stats_IOError           (void);
    void    Stats_ReceiveTimeout    (void);
    void    Stats_TransmitTimeout   (void);
	
    void    Stats_IFrameRec         (void);
    void    Stats_IFrameSent        (void);

    void    Stats_UFrameRec         (void);
    void    Stats_UFrameSent        (void);
    
    void    Stats_PacketDropped     (void);
    void    Stats_PacketResent      (void);
    
    void    Stats_RRRec             (void);
    void    Stats_RRSent            (void);

    void    Stats_RNRRec            (void);
    void    Stats_RNRSent           (void);

    void    Stats_REJRec            (void);
    void    Stats_REJSent           (void);

    void    Stats_SREJRec           (void);
    void    Stats_SREJSent          (void);
    
private:
    TIrGlue                 *fIrDA;             // hmmm
    AppleIrDASerial         *fDriver;           // back to scc or usb driver
    CBufferSegment          *fGetBuffer;        // input buffer or nil if no read pending
    IrDAStatus              fIrStatus;
    UInt8                   fLAPAddr;           // our lap address for packet filtering / validation
    UInt32                  fSpeed;             // current speed (needed to save here?)
    UInt32                  fBofs;              // current bof count (needed to save here?)
	    
    Boolean                 fXmitting;          // true if waiting for xmit complete
};


inline UInt8 CIrDevice::GetLAPAddress(void)         { return fLAPAddr; };
inline UInt32 CIrDevice::GetSpeed(void)             { return fSpeed; };
inline void CIrDevice::GetStatus(IrDAStatus *status)    { *status = fIrStatus; };

inline void CIrDevice::Stats_DataPacketIn   (void)  { fIrStatus.dataPacketsIn++;    };
inline void CIrDevice::Stats_DataPacketOut  (void)  { fIrStatus.dataPacketsOut++;   };
inline void CIrDevice::Stats_CRCError       (void)  { fIrStatus.crcErrors++;        };
inline void CIrDevice::Stats_IOError        (void)  { fIrStatus.ioErrors++;         };

inline void CIrDevice::Stats_ReceiveTimeout (void)  { fIrStatus.recTimeout++;   };
inline void CIrDevice::Stats_TransmitTimeout(void)  { fIrStatus.xmitTimeout++;  };


inline void CIrDevice::Stats_IFrameRec      (void)  { fIrStatus.iFrameRec++;    };
inline void CIrDevice::Stats_IFrameSent     (void)  { fIrStatus.iFrameSent++;   };
inline void CIrDevice::Stats_UFrameRec      (void)  { fIrStatus.uFrameRec++;    };
inline void CIrDevice::Stats_UFrameSent     (void)  { fIrStatus.uFrameSent++;   };

inline void CIrDevice::Stats_PacketDropped  (void)  { fIrStatus.dropped++;      };
inline void CIrDevice::Stats_PacketResent   (void)  { fIrStatus.resent++;       };

inline void CIrDevice::Stats_RRRec          (void)  { fIrStatus.rrRec++;            };
inline void CIrDevice::Stats_RRSent         (void)  { fIrStatus.rrSent++;       };

inline void CIrDevice::Stats_RNRRec         (void)  { fIrStatus.rnrRec++;       };
inline void CIrDevice::Stats_RNRSent        (void)  { fIrStatus.rnrSent++;      };

inline void CIrDevice::Stats_REJRec         (void)  { fIrStatus.rejRec++;       };
inline void CIrDevice::Stats_REJSent        (void)  { fIrStatus.rejSent++;      };

inline void CIrDevice::Stats_SREJRec        (void)  { fIrStatus.srejRec++;      };
inline void CIrDevice::Stats_SREJSent       (void)  { fIrStatus.srejSent++;     };


#endif // __IRDEVICE_H

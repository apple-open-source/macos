/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1999-2000 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Tom Sherman
 *
 */


#ifndef _IOKIT_MACRISC2_H
#define _IOKIT_MACRISC2_H

#include <IOKit/platform/ApplePlatformExpert.h>

enum
{
    kMacRISC2TypePowerMac = 1,
    kMacRISC2TypePowerBook,
};

class MacRISC2PE : public ApplePlatformExpert
{
    OSDeclareDefaultStructors(MacRISC2PE);
  
    friend class MacRISC2CPU;
  
private:
    unsigned long *uniNBaseAddress;
    unsigned long uniNVersion;
    IOService * usb1;
    IOService * usb2;
    IOService * keylargoUSB1;
    IOService * keylargoUSB2;
    class IOPMPagingPlexus * plexus;
    class IOPMSlotsMacRISC2 * slotsMacRISC2;
    IOLock * mutex;
    
    virtual unsigned long readUniNReg(unsigned long offest);
    virtual void writeUniNReg(unsigned long offest, unsigned long data);
  
    void getDefaultBusSpeeds(long *numSpeeds, unsigned long **speedList);
    void enableUniNEthernetClock(bool enable);
    void enableUniNFireWireClock(bool enable);
    void enableUniNFireWireCablePower(bool enable);
    IOReturn accessUniN15PerformanceRegister(bool write, long regNumber, unsigned long *data);
  
    void PMInstantiatePowerDomains ( void );
    void PMRegisterDevice(IOService * theNub, IOService * theDevice);
    IORegistryEntry * retrievePowerMgtEntry (void);
  
public:
    virtual bool start(IOService *provider);
    virtual bool platformAdjustService(IOService *service);
    virtual IOReturn callPlatformFunction(const OSSymbol *functionName,
					bool waitForFunction,
                                        void *param1, void *param2,
                                        void *param3, void *param4);
};

// Uni-North Register Information

#define kUniNVersion               (0x0000)
#define kUniNVersion107            (0x0003)
#define kUniNVersion10A            (0x0007)
#define kUniNVersion150            (0x0010)

#define kUniNClockControl          (0x0020)
#define kUniNFirewireClockEnable   (1 << 2)
#define kUniNEthernetClockEnable   (1 << 1)
#define kUniNPCI2ClockEnable       (1 << 0)

#define kUniNPowerMngmnt           (0x0030)
#define kUniNNormal                (0x00)
#define kUniNIdle2                 (0x01)
#define kUniNSleep                 (0x02)

#define kUniNArbCtrl               (0x0040)
#define kUniNArbCtrlQAckDelayShift (15)
#define kUniNArbCtrlQAckDelayMask  (0x0e1f8000)
#define kUniNArbCtrlQAckDelay      (0x30)
#define kUniNArbCtrlQAckDelay105   (0x00)

#define kUniNHWInitState           (0x0070)
#define kUniNHWInitStateSleeping   (0x01)
#define kUniNHWInitStateRunning    (0x02)

// Uni-N 1.5 Performance Monitoring Registers
#define kUniNMMCR                  (0x0F00)
#define kUniNMCMDR                 (0x0F10)
#define kUniNMPMC1                 (0x0F20)
#define kUniNMPMC2                 (0x0F30)
#define kUniNMPMC3                 (0x0F40)
#define kUniNMPMC4                 (0x0F50)

#endif /* ! _IOKIT_MACRISC2_H */

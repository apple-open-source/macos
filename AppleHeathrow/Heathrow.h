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
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Josh de Cesare
 *
 */

#ifndef _IOKIT_HEATHROW_H
#define _IOKIT_HEATHROW_H

#include <IOKit/platform/AppleMacIO.h>

#include <IOKit/IOInterrupts.h>
#include <IOKit/IOInterruptController.h>

#define kPrimaryHeathrow   (0)
#define kSecondaryHeathrow (1)

#define kNumVectors        (64)
#define kVectorsPerReg     (32)

#define kTypeLevelMask     (0x1FF00000)

#define kEvents1Offset     (0x00020)
#define kEvents2Offset     (0x00010)
#define kMask1Offset       (0x00024)
#define kMask2Offset       (0x00014)
#define kClear1Offset      (0x00028)
#define kClear2Offset      (0x00018)
#define kLevels1Offset     (0x0002C)
#define kLevels2Offset     (0x0001C)
#define kChassisLightColor   (0x00032)


class HeathrowInterruptController;

class Heathrow : public AppleMacIO
{
  OSDeclareDefaultStructors(Heathrow);

private:

    /* Feature Control Register */

    enum {
            heathrowFCTrans         = 24,			// 0 = Transceiver On (for SCC ports)
            heathrowFCMBPwr         = 25,			// 1 = power off Media Bay
            heathrowFCPCIMBEn       = 26,			// 1 = enable PCI Media Bay
            heathrowFCATAMBEn       = 27,			// 1 = enable ATA Media Bay
            heathrowFCFloppyEn      = 28,			// 1 = enable floppy
            heathrowFCATAINTEn      = 29,			// 1 = enable internal ATA inputs
            heathrowFCATA0Reset     = 30,			// reset ATA0
            heathrowFCMBReset       = 31,			// reset Media Bay
            heathrowFCIOBusEn       = 16,			// IO Bus Enable
            heathrowFCSCCCEn        = 17,			// 0 = Stop SCC clock
            heathrowFCSCSICEn       = 18,			// 0 = Stop SCSE clock
            heathrowFCSWIMCEn       = 19,			// 0 = Stop SWIM clock
            heathrowFCSndPwr        = 20,			// 0 = power off to sound chip
            heathrowFCSndClkEn      = 21,			// 1 = enable external shift sound clock
            heathrowFCSCCAEn        = 22,			// 1 = enable SCCA
            heathrowFCSCCBEn        = 23,			// 1 = enable SCCB
            heathrowFCVIAPort       = 8,			// 1 = VIA functions in port mode
            heathrowFCPWM           = 9,			// 0 = turns off PWM counters
            heathrowFCHookPB        = 10,			// changes functions of IO pins
            heathrowFCSWIM3         = 11,			// changes functions of floppy pins
            heathrowFCAud22         = 12,			// 1 = SND_22M is running
            heathrowFCSCSILink      = 13,			//
            heathrowFCArbByPass     = 14,			// 1 = internal arbiter by passed
            heathrowFCATA1Reset     = 15,			//
            heathrowFCSCCPClk       = 0,			// 1 = SCC pClk forced low
            heathrowFCResetSCC      = 1,			// 1 = reset SCC cell

            heathrowFCMediaBaybits  = (1<<heathrowFCPCIMBEn)|(1<<heathrowFCATAMBEn)|(1<<heathrowFCFloppyEn),
            heathrowFCMBlogical	    = (1<<heathrowFCMBPwr)|(1<<heathrowFCMBReset)	// these bits are negative true logic
    };
    
    // register backup (to save the status before to sleep and restore
    // at wake).

    // 6522 VIA1 (and VIA2) register offsets
    enum
          {
          vBufB  =   0,        // BUFFER B
          vBufAH =   0x200,    // buffer a (with handshake) [ Dont use! ]
          vDIRB  =   0x400,    // DIRECTION B
          vDIRA  =   0x600,    // DIRECTION A
          vT1C   =   0x800,    // TIMER 1 COUNTER (L.O.)
          vT1CH  =   0xA00,    // timer 1 counter (high order)
          vT1L   =   0xC00,    // TIMER 1 LATCH (L.O.)
          vT1LH  =   0xE00,    // timer 1 latch (high order)
          vT2C   =   0x1000,   // TIMER 2 LATCH (L.O.)
          vT2CH  =   0x1200,   // timer 2 counter (high order)
          vSR    =   0x1400,   // SHIFT REGISTER
          vACR   =   0x1600,   // AUX. CONTROL REG.
          vPCR   =   0x1800,   // PERIPH. CONTROL REG.
          vIFR   =   0x1A00,   // INT. FLAG REG.
          vIER   =   0x1C00,   // INT. ENABLE REG.
          vBufA  =   0x1E00,   // BUFFER A
          vBufD  =   vBufA     // disk head select is buffer A
          };

    // This is a short version of the IODBDMAChannelRegisters which includes only
    // the registers we actually mean to save
    struct DBDMAChannelRegisters {
        UInt32 	commandPtrLo;
        UInt32 	interruptSelect;
        UInt32 	branchSelect;
        UInt32 	waitSelect;
    };
    typedef struct DBDMAChannelRegisters DBDMAChannelRegisters;
    typedef volatile DBDMAChannelRegisters *DBDMAChannelRegistersPtr;

    struct HeathrowState {
        bool                     	thisStateIsValid;
        UInt32				interruptMask1;
        UInt32				interruptMask2;
        UInt32				featureControlReg;
        UInt32				auxControlReg;
        DBDMAChannelRegisters		savedDBDMAState[12];
        UInt8				savedVIAState[9];
    };
    typedef struct HeathrowState HeathrowState;
    HeathrowState savedState;

    // Remeber if the media bay needs to be turnedOn:
    bool mediaIsOn;

private:
  IOLogicalAddress             heathrowBaseAddress;
  long                         heathrowNum;
  HeathrowInterruptController  *interruptController;
  
  virtual bool installInterrupts(IOService *provider);
  virtual OSSymbol *getInterruptControllerName(void);
  
  virtual void processNub(IOService *nub);
  virtual void enableMBATA();
  virtual void powerMediaBay(bool powerOn, UInt8 whichDevice);

  void EnableSCC(bool state);
  void PowerModem(bool state);
  void ModemResetLow();
  void ModemResetHigh();

  // callPlatformFunction symbols
  const OSSymbol 	*heathrow_sleepState;
  const OSSymbol 	*heathrow_powerMediaBay;
  const OSSymbol 	*heathrow_set_light;
  const OSSymbol 	*heathrow_writeRegUInt8;
  const OSSymbol 	*heathrow_safeWriteRegUInt8;
  const OSSymbol 	*heathrow_safeReadRegUInt8;
  const OSSymbol 	*heathrow_safeWriteRegUInt32;
  const OSSymbol 	*heathrow_safeReadRegUInt32;

  // this is to ensure mutual exclusive access to
  // the keylargo registers:
  IOSimpleLock *mutex;

public:
  virtual bool start(IOService *provider);

  virtual IOReturn callPlatformFunction(const OSSymbol *functionName,
					bool waitForFunction,
                                        void *param1, void *param2,
                                        void *param3, void *param4);

  // PM MEthods:
  void initForPM (IOService *provider);
  IOReturn setPowerState(unsigned long powerStateOrdinal, IOService* whatDevice);
  
  // Sleep/Wake methods:
  virtual void sleepState(bool sleepMe);
  virtual void saveVIAState(void);
  virtual void restoreVIAState(void);
  virtual void saveDMAState(void);
  virtual void restoreDMAState(void);
  virtual void saveGPState(void);
  virtual void restoreGPState(void);
  virtual void saveInterruptState(void);
  virtual void restoreInterruptState(void);
  virtual void setChassisLightFullpower(bool fullpwr);

  virtual UInt8     readRegUInt8(unsigned long offset);
  virtual void      writeRegUInt8(unsigned long offset, UInt8 data);
  virtual UInt32    readRegUInt32(unsigned long offset);
  virtual void      writeRegUInt32(unsigned long offset, UInt32 data);

  // share register access:
  void safeWriteRegUInt8(unsigned long offset, UInt8 mask, UInt8 data);
  UInt8 safeReadRegUInt8(unsigned long offset);
  void safeWriteRegUInt32(unsigned long offset, UInt32 mask, UInt32 data);
  UInt32 safeReadRegUInt32(unsigned long offset);
};


class HeathrowInterruptController : public IOInterruptController
{
  OSDeclareDefaultStructors(HeathrowInterruptController);
  
private:
  IOService         *parentNub;
  IOLock            *taskLock;
  IOLogicalAddress  interruptControllerBase;
  unsigned long     pendingEvents1;
  unsigned long     pendingEvents2;
  unsigned long     events1Reg;
  unsigned long     events2Reg;
  unsigned long     mask1Reg;
  unsigned long     mask2Reg;
  unsigned long     clear1Reg;
  unsigned long     clear2Reg;
  unsigned long     levels1Reg;
  unsigned long     levels2Reg;

  void privDisableVectorHard(long vectorNumber, IOInterruptVector *vector);
  void privEnableVector(long vectorNumber, IOInterruptVector *vector);
  void privCauseVector(long vectorNumber, IOInterruptVector *vector);

public:
  virtual IOReturn initInterruptController(IOService *provider,
					   IOLogicalAddress iBase);

  virtual void	clearAllInterrupts(void);
  
  virtual IOInterruptAction getInterruptHandlerAddress(void);
  virtual IOReturn handleInterrupt(void *refCon, IOService *nub, int source);
  
  virtual bool vectorCanBeShared(long vectorNumber, IOInterruptVector *vector);
  virtual int  getVectorType(long vectorNumber, IOInterruptVector *vector);
  virtual void disableVectorHard(long vectorNumber, IOInterruptVector *vector);
  virtual void enableVector(long vectorNumber, IOInterruptVector *vector);
  virtual void causeVector(long vectorNumber, IOInterruptVector *vector);
};


#endif /* ! _IOKIT_HEATHROW_H */

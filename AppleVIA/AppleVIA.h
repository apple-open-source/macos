/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Josh de Cesare
 *
 */

#ifndef _IOKIT_APPLEVIA_H
#define _IOKIT_APPLEVIA_H

#include <IOKit/IOInterrupts.h>
#include <IOKit/IOInterruptController.h>

#define kInterruptControllerName "VIAInterruptController"

#define kVIADeviceTypeCuda     (0)
#define kVIADeviceTypePMU      (1)

#define kNumVectors (7)

#define kAuxControlOffset      (0x01600)
#define kT1CounterLowOffset    (0x00800)
#define kT1CounterHightOffset  (0x00A00)
#define kT1LatchLowOffset      (0x00C00)
#define kT1LatchHighOffset     (0x00E00)

#define kIEOffset              (0x01C00)
#define kIFOffset              (0x01A00)
#define kPCOffset              (0x01800)

class AppleVIADevice;
class AppleVIAInterruptController;

class AppleVIA : public IOService
{
  OSDeclareDefaultStructors(AppleVIA);
  
private:
  IOLogicalAddress            viaBaseAddress;
  int                         viaDeviceType;
  AppleVIADevice              *viaDevice;
  AppleVIAInterruptController *interruptController;
  
public:
  virtual bool start(IOService *provider);
  virtual AppleVIADevice *createNub(void);
};

class AppleVIADevice : public IOService
{
  OSDeclareDefaultStructors(AppleVIADevice);
public: 
};


class AppleVIAInterruptController : public IOInterruptController
{
  OSDeclareDefaultStructors(AppleVIAInterruptController);
  
private:
  IOService              *parentNub;
  unsigned char          pendingEvents;
  volatile unsigned char *IEReg;
  volatile unsigned char *IFReg;
  volatile unsigned char *PCReg;
  
public:
  virtual IOReturn initInterruptController(IOService *provider, IOLogicalAddress interruptControllerBase);
  
  virtual IOInterruptAction getInterruptHandlerAddress(void);
  virtual IOReturn handleInterrupt(void *refCon,
				   IOService *nub, int source);
  
  virtual void disableVectorHard(long vectorNumber, IOInterruptVector *vector);
  virtual void enableVector(long vectorNumber, IOInterruptVector *vector);
  virtual void causeVector(long vectorNumber, IOInterruptVector *vector);
};


#endif /* ! _IOKIT_APPLEVIA_H */

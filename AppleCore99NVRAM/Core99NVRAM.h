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


#ifndef _IOKIT_CORE99NVRAM_H
#define _IOKIT_CORE99NVRAM_H

#include <IOKit/nvram/IONVRAMController.h>

// Commands for the BootROM Flash.
#define kCore99NVRAMIdentifyCmd       (0x90)
#define kCore99NVRAMManufactureIDAddr (0x00)
#define kCore99NVRAMDeviceIDAddr      (0x01)
#define kCore99NVRAMResetDeviceCmd    (0xFF)
#define kCore99NVRAMEraseSetupCmd     (0x20)
#define kCore99NVRAMEraseConfirmCmd   (0xD0)
#define kCore99NVRAMWriteSetupCmd     (0x40)
#define kCore99NVRAMWriteConfirmCmd   (0x70)

// Status for the BootROM Flash.
#define kCore99NVRAMStatusRegErrorMask         (0x38)
#define kCore99NVRAMStatusRegVoltageErrorMask  (0x08)
#define kCore99NVRAMStatusRegWriteErrorMask    (0x10)
#define kCore99NVRAMStatusRegEraseErrorMask    (0x20)
#define kCore99NVRAMStatusRegSequenceErrorMask (0x30)
#define kCore99NVRAMStatusRegCompletionMask    (0x80)

// Manufacturer Specific Information.
#define kCore99NVRAMMicronManufactureID      (0x89)
#define kCore99NVRAMMicronTopBootDeviceID    (0x98)
#define kCore99NVRAMMicronBottomBootDeviceID (0x99)
#define kCore99NVRAMSharpManufactureID       (0xB0)
#define kCore99NVRAMSharpDeviceID            (0x4B)


#define kCore99NVRAMSize        (0x2000)
#define kCore99NVRAMMapSize     (kCore99NVRAMSize * 4)
#define kCore99NVRAMAreaAOffset (kCore99NVRAMSize * 0)
#define kCore99NVRAMAreaBOffset (kCore99NVRAMSize * 1)
#define kCore99NVRAMAdlerStart  (20)
#define kCore99NVRAMAdlerSize   (kCore99NVRAMSize - kCore99NVRAMAdlerStart)
#define kCore99NVRAMSignature   (0x5A)

struct Core99NVRAMHeader {
  unsigned char  signature;
  unsigned char  checksum;
  unsigned short length;
  char           name[12];
  unsigned long  adler32;
  unsigned long  generation;
  unsigned long  reserved1;
  unsigned long  reserved2;
};
typedef struct Core99NVRAMHeader Core99NVRAMHeader;


class Core99NVRAM : public IONVRAMController
{
  OSDeclareDefaultStructors(Core99NVRAM);

private:
  unsigned char *nvramBaseAddress;
  volatile unsigned char *nvramCurrent;
  volatile unsigned char *nvramNext;
  unsigned char *nvramShadow;
  unsigned long generation;
  
  virtual IOReturn eraseBlock(void);
  virtual IOReturn verifyEraseBlock(void);
  virtual IOReturn writeBlock(unsigned char *sourceAddress);
  virtual IOReturn verifyWriteBlock(unsigned char *sourceAddress);
  virtual IOReturn waitForCommandDone(void);
    
  virtual unsigned long validateGeneration(unsigned char *nvramBuffer);
  virtual unsigned char chrpCheckSum(unsigned char *buffer);
  virtual unsigned long adler32(unsigned char *buffer, long length);
  
public:
  virtual bool start(IOService *provider);
  
  virtual void sync(void);
  virtual IOReturn read(IOByteCount offset, UInt8 *buffer,
			IOByteCount length);
  virtual IOReturn write(IOByteCount offset, UInt8 *buffer,
			 IOByteCount length);
};

#endif /* ! _IOKIT_CORE99NVRAM_H */

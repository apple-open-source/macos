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
 *  DRI: Josh de Cesare
 *
 */

#include <IOKit/IOLib.h>
#include <IOKit/IODeviceMemory.h>

#include "Core99NVRAM.h"

// Note: Since the storage for NVRAM is inside the BootROM's high
//       logevity flash section, it should be good for a lifetime
//       of erase and write cycles.  However, all care should be
//       exersized when writing.  Writes should be done only once
//       per boot, and only if the NVRAM has changed.  If periodic
//       syncing is desired it should have a period no less than
//       five minutes.

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#define super IONVRAMController

OSDefineMetaClassAndStructors(Core99NVRAM, IONVRAMController);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool Core99NVRAM::start(IOService *provider)
{
  IOMemoryMap        *nvramMemoryMap;
  unsigned long      gen1, gen2;
  
  // Get the base address for the nvram.
  nvramMemoryMap = provider->mapDeviceMemoryWithIndex(0);
  if (nvramMemoryMap == 0) return false;
  nvramBaseAddress = (unsigned char *)nvramMemoryMap->getVirtualAddress();
  
  // Allocte the nvram shadow.
  nvramShadow = (unsigned char *)IOMalloc(kCore99NVRAMSize);
  if (nvramShadow == 0) return false;

  // Find the current nvram partition and set the next.
  gen1 = validateGeneration(nvramBaseAddress + kCore99NVRAMAreaAOffset);
  gen2 = validateGeneration(nvramBaseAddress + kCore99NVRAMAreaBOffset);
  
  if (gen1 > gen2) {
    generation = gen1;
    nvramCurrent = nvramBaseAddress + kCore99NVRAMAreaAOffset;
    nvramNext    = nvramBaseAddress + kCore99NVRAMAreaBOffset;
  } else {
    generation = gen2;
    nvramCurrent = nvramBaseAddress + kCore99NVRAMAreaBOffset;
    nvramNext    = nvramBaseAddress + kCore99NVRAMAreaAOffset;
  }
  
  // Copy the nvram into the shadow.
  bcopy(nvramCurrent, nvramShadow, kCore99NVRAMSize);
  
  return super::start(provider);
}

void Core99NVRAM::sync(void)
{
  Core99NVRAMHeader *header;
  unsigned char     *tmpBuffer;
  
  // Don't write the BootROM if nothing has changed.
  if (!bcmp(nvramShadow, nvramCurrent, kCore99NVRAMSize)) return;
  
  header = (Core99NVRAMHeader *)nvramShadow;
  
  header->generation = ++generation;
  
  header->checksum = chrpCheckSum(nvramShadow);
  
  header->adler32 = adler32(nvramShadow + kCore99NVRAMAdlerStart,
			    kCore99NVRAMAdlerSize);
  
  if (eraseBlock() != kIOReturnSuccess) return;
  
  if (writeBlock(nvramShadow) != kIOReturnSuccess) return;
  
  tmpBuffer = (unsigned char *)nvramCurrent;
  nvramCurrent = nvramNext;
  nvramNext = tmpBuffer;
}

IOReturn Core99NVRAM::read(IOByteCount offset, UInt8 *buffer,
			   IOByteCount length)
{
  if (nvramShadow == 0) return kIOReturnNotReady;
  
  if ((buffer == 0) || (length <= 0) || (offset < 0) ||
      ((offset + length) > kCore99NVRAMSize))
    return kIOReturnBadArgument;
  
  bcopy(nvramShadow + offset, buffer, length);
  
  return kIOReturnSuccess;
}

IOReturn Core99NVRAM::write(IOByteCount offset, UInt8 *buffer,
			    IOByteCount length)
{
  if (nvramShadow == 0) return kIOReturnSuccess;
  
  if ((buffer == 0) || (length <= 0) || (offset < 0) ||
      ((offset + length) > kCore99NVRAMSize))
    return kIOReturnBadArgument;
  
  bcopy(buffer, nvramShadow + offset, length);
  
  return kIOReturnSuccess;
}

IOReturn Core99NVRAM::eraseBlock(void)
{
  IOReturn error;
  
  // Write the Erase Setup Command.
  *nvramNext = kCore99NVRAMEraseSetupCmd;
  eieio();
  
  // Write the Erase Confirm Command.
  *nvramNext = kCore99NVRAMEraseConfirmCmd;
  eieio();
  
  error = waitForCommandDone();
  
  // Write the Reset Command.
  *nvramNext = kCore99NVRAMResetDeviceCmd;
  eieio();
  
  if (error == kIOReturnSuccess) {
    error = verifyEraseBlock();
  }
  
  return error;
}

IOReturn Core99NVRAM::verifyEraseBlock(void)
{
  long cnt;
  
  for (cnt = 0; cnt < kCore99NVRAMSize; cnt++) {
    if (nvramNext[cnt] != 0xFF) return kIOReturnInvalid;
  }
  
  return kIOReturnSuccess;
}

IOReturn Core99NVRAM::writeBlock(unsigned char *sourceAddress)
{
  long     cnt;
  IOReturn error;
  
  // Write the data byte by byte.
  for (cnt = 0; cnt < kCore99NVRAMSize; cnt++) {
    nvramNext[cnt] = kCore99NVRAMWriteSetupCmd;
    eieio();
    
    nvramNext[cnt] = sourceAddress[cnt];
    eieio();
    
    error = waitForCommandDone();
    if (error != kIOReturnSuccess) break;
  }
  
  // Write the Reset Command.
  *nvramNext = kCore99NVRAMResetDeviceCmd;
  eieio();
  
  if (error == kIOReturnSuccess) {
    error = verifyWriteBlock(sourceAddress);
  }
  
  return error;
}

IOReturn Core99NVRAM::verifyWriteBlock(unsigned char *sourceAddress)
{
  long cnt;
  
  for (cnt = 0; cnt < kCore99NVRAMSize; cnt++) {
    if (nvramNext[cnt] != sourceAddress[cnt]) return kIOReturnInvalid;
  }
  
  return kIOReturnSuccess;
}

IOReturn Core99NVRAM::waitForCommandDone(void)
{
  unsigned char status;
  
  // There should be a time out in here...
  do {
    status = *nvramNext;
    eieio();
  } while ((status & kCore99NVRAMStatusRegCompletionMask) == 0);
  
  // Check for errors.
  if (status & kCore99NVRAMStatusRegErrorMask) return kIOReturnInvalid;
  
  return kIOReturnSuccess;
}

unsigned long Core99NVRAM::validateGeneration(unsigned char *nvramBuffer)
{
  Core99NVRAMHeader *header = (Core99NVRAMHeader *)nvramBuffer;
  
  // First validate the signature.
  if (header->signature != kCore99NVRAMSignature) return 0;
  
  // Next make sure the header's checksum matches.
  if (header->checksum != chrpCheckSum(nvramBuffer)) return 0;
  
  // Make sure the adler checksum matches.
  if (header->adler32 != adler32(nvramBuffer + kCore99NVRAMAdlerStart,
				 kCore99NVRAMAdlerSize))
    return 0;
  
  return header->generation;
}

unsigned char Core99NVRAM::chrpCheckSum(unsigned char *buffer)
{
  long          cnt;
  unsigned char i_sum, c_sum;
  
  c_sum = 0;
  
  for (cnt = 0; cnt < 16; cnt++) {
    // Skip the checksum.
    if (cnt == 1) continue;
    
    i_sum = c_sum + buffer[cnt];
    if (i_sum < c_sum) i_sum += 1;
    c_sum = i_sum;
  }
  
  return c_sum;
}

unsigned long Core99NVRAM::adler32(unsigned char *buffer, long length)
{
  long          cnt;
  unsigned long result, lowHalf, highHalf;
  
  lowHalf = 1;
  highHalf = 0;
  
  for (cnt = 0; cnt < length; cnt++) {
    if ((cnt % 5000) == 0) {
      lowHalf  %= 65521L;
      highHalf %= 65521L;
    }
    
    lowHalf += buffer[cnt];
    highHalf += lowHalf;
  }
  
  lowHalf  %= 65521L;
  highHalf %= 65521L;
  
  result = (highHalf << 16) | lowHalf;
  
  return result;
}

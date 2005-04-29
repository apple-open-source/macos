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

#ifndef _APPLESMBIOS_SMBIOS_H
#define _APPLESMBIOS_SMBIOS_H

#include <IOKit/IOLib.h>

//
// Based on System Management BIOS Reference Specification v2.3
//

typedef UInt8  SMBString;
typedef UInt8  SMBByte;
typedef UInt16 SMBWord;
typedef UInt32 SMBDWord;
typedef UInt64 SMBQWord;

#pragma pack(1)             // enable 8-bit struct packing

struct DMIEntryPoint {
    SMBByte    anchor[5];
    SMBByte    checksum;
    SMBWord    tableLength;
    SMBDWord   tableAddress;
    SMBWord    structureCount;
    SMBByte    bcdRevision;
};

//
// Header common to all SMBIOS structures
//

struct SMBStructHeader {
    SMBByte    type;
    SMBByte    length;
    SMBWord    handle;
};

#define SMB_STRUCT_HEADER  SMBStructHeader header;

//
// SMBIOS structure types.
//

enum {
    kSMBStructTypeBIOSInformation      =  0,
    kSMBStructTypeSystemInformation    =  1,
    kSMBStructTypeSystemEnclosure      =  3,
    kSMBStructTypeProcessorInformation =  4,
    kSMBStructTypeMemoryModule         =  6,
    kSMBStructTypeCacheInformation     =  7,
    kSMBStructTypePhysicalMemoryArray  = 16,
    kSMBStructTypeMemoryDevice         = 17
};

//
// BIOS Information (Type 0)
//

struct SMBStructBIOSInformation {
    SMB_STRUCT_HEADER               // Type 0
    SMBString  vendor;              // BIOS vendor name
    SMBString  version;             // BIOS version
    SMBWord    startSegment;        // BIOS segment start
    SMBString  releaseDate;         // BIOS release date
    SMBByte    romSize;             // (n); 64K * (n+1) bytes
    SMBQWord   characteristics;     // supported BIOS functions
};

//
// System Information (Type 1)
//

struct SMBStructSystemInformation {
    // 2.0+ spec (8 bytes)
    SMB_STRUCT_HEADER               // Type 1
    SMBString  manufacturer;
    SMBString  productName;
    SMBString  version;
    SMBString  serialNumber;
    // 2.1+ spec (25 bytes)
    SMBByte    uuid[16];            // can be all 0 or all 1's
    SMBByte    wakeupReason;        // reason for system wakeup
};

//
// System Enclosure (Type 3)
//

struct SMBStructSystemEnclosure {
    SMB_STRUCT_HEADER               // Type 3
    SMBString  manufacturer;
    SMBByte    type;
    SMBString  version;
    SMBString  serialNumber;
    SMBString  assetTagNumber;
    SMBByte    bootupState;
    SMBByte    powerSupplyState;
    SMBByte    thermalState;
    SMBByte    securityStatus;
    SMBDWord   oemDefined;
};

//
// Processor Information (Type 4)
//

struct SMBStructProcessorInformation {
    // 2.0+ spec (26 bytes)
    SMB_STRUCT_HEADER               // Type 4
    SMBString  socketDesignation;
    SMBByte    processorType;       // CPU = 3
    SMBByte    processorFamily;     // processor family enum
    SMBString  manufacturer;
    SMBQWord   processorID;         // based on CPUID
    SMBString  processorVersion;
    SMBByte    voltage;             // bit7 cleared indicate legacy mode
    SMBWord    externalClock;       // external clock in MHz
    SMBWord    maximumClock;        // max internal clock in MHz
    SMBWord    currentClock;        // current internal clock in MHz
    SMBByte    status;
    SMBByte    processorUpgrade;    // processor upgrade enum
    // 2.1+ spec (32 bytes)
    SMBWord    L1CacheHandle;
    SMBWord    L2CacheHandle;
    SMBWord    L3CacheHandle;
    // 2.3+ spec (35 bytes)
    SMBString  serialNumber;
    SMBString  assetTag;
    SMBString  partNumber;
};

//
// Memory Module Information (Type 6)
// Obsoleted since SMBIOS version 2.1
//

struct SMBStructMemoryModule {
    SMB_STRUCT_HEADER               // Type 6
    SMBString  socketDesignation;
    SMBByte    bankConnections;
    SMBByte    currentSpeed;
    SMBWord    currentMemoryType;
    SMBByte    installedSize;
    SMBByte    enabledSize;
    SMBByte    errorStatus;
};

#define kSMBMemoryModuleSizeNotDeterminable 0x7D
#define kSMBMemoryModuleSizeNotEnabled      0x7E
#define kSMBMemoryModuleSizeNotInstalled    0x7F

//
// Cache Information (Type 7)
//

struct SMBStructCacheInformation {
    SMB_STRUCT_HEADER               // Type 7
    SMBString  socketDesignation;
    SMBWord    cacheConfiguration;
    SMBWord    maximumCacheSize;
    SMBWord    installedSize;
    SMBWord    supportedSRAMType;
    SMBWord    currentSRAMType;
    SMBByte    cacheSpeed;
    SMBByte    errorCorrectionType;
    SMBByte    systemCacheType;
    SMBByte    associativity;
};

//
// Physical Memory Array (Type 16)
//

struct SMBStructPhysicalMemoryArray {
    // 2.1+ spec (15 bytes)
    SMB_STRUCT_HEADER               // Type 16
    SMBByte    physicalLocation;    // physical location
    SMBByte    arrayUse;            // the use for the memory array
    SMBByte    errorCorrection;     // error correction/detection method
    SMBDWord   maximumCapacity;     // maximum memory capacity in kilobytes
    SMBWord    errorHandle;         // handle of a previously detected error
    SMBWord    numMemoryDevices;    // number of memory slots or sockets
};

//
// Memory Device (Type 17)
//

struct SMBStructMemoryDevice {
    // 2.1+ spec (21 bytes)
    SMB_STRUCT_HEADER               // Type 17
    SMBWord    arrayHandle;         // handle of the parent memory array
    SMBWord    errorHandle;         // handle of a previously detected error
    SMBWord    totalWidth;          // total width in bits; including ECC bits
    SMBWord    dataWidth;           // data width in bits
    SMBWord    memorySize;          // bit15 is scale, 0 = MB, 1 = KB
    SMBByte    formFactor;          // memory device form factor
    SMBByte    deviceSet;           // parent set of identical memory devices
    SMBString  deviceLocator;       // labeled socket; e.g. "SIMM 3"
    SMBString  bankLocator;         // labeled bank; e.g. "Bank 0" or "A"
    SMBByte    memoryType;          // type of memory
    SMBWord    memoryTypeDetail;    // additional detail on memory type
    // 2.3+ spec (27 bytes)
    SMBWord    memorySpeed;         // speed of device in MHz (0 for unknown)
    SMBString  manufacturer;
    SMBString  serialNumber;
    SMBString  assetTag;
    SMBString  partNumber;
};

static const char * SMBMemoryDeviceTypes[] =
{
    "RAM",    "RAM",    "RAM",    "DRAM",
    "EDRAM",  "VRAM",   "SRAM",   "RAM",
    "ROM",    "FLASH",  "EEPROM", "FEPROM",
    "EPROM",  "CDRAM",  "3DRAM",  "SDRAM",
    "SGRAM",  "RDRAM",  "DDR"
};

static const int kSMBMemoryDeviceTypeCount = sizeof(SMBMemoryDeviceTypes)   /
                                             sizeof(SMBMemoryDeviceTypes[0]);

#if 0
static const char * SMBMemoryDeviceDetailTypes[] =
{
    NULL, NULL, NULL, "Fast-paged",
    "Static column", "Pseudo-static", "RAMBUS", "Synchronous",
    "CMOS", "EDO", "Window", "Cache",
    "Non-volatile", NULL, NULL, NULL
};
#endif

#pragma options align=reset // reset to default struct packing

#endif /* !_APPLESMBIOS_SMBIOS_H */

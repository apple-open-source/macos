/*
 * Copyright (c) 1998-2006 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 2.0 (the
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

#ifndef _APPLE_SMBIOS_H
#define _APPLE_SMBIOS_H

#include <IOKit/IOService.h>
#include <IOKit/IOKitKeys.h>
#include "SMBIOS.h"

class SMBPackedStrings;

struct SystemSlotEntry {
    queue_chain_t   chain;
    UInt16          slotID;
    UInt8           slotType;
    UInt8           slotUsage;
    const char *    slotName;
};

class AppleSMBIOS : public IOService
{
    OSDeclareDefaultStructors( AppleSMBIOS )

protected:
    IOService *             fRoot;
    int                     fVerbose;
    IOMemoryMap *           fDMIMemoryMap;
    queue_head_t *          fSlotQueueHead;
    IOService *             fROMNode;

    OSData *                memSlotsData;
    OSData *                memTypesData;
    OSData *                memSizesData;
    OSData *                memSpeedData;
    OSData *                memInfoData;
    OSData *                memManufData;
    OSData *                memSerialData;
    OSData *                memPartData;
    UInt64                  memSizeTotal;
	
	void *					SMBIOSTable;
	UInt16					SMBIOSTableLength;

    enum { kMemDataSize = 64 };


    enum {
        kNoMemoryInfo,
        kMemoryModuleInfo,
        kMemoryDeviceInfo
    }                       memInfoSource;
	
	const SMBStructHeader *	getSMBIOSRecord( SMBWord record );

    static bool             serializeSMBIOS(
                                void *                  target,
                                void *                  refcon,
                                OSSerialize *           s );

    bool                    findSMBIOSTableEFI( void );

    OSData *                getSlotNameWithSlotId( int  slotId );

    void                    adjustPCIDeviceEFI(
                                IOService *             pciDevice );

    IOReturn                callPlatformFunction(
                                const char *            functionName,
                                bool                    waitForFunction,
                                void *                  param1,
                                void *                  param2,
                                void *                  param3,
                                void *                  param4 );

    void                    decodeSMBIOSTable(
                                const void *            tableData,
                                UInt16                  tableLength,
                                UInt16                  structureCount );

    void                    decodeSMBIOSStructure(
                                const SMBStructHeader * structureHeader,
                                const void *            tableBoundary );

    void                    processSMBIOSStructure(
                                const SMBBIOSInformation *      bios,
                                SMBPackedStrings *              strings );

    void                    processSMBIOSStructure(
                                const SMBSystemInformation *    sys,
                                SMBPackedStrings *              strings );

    void                    processSMBIOSStructure(
                                const SMBProcessorInformation * cpu,
                                SMBPackedStrings *              strings );

    void                    processSMBIOSStructure(
                                const SMBCacheInformation *     cache,
                                SMBPackedStrings *              strings );

    void                    processSMBIOSStructure(
                                const SMBMemoryDevice *         memory,
                                SMBPackedStrings *              strings );

    void                    processSMBIOSStructure(
                                const SMBMemoryModule *         memory,
                                SMBPackedStrings *              strings );

    void                    processSMBIOSStructure(
                                const SMBSystemSlot *           slot,
                                SMBPackedStrings *              strings );

    void                    processSMBIOSStructure(
                                const SMBFirmwareVolume *       fv,
                                SMBPackedStrings *              strings );
								
	void					processSMBIOSStructure(
								const SMBMemorySPD *			spd,
								SMBPackedStrings *              strings );

    void                    processSMBIOSStructure(
                                const SMBOemProcessorType *     cpu,
                                SMBPackedStrings *              strings );

    void                    updateDeviceTree( void );

public:
    virtual bool            start( IOService * provider );

    virtual void            free( void );
};

#endif /* !_APPLE_SMBIOS_H */

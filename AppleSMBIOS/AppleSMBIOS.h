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

#ifndef _APPLE_SMBIOS_H
#define _APPLE_SMBIOS_H

#include <IOKit/IOService.h>
#include "SMBIOS.h"

class SMBPackedStrings;

class AppleSMBIOS : public IOService
{
    OSDeclareDefaultStructors( AppleSMBIOS )

protected:
    IOService *  _provider;
    IOService *  _platform;
    int          _verbose;

    // memory node properties.

    OSData * memSlotsData;
    OSData * memTypesData;
    OSData * memSizesData;

    enum {
        kNoMemoryInfo,
        kMemoryModuleInfo,
        kMemoryDeviceInfo
    } memInfoSource;

    // SMBIOS table search.

    bool fetchSMBIOSTable();

    // SMBIOS table/structure decoding.

    void decodeSMBIOSTable( const void * tableData, UInt16 tableLength,
                            UInt16 structureCount );

    void decodeSMBIOSStructure( const SMBStructHeader * structureHeader,
                                const void *            tableBoundary );

    // Handlers for each type of SMBIOS structure.

    void processSMBIOSStructure( const SMBStructBIOSInformation * bios,
                                 SMBPackedStrings * strings );

    void processSMBIOSStructure( const SMBStructSystemInformation * sys,
                                 SMBPackedStrings * strings );

    void processSMBIOSStructure( const SMBStructSystemEnclosure * enclosure,
                                 SMBPackedStrings * strings );

    void processSMBIOSStructure( const SMBStructProcessorInformation * cpu,
                                 SMBPackedStrings * strings );

    void processSMBIOSStructure( const SMBStructCacheInformation * cache,
                                 SMBPackedStrings * strings );

    void processSMBIOSStructure( const SMBStructMemoryDevice * memory,
                                 SMBPackedStrings * strings );

    void processSMBIOSStructure( const SMBStructMemoryModule * memory,
                                 SMBPackedStrings * strings );

    void updateDeviceTree();

public:
    virtual bool start( IOService * provider );
    virtual void free();
};

#endif /* !_APPLE_SMBIOS_H */

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

#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IODeviceTreeSupport.h>
#include "AppleSMBIOS.h"

#if     DEBUG
#define DEBUG_LOG(args...)  IOLog(args)
#else
#define DEBUG_LOG(args...)
#endif

//---------------------------------------------------------------------------
// SMBPackedStrings class

class SMBPackedStrings 
{
protected:
    const char * _start;
    const char * _limit;

public:
    SMBPackedStrings( const SMBStructHeader * header, const void * limit );

	const char * stringAtIndex( UInt8 index, UInt8 * length = 0 ) const;

    void setDataProperty( IORegistryEntry * entry,
                          const char *      key,
                          UInt8             index ) const;

    void setStringProperty( IORegistryEntry * entry,
                            const char *      key,
                            UInt8             index ) const;
};

SMBPackedStrings::SMBPackedStrings( const SMBStructHeader * header,
                                    const void *            limit )
{
    _start = (const char *) header + header->length;
    _limit = (const char *) limit;
}
    
const char * SMBPackedStrings::stringAtIndex( UInt8 index, UInt8 * length ) const
{
    const char * last = 0;
    const char * next = _start;

    if ( length ) *length = 0;

    while ( index-- )
    {
        last = 0;
        for ( const char * cp = next; cp < _limit; cp++ )
        {
            if ( *cp == '\0' )
            {
                last = next;
                next = cp + 1;
                break;
            }
        }
        if ( last == 0 ) break;
    }

    if ( last )
    {
        while (*last == ' ') last++;
        if (length)
        {
            UInt8 len;
            for ( len = next - last - 1; len && last[len - 1] == ' '; len-- )
                ;
            *length = len; // number of chars not counting the terminating NULL
        }
    }

	return last ? last : "";
}

void SMBPackedStrings::setDataProperty( IORegistryEntry * entry,
                                        const char *      key,
                                        UInt8             index ) const
{
    UInt8        length;
    const char * string = SMBPackedStrings::stringAtIndex(index, &length);
    
    if (length)
    {
        OSData * data = OSData::withCapacity(length + 1);
        if (data)
        {
            data->appendBytes(string, length);
            data->appendByte('\0', 1);
            entry->setProperty(key, data);
            data->release();
        }
    }
}

void SMBPackedStrings::setStringProperty( IORegistryEntry * entry,
                                          const char *      key,
                                          UInt8             index ) const
{
    UInt8        length;
    const char * string = SMBPackedStrings::stringAtIndex(index, &length);

    if (length)
    {
        OSString * strObj = OSString::withCString(string);
        if (strObj)
        {
            strObj->setChar('\0', length);
            entry->setProperty(key, strObj);
            strObj->release();
        }
    }
}

//---------------------------------------------------------------------------
// AppleSMBIOS class

#define super IOService

OSDefineMetaClassAndStructors( AppleSMBIOS, IOService )

//---------------------------------------------------------------------------

bool AppleSMBIOS::start( IOService * provider )
{
    if ( super::start( provider ) != true     ||
         IOService::getResourceService() == 0 ||
         IOService::getResourceService()->getProperty("SMBIOS") )
    {
        return false;
    }

    _provider = provider;
    _platform = getServiceRoot();
    if ( !_provider || !_platform ) return false;

    _verbose = 0;
    PE_parse_boot_arg( "smbios", &_verbose );

    memInfoSource = kNoMemoryInfo;
    memSlotsData = OSData::withCapacity(64);
    memTypesData = OSData::withCapacity(64);
    memSizesData = OSData::withCapacity(64);    
    if ( !memSlotsData || !memTypesData || !memSizesData ) return false;

    // Fetch platform info stored in SMBIOS table.

    if ( fetchSMBIOSTable() != true )
    {
        return false;
    }

    // Update device tree based on SMBIOS info discovered.

    updateDeviceTree();

    publishResource("SMBIOS");

    registerService();

    return true;
}

//---------------------------------------------------------------------------

void AppleSMBIOS::free()
{
    #define RELEASE(x) do { if (x) { (x)->release(); (x) = 0; } } while(0)

    RELEASE( memSlotsData );
    RELEASE( memTypesData );
    RELEASE( memSizesData );

    super::free();
}

//---------------------------------------------------------------------------

bool AppleSMBIOS::fetchSMBIOSTable()
{
    enum { kROMScanStart = 0xf0000, kROMScanBytes = 0x10000 };

    bool                 success = false;
    DMIEntryPoint        dmi;
    void *               table;
    IOMemoryDescriptor * romMemory = IOMemoryDescriptor::withPhysicalAddress(
                                        kROMScanStart, kROMScanBytes,
                                        kIODirectionOutIn );

    if ( romMemory )
    {
        // Scan for a "_DMI_" signature at "paragraph" memory boundaries.

        for ( UInt32 offset = 0; offset < kROMScanBytes; offset += 16 )
        {
            romMemory->readBytes( offset, &dmi, sizeof(dmi) );
            if ( memcmp( dmi.anchor, "_DMI_", 5 ) == 0 )
            {
                UInt8   checksum = 0;
                UInt8 * cp = (UInt8 *) &dmi;
                for ( UInt i = 0; i < sizeof(dmi); i++ )
                    checksum += *cp++;

                DEBUG_LOG("DMI checksum       = 0x%x\n",  checksum);
                DEBUG_LOG("DMI tableLength    = %d\n",    dmi.tableLength);
                DEBUG_LOG("DMI tableAddress   = 0x%lx\n", dmi.tableAddress);
                DEBUG_LOG("DMI structureCount = %d\n",    dmi.structureCount);
                DEBUG_LOG("DMI bcdRevision    = %x\n",    dmi.bcdRevision);

                // Add DMI revision checking?

                if ( checksum || !dmi.tableLength || !dmi.structureCount )
                    break;

                if (( table = IOMalloc(dmi.tableLength) ))
                {
                    if ( romMemory->initWithPhysicalAddress(
                                        dmi.tableAddress,
                                        dmi.tableLength,
                                        kIODirectionOutIn) )
                    {
                        romMemory->readBytes( 0, table, dmi.tableLength );
                        decodeSMBIOSTable( table, dmi.tableLength,
                                                  dmi.structureCount );
                    }
                    // Publish the whole table for a DMI browser app,
                    // app can match on SMBIOS resources.
                    setProperty("StructureTableData", table, dmi.tableLength);
                    IOFree( table, dmi.tableLength );
                    success = true;
                }
                break;
            }
        }

        romMemory->release();
    }

    return success;
}

//---------------------------------------------------------------------------

void AppleSMBIOS::decodeSMBIOSTable( const void * tableData,
                                     UInt16       tableLength,
                                     UInt16       structureCount )
{
    const SMBStructHeader * header;
    const UInt8 *           next = (const UInt8 *) tableData;
    const UInt8 *           end  = next + tableLength;

    while ( structureCount-- && (end > next + sizeof(SMBStructHeader)) )
    {
        header = (const SMBStructHeader *) next;
        if (header->length > end - next) break;

        decodeSMBIOSStructure( header, end );

        // Skip the formatted area of the structure.

        next += header->length;

        // Skip the unformatted structure area at the end (strings).
        // Look for a terminating double NULL.

        for ( ; end > next + sizeof(SMBStructHeader); next++ )
        {
            if ( next[0] == 0 && next[1] == 0 )
            {
                next += 2; break;
            }
        }
    }
}

//---------------------------------------------------------------------------

void
AppleSMBIOS::decodeSMBIOSStructure( const SMBStructHeader * structureHeader,
                                    const void *            tableBoundary )
{
    const union SMBStructUnion {
        SMBStructBIOSInformation      bios;
        SMBStructSystemInformation    system;
        SMBStructSystemEnclosure      enclosure;
        SMBStructProcessorInformation cpu;
        SMBStructCacheInformation     cache;
        SMBStructPhysicalMemoryArray  memoryArray;
        SMBStructMemoryDevice         memoryDevice;
        SMBStructMemoryModule         memoryModule;
    } * u = (const SMBStructUnion *) structureHeader;

    SMBPackedStrings strings = SMBPackedStrings( structureHeader,
                                                 tableBoundary );

    switch ( structureHeader->type )
    {
        case kSMBStructTypeBIOSInformation:
            processSMBIOSStructure( &u->bios, &strings );
            break;

        case kSMBStructTypeSystemInformation:
            processSMBIOSStructure( &u->system, &strings );
            break;

        case kSMBStructTypeSystemEnclosure:
            processSMBIOSStructure( &u->enclosure, &strings );
            break;

        case kSMBStructTypeProcessorInformation:
            processSMBIOSStructure( &u->cpu, &strings );
            break;

        case kSMBStructTypeCacheInformation:
            processSMBIOSStructure( &u->cache, &strings );
            break;

        case kSMBStructTypeMemoryDevice:
            processSMBIOSStructure( &u->memoryDevice, &strings );
            break;

        case kSMBStructTypeMemoryModule:
            processSMBIOSStructure( &u->memoryModule, &strings );
            break;
    }
}

//---------------------------------------------------------------------------

void
AppleSMBIOS::processSMBIOSStructure( const SMBStructBIOSInformation * bios,
                                     SMBPackedStrings * strings )
{
    char location[9];

    if ( bios->header.length < sizeof(SMBStructBIOSInformation) ) return;

    IOService * romNode = new IOService;

    if ( romNode && (false == romNode->init())) {
        romNode->release();
        romNode = 0;
    }
    if ( romNode )
    {
        romNode->setName("rom");
        sprintf(location, "%x", bios->startSegment << 4);
        romNode->setLocation(location);

        strings->setDataProperty(romNode, "vendor", bios->vendor);
        strings->setDataProperty(romNode, "version", bios->version);
        strings->setDataProperty(romNode, "release-date", bios->releaseDate);
        strings->setDataProperty(romNode, "characteristics",
                                           bios->characteristics);

        romNode->setProperty("rom-size", (bios->romSize + 1) * 0x10000, 32 );

        romNode->attachToParent( _platform, gIODTPlane );
        romNode->release();
    }
}

//---------------------------------------------------------------------------

void
AppleSMBIOS::processSMBIOSStructure( const SMBStructSystemInformation * sys,
                                     SMBPackedStrings * strings )
{
	if ( sys->header.length < 8 ) return;
    strings->setDataProperty(_platform, "manufacturer", sys->manufacturer);
    strings->setDataProperty(_platform, "product-name", sys->productName);
    strings->setDataProperty(_platform, "version", sys->version);
    strings->setDataProperty(_platform, "serial-number", sys->serialNumber);

    //IOLog("manufacturer = %s\n", strings->stringAtIndex(sys->manufacturer));
    //IOLog("product-name = %s\n", strings->stringAtIndex(sys->productName));
}

//---------------------------------------------------------------------------

void
AppleSMBIOS::processSMBIOSStructure( const SMBStructSystemEnclosure * sys,
                                     SMBPackedStrings * strings )
{
#if 0
    IOLog("enclosure:\n");
    IOLog("manufacturer = %s\n", strings->stringAtIndex(sys->manufacturer));
    IOLog("type = 0x%x\n", sys->type);
    IOLog("version = %s\n", strings->stringAtIndex(sys->version));
    IOLog("serial-number = %s\n", strings->stringAtIndex(sys->serialNumber));
    IOLog("asset-tag-number = %s\n", strings->stringAtIndex(sys->assetTagNumber));
    IOLog("bootup-state = 0x%x\n", sys->bootupState);
    IOLog("power-supply-state = 0x%x\n", sys->powerSupplyState);
    IOLog("thermal-state = 0x%x\n", sys->thermalState);
    IOLog("security-status = 0x%x\n", sys->securityStatus);
    IOLog("oem-defined = 0x%x\n", sys->oemDefined);
#endif
}

//---------------------------------------------------------------------------

void
AppleSMBIOS::processSMBIOSStructure( const SMBStructProcessorInformation * cpu,
                                     SMBPackedStrings * strings )
{
	if ( cpu->header.length < 26 ) return;

    if (_verbose)
    {
        IOLog("--- SMBIOS Processor Information --\n");
        IOLog("socketDesignation = %s\n",  
              strings->stringAtIndex(cpu->socketDesignation));
        IOLog("processorType     = 0x%x\n", cpu->processorType);
        IOLog("processorFamily   = 0x%x\n", cpu->processorFamily);
        IOLog("manufacturer      = %s\n", 
              strings->stringAtIndex(cpu->manufacturer));
        IOLog("processorID       = 0x%lx%08lx\n",
              (UInt32)(cpu->processorID >> 32),
              (UInt32)cpu->processorID);
        IOLog("processorVersion  = %s\n",
              strings->stringAtIndex(cpu->processorVersion));
        IOLog("voltage           = 0x%x\n", cpu->voltage);
        IOLog("externalClock     = 0x%x\n", cpu->externalClock);
        IOLog("maximumClock      = 0x%x\n", cpu->maximumClock);
        IOLog("currentClock      = 0x%x\n", cpu->currentClock);
        IOLog("status            = 0x%x\n", cpu->status);
        IOLog("processorUpgrade  = 0x%x\n", cpu->processorUpgrade);
    }

    UInt32 busClockRateMHz = cpu->externalClock;

    if (busClockRateMHz > 0) {
        callPlatformFunction("SetBusClockRateMHz",     /* function */
                             false,                    /* waitForFunction */
                             (void *) busClockRateMHz, /* speed in MHz */
                             0, 0, 0);
    }
}

//---------------------------------------------------------------------------

void
AppleSMBIOS::processSMBIOSStructure( const SMBStructCacheInformation * cache,
                                     SMBPackedStrings * strings )
{
    if (_verbose)
    {
        IOLog("--- SMBIOS Cache Information --\n");
        IOLog("cache socket   = %s\n",
              strings->stringAtIndex(cache->socketDesignation));
        IOLog("cache config   = 0x%x\n", cache->cacheConfiguration);
        IOLog("max cache size = 0x%x\n", cache->maximumCacheSize);
        IOLog("installed size = 0x%x\n", cache->installedSize);
    }
}

//---------------------------------------------------------------------------

void
AppleSMBIOS::processSMBIOSStructure( const SMBStructMemoryDevice * memory,
                                     SMBPackedStrings * strings )
{
	UInt8        deviceLocatorLength;
	const char * deviceLocatorString;
    UInt8        bankLocatorLength;
    const char * bankLocatorString;
    UInt8        memoryType;

    union {
        UInt64 ull;
        UInt32 ul[2];
    } memoryBytes;

    if (memInfoSource == kMemoryModuleInfo) return;
    memInfoSource = kMemoryDeviceInfo;

	if ( memory->header.length < 21 ) return;

    // update memSlotsData

    deviceLocatorString = strings->stringAtIndex( memory->deviceLocator, 
                                                  &deviceLocatorLength );

    bankLocatorString = strings->stringAtIndex( memory->bankLocator, 
                                                &bankLocatorLength );

    if ( deviceLocatorLength || bankLocatorString )
    {
        if ( memSlotsData->getLength() == 0 )
            memSlotsData->appendBytes("   ", 4);
        if ( deviceLocatorLength )
            memSlotsData->appendBytes( deviceLocatorString, deviceLocatorLength );
        if ( deviceLocatorLength && bankLocatorLength )
            memSlotsData->appendByte('/', 1);
        if ( bankLocatorLength )
            memSlotsData->appendBytes( bankLocatorString, bankLocatorLength );
        memSlotsData->appendByte('\0', 1);
    }

    // update memTypesData

#if 0 // disabled, information adds little value
    for ( int bit = 15; bit >= 0; bit-- )
    {
        if ( ( memory->memoryTypeDetail & (1 << bit) ) &&
             ( SMBMemoryTypeDetailStringTable[bit] != NULL ) )
        {
            memTypesData->appendBytes( SMBMemoryTypeDetailStringTable[bit],
                                strlen(SMBMemoryTypeDetailStringTable[bit]));
            memTypesData->appendByte( ' ', 1 );
        }
    }
#endif /* 0 */

    memoryType = memory->memoryType;
    if ( memoryType > kSMBMemoryDeviceTypeCount - 1 )
        memoryType = 0x02; // unknown type

    memTypesData->appendBytes( SMBMemoryDeviceTypes[memoryType],
                        strlen(SMBMemoryDeviceTypes[memoryType]) + 1 );

    // update memSizesData

    memoryBytes.ull = (memory->memorySize & 0x7fff) * 1024;
    if ((memory->memorySize & 0x8000) == 0)
        memoryBytes.ull *= 1024;

    memSizesData->appendBytes( &memoryBytes.ul[1], 4 );
    memSizesData->appendBytes( &memoryBytes.ul[0], 4 );

    // What about
    // "dimm-speeds", "available", "dimm-info"
}

//---------------------------------------------------------------------------

void
AppleSMBIOS::processSMBIOSStructure( const SMBStructMemoryModule * memory,
                                     SMBPackedStrings * strings )
{
	UInt8        socketLength;
	const char * socketString;
    UInt8        memorySize;

    union {
        UInt64 ull;
        UInt32 ul[2];
    } memoryBytes;

    if (memInfoSource == kMemoryDeviceInfo) return;
    memInfoSource = kMemoryModuleInfo;

	if (memory->header.length < sizeof(SMBStructMemoryModule))
        return;

    // update memSlotsData

    socketString = strings->stringAtIndex( memory->socketDesignation, 
                                           &socketLength );

    if ( socketString )
    {
        if (memSlotsData->getLength() == 0)
            memSlotsData->appendBytes("   ", 4);
        if (socketLength)
            memSlotsData->appendBytes( socketString, socketLength );
        memSlotsData->appendByte('\0', 1);
    }

    // update memTypesData

    memTypesData->appendBytes("DRAM", 5);

    // update memSizesData

    memorySize = memory->enabledSize & 0x7F;
    if (memorySize >= kSMBMemoryModuleSizeNotDeterminable)
        memoryBytes.ull = 0;
    else
        memoryBytes.ull = (1ULL << memorySize) * (1024 * 1024);

    memSizesData->appendBytes( &memoryBytes.ul[1], 4 );
    memSizesData->appendBytes( &memoryBytes.ul[0], 4 );
}

//---------------------------------------------------------------------------

void AppleSMBIOS::updateDeviceTree()
{
    IOService * memoryNode = new IOService;

    if ( memoryNode && (false == memoryNode->init())) {
        memoryNode->release();
        memoryNode = 0;
    }
    if ( memoryNode )
    {
        memoryNode->setName("memory");
        // memoryNode->setLocation("0");
        memoryNode->setProperty( "slot-names", memSlotsData );
        memoryNode->setProperty( "dimm-types", memTypesData );
        memoryNode->setProperty( "reg",        memSizesData );
        memoryNode->attachToParent( _platform, gIODTPlane );
        memoryNode->release();
    }
}

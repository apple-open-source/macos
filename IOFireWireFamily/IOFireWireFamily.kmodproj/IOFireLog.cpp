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
 
#if FIRELOGCORE

#include "IOFireLogPriv.h"

// FireLog
//
//

void FireLog( const char *format, ... )
{
    IOFireLog * firelog;
    va_list ap;
    va_start(ap, format);

    firelog = IOFireLog::getFireLog();
    if( firelog )
        firelog->logString( format, ap );
    
    va_end(ap);
}

#endif

#if FIRELOGCORE

extern "C" {
#include <kern/clock.h>
}

#define kFireLogVersionKey 		0x19		// arbitrary
#define kFireLogSizeKey 		0x1a		// arbitrary
#define kFireLogAddressHiKey 	0x1b		// arbitrary
#define kFireLogAddressLoKey 	0x1c		// arbitrary
#define kFireLogRandomIDKey		0x1d		// arbitrary
#define kFireLogMaxEntrySizeKey	0x1e		// arbitrary

#define kFireLogTempBufferSize 255

OSObject * 	IOFireLog::sFireLog = NULL;
int			IOFireLog::sTempBufferIndex = 0;
char 		IOFireLog::sTempBuffer[kFireLogTempBufferSize];

OSDefineMetaClassAndStructors(IOFireLog, OSObject)

// create
//
//

IOReturn IOFireLog::create( void )
{
    IOReturn 	status = kIOReturnSuccess;
    IOFireLog * firelog;
            
    if( status == kIOReturnSuccess )
    {
        firelog = new IOFireLog;
        if( firelog == NULL )
            status = kIOReturnNoMemory;
    }
    
    if( status == kIOReturnSuccess )
    {
        status = firelog->initialize();
    }
    
    //
    // atomically update our global
    //
    
    if( status == kIOReturnSuccess )
    {
        Boolean result = false;
        while( sFireLog == NULL && result == false )
        {
            result = OSCompareAndSwap( NULL, (UInt32)firelog, (UInt32*)&sFireLog );
        }
        
        if( result == false )
        {
            firelog->release();
        }
        else if( sFireLog != NULL ) // unnecessary
        {
            IOLog( "Welcome to FireLog. (built %s %s)\n", __TIME__, __DATE__ );
            FireLog( "Welcome to FireLog. (built %s %s)\n", __TIME__, __DATE__ );
        }
    }
    
    return status;
}

// init
//
//

IOReturn IOFireLog::initialize( void )
{
    IOReturn 		status = kIOReturnSuccess;
    AbsoluteTime 	time;
    
    if( kFireLogSize & 0x00000003 )  // must be 32 bit aligned
        panic( "FireLog : illegal log size 0x%08lx\n", kFireLogSize ); 
    
    fLogSize = kFireLogSize + sizeof(FireLogHeader);
    
    IOFWGetAbsoluteTime(&time);
    fRandomID = (uint32_t)AbsoluteTime_to_scalar(&time);
    fRandomID &= 0x00ffffff;
    
    fController = NULL;
    
    // allocate mem for log
    fLogDescriptor = IOBufferMemoryDescriptor::withCapacity( fLogSize, kIODirectionOutIn, true );
    if( fLogDescriptor == NULL )
        status =  kIOReturnNoMemory;

    if( status == kIOReturnSuccess )
    {
        fLogDescriptor->setLength( fLogSize );

        IOPhysicalLength lengthOfSegment = 0;
        IOPhysicalAddress phys = fLogDescriptor->getPhysicalSegment( 0, &lengthOfSegment );
        if( lengthOfSegment != 0 )
        {
            fLogPhysicalAddress = phys;
            IOLog( "FireLog : physical address = 0x%08lx, length = %ld\n", (UInt32)fLogPhysicalAddress, lengthOfSegment );
        }
        else
        {
            status =  kIOReturnNoMemory;
        }
    }

    if( status == kIOReturnSuccess )
    {
        // if physically contiguous, must be virtually contiguous
        fLogBuffer = (FireLogHeader*)fLogDescriptor->getBytesNoCopy();  
//        IOLog( "FireLog : virtual address = 0x%08lx\n", (UInt32)fLogBuffer );
    
        fLogStart = ((char*)fLogBuffer) + sizeof(FireLogHeader);
        fLogEnd = fLogStart + kFireLogSize;
    
        // clear log
        bzero( fLogBuffer, fLogSize );
    
        fNeedSpace = false;
        fLogBuffer->start = 0;
        fLogBuffer->end = 0;
    }

    if( status == kIOReturnSuccess )
    {
    	fLock = IOLockAlloc();
        if( fLock == NULL )
            status = kIOReturnError;
    }
    
    return status;
}

// free
//
//

void IOFireLog::free( void )
{ 
    if( fLogDescriptor )
    {
        fLogDescriptor->release();
        fLogDescriptor = NULL;
    }

    if( fLock != NULL )
    {
        IOLockFree( fLock );
        fLock = NULL;
    }

    OSObject::free();
}

IOFireWireController * IOFireLog::getMainController( void )
{
    return fController;
}

void IOFireLog::setMainController( IOFireWireController * controller )
{
    IOLockLock( fLock );
    		
    if( fController == NULL )
        fController = controller;

    if( controller == NULL )
        fController = NULL;

    IOLockUnlock( fLock );
}

IOPhysicalAddress IOFireLog::getLogPhysicalAddress( void )
{
    return fLogPhysicalAddress;
}

UInt32 IOFireLog::getLogSize( void )
{
    return fLogSize;
}

UInt32 IOFireLog::getRandomID( void )
{
    return fRandomID;
}
    
// firelog_putc
//
//
    
void IOFireLog::firelog_putc( char c )
{
    sTempBuffer[sTempBufferIndex] = c;
    sTempBufferIndex++;
}

// getFireLog
//
//

IOFireLog * IOFireLog::getFireLog( void )
{
    if( sFireLog == NULL )
        IOFireLog::create();
        
    return (IOFireLog*)sFireLog;
}


// logString
//
//

void IOFireLog::logString( const char *format, va_list ap )
{
    UInt32 			cycleTime;
    AbsoluteTime	absolute_time;
    uint64_t 		time;
    
    UInt32 new_encoded_end;
    UInt32 new_encoded_start;
        
    UInt32	length;
    UInt32	str_length;
    char *  data_ptr;
    char *	length_ptr;
    bool 	controllerExists = false;
	
	if( fController )
    {
		controllerExists = true;
	}
	
	if( controllerExists )
	{
		fController->closeGate();
	}
	
    IOLockLock( fLock );
	
//    IOLog( "FireLog : logString\n" );

    // get cycleTime
    if( controllerExists )
        fController->getCycleTime(cycleTime);
    else
        cycleTime = 0;
    
    // get uptime
    IOFWGetAbsoluteTime( &absolute_time );
    absolutetime_to_nanoseconds( absolute_time, &time );

    // clear the temp buffer
    sTempBufferIndex = 0;
    bzero( sTempBuffer, kFireLogTempBufferSize );
    
    _doprnt(format, &ap, IOFireLog::firelog_putc, 16);
    
    str_length = strlen(sTempBuffer) + 1; // account for /0
    
    // length is : uptime + cycletime + string + entry length
    length = ((( sizeof(uint64_t) + 
                 sizeof(UInt32) + 
                 str_length + 
                 sizeof(UInt32)) + 
                 3) & (~0x00000003)); // quadlet align the length
    
    // make space
    
//    IOLog( "FireLog : \"%s\"", sTempBuffer );
    
    {
        char * min_new_start;
        char * cur_new_start;
        
        new_encoded_end = fLogBuffer->end;
        length_ptr = encodedToLogical( new_encoded_end );
        
        if( length_ptr + length < fLogEnd )
        {
            new_encoded_end += sizeToEncoded( sizeof(UInt32) );
            data_ptr = length_ptr + sizeof(UInt32);
        }
        else
        {
            new_encoded_end += sizeToEncoded( fLogEnd - length_ptr );
            data_ptr = fLogStart;
        }
        
        //
        // free up space if necessary
        //
        
        new_encoded_start = fLogBuffer->start;
        cur_new_start = encodedToLogical( new_encoded_start );
        
        // if the new entry has wrapped			
        if( data_ptr < length_ptr )      
        {
            if( fNeedSpace )
            {
                // and start hasn't wrapped
                if( cur_new_start > length_ptr )
                {
                    // wrap start
                    while( true )
                    {
                        UInt32 entry_length = *((UInt32*)cur_new_start);
                        if( cur_new_start + entry_length < fLogEnd )
                        {
                            new_encoded_start += sizeToEncoded( entry_length );
                            cur_new_start += entry_length;
                        }
                        else
                        {
                            new_encoded_start += sizeToEncoded( fLogEnd - cur_new_start + entry_length - sizeof(UInt32) );
                            cur_new_start = fLogStart + entry_length - sizeof(UInt32);
                            break;
                        }
                    }
                }
            }
            else
            {
                // move start from now on
                fNeedSpace = true;
            }
        }
			
        // move beginning
        if( fNeedSpace )
        {
            min_new_start = data_ptr + length;
  			
            // if start has wrapped, but the new entry hasn't, don't do anything
            if( !((cur_new_start < length_ptr) && (data_ptr > length_ptr)) )
            {		
                while( cur_new_start < min_new_start )
                {
                    UInt32 entry_length = *((UInt32*)cur_new_start);
                    if( cur_new_start + entry_length < fLogEnd )
                    {
                        new_encoded_start += sizeToEncoded( entry_length );
                        cur_new_start += entry_length;
                    }
                    else
                    {
                        new_encoded_start += sizeToEncoded( fLogEnd - cur_new_start + entry_length - sizeof(UInt32) );
                        cur_new_start = fLogStart + entry_length - sizeof(UInt32);
                        break;
                    }
                }
            }
            
            fLogBuffer->start = new_encoded_start;
        }
    }
    
   // IOLog( "FireLog : made space\n" );
    
    // append sTempBuffer to the log
    *((uint64_t*)data_ptr) = time;
    data_ptr += sizeof(uint64_t);
    *((UInt32*)data_ptr) = cycleTime;
    data_ptr += sizeof(UInt32);
    bcopy( sTempBuffer, data_ptr, str_length );
    data_ptr += ((str_length+3) & ~0x00000003);
    *((UInt32*)data_ptr) = 0;  // next length is zero    
    
    // update length
    *((UInt32*)length_ptr) = length;
    
    new_encoded_end += sizeToEncoded( length - sizeof( UInt32 )  );
    
    // update end pointer
    fLogBuffer->end = new_encoded_end;
    
    //IOLog( "FireLog : log done.\n" );
    
    IOLockUnlock( fLock );	
	
	if( controllerExists )
	{
		fController->openGate();
	}
}

//////////////////////////////

OSDefineMetaClassAndStructors(IOFireLogPublisher, OSObject)

// create
//
//

IOFireLogPublisher * IOFireLogPublisher::create( IOFireWireController * controller )
{
    IOReturn 	status = kIOReturnSuccess;
    IOFireLogPublisher * firelogPublisher;
        
    if( status == kIOReturnSuccess )
    {
        firelogPublisher = new IOFireLogPublisher;
        if( firelogPublisher == NULL )
            status = kIOReturnNoMemory;
    }
    
    if( status == kIOReturnSuccess )
    {
        status = firelogPublisher->initWithController( controller );
    }
    
    if( status != kIOReturnSuccess )
    {
        firelogPublisher = NULL;
    }
    else
    {
        FireLog( "IOFireWireController @ 0x%08lx created new IOFireLogPublisher @ 0x%08lx\n", (UInt32)controller, (UInt32)firelogPublisher );
    }
    
    return firelogPublisher;
}

// initWithController
//
//

IOReturn IOFireLogPublisher::initWithController( IOFireWireController* controller )
{
    IOReturn status = kIOReturnSuccess;
    
    IOPhysicalAddress 	log_physical_address;
    UInt32 		log_size = 0;
    UInt32		log_random_id = 0;
	
    if( status == kIOReturnSuccess )
    {
        // get controller reference
        fController = controller;
        fController->retain();
        
        // get firelog reference
        fFireLog = IOFireLog::getFireLog();
        if( fFireLog == NULL )
            status = kIOReturnNoMemory;
        else
            fFireLog->retain();
    }

    //
    // take cycle timestamp from the first apple interface we discover.
    //
    
    IORegistryEntry* parent = controller->getParentEntry( gIOServicePlane ); // parent = the link
    parent = parent->getParentEntry( gIOServicePlane ); // parent = pci nub
	
    UInt32 vendor_id = 0;
    {
        OSData * vendor_id_data;
        UInt32 * vendor_id_ptr = NULL;
        vendor_id_data = (OSData*)parent->getProperty( "vendor-id" );
        if( vendor_id_data != NULL )
            vendor_id_ptr = (UInt32*)vendor_id_data->getBytesNoCopy();
        if( vendor_id_ptr != NULL)
            vendor_id = *vendor_id_ptr;
    }
    
    UInt32 subsystem_id = 0;
    {
        OSData * subsystem_id_data;
        UInt32 * subsystem_id_ptr = NULL;
        subsystem_id_data = (OSData*)parent->getProperty( "subsystem-id" );
        if( subsystem_id_data != NULL )
            subsystem_id_ptr = (UInt32*)subsystem_id_data->getBytesNoCopy();
        if( subsystem_id_ptr != NULL)
            subsystem_id = *subsystem_id_ptr;
    }

    UInt32 subsystem_vendor_id = 0;
    {
        OSData * subsystem_vendor_id_data;
        UInt32 * subsystem_vendor_id_ptr = NULL;
        subsystem_vendor_id_data = (OSData*)parent->getProperty( "subsystem-vendor-id" );
        if( subsystem_vendor_id_data != NULL )
            subsystem_vendor_id_ptr = (UInt32*)subsystem_vendor_id_data->getBytesNoCopy();
        if( subsystem_vendor_id_ptr != NULL)
            subsystem_vendor_id = *subsystem_vendor_id_ptr;
    }
		
//    IOLog( "vendor_id = 0x%08lx, subsystem_id = 0x%08lx, subsystem_vendor_id = 0x%08lx\n", vendor_id, subsystem_id, subsystem_vendor_id );
    
	// the goal of this code is to pick the FireWire interface on the motherboard 
	// to grab FireWire cycle time information, however this code is still a work
	// in progress and can be fooled into picking a PCI card for cycle time information
	
    if( vendor_id == 0x106b )
    {
        fFireLog->setMainController( fController );
    }
	else if ( subsystem_vendor_id == 0x106b )
    {
		// B&W G3
		// PCI Graphics G4
        fFireLog->setMainController( fController );
    }
	else if( subsystem_vendor_id == 0x11c1 )
    {
		// Some G4 Towers
        // Some PowerBook G4s
		
		// could pick some PCI cards
		fFireLog->setMainController( fController );
    }
	else if ( vendor_id == 0x104c )
	{
		// Some G4 Towers
		
		// could pick some PCI cards
        fFireLog->setMainController( fController );
	}
    else if ( subsystem_id == 0 && subsystem_vendor_id == 0 )
    {
		// could pick some PCI cards
        fFireLog->setMainController( fController );
    }
	
    //
    // publish unit directory
    //
    
    if( status == kIOReturnSuccess )
    {
        log_physical_address = fFireLog->getLogPhysicalAddress();
        log_size = fFireLog->getLogSize();
        log_random_id = fFireLog->getRandomID();
    }
    
    if( status == kIOReturnSuccess )
    {
        fUnitDir = IOLocalConfigDirectory::create();
        if( fUnitDir == NULL)
            status = kIOReturnNoMemory;
    }
    
    if( status == kIOReturnSuccess )
    {        
        fUnitDir->addEntry(kConfigUnitSpecIdKey, (UInt32)0x27);        
        fUnitDir->addEntry(kConfigUnitSwVersionKey, (UInt32)0x1);   
        fUnitDir->addEntry(kFireLogVersionKey, (UInt32)3 );
        fUnitDir->addEntry(kFireLogSizeKey, log_size );
        fUnitDir->addEntry(kFireLogAddressHiKey, (UInt32)(log_physical_address >> 16) );
        fUnitDir->addEntry(kFireLogAddressLoKey, (UInt32)(log_physical_address & 0x0000FFFF) );
        fUnitDir->addEntry(kFireLogRandomIDKey, (UInt32)(log_random_id & 0x00FFFFFF) );
        fUnitDir->addEntry(kFireLogMaxEntrySizeKey, (UInt32)( sizeof(uint64_t) + sizeof(UInt32) + kFireLogTempBufferSize + sizeof(UInt32)) );
    }
    
    if( status == kIOReturnSuccess )
    {
        fController->AddUnitDirectory( fUnitDir );
    }    

    //
    // allocate and register a physical address space
    //
    
    if( status == kIOReturnSuccess )
    {
        fLogDescriptor = IOMemoryDescriptor::withPhysicalAddress( log_physical_address, log_size, kIODirectionOut );
        if( fLogDescriptor == NULL )
            status = kIOReturnNoMemory;
    }
    
    if( status == kIOReturnSuccess )
    {
        fLogPhysicalAddressSpace = fController->createPhysicalAddressSpace( fLogDescriptor );
        if( fLogPhysicalAddressSpace == NULL )
            status = kIOReturnNoMemory;
    }

    if( status == kIOReturnSuccess )
    {
        status = fLogPhysicalAddressSpace->activate();
    }
    
    return status;
}

// free
//
//

void IOFireLogPublisher::free( void )
{
    if( fLogPhysicalAddressSpace )
    {
        fLogPhysicalAddressSpace->deactivate();
        fLogPhysicalAddressSpace->release();
        fLogPhysicalAddressSpace = NULL;
    }
    
    if( fLogDescriptor )
    {
        fLogDescriptor->release();
        fLogDescriptor = NULL;
    }

    if( fUnitDir )
    {
        fController->RemoveUnitDirectory( fUnitDir );
        fUnitDir->release();
        fUnitDir = NULL;
    }
    
    if( fFireLog->getMainController() == fController )
    {
        fFireLog->setMainController( NULL );
    }
    
    if( fController )
    {        
        fController->release();
        fController = NULL;
    }
    
    if( fFireLog )
    {
        fFireLog->release();
        fFireLog = NULL;
    }
    
    OSObject::free();    
}

#endif

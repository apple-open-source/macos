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
 
#ifndef _IOKIT_IOFIRELOGPRIV_H
#define _IOKIT_IOFIRELOGPRIV_H

#if FIRELOGCORE

#include <IOKit/firewire/IOFireLog.h>

#include <libkern/c++/OSObject.h>
#include <IOKit/system.h>

#import <IOKit/firewire/IOFireWireController.h>
#import <IOKit/firewire/IOLocalConfigDirectory.h>

#include <IOKit/IOBufferMemoryDescriptor.h>

#define kFireLogSize (12*1024*1024)    // 8MB
//#define kFireLogSize (512*1024)    // 512KB

class IOFireLog : public OSObject
{
    OSDeclareAbstractStructors(IOFireLog)

protected:

    typedef struct
    {
        UInt32	start;
        UInt32	end;
    } FireLogHeader;
    
    static OSObject * 	sFireLog;
    static int			sTempBufferIndex;
    static char 		sTempBuffer[255];
    
    IOFireWireController *		fController;
    IOBufferMemoryDescriptor * 	fLogDescriptor;
    IOFWAddressSpace * 			fLogPhysicalAddressSpace;
    IOPhysicalAddress			fLogPhysicalAddress;
    FireLogHeader *				fLogBuffer;
    char *						fLogStart;
    char *						fLogEnd;
    IOLocalConfigDirectory *	fUnitDir;
    IOLock *					fLock;
    bool						fNeedSpace;
    UInt32 						fLogSize;
    UInt32						fRandomID;
	
    static void firelog_putc( char c );
    virtual IOReturn initialize( void );

    inline char * logicalToPhysical( char * logical )
        { return (logical - ((char*)fLogBuffer) + ((char*)fLogPhysicalAddress)); }
    inline char * physicalToLogical( char * physical )
        { return (physical - ((char*)fLogPhysicalAddress) + ((char*)fLogBuffer)); }
   
    inline char * encodedToLogical( UInt32 encoded )
        { return ( fLogStart + ((encoded % (kFireLogSize>>2))<<2) ); }

    inline UInt32 sizeToEncoded( UInt32 size )
        { return (size >> 2); }
        
public:

    static IOReturn create( );
    
    virtual void free( void );
    
    static IOFireLog * getFireLog( void );
    
    virtual void setMainController( IOFireWireController * controller );
    virtual IOFireWireController * getMainController( void );
    virtual IOPhysicalAddress getLogPhysicalAddress( void );
    virtual UInt32 getLogSize( void );
    virtual UInt32 getRandomID( void );
	
    virtual void logString( const char *format, va_list ap );
    
};

// IOFireLogPublisher
//
// publishes firelog on a controller
//

class IOFireLogPublisher : public OSObject
{
    OSDeclareAbstractStructors(IOFireLogPublisher)

protected:
    
    IOFireWireController *		fController;
    IOFireLog *					fFireLog;
    IOMemoryDescriptor * 		fLogDescriptor;
    IOFWAddressSpace * 			fLogPhysicalAddressSpace;
    IOLocalConfigDirectory *	fUnitDir;

    virtual IOReturn initWithController( IOFireWireController* controller );
        
public:

    static IOFireLogPublisher * create( IOFireWireController * controller );
    virtual void free( void );
    
};

#endif

#endif

/*
 * Copyright (c) 2004-2007 Apple Inc. All rights reserved.
 *
 * IMPORTANT:  This Apple software is supplied to you by Apple Inc. ("Apple") in 
 * consideration of your agreement to the following terms, and your use, installation, 
 * modification or redistribution of this Apple software constitutes acceptance of these
 * terms.  If you do not agree with these terms, please do not use, install, modify or 
 * redistribute this Apple software.
 *
 * In consideration of your agreement to abide by the following terms, and subject to these 
 * terms, Apple grants you a personal, non exclusive license, under Apple’s copyrights in this 
 * original Apple software (the “Apple Software”), to use, reproduce, modify and redistribute 
 * the Apple Software, with or without modifications, in source and/or binary forms; provided 
 * that if you redistribute the Apple Software in its entirety and without modifications, you 
 * must retain this notice and the following text and disclaimers in all such redistributions 
 * of the Apple Software.  Neither the name, trademarks, service marks or logos of Apple 
 * Computer, Inc. may be used to endorse or promote products derived from the Apple Software 
 * without specific prior written permission from Apple. Except as expressly stated in this 
 * notice, no other rights or licenses, express or implied, are granted by Apple herein, 
 * including but not limited to any patent rights that may be infringed by your derivative 
 * works or by other works in which the Apple Software may be incorporated.
 * 
 * The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, 
 * EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-
 * INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE 
 * SOFTWARE OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS. 
 *
 * IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS 
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, 
 * REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND 
 * WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR 
 * OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


//-----------------------------------------------------------------------------
//	Includes
//-----------------------------------------------------------------------------

#include "Probing.h"

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>
#include <IOKit/scsi/SCSITask.h>


//-----------------------------------------------------------------------------
//	Macros
//-----------------------------------------------------------------------------

#define DEBUG 0

#define DEBUG_ASSERT_COMPONENT_NAME_STRING "Probing"

#if DEBUG
#define DEBUG_ASSERT_MESSAGE(componentNameString,	\
							 assertionString,		\
							 exceptionLabelString,	\
							 errorString,			\
							 fileName,				\
							 lineNumber,			\
							 errorCode)				\
DebugAssert(componentNameString,					\
					   assertionString,				\
					   exceptionLabelString,		\
					   errorString,					\
					   fileName,					\
					   lineNumber,					\
					   errorCode)					\

static void
DebugAssert ( const char *	componentNameString,
			  const char *	assertionString,
			  const char *	exceptionLabelString,
			  const char *	errorString,
			  const char *	fileName,
			  long			lineNumber,
			  int			errorCode )
{
	
	if ( ( assertionString != NULL ) && ( *assertionString != '\0' ) )
		printf ( "Assertion failed: %s: %s\n", componentNameString, assertionString );
	else
		printf ( "Check failed: %s:\n", componentNameString );
	if ( exceptionLabelString != NULL )
		printf ( "	 %s\n", exceptionLabelString );
	if ( errorString != NULL )
		printf ( "	 %s\n", errorString );
	if ( fileName != NULL )
		printf ( "	 file: %s\n", fileName );
	if ( lineNumber != 0 )
		printf ( "	 line: %ld\n", lineNumber );
	if ( errorCode != 0 )
		printf ( "	 error: %d\n", errorCode );
	
}

#endif	/* DEBUG */

#include <AssertMacros.h>


//-----------------------------------------------------------------------------
//	Constants
//-----------------------------------------------------------------------------

#define kIOSCSIParallelInterfaceControllerClassString	"IOSCSIParallelInterfaceController"


//-----------------------------------------------------------------------------
//	Prototypes
//-----------------------------------------------------------------------------

static IOReturn
ReprobeTargetDevice ( io_service_t controller, SCSITargetIdentifier targetID );


//-----------------------------------------------------------------------------
//	ReprobeDomainTarget - Reprobes device at targetID on a SCSI Domain
//-----------------------------------------------------------------------------


IOReturn
ReprobeDomainTarget ( UInt64				domainID,
					  SCSITargetIdentifier	targetID )
{
	
	IOReturn			result		= kIOReturnSuccess;
	io_service_t		service		= MACH_PORT_NULL;
	io_iterator_t		iterator	= MACH_PORT_NULL;
	boolean_t			found		= false;
	
	// First, let's find all the SCSI Parallel Controllers.
	result = IOServiceGetMatchingServices ( kIOMasterPortDefault,
											IOServiceMatching ( kIOSCSIParallelInterfaceControllerClassString ),
											&iterator );
	
	require ( ( result == kIOReturnSuccess ), ErrorExit );
	
	service = IOIteratorNext ( iterator );
	while ( service != MACH_PORT_NULL )
	{
		
		// Have we found the one with the specified domainID yet?
		if ( found == false )
		{
			
			CFMutableDictionaryRef	deviceDict	= NULL;
			CFDictionaryRef			subDict		= NULL;
			
			// Get the properties for this node from the IORegistry
			result = IORegistryEntryCreateCFProperties ( service,
														 &deviceDict,
														 kCFAllocatorDefault,
														 0 );
			
			// Get the protocol characteristics dictionary
			subDict = ( CFDictionaryRef ) CFDictionaryGetValue ( deviceDict, CFSTR ( kIOPropertyProtocolCharacteristicsKey ) );
			if ( subDict != NULL )
			{
				
				CFNumberRef		deviceDomainIDRef = 0;
				
				// Get the SCSI Domain Identifier value
				deviceDomainIDRef = ( CFNumberRef ) CFDictionaryGetValue ( subDict, CFSTR ( kIOPropertySCSIDomainIdentifierKey ) );
				if ( deviceDomainIDRef != 0 )
				{
					
					UInt64	deviceDomainID = 0;
					
					// Get the value from the CFNumberRef.
					if ( CFNumberGetValue ( deviceDomainIDRef, kCFNumberLongLongType, &deviceDomainID ) )
					{
						
						// Does the domainID match?
						if ( domainID == deviceDomainID )
						{
							
							// Find the target device and reprobe it.
							result = ReprobeTargetDevice ( service, targetID );
							found = true;
							
						}
						
					}
				
				}
				
			}
			
			if ( deviceDict != NULL )
				CFRelease ( deviceDict );
			
		}
		
		IOObjectRelease ( service );
		
		service = IOIteratorNext ( iterator );
		
	}
	
	IOObjectRelease ( iterator );
	iterator = MACH_PORT_NULL;
	
	if ( found == false )
		result = kIOReturnNoDevice;
	
	
ErrorExit:
	
	
	return result;
	
}


//-----------------------------------------------------------------------------
//	ReprobeTargetDevice - 	Actually performs the reprobe if it can find the
//							IOSCSIParallelInterfaceDevice at the targetID.
//-----------------------------------------------------------------------------

static IOReturn
ReprobeTargetDevice ( io_service_t controller, SCSITargetIdentifier targetID )
{
	
	IOReturn		result 		= kIOReturnSuccess;
	io_iterator_t	childIter	= MACH_PORT_NULL;
	io_service_t	service		= MACH_PORT_NULL;
	boolean_t		found		= false;
	
	// We find the children for the controller and iterate over them looking for the
	// one which has a targetID which matches.
	result = IORegistryEntryGetChildIterator ( controller, kIOServicePlane, &childIter );
	require ( ( result == kIOReturnSuccess ), ErrorExit );
	
	service = IOIteratorNext ( childIter );	
	while ( service != MACH_PORT_NULL )
	{

		// Did we find our device yet? If not, then try to find it. If we have already
		// found it, we still need to call IOObjectRelease on the io_service_t
		// or it will have an artificial retain count on it.
		if ( found == false )
		{
			
			CFMutableDictionaryRef	deviceDict	= NULL;
			CFDictionaryRef			subDict		= NULL;
			
			// Get the properties for this node from the IORegistry
			result = IORegistryEntryCreateCFProperties ( service,
														 &deviceDict,
														 kCFAllocatorDefault,
														 0 );
			
			// Get the protocol characteristics dictionary
			subDict = ( CFDictionaryRef ) CFDictionaryGetValue ( deviceDict, CFSTR ( kIOPropertyProtocolCharacteristicsKey ) );
			if ( subDict != NULL )
			{
				
				CFNumberRef		deviceTargetIDRef = 0;
				
				// Get the targetID value
				deviceTargetIDRef = ( CFNumberRef ) CFDictionaryGetValue ( subDict, CFSTR ( kIOPropertySCSITargetIdentifierKey ) );
				if ( deviceTargetIDRef != 0 )
				{
					
					UInt64	deviceTargetID = 0;
					
					// Get the value from the CFNumberRef.
					if ( CFNumberGetValue ( deviceTargetIDRef, kCFNumberLongLongType, &deviceTargetID ) )
					{
						
						// Does it match?
						if ( targetID == deviceTargetID )
						{
							
							// Reprobe the device.
							result = IOServiceRequestProbe ( service, 0 );
							found = true;
							
						}
						
					}
					
				}
				
			}
			
			if ( deviceDict != NULL )
				CFRelease ( deviceDict );
			
		}
		
		IOObjectRelease ( service );
		
		service = IOIteratorNext ( childIter );
		
	}
	
	IOObjectRelease ( childIter );
	childIter = MACH_PORT_NULL;
	
	if ( found == false )
		result = kIOReturnNoDevice;
	
	
ErrorExit:
	
	
	return result;
	
}
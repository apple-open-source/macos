/*
 * © Copyright 2003 Apple Computer, Inc. All rights reserved.
 *
 * IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. (“Apple”) in 
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


//—————————————————————————————————————————————————————————————————————————————
//	Includes
//—————————————————————————————————————————————————————————————————————————————

#include <ctype.h>
#include <stdio.h>
#include <sys/time.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_init.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>
#include <IOKit/storage/ata/ATASMARTLib.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <CoreFoundation/CoreFoundation.h>


//—————————————————————————————————————————————————————————————————————————————
//	Macros
//—————————————————————————————————————————————————————————————————————————————

#define DEBUG			0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING "ATASMARTLeakFinder"
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

#include <AssertMacros.h>


//—————————————————————————————————————————————————————————————————————————————
//	Constants
//—————————————————————————————————————————————————————————————————————————————

#define kIOATASMARTUserClientString			"ATASMARTUserClient"
#define kIOCommandGateString				"IOCommandGate"
#define kIOATABusCommandString				"IOATABusCommand"
#define kIOATABlockStorageDeviceString		"IOATABlockStorageDevice"
#define kIOATAFeaturesKey					"ATA Features"
#define kIOClassesKey						"Classes"

#define kATADefaultSectorSize				512


//—————————————————————————————————————————————————————————————————————————————
//	Prototypes
//—————————————————————————————————————————————————————————————————————————————

static IOReturn
GetServiceObject ( io_service_t * obj );

static void
FindLeaks ( io_service_t obj );

static CFStringRef
GetDeviceDescription ( CFDictionaryRef dict );

static IOReturn
PrintStatistics ( CFArrayRef array );


//—————————————————————————————————————————————————————————————————————————————
//	•	main - Our main entry point
//—————————————————————————————————————————————————————————————————————————————

int
main ( int argc, const char * argv[] )
{
	
	IOReturn			status 	= kIOReturnSuccess;
	io_service_t		obj		= MACH_PORT_NULL;
	CFMutableArrayRef	array	= NULL;
	
	array = CFArrayCreateMutable ( kCFAllocatorDefault, 3, &kCFTypeArrayCallBacks );
	require ( ( array != NULL ), ErrorExit );
	
	CFArrayAppendValue ( array, CFRetain ( CFSTR ( kIOATASMARTUserClientString ) ) );
	CFArrayAppendValue ( array, CFRetain ( CFSTR ( kIOCommandGateString ) ) );
	CFArrayAppendValue ( array, CFRetain ( CFSTR ( kIOATABusCommandString ) ) );
	
	status = GetServiceObject ( &obj );
	if ( ( status == kIOReturnNoDevice ) || ( obj == MACH_PORT_NULL ) )
	{
		printf ( "\nNo S.M.A.R.T.-capable devices found for leak testing\n" );
	}
	require_action ( ( status == kIOReturnSuccess ), ReleaseArray, status = -1 );
	require_action ( ( obj != MACH_PORT_NULL ), ReleaseArray, status = -1 );
	
	status = PrintStatistics ( array );
	require ( ( status == kIOReturnSuccess ), ReleaseObject );
	
	FindLeaks ( obj );
	
	status = PrintStatistics ( array );
	require ( ( status == kIOReturnSuccess ), ReleaseObject );
	
	
ReleaseObject:
	
	
	require ( ( obj != MACH_PORT_NULL ), ReleaseArray );
	IOObjectRelease ( obj );
	obj = MACH_PORT_NULL;
	
	
ReleaseArray:
	
	
	CFRelease ( array );
	array = NULL;
	
	
ErrorExit:
	
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	•	GetServiceObject - Gets a service object for us to use
//—————————————————————————————————————————————————————————————————————————————

IOReturn
GetServiceObject ( io_service_t * obj )
{
	
	IOReturn		err 		= kIOReturnNoResources;
	io_iterator_t	iter		= MACH_PORT_NULL;
	io_object_t		service		= MACH_PORT_NULL;
	
	err = IOServiceGetMatchingServices ( kIOMasterPortDefault,
										 IOServiceMatching ( kIOATABlockStorageDeviceString ),
										 &iter );
	
	require ( ( err == KERN_SUCCESS ), ErrorExit );
	
	while ( ( service = IOIteratorNext ( iter ) ) != NULL )
	{
		
		CFMutableDictionaryRef	dict;
		CFDictionaryRef			deviceCharacteristics;
		CFNumberRef				features;
		UInt32					value;
		
		err = IORegistryEntryCreateCFProperties ( service, &dict, kCFAllocatorDefault, 0  );
		check ( err == KERN_SUCCESS );
		
		if ( err != KERN_SUCCESS )
		{
			
			err = IOObjectRelease ( service );
			continue;
			
		}
		
		if ( CFDictionaryGetValueIfPresent ( dict,
											 CFSTR ( kIOPropertyDeviceCharacteristicsKey ),
											 ( void * ) &deviceCharacteristics ) == false )
		{
			continue;
		}
		
		// We have the device characteristics
		if ( CFDictionaryGetValueIfPresent ( deviceCharacteristics,
											 CFSTR ( kIOATAFeaturesKey ),
											 ( void * ) &features ) == false )
		{
			continue;
		}
		
		// We now have the features in a CFNumberRef. Convert to UInt32
		if ( CFNumberGetValue ( features, kCFNumberLongType, &value ) == false )
		{
			continue;
		}
		
		if ( ( value & kIOATAFeatureSMART ) != kIOATAFeatureSMART )
		{
			continue;
		}
		
		if ( *obj == MACH_PORT_NULL )
		{
			
			*obj = service;
			IOObjectRetain ( service );
			
		}
		
		IOObjectRelease ( service );
		
	}
	
	err = IOObjectRelease ( iter );
	
	
ErrorExit:	
	
	
	return err;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	•	FindLeaks - Opens and closes a device interface to test if the
//					kernel side implementation is leaking objects
//—————————————————————————————————————————————————————————————————————————————

void
FindLeaks ( io_service_t obj )
{
	
	IOReturn				status			= kIOReturnSuccess;
	HRESULT					result			= S_OK;
	IOCFPlugInInterface **	plugIn 			= NULL;
	IOATASMARTInterface **	smartInterface	= NULL;
	SInt32					score			= 0;
	CFMutableDictionaryRef	dict			= 0;			
	
	require ( ( obj != MACH_PORT_NULL ), ErrorExit );
	
	status = IORegistryEntryCreateCFProperties ( obj,
												 &dict,
												 kCFAllocatorDefault,
												 0 );
	
	require ( ( status == kIOReturnSuccess ), ErrorExit );
	
	printf ( "\n" );
	printf ( "Using: " );
	fflush ( stdout );
	CFShow ( GetDeviceDescription ( dict ) );
	
	status = IOCreatePlugInInterfaceForService ( obj,
												 kIOATASMARTUserClientTypeID,
												 kIOCFPlugInInterfaceID,
												 &plugIn,
												 &score );
	
	require ( ( status == kIOReturnSuccess ), ReleaseProperties );
	
	result = ( *plugIn )->QueryInterface ( plugIn,
										   CFUUIDGetUUIDBytes ( kIOATASMARTInterfaceID ),
										   ( LPVOID ) &smartInterface );
	require ( ( result == S_OK ), ReleasePlugIn );
	
	require ( ( smartInterface != NULL ), ReleasePlugIn );
	( *smartInterface )->Release ( smartInterface );
	smartInterface = NULL;
	
	
ReleasePlugIn:
	
	
	status = IODestroyPlugInInterface ( plugIn );
	require ( ( status == kIOReturnSuccess ), ReleaseProperties );

ReleaseProperties:
	
	
	require ( ( dict != NULL ), ErrorExit );
	CFRelease ( dict );
	dict = NULL;
	
	
ErrorExit:
	
	
	return;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	•	GetDeviceDescription - Creates a device description. Caller must call
//							   CFRelease on returned CFStringRef if non-NULL.
//—————————————————————————————————————————————————————————————————————————————

static CFStringRef
GetDeviceDescription ( CFDictionaryRef dict )
{
	
	CFMutableStringRef		description = NULL;
	CFDictionaryRef			deviceDict	= NULL;
	CFStringRef				product		= NULL;
	
	require ( ( dict != NULL ), Exit );
	
	deviceDict = ( CFDictionaryRef ) CFDictionaryGetValue ( dict, CFSTR ( kIOPropertyDeviceCharacteristicsKey ) );
	require ( ( deviceDict != 0 ), Exit );
	
	product = ( CFStringRef ) CFDictionaryGetValue ( deviceDict, CFSTR ( kIOPropertyProductNameKey ) );
	
	description = CFStringCreateMutableCopy ( kCFAllocatorDefault, 0, product );
	require ( ( description != 0 ), Exit );
	
	
Exit:
	
	
	return description;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	•	PrintStatistics - Prints statistics for the array of class names.
//—————————————————————————————————————————————————————————————————————————————

IOReturn
PrintStatistics ( CFArrayRef array )
{
	
	io_registry_entry_t		root		= MACH_PORT_NULL;
	CFDictionaryRef			dictionary	= NULL;
	CFDictionaryRef			props		= NULL;
	CFStringRef				key			= NULL;
	CFNumberRef				num			= NULL;
	IOReturn				status		= kIOReturnSuccess;
	int						count		= 0;
	int						index		= 0;
	
	// Obtain the registry root entry.
	root = IORegistryGetRootEntry ( kIOMasterPortDefault );
	require ( ( root != NULL ), ErrorExit );
	
	status = IORegistryEntryCreateCFProperties (
			root,
			( void * ) &props,
			kCFAllocatorDefault,
			kNilOptions );
	
	require ( ( KERN_SUCCESS == status ), ReleaseRoot );
	require ( ( CFDictionaryGetTypeID ( ) == CFGetTypeID ( props ) ), ReleaseRoot );
	
	dictionary = ( CFDictionaryRef ) CFDictionaryGetValue (
										props,
										CFSTR ( kIOKitDiagnosticsKey ) );
	
	require ( ( dictionary != NULL ), ReleaseProps );
	require ( ( CFDictionaryGetTypeID ( ) == CFGetTypeID ( dictionary ) ), ReleaseProps );
	
	dictionary = ( CFDictionaryRef ) CFDictionaryGetValue ( dictionary, CFSTR ( kIOClassesKey ) );
	require ( ( dictionary != NULL ), ReleaseProps );
	require ( ( CFDictionaryGetTypeID ( ) == CFGetTypeID ( dictionary ) ), ReleaseProps );

	printf ( "----------------------------------------------------------------\n" );
	
	count = CFArrayGetCount ( array );
	
	for ( index = 0; index < count; index++ )
	{
		
		SInt32	num32 = 0;
		
		key = ( CFStringRef ) CFArrayGetValueAtIndex ( array, index );
		require ( ( key != NULL ), ReleaseProps );
		
		num = ( CFNumberRef ) CFDictionaryGetValue ( dictionary, key );
		
		if ( num == NULL )
			continue;
		
		require ( ( CFNumberGetTypeID ( ) == CFGetTypeID ( num ) ), ReleaseProps );
		CFNumberGetValue ( num, kCFNumberSInt32Type, &num32 );
		if ( index < ( count - 1 ) )
			printf ( "%s = %d, ", CFStringGetCStringPtr ( key, CFStringGetSystemEncoding ( ) ), ( int ) num32 );
		else
			printf ( "%s = %d", CFStringGetCStringPtr ( key, CFStringGetSystemEncoding ( ) ), ( int ) num32 );
		
	}
	
	if ( num != NULL )
	{
		printf ( "\n----------------------------------------------------------------\n" );
	}
	
	
ReleaseProps:
	
	
	CFRelease ( props );
	props = NULL;
	
	
ReleaseRoot:
	
	
	IOObjectRelease ( root );
	root = NULL;
	
	
ErrorExit:
	
	
	return status;
	
}
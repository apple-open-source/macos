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

#define DEBUG 			0
#define DEBUG_ALL		0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING "SMARTUnitTest"
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

#define kATADefaultSectorSize				512
#define kIOATABlockStorageDeviceClass		"IOATABlockStorageDevice"
#define kIOATAFeaturesKey					"ATA Features"


//—————————————————————————————————————————————————————————————————————————————
//	Prototypes
//—————————————————————————————————————————————————————————————————————————————

static IOReturn
GetServiceObject ( io_service_t * obj );

static IOReturn
PerformSMARTUnitTest ( io_service_t service );

static void
PrintIdentifyData ( IOATASMARTInterface ** smartInterface );

static void
PrintSMARTData ( IOATASMARTInterface ** smartInterface );


//—————————————————————————————————————————————————————————————————————————————
//	•	main - Our main entry point
//—————————————————————————————————————————————————————————————————————————————

int
main ( int argc, const char * argv[] )
{
	
	IOReturn		status	= kIOReturnSuccess;
	io_service_t	obj		= MACH_PORT_NULL;
	
	status = GetServiceObject ( &obj );
	if ( ( status == kIOReturnNoDevice ) || ( obj == MACH_PORT_NULL ) )
	{
		printf ( "\nNo S.M.A.R.T.-capable devices found for leak testing\n" );
	}
	require_action ( ( status == kIOReturnSuccess ), ErrorExit, status = -1 );
	require_action ( ( obj != MACH_PORT_NULL ), ErrorExit, status = -1 );
	
	status = PerformSMARTUnitTest ( obj );
	require_action ( ( status == kIOReturnSuccess ), ReleaseObject, status = -1 );
	
	
ReleaseObject:
	
	
	require ( ( obj != MACH_PORT_NULL ), ErrorExit );
	IOObjectRelease ( obj );
	obj = MACH_PORT_NULL;
	
	
ErrorExit:
	
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	•	GetServiceObject - Gets a service object for us to use
//—————————————————————————————————————————————————————————————————————————————

static IOReturn
GetServiceObject ( io_service_t * obj )
{
	
	IOReturn		err 		= kIOReturnNoResources;
	io_iterator_t	iter		= MACH_PORT_NULL;
	io_object_t		service		= MACH_PORT_NULL;
	
	err = IOServiceGetMatchingServices ( kIOMasterPortDefault,
										 IOServiceMatching ( kIOATABlockStorageDeviceClass ),
										 &iter );
	
	require ( ( err == kIOReturnSuccess ), ErrorExit );
	
	while ( ( service = IOIteratorNext ( iter ) ) != NULL )
	{
		
		CFMutableDictionaryRef	dict;
		CFDictionaryRef			deviceCharacteristics;
		CFNumberRef				features;
		UInt32					value;
		
		err = IORegistryEntryCreateCFProperties ( service, &dict, kCFAllocatorDefault, 0  );
		check ( err == kIOReturnSuccess );
		
		if ( err != kIOReturnSuccess )
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
//	•	PerformSMARTUnitTest - Performs S.M.A.R.T. unit test on device
//—————————————————————————————————————————————————————————————————————————————

static IOReturn
PerformSMARTUnitTest ( io_service_t service )
{
	
	IOCFPlugInInterface **		cfPlugInInterface	= NULL;
	IOATASMARTInterface **		smartInterface		= NULL;
	SInt32						score				= 0;
	HRESULT						herr				= S_OK;
	IOReturn					err					= kIOReturnSuccess;
	
	err = IOCreatePlugInInterfaceForService (	service,
												kIOATASMARTUserClientTypeID,
												kIOCFPlugInInterfaceID,
												&cfPlugInInterface,
												&score );
	
	require_string ( ( err == kIOReturnSuccess ), ErrorExit,
					 "IOCreatePlugInInterfaceForService failed" );
	
	herr = ( *cfPlugInInterface )->QueryInterface (
										cfPlugInInterface,
										CFUUIDGetUUIDBytes ( kIOATASMARTInterfaceID ),
										( LPVOID ) &smartInterface );
	
	require_string ( ( herr == S_OK ), DestroyPlugIn,
					 "QueryInterface failed" );
	
	PrintIdentifyData ( smartInterface );
	PrintSMARTData ( smartInterface );
	
	( *smartInterface )->Release ( smartInterface );
	smartInterface = NULL;
	
	
DestroyPlugIn:
	
	
	IODestroyPlugInInterface ( cfPlugInInterface );
	cfPlugInInterface = NULL;
	
	
ErrorExit:
	
	
	return err;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	•	PrintIdentifyData - Prints ATA IDENTIFY data for device
//—————————————————————————————————————————————————————————————————————————————

void
PrintIdentifyData ( IOATASMARTInterface ** smartInterface )
{
	
	IOReturn	error	= kIOReturnSuccess;
	UInt8 *		buffer	= NULL;
	UInt32		length	= kATADefaultSectorSize;
	
	buffer = ( UInt8 * ) malloc ( kATADefaultSectorSize );
	require ( ( buffer != NULL ), ErrorExit );
	
	bzero ( buffer, kATADefaultSectorSize );
	
	error = ( *smartInterface )->GetATAIdentifyData ( smartInterface,
													  buffer,
													  kATADefaultSectorSize,
													  &length );
	
	require_string ( ( error == kIOReturnSuccess ), ErrorExit,
					 "GetATAIdentifyData failed" );
	
	#if DEBUG_ALL
	{
		
		UInt8 *		ptr		= ( UInt8 * ) buffer;
		int			index	= 0;
		
		printf ( "\n" );
		printf ( "Identify Data\n" );
		printf ( "---------------------------------------------------------\n" );
		printf ( "Offset  |      Data (in hex)\n" );
		printf ( "hex dec | \n" );
		printf ( "---------------------------------------------------------\n" );
		
		for ( index = 0; index < kATADefaultSectorSize; index += 16 )
		{
			
			printf ( "%03x ", index );
			printf ( "%03d | ", index );
			
			printf ( "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
					 ptr[index+0], ptr[index+1], ptr[index+2], ptr[index+3],
					 ptr[index+4], ptr[index+5], ptr[index+6], ptr[index+7],
					 ptr[index+8], ptr[index+9], ptr[index+10], ptr[index+11],
					 ptr[index+12], ptr[index+13], ptr[index+14], ptr[index+15] );
			
		}
		
		printf ( "\n" );
		
	}
	#endif /* DEBUG_ALL */
	
	// terminate the strings with 0's
	// this changes the identify data, so we MUST do this part last.
	buffer[94] = 0;
	buffer[40] = 0;
	
	// Model number runs from byte 54 to 93 inclusive - byte 94 is set to 
	// zero to terminate that string
	printf ( "Model = %s\n", &buffer[54] );
	
	// now that we have made a deep copy of the model string, poke a 0 into byte 54 
	// in order to terminate the fw-vers string which runs from bytes 46 to 53 inclusive.
	buffer[54] = 0;
	
	printf ( "Firmware = %s\n", &buffer[46] );	
	printf ( "Serial number = %s\n", &buffer[20] );
	
	free ( buffer );
	buffer = NULL;
	
	
ErrorExit:
	
	
	return;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	•	PrintSMARTData - Prints S.M.A.R.T data for device
//—————————————————————————————————————————————————————————————————————————————

void
PrintSMARTData ( IOATASMARTInterface ** smartInterface )
{
	
	IOReturn				error				= kIOReturnSuccess;
	Boolean					conditionExceeded	= false;
	int						index				= 0;
	char					buffer[kATADefaultSectorSize];
	ATASMARTData			smartData;
	ATASMARTLogDirectory	logDirectory;
	
	bzero ( &smartData, sizeof ( smartData ) );
	bzero ( &logDirectory, sizeof ( logDirectory ) );
	
	error = ( *smartInterface )->SMARTEnableDisableOperations ( smartInterface, true );
	require_string ( ( error == kIOReturnSuccess ), ErrorExit,
					 "SMARTEnableDisableOperations failed" );
	
	error = ( *smartInterface )->SMARTEnableDisableAutosave ( smartInterface, true );
	require_string ( ( error == kIOReturnSuccess ), ErrorExit,
					 "SMARTEnableDisableAutosave failed" );

	error = ( *smartInterface )->SMARTReturnStatus ( smartInterface, &conditionExceeded );
	require_string ( ( error == kIOReturnSuccess ), ErrorExit,
					 "SMARTReturnStatus failed" );
	
	if ( conditionExceeded )
	{
		printf ( "SMART condition exceeded, drive will fail soon\n" );
	}
	
	else
	{
		printf ( "SMART condition not exceeded, drive OK\n" );
	}
	
	error = ( *smartInterface )->SMARTExecuteOffLineImmediate ( smartInterface, false );
	require_string ( ( error == kIOReturnSuccess ), ErrorExit,
					 "SMARTExecuteOffLineImmediate failed" );

	error = ( *smartInterface )->SMARTReadData ( smartInterface, &smartData );
	require_string ( ( error == kIOReturnSuccess ), ErrorExit,
					 "SMARTReadData failed" );
	
	#if DEBUG_ALL
	{
		
		UInt8 *		ptr = ( UInt8 * ) &smartData;
		
		printf ( "\n" );
		printf ( "SMART Data\n" );
		printf ( "---------------------------------------------------------\n" );
		printf ( "Offset  |      Data (in hex)\n" );
		printf ( "hex dec | \n" );
		printf ( "---------------------------------------------------------\n" );

		for ( index = 0; index < sizeof ( ATASMARTData ); index += 16 )
		{
			
			printf ( "%03x ", index );
			printf ( "%03d | ", index );
			
			printf ( "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
					 ptr[index+0], ptr[index+1], ptr[index+2], ptr[index+3],
					 ptr[index+4], ptr[index+5], ptr[index+6], ptr[index+7],
					 ptr[index+8], ptr[index+9], ptr[index+10], ptr[index+11],
					 ptr[index+12], ptr[index+13], ptr[index+14], ptr[index+15] );
			
		}
		
		printf ( "\n" );
		
	}
	#endif /* DEBUG_ALL */
	
	error = ( *smartInterface )->SMARTValidateReadData ( smartInterface, &smartData );
	if ( error != kIOReturnSuccess )
	{
		
		printf ( "SMARTValidateReadData failed: %s(%x,%d)\n",
				 mach_error_string ( error ), error, error & 0xFFFFFF );
		
	}

	else
	{
		
		printf ( "Checksum valid, SMART Data is OK\n" );
		
	}

	error = ( *smartInterface )->SMARTReadLogDirectory ( smartInterface, &logDirectory );
	if ( error != kIOReturnSuccess )
	{
		
		if ( error == kIOReturnUnsupported )
		{
			printf ( "SMARTReadLogDirectory not supported\n" );
		}

		else if ( error == kIOReturnNotReadable )
		{
			printf ( "SMARTReadLogDirectory unreadable\n" );
		}
		
		else
		{
			printf ( "SMARTReadLogDirectory failed: %s(%x,%d)\n", mach_error_string ( error ), error, error & 0xFFFFFF );
		}
		
	}
	
	#if DEBUG_ALL
	else
	{
		
		UInt8 *		ptr = ( UInt8 * ) &logDirectory;
		
		printf ( "\n" );
		printf ( "SMART Log Directory Data\n" );
		printf ( "---------------------------------------------------------\n" );
		printf ( "Offset  |      Data (in hex)\n" );
		printf ( "hex dec | \n" );
		printf ( "---------------------------------------------------------\n" );

		for ( index = 0; index < sizeof ( logDirectory ); index += 16 )
		{
			
			printf ( "%03x ", index );
			printf ( "%03d | ", index );
			
			printf ( "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
					 ptr[index+0], ptr[index+1], ptr[index+2], ptr[index+3],
					 ptr[index+4], ptr[index+5], ptr[index+6], ptr[index+7],
					 ptr[index+8], ptr[index+9], ptr[index+10], ptr[index+11],
					 ptr[index+12], ptr[index+13], ptr[index+14], ptr[index+15] );
			
		}
		
		printf ( "\n" );
		
	}
	#endif /* DEBUG_ALL */
	
	for ( index = 0; index < kATADefaultSectorSize; index++ )
	{
		buffer[index] = index;
	}
	
	index = 0x81;	// First host addressable log area is 0x81.
	
	error = ( *smartInterface )->SMARTWriteLogAtAddress ( smartInterface, index, ( void * ) buffer, kATADefaultSectorSize );
	if ( error != kIOReturnSuccess )
	{
		
		if ( error == kIOReturnUnsupported )
		{
			printf ( "SMARTWriteLogAtAddress not supported\n" );
		}

		else if ( error == kIOReturnNotWritable )
		{
			printf ( "SMARTWriteLogAtAddress unwritable\n" );
		}
		
		else
		{
			printf ( "SMARTWriteLogAtAddress failed: %s(%x,%d)\n", mach_error_string ( error ), error, error & 0xFFFFFF );
		}
		
	}
	
	error = ( *smartInterface )->SMARTReadLogDirectory ( smartInterface, &logDirectory );
	if ( error != kIOReturnSuccess )
	{
		
		if ( error == kIOReturnUnsupported )
		{
			printf ( "SMARTReadLogDirectory not supported\n" );
		}

		else if ( error == kIOReturnNotReadable )
		{
			printf ( "SMARTReadLogDirectory unreadable\n" );
		}
		
		else
		{
			printf ( "SMARTReadLogDirectory failed: %s(%x,%d)\n", mach_error_string ( error ), error, error & 0xFFFFFF );
		}
		
	}
	
	#if DEBUG_ALL
	else
	{
		
		UInt8 *		ptr = ( UInt8 * ) &logDirectory;
		
		printf ( "\n" );
		printf ( "SMART Log Directory Data\n" );
		printf ( "---------------------------------------------------------\n" );
		printf ( "Offset  |      Data (in hex)\n" );
		printf ( "hex dec | \n" );
		printf ( "---------------------------------------------------------\n" );

		for ( index = 0; index < sizeof ( logDirectory ); index += 16 )
		{
			
			printf ( "%03x ", index );
			printf ( "%03d | ", index );
			
			printf ( "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
					 ptr[index+0], ptr[index+1], ptr[index+2], ptr[index+3],
					 ptr[index+4], ptr[index+5], ptr[index+6], ptr[index+7],
					 ptr[index+8], ptr[index+9], ptr[index+10], ptr[index+11],
					 ptr[index+12], ptr[index+13], ptr[index+14], ptr[index+15] );
			
		}
		
		printf ( "\n" );
		
	}
	#endif /* DEBUG_ALL */
	
	bzero ( buffer, kATADefaultSectorSize );
	
	index = 0x81;	// First unreserved area
	
	error = ( *smartInterface )->SMARTReadLogAtAddress ( smartInterface,
														 index,
														 ( void * ) buffer,
														 kATADefaultSectorSize );
	if ( error != kIOReturnSuccess )
	{
		
		printf ( "SMARTReadLogAtAddress failed: %s(%x,%d)\n",
				 mach_error_string ( error ), error, error & 0xFFFFFF );
		
	}
	
	#if DEBUG_ALL
	else
	{
		
		UInt8 *		ptr = ( UInt8 * ) buffer;
		bool		ok	= true;
		
		printf ( "\n" );
		printf ( "SMART Log Data at Address 0x%02x\n", index );
		printf ( "---------------------------------------------------------\n" );
		printf ( "Offset  |      Data (in hex)\n" );
		printf ( "hex dec | \n" );
		printf ( "---------------------------------------------------------\n" );
		
		for ( index = 0; index < sizeof ( buffer ); index += 16 )
		{
			
			printf ( "%03x ", index );
			printf ( "%03d | ", index );
			
			printf ( "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
					 ptr[index+0], ptr[index+1], ptr[index+2], ptr[index+3],
					 ptr[index+4], ptr[index+5], ptr[index+6], ptr[index+7],
					 ptr[index+8], ptr[index+9], ptr[index+10], ptr[index+11],
					 ptr[index+12], ptr[index+13], ptr[index+14], ptr[index+15] );
			
		}
		
		printf ( "\n" );
		
		for ( index = 0; index < kATADefaultSectorSize; index++ )
		{
			
			if ( ptr[index] != ( index & 0xFF ) )
			{
				
				printf ( "SMART Log data incorrect at byte %d\n", index );
				ok = false;
				
			}
			
		}
		
		if ( ok == true )
		{
			printf ( "Verified log directory data\n" );
		}
		
	}
	#endif /* DEBUG_ALL */
	
	
ErrorExit:
	
	
	return;
	
}
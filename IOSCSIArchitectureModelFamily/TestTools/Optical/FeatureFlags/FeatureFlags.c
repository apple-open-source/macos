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

#include <stdio.h>
#include <stdbool.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <IOKit/scsi/IOSCSIMultimediaCommandsDevice.h>


//—————————————————————————————————————————————————————————————————————————————
//	Macros
//—————————————————————————————————————————————————————————————————————————————

#define DEBUG 0

#define DEBUG_ASSERT_COMPONENT_NAME_STRING "FeatureFlags"

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


//—————————————————————————————————————————————————————————————————————————————
//	Constants
//—————————————————————————————————————————————————————————————————————————————

#define kIOCDBlockStorageDeviceClassString		"IOCDBlockStorageDevice"
#define kAppleLowPowerPollingKey				"Low Power Polling"


//—————————————————————————————————————————————————————————————————————————————
//	Prototypes
//—————————————————————————————————————————————————————————————————————————————

static io_iterator_t
GetDeviceIterator ( const char * deviceClass );

static void
GetFeaturesFlagsForDrive ( CFDictionaryRef	dict,
						   UInt32 *			cdFlags,
						   UInt32 *			dvdFlags,
						   Boolean *		lowPowerPoll );

static void
PrintFeaturesForEachDevice ( io_iterator_t iter );

static CFMutableDictionaryRef
GetRegistryEntryProperties ( io_service_t service );

static CFStringRef
GetDriveDescription ( CFDictionaryRef dict );

static void
PrintCDFeatures ( UInt32 cdFlags );

static void
PrintDVDFeatures ( UInt32 dvdFlags );

static void
PrintLowPowerPollingSupport ( Boolean supported );

static __inline__ bool IsBitSet ( UInt32 flags, UInt32 mask );


//—————————————————————————————————————————————————————————————————————————————
//	•	main - Our main entry point
//—————————————————————————————————————————————————————————————————————————————

int
main ( int argc, const char * argv[] )
{
	
	io_iterator_t	iter		= MACH_PORT_NULL;
	int				result		= -1;
	
	iter = GetDeviceIterator ( kIOCDBlockStorageDeviceClassString );
	require ( ( iter != MACH_PORT_NULL ), ErrorExit );
	
	PrintFeaturesForEachDevice ( iter );
	
	IOObjectRelease ( iter );
	iter = MACH_PORT_NULL;
	
	result = 0;
	
	
ErrorExit:
	
	
	return 0;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	•	GetDeviceIterator - Gets an io_iterator_t for our class type
//—————————————————————————————————————————————————————————————————————————————

static io_iterator_t
GetDeviceIterator ( const char * deviceClass )
{
	
	IOReturn			err			= kIOReturnSuccess;
	io_iterator_t		iterator	= MACH_PORT_NULL;
	
	err = IOServiceGetMatchingServices ( kIOMasterPortDefault,
										 IOServiceMatching ( deviceClass ),
										 &iterator );
	check ( err == kIOReturnSuccess );
	
	return iterator;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	•	GetFeaturesFlagsForDrive -	Gets the bitfield which represents the
//									features flags.
//—————————————————————————————————————————————————————————————————————————————

static void
GetFeaturesFlagsForDrive ( CFDictionaryRef	dict,
						   UInt32 *			cdFlags,
						   UInt32 *			dvdFlags,
						   Boolean *		lowPowerPoll )
{
	
	CFDictionaryRef			propertiesDict	= 0;
	CFNumberRef				flagsNumberRef	= 0;
	CFBooleanRef			boolRef			= 0;
	
	*cdFlags		= 0;
	*dvdFlags		= 0;
	*lowPowerPoll	= false;
	
	propertiesDict = ( CFDictionaryRef ) CFDictionaryGetValue ( dict, CFSTR ( kIOPropertyDeviceCharacteristicsKey ) );
	require ( ( propertiesDict != 0 ), ErrorExit );
	
	// Get the CD features
	flagsNumberRef = ( CFNumberRef ) CFDictionaryGetValue ( propertiesDict, CFSTR ( kIOPropertySupportedCDFeatures ) );
	if ( flagsNumberRef != 0 )
	{
		
		CFNumberGetValue ( flagsNumberRef, kCFNumberLongType, cdFlags );
		
	}
	
	// Get the DVD features
	flagsNumberRef = ( CFNumberRef ) CFDictionaryGetValue ( propertiesDict, CFSTR ( kIOPropertySupportedDVDFeatures ) );
	if ( flagsNumberRef != 0 )
	{
		
		CFNumberGetValue ( flagsNumberRef, kCFNumberLongType, dvdFlags );
		
	}
	
	boolRef = ( CFBooleanRef ) CFDictionaryGetValue ( propertiesDict, CFSTR ( kAppleLowPowerPollingKey ) );
	if ( boolRef != 0 )
	{
		
		*lowPowerPoll = CFBooleanGetValue ( boolRef );
		
	}
	
	
ErrorExit:
	
	
	return;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	•	PrintFeaturesForEachDevice - Prints CD and DVD features for each device
//									 in the iterator.
//—————————————————————————————————————————————————————————————————————————————

static void
PrintFeaturesForEachDevice ( io_iterator_t iter )
{
	
	UInt32				cdFlags			= 0;
	UInt32				dvdFlags		= 0;
	Boolean				lowPowerPoll	= false;
	io_service_t		service			= MACH_PORT_NULL;
	CFDictionaryRef		properties		= NULL;
	CFStringRef			description 	= NULL;
	
	while ( ( service = IOIteratorNext ( iter ) ) != MACH_PORT_NULL )
	{
		
		properties	= GetRegistryEntryProperties ( service );
		description = GetDriveDescription ( properties );
		
		printf ( "\nDevice: " );
		fflush ( stdout );
		CFShow ( description );
		CFRelease ( description );
		printf ( "----------------------------------\n" );
		
		GetFeaturesFlagsForDrive ( properties, &cdFlags, &dvdFlags, &lowPowerPoll );
		PrintLowPowerPollingSupport ( lowPowerPoll );		
		PrintCDFeatures ( cdFlags );
		PrintDVDFeatures ( dvdFlags );
		
		CFRelease ( properties	);
		
		IOObjectRelease ( service );
		
		printf ( "\n\n" );
		
	}
	
}


//—————————————————————————————————————————————————————————————————————————————
//	•	GetRegistryEntryProperties - Gets the registry entry properties for
//									 an io_service_t.
//—————————————————————————————————————————————————————————————————————————————

static CFMutableDictionaryRef
GetRegistryEntryProperties ( io_service_t service )
{
	
	IOReturn				err		= kIOReturnSuccess;
	CFMutableDictionaryRef	dict	= 0;
	
	err = IORegistryEntryCreateCFProperties (	service,
												&dict,
												kCFAllocatorDefault,
												0 );					
	check ( err == kIOReturnSuccess );
	
	return dict;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	•	GetDriveDescription - Creates a drive description. Caller must call
//							  CFRelease on returned CFStringRef if non-NULL.
//—————————————————————————————————————————————————————————————————————————————

static CFStringRef
GetDriveDescription ( CFDictionaryRef dict )
{
	
	CFMutableStringRef		description = NULL;
	CFDictionaryRef			deviceDict	= NULL;
	CFStringRef				vendor		= NULL;
	CFStringRef				product		= NULL;
	
	require ( ( dict != 0 ), Exit );
	
	deviceDict = ( CFDictionaryRef ) CFDictionaryGetValue ( dict, CFSTR ( kIOPropertyDeviceCharacteristicsKey ) );
	require ( ( deviceDict != 0 ), Exit );
	
	vendor	= ( CFStringRef ) CFDictionaryGetValue ( deviceDict, CFSTR ( kIOPropertyVendorNameKey ) );
	product = ( CFStringRef ) CFDictionaryGetValue ( deviceDict, CFSTR ( kIOPropertyProductNameKey ) );
	
	description = CFStringCreateMutable ( kCFAllocatorDefault, 0 );
	require ( ( description != 0 ), Exit );
	
	CFStringAppend ( description, vendor );
	CFStringAppend ( description, CFSTR ( " " ) );
	CFStringAppend ( description, product );
	
	check ( description );
	
	
Exit:
	
	
	return description;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	•	PrintLowPowerPollingSupport - Prints Low Power Polling support.
//—————————————————————————————————————————————————————————————————————————————

static void
PrintLowPowerPollingSupport ( Boolean lowPowerPoll )
{
		
	if ( lowPowerPoll == true )
	{
		printf ( "Drive supports low power polling.\n" );
	}
	
	else
	{
		printf ( "Drive does NOT support low power polling.\n" );
	}
	
}


//—————————————————————————————————————————————————————————————————————————————
//	•	PrintCDFeatures - Prints CD features based on bits set in field.
//—————————————————————————————————————————————————————————————————————————————

static void
PrintCDFeatures ( UInt32 cdFlags )
{
	
	require ( ( cdFlags != 0 ), Exit );
	
	if ( IsBitSet ( cdFlags, kCDFeaturesAnalogAudioMask ) )
	{
		
		printf ( "Drive supports analog audio\n" );
		
	}
	
	if ( IsBitSet ( cdFlags, kCDFeaturesReadStructuresMask ) )
	{
		
		printf ( "CD-ROM\n" );
		
	}

	if ( IsBitSet ( cdFlags, kCDFeaturesWriteOnceMask ) )
	{
		
		printf ( "CD-R\n" );
		
	}

	if ( IsBitSet ( cdFlags, kCDFeaturesReWriteableMask ) )
	{
		
		printf ( "CD-RW\n" );
		
	}

	if ( IsBitSet ( cdFlags, kCDFeaturesCDDAStreamAccurateMask ) )
	{
		
		printf ( "Drive is CD-DA Stream Accurate\n" );
		
	}

	if ( IsBitSet ( cdFlags, kCDFeaturesPacketWriteMask ) )
	{
		
		printf ( "Drive supports packet writing\n" );
		
	}
		
	if ( IsBitSet ( cdFlags, kCDFeaturesTAOWriteMask ) )
	{
		
		printf ( "Drive supports TAO writing\n" );
		
	}

	if ( IsBitSet ( cdFlags, kCDFeaturesSAOWriteMask ) )
	{
		
		printf ( "Drive supports SAO writing\n" );
		
	}

	if ( IsBitSet ( cdFlags, kCDFeaturesRawWriteMask ) )
	{
		
		printf ( "Drive supports RAW writing\n" );
		
	}

	if ( IsBitSet ( cdFlags, kCDFeaturesTestWriteMask ) )
	{
		
		printf ( "Drive supports Test Write writing\n" );
		
	}

	if ( IsBitSet ( cdFlags, kCDFeaturesBUFWriteMask ) )
	{
		
		printf ( "Drive supports BUF Write writing\n" );
		
	}		
	
	
Exit:
	
	
	return;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	•	PrintDVDFeatures - Prints DVD features based on bits set in field.
//—————————————————————————————————————————————————————————————————————————————

static void
PrintDVDFeatures ( UInt32 dvdFlags )
{
	
	require ( ( dvdFlags != 0 ), Exit );
	
	if ( IsBitSet ( dvdFlags, kDVDFeaturesCSSMask ) )
	{
		
		printf ( "Drive supports CSS\n" );
		
	}
	
	if ( IsBitSet ( dvdFlags, kDVDFeaturesReadStructuresMask ) )
	{
		
		printf ( "DVD-ROM\n" );
		
	}
	
	if ( IsBitSet ( dvdFlags, kDVDFeaturesWriteOnceMask ) )
	{
		
		printf ( "DVD-R\n" );
		
	}
	
	if ( IsBitSet ( dvdFlags, kDVDFeaturesRandomWriteableMask ) )
	{
		
		printf ( "DVD-RAM\n" );
		
	}
	
	if ( IsBitSet ( dvdFlags, kDVDFeaturesReWriteableMask ) )
	{
		
		printf ( "DVD-RW\n" );
		
	}

	if ( IsBitSet ( dvdFlags, kDVDFeaturesTestWriteMask ) )
	{
		
		printf ( "Drive supports Test Write writing\n" );
		
	}

	if ( IsBitSet ( dvdFlags, kDVDFeaturesBUFWriteMask ) )
	{
		
		printf ( "Drive supports BUF Write writing\n" );
		
	}
	
	if ( IsBitSet ( dvdFlags, kDVDFeaturesPlusRMask ) )
	{
		
		printf ( "Drive supports DVD+R\n" );
		
	}
	
	if ( IsBitSet ( dvdFlags, kDVDFeaturesPlusRWMask ) )
	{
		
		printf ( "Drive supports DVD+RW\n" );
		
	}
	
	
Exit:
	
	
	return;
	
}


static __inline__ bool
IsBitSet ( UInt32 flags, UInt32 mask )
{
	
	return ( ( flags & mask ) != 0 );
	
}
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
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <paths.h>
#include <sys/param.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOMediaBSDClient.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOCDMediaBSDClient.h>
#include <IOKit/storage/IOCDMedia.h>
#include <IOKit/storage/IOCDTypes.h>
#include <IOKit/storage/IODVDMediaBSDClient.h>
#include <IOKit/storage/IODVDMedia.h>
#include <IOKit/storage/IODVDTypes.h>
#include <CoreFoundation/CoreFoundation.h>

//—————————————————————————————————————————————————————————————————————————————
//	Macros
//—————————————————————————————————————————————————————————————————————————————

#define DEBUG 0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING "OpticalMediaSpeed"
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

enum
{
	kUnknown			= -1,
	kAllDevices 		= 0,
	kSpecificDevice 	= 1
};


//—————————————————————————————————————————————————————————————————————————————
//	Prototypes
//—————————————————————————————————————————————————————————————————————————————

static IOReturn
ValidateArguments ( int argc, const char * argv[], int * selector );

static void
PrintUsage ( void );

static IOReturn
PrintSpeedForAllOpticalMedia ( void );

static IOReturn
PrintSpeedForDisc ( io_object_t opticalMedia );

static IOReturn
PrintSpeedForBSDNode ( const char * bsdNode );

static int
OpenMedia ( const char * bsdPath );

static void
CloseMedia ( int fileDescriptor );

static UInt16
GetSpeed ( int fileDescriptor );


//—————————————————————————————————————————————————————————————————————————————
//	•	main - Our main entry point
//—————————————————————————————————————————————————————————————————————————————

int
main ( int argc, const char * argv[] )
{
	
	int		result		= 0;
	int		selector	= 0;
	
	result = ValidateArguments ( argc, argv, &selector );
	require_noerr_action ( result, ErrorExit, PrintUsage ( ) );
	
	printf ( "\n" );
	
	switch ( selector )
	{
		
		case kAllDevices:
			result = PrintSpeedForAllOpticalMedia ( );
			require_action ( ( result == kIOReturnSuccess ), ErrorExit, result = -1 );
			break;
			
		case kSpecificDevice:
			result = PrintSpeedForBSDNode ( argv[argc - 1] );
			require_action ( ( result == kIOReturnSuccess ), ErrorExit, result = -1 );
			break;
			
		default:
			break;
			
	}
	
	
ErrorExit:
	
	
	return result;
	
}


//———————————————————————————————————————————————————————————————————————————
//	ValidateArguments - Validates arguments
//———————————————————————————————————————————————————————————————————————————

static IOReturn
ValidateArguments ( int argc, const char * argv[], int * selector )
{
	
	IOReturn	result = kIOReturnError;
	
	require ( ( argc > 1 ), ErrorExit );
	require ( ( argc < 4 ), ErrorExit );
	
	if ( argc == 2 )
	{
		
		require ( ( strcmp ( argv[1], "-a" ) == 0 ), ErrorExit );
		*selector = kAllDevices;
		
	}
	
	else if ( argc == 3 )
	{
		
		require ( ( strcmp ( argv[1], "-d" ) == 0 ), ErrorExit );
		*selector = kSpecificDevice;
		
	}
	
	result = kIOReturnSuccess;
	
	
ErrorExit:
	
	
	return result;
	
}


//———————————————————————————————————————————————————————————————————————————
//	PrintSpeedForAllOpticalMedia - Prints media access speed all CD/DVD Media
//———————————————————————————————————————————————————————————————————————————

static IOReturn
PrintSpeedForAllOpticalMedia ( void )
{
	
	IOReturn		error 			= kIOReturnSuccess;
	io_iterator_t	iter			= MACH_PORT_NULL;
	io_object_t		opticalMedia	= MACH_PORT_NULL;
	bool			foundOne		= false;
	
	error = IOServiceGetMatchingServices ( 	kIOMasterPortDefault,
											IOServiceMatching ( kIOCDMediaClass ),
											&iter );
	
	if ( error == kIOReturnSuccess )
	{
		
		opticalMedia = IOIteratorNext ( iter );
		
		while ( opticalMedia != MACH_PORT_NULL )
		{
			
			error = PrintSpeedForDisc ( opticalMedia );
			IOObjectRelease ( opticalMedia );
			opticalMedia = IOIteratorNext ( iter );
			
			if ( foundOne == false )
				foundOne = true;
			
		}
		
		IOObjectRelease ( iter );
		iter = NULL;
		
	}
	
	error = IOServiceGetMatchingServices ( 	kIOMasterPortDefault,
											IOServiceMatching ( kIODVDMediaClass ),
											&iter );

	if ( error == kIOReturnSuccess )
	{
		
		opticalMedia = IOIteratorNext ( iter );
		
		while ( opticalMedia != MACH_PORT_NULL )
		{
			
			error = PrintSpeedForDisc ( opticalMedia );
			IOObjectRelease ( opticalMedia );
			opticalMedia = IOIteratorNext ( iter );
			
			if ( foundOne == false )
				foundOne = true;
			
		}
		
		IOObjectRelease ( iter );
		iter = NULL;
		
	}
	
	if ( foundOne == false )
	{
		
		printf ( "No optical media found\n" );
		
	}
	
	return error;
	
}


//———————————————————————————————————————————————————————————————————————————
//	PrintSpeedForDisc - Prints speed for a particular optical media
//———————————————————————————————————————————————————————————————————————————

static IOReturn
PrintSpeedForDisc ( io_object_t opticalMedia )
{
	
	IOReturn				error 		= kIOReturnError;
	CFMutableDictionaryRef	properties 	= NULL;
	CFStringRef				bsdNode		= NULL;
	const char *			bsdName		= NULL;
	
	error = IORegistryEntryCreateCFProperties ( opticalMedia,
												&properties,
												kCFAllocatorDefault,
												kNilOptions );
	require ( ( error == kIOReturnSuccess ), ErrorExit );
	
	bsdNode = ( CFStringRef ) CFDictionaryGetValue ( properties, CFSTR ( kIOBSDNameKey ) );
	require ( ( bsdNode != NULL ), ReleaseProperties );
	
	bsdName = CFStringGetCStringPtr ( bsdNode, CFStringGetSystemEncoding ( ) );
	require ( ( bsdName != NULL ), ReleaseProperties );
	
	error = PrintSpeedForBSDNode ( bsdName );
	require ( ( error == kIOReturnSuccess ), ReleaseProperties );
	
	
ReleaseProperties:
	
	
	require_quiet ( ( properties != NULL ), ErrorExit );
	CFRelease ( properties );
	properties = NULL;
	
	
ErrorExit:
	
	
	return error;
	
}


//———————————————————————————————————————————————————————————————————————————
//	PrintSpeedForBSDNode - Prints speed for a particular BSD Node
//———————————————————————————————————————————————————————————————————————————

static IOReturn
PrintSpeedForBSDNode ( const char * bsdNode )
{
	
	IOReturn		error			= kIOReturnError;
	int 			fileDescriptor	= 0;
	UInt16			speed			= 0;
	char 			deviceName[MAXPATHLEN];
	
	if ( strncmp ( bsdNode, "/dev/rdisk", 10 ) == 0 )
	{
		 sprintf ( deviceName, "%s", bsdNode );
	}
	
	else if ( strncmp ( bsdNode, "/dev/disk", 9 ) == 0 )
	{
		sprintf ( deviceName, "%s", bsdNode );
	}
	
	else if ( strncmp ( bsdNode, "disk", 4 ) == 0 )
	{
		sprintf ( deviceName, "%s%s", "/dev/", bsdNode );
	}
	
	else
	{
		goto ErrorExit;
	}
	
	fileDescriptor = OpenMedia ( deviceName );
	require ( ( fileDescriptor > 0 ), ErrorExit );
	
	speed = GetSpeed ( fileDescriptor );
	printf ( "%s: speed = %d KB/s\n", deviceName, speed );
	
	CloseMedia ( fileDescriptor );
	
	
ErrorExit:
	
	
	return error;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	•	OpenMedia - Given the path for a device, open that device
//—————————————————————————————————————————————————————————————————————————————

int
OpenMedia ( const char * bsdPath )
{
	
	int	fileDescriptor;
	
	// This open() call will fail with a permissions error if the sample has been changed to
	// look for non-removable media. This is because device nodes for fixed-media devices are
	// owned by root instead of the current console user.
	fileDescriptor = open ( bsdPath, O_RDONLY );
	if ( fileDescriptor == -1 )
	{
		
		printf ( "Error opening device %s: ", bsdPath );
		
	}
	
	return fileDescriptor;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	•	CloseMedia - Given the file descriptor for a device, close that device
//—————————————————————————————————————————————————————————————————————————————

static void
CloseMedia ( int fileDescriptor )
{
	close ( fileDescriptor );
}


//—————————————————————————————————————————————————————————————————————————————
//	•	GetSpeed - Gets the media access speed of the device in kilobytes/sec
//—————————————————————————————————————————————————————————————————————————————

UInt16
GetSpeed ( int fileDescriptor )
{
	
	UInt16	speed	= 0;
	int		result	= 0;
	
	result = ioctl ( fileDescriptor, DKIOCCDGETSPEED, &speed );
	if ( ( result == -1 ) && ( errno == ENOTTY ) )
	{
		
		result = ioctl ( fileDescriptor, DKIOCDVDGETSPEED, &speed );
		
	}
	
	if ( result != 0 )
	{
		
		perror ( "Error getting speed\n" );
		
	}
	
	return speed;
	
}


//———————————————————————————————————————————————————————————————————————————
//	•	PrintUsage - Prints out usage
//———————————————————————————————————————————————————————————————————————————

void
PrintUsage ( void )
{
	
	printf ( "\n" );
	printf ( "Usage: OpticalMediaSpeed [-a] [-d label]\n" );
	printf ( "\t\t" );
	printf ( "-a Print the media access speed from all available optical media\n" );
	printf ( "\t\t" );
	printf ( "-d Enter a specific bsd-style disk path to use" );
	printf ( "\t\t" );
	printf ( "e.g. /dev/disk0, /dev/rdisk2, disk3" );
	printf ( "\n" );
	
}
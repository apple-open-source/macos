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
#include <sys/mount.h>
#include <sys/param.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOCDTypes.h>
#include <IOKit/storage/IOCDMedia.h>


//—————————————————————————————————————————————————————————————————————————————
//	Macros
//—————————————————————————————————————————————————————————————————————————————

#define DEBUG 0

#define DEBUG_ASSERT_COMPONENT_NAME_STRING "CDTOC"

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

// Enums for trackType (digital data or audio)
enum
{
	kDigitalDataBit	= 2,
	kDigitalDataMask = ( 1 << kDigitalDataBit )
};

#define kAudioTrackString	"Track is Audio"
#define kDataTrackString	"Track is Data"

enum
{
	kUnknown			= -1,
	kAllDevices 		= 0,
	kSpecificDevice 	= 1
};

//—————————————————————————————————————————————————————————————————————————————
//	Structures
//—————————————————————————————————————————————————————————————————————————————


//———————————————————————————————————————————————————————————————————————————
//	SubQTOCInfo - 	Structure which describes the SubQTOCInfo defined in
//					• MMC-2 NCITS T10/1228D SCSI MultiMedia Commands Version 2
//					  rev 9.F April 1, 1999, p. 215				
//					• ATAPI SFF-8020 rev 1.2 Feb 24, 1994, p. 149
//———————————————————————————————————————————————————————————————————————————


struct SubQTOCInfo
{
	
	UInt8		sessionNumber;
#ifdef __LITTLE_ENDIAN__
    UInt8		control:4;
    UInt8		address:4;
#else /* !__LITTLE_ENDIAN__ */
    UInt8		address:4;
    UInt8		control:4;
#endif /* !__LITTLE_ENDIAN__ */
	UInt8		tno;
	UInt8		point;
	UInt8		ATIP[3];
	UInt8		zero;
	union {
		
		struct {
			UInt8		minutes;
			UInt8		seconds;
			UInt8		frames;
		} startPosition;
		
		struct {
			UInt8		firstTrackNum;
			UInt8		discType;
			UInt8		reserved;
		} A0PMSF;
		
		struct {
			UInt8		lastTrackNum;
			UInt8		reserved[2];
		} A1PMSF;
		
		struct {
			UInt8		minutes;
			UInt8		seconds;
			UInt8		frames;
		} leadOutStartPosition;
		
		struct {
			UInt8		skipIntervalPointers;
			UInt8		skipTrackPointers;
			UInt8		reserved;
		} B1;

		struct {
			UInt8		skipNumber[3];
		} B2toB4;

		struct {
			UInt8		minutes;
			UInt8		seconds;
			UInt8		frames;
		} hybridDiscLeadInArea;

	} PMSF;
	
};
typedef struct SubQTOCInfo SubQTOCInfo;


//———————————————————————————————————————————————————————————————————————————
//	Prototypes
//———————————————————————————————————————————————————————————————————————————

static IOReturn
ValidateArguments ( int argc, const char * argv[], int * selector );

static void
PrintUsage ( void );

static UInt8 *
CreateBufferFromCFData ( CFDataRef theData );

static IOReturn
PrintTOCForAllCDMedia ( void );

static IOReturn
PrintTOCForDisc ( io_object_t cdMedia );

static IOReturn
PrintTOCForBSDNode ( const char * bsdNode );

static void
PrintTOCData ( CDTOC * TOCInfo, OptionBits options );


//———————————————————————————————————————————————————————————————————————————
//	Main
//———————————————————————————————————————————————————————————————————————————

int
main ( int argc, const char * argv[] )
{
	
	int		result		= 0;
	int		selector	= 0;
	
	result = ValidateArguments ( argc, argv, &selector );
	require_noerr_action ( result, ErrorExit, PrintUsage ( ) );
	
	switch ( selector )
	{
		
		case kAllDevices:
			result = PrintTOCForAllCDMedia ( );
			require_action ( ( result == kIOReturnSuccess ), ErrorExit, result = -1 );
			break;
			
		case kSpecificDevice:
			result = PrintTOCForBSDNode ( argv[argc - 1] );
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
//	PrintTOCForAllCDMedia - Prints TOC data for all CD Media
//———————————————————————————————————————————————————————————————————————————

static IOReturn
PrintTOCForAllCDMedia ( void )
{
	
	IOReturn		error 	= kIOReturnSuccess;
	io_iterator_t	iter	= MACH_PORT_NULL;
	io_object_t		cdMedia	= MACH_PORT_NULL;
	
	error = IOServiceGetMatchingServices ( 	kIOMasterPortDefault,
											IOServiceMatching ( kIOCDMediaClass ),
											&iter );
	
	require_string ( ( error == kIOReturnSuccess ), ErrorExit, "No CD media found\n" );
	
	cdMedia = IOIteratorNext ( iter );
	if ( cdMedia == MACH_PORT_NULL )
	{
		printf ( "No CD media found\n" );
	}
	
	while ( cdMedia != MACH_PORT_NULL )
	{
		
		error = PrintTOCForDisc ( cdMedia );
		IOObjectRelease ( cdMedia );
		cdMedia = IOIteratorNext ( iter );
		
	}
	
	
	IOObjectRelease ( iter );
	iter = NULL;
	
	
ErrorExit:
	
	
	return error;
	
}


//———————————————————————————————————————————————————————————————————————————
//	PrintTOCForDisc - Prints TOC data for a particular CD Media
//———————————————————————————————————————————————————————————————————————————

static IOReturn
PrintTOCForDisc ( io_object_t cdMedia )
{
	
	IOReturn				error 		= kIOReturnSuccess;
	CFMutableDictionaryRef	properties 	= 0;
	UInt8 *					TOCInfo		= NULL;
	CFDataRef     			data     	= NULL;
	
	error = IORegistryEntryCreateCFProperties ( cdMedia,
												&properties,
												kCFAllocatorDefault,
												kNilOptions );
	
	require ( ( error == kIOReturnSuccess ), ErrorExit );
	

	// Get the TOCInfo
	data = ( CFDataRef ) CFDictionaryGetValue ( properties, 
												CFSTR ( kIOCDMediaTOCKey ) );
	require ( ( data != NULL ), ReleaseProperties );
	
	TOCInfo = CreateBufferFromCFData ( data );
	require ( ( TOCInfo != NULL ), ReleaseProperties );
	
	// Print the TOC
	PrintTOCData ( ( CDTOC * ) TOCInfo, 0 );
	
	// free the memory
	free ( ( char * ) TOCInfo );
	
	
ReleaseProperties:
	
	
	require_quiet ( ( properties != NULL ), ErrorExit );
	CFRelease ( properties );
	properties = NULL;
	
	
ErrorExit:
	
	
	return error;
		
}


//———————————————————————————————————————————————————————————————————————————
//	PrintTOCForBSDNode - Prints TOC data for a particular BSD Node
//———————————————————————————————————————————————————————————————————————————

static IOReturn
PrintTOCForBSDNode ( const char * bsdNode )
{

	IOReturn		error	= kIOReturnError;
	io_object_t		object	= MACH_PORT_NULL;
	char *			bsdName = NULL;
	char 			deviceName[MAXPATHLEN];
	
	sprintf ( deviceName, "%s", bsdNode );
	
	if ( !strncmp ( deviceName, "/dev/r", 6 ) )
	{
		
		// Strip off the /dev/r from /dev/rdiskX
		bsdName = &deviceName[6];
		
	}
	
	else if ( !strncmp ( deviceName, "/dev/", 5 ) )
	{
		
		// Strip off the /dev/r from /dev/rdiskX
		bsdName = &deviceName[5];
		
	}
	
	else
	{
		
		bsdName = deviceName;
		
	}
	
	require_action ( ( strncmp ( bsdName, "disk", 4 ) == 0 ), ErrorExit, PrintUsage ( ) );
	
	object = IOServiceGetMatchingService ( 	kIOMasterPortDefault,
											IOBSDNameMatching ( kIOMasterPortDefault, 0, bsdName ) );
	
	if ( object == MACH_PORT_NULL )
	{
		printf ( "No device node found at %s\n", bsdNode );
	}
	require ( ( object != MACH_PORT_NULL ), ErrorExit );
	
	if ( IOObjectConformsTo ( object, kIOCDMediaClass ) == false )
	{
		printf ( "Device node found at %s is not CD media\n", bsdNode );
	}
	require ( IOObjectConformsTo ( object, kIOCDMediaClass ), ReleaseObject );
	
	PrintTOCForDisc ( object );
	
	
ReleaseObject:
	
	
	IOObjectRelease ( object );
	object = NULL;
	
	
ErrorExit:
	
	
	return error;
	
}


//———————————————————————————————————————————————————————————————————————————
//	CreateBufferFromCFData - Allocates memory for a C-string and copies
//							 the contents of the CFString to it.
//
//	NB:	The calling function should dispose of the memory
//———————————————————————————————————————————————————————————————————————————

UInt8 *
CreateBufferFromCFData ( CFDataRef theData )
{

	CFRange				range;
	CFIndex          	bufferLength 	= CFDataGetLength ( theData ) + 1;
	UInt8 *           	buffer			= ( UInt8 * ) malloc ( bufferLength );
	
	range = CFRangeMake ( 0, bufferLength );
	
	if ( buffer != NULL )
		CFDataGetBytes ( theData, range, buffer );
		
	return buffer;

}


//———————————————————————————————————————————————————————————————————————————
//	PrintTOCData - Prints out the TOC Data
//———————————————————————————————————————————————————————————————————————————

void
PrintTOCData ( CDTOC * TOCInfo, OptionBits options )
{
	
	SubQTOCInfo *			trackDescriptorPtr	= NULL;
	UInt16					numberOfDescriptors = 0;
	
	if ( TOCInfo != NULL )
	{
		
		numberOfDescriptors = CDTOCGetDescriptorCount ( TOCInfo );
		
		printf ( "//---------------------------------------\n" );
		printf ( "//CDTOC info\n" );
		printf ( "//---------------------------------------\n\n" );

		printf ( "Number of descriptors\t= %d\n", numberOfDescriptors );
		
		if ( numberOfDescriptors <= 0 )
		{
			
			printf ( "No tracks on this CD...\n" );
			
		}
		
		printf ( "length\t\t\t= %d\n",		OSSwapBigToHostInt16 ( TOCInfo->length ) );
		printf ( "sessionFirst\t\t= %d\n",	TOCInfo->sessionFirst );
		printf ( "sessionLast\t\t= %d\n\n",	TOCInfo->sessionLast );
		
		trackDescriptorPtr = ( SubQTOCInfo * ) TOCInfo->descriptors;
		
		while ( numberOfDescriptors > 0 )
		{
			
			printf ( "//---------------------------------------\n" );
			if ( trackDescriptorPtr->point < 100 )
			{
				
				printf ( "// SubQTOCInfo - Track %d\n", trackDescriptorPtr->point );
			
			}
			
			else
			{	
				
				printf ( "// SubQTOCInfo - Track %x\n", trackDescriptorPtr->point );
			
			}
			
			printf ( "//---------------------------------------\n\n" );

			printf ( "sessionNumber\t\t= %d\n", 	trackDescriptorPtr->sessionNumber );
			printf ( "address\t\t\t= %d\n", 		trackDescriptorPtr->address );
			printf ( "control\t\t\t= %d\t", 		trackDescriptorPtr->control );
			printf ( "%s\n", 						( trackDescriptorPtr->control & kDigitalDataMask || ( trackDescriptorPtr->point < 1 || trackDescriptorPtr->point > 99 ) ) ? kDataTrackString : kAudioTrackString );
			printf ( "tno\t\t\t= %d\n", 			trackDescriptorPtr->tno );
			printf ( "ATIP[0]\t\t\t= %d\n", 		trackDescriptorPtr->ATIP[0] );
			printf ( "ATIP[1]\t\t\t= %d\n", 		trackDescriptorPtr->ATIP[1] );
			printf ( "ATIP[2]\t\t\t= %d\n", 		trackDescriptorPtr->ATIP[2] );
			printf ( "zero\t\t\t= %d\n", 			trackDescriptorPtr->zero );
			
			switch ( trackDescriptorPtr->point )
			{
				
				case 0xA0:
					printf ( "firstTrackNum\t\t= %d\n", 	trackDescriptorPtr->PMSF.A0PMSF.firstTrackNum );
					printf ( "discType\t\t= %d\t", 			trackDescriptorPtr->PMSF.A0PMSF.discType );
					switch ( trackDescriptorPtr->PMSF.A0PMSF.discType )
					{
						
						case 0:
							printf ( "(CD-DA or CD-ROM with first track in Mode 1)\n" );
							break;
						
						case 16:
							printf ( "(CD-I disc)\n" );
							break;
						
						case 32:
							printf ( "(CD-ROM XA disc with first track in Mode 2)\n" );
							break;
						
					}
					
					printf ( "reserved\t\t= %d\n", 		trackDescriptorPtr->PMSF.A0PMSF.reserved );
					break;

				case 0xA1:
					printf ( "lastTrackNum\t\t= %d\n", 	trackDescriptorPtr->PMSF.A1PMSF.lastTrackNum );
					printf ( "reserved[0]\t\t= %d\n", 	trackDescriptorPtr->PMSF.A1PMSF.reserved[0] );
					printf ( "reserved[1]\t\t= %d\n", 	trackDescriptorPtr->PMSF.A1PMSF.reserved[1] );
					break;

				case 0xA2:
					printf ( "\nLead-Out Area\n" );
					printf ( "minutes\t= %d\n", 	trackDescriptorPtr->PMSF.leadOutStartPosition.minutes );
					printf ( "seconds\t= %d\n", 	trackDescriptorPtr->PMSF.leadOutStartPosition.seconds );
					printf ( "frames\t= %d\n", 		trackDescriptorPtr->PMSF.leadOutStartPosition.frames );
					break;

				case 0xB0:
					printf ( "minutes\t= %d\n", 	trackDescriptorPtr->PMSF.leadOutStartPosition.minutes );
					printf ( "seconds\t= %d\n", 	trackDescriptorPtr->PMSF.leadOutStartPosition.seconds );
					printf ( "frames\t= %d\n", 		trackDescriptorPtr->PMSF.leadOutStartPosition.frames );
					break;

				case 0xB1:
					printf ( "skipIntervalPointers = %d\n", 	trackDescriptorPtr->PMSF.B1.skipIntervalPointers );
					printf ( "skipTrackPointers = %d\n", 		trackDescriptorPtr->PMSF.B1.skipTrackPointers );
					printf ( "reserved = %d\n", 				trackDescriptorPtr->PMSF.B1.reserved );
					break;

				case 0xB2:
				case 0xB3:
				case 0xB4:
					printf ( "skipnumber[0] = %d\n", 	trackDescriptorPtr->PMSF.B2toB4.skipNumber[0] );
					printf ( "skipnumber[1] = %d\n", 	trackDescriptorPtr->PMSF.B2toB4.skipNumber[1] );
					printf ( "skipnumber[2] = %d\n", 	trackDescriptorPtr->PMSF.B2toB4.skipNumber[2] );
					break;
				
				case 0xC0:
					printf ( "minutes = %d\n", 	trackDescriptorPtr->PMSF.hybridDiscLeadInArea.minutes );
					printf ( "seconds = %d\n", 	trackDescriptorPtr->PMSF.hybridDiscLeadInArea.seconds );
					printf ( "frames = %d\n", 	trackDescriptorPtr->PMSF.hybridDiscLeadInArea.frames );
					break;

				default:
					// Must be a number
					if ( trackDescriptorPtr->address == 1 )
					{
					
						printf ( "minutes\t\t\t= %d\n", 	trackDescriptorPtr->PMSF.startPosition.minutes );
						printf ( "seconds\t\t\t= %d\n", 	trackDescriptorPtr->PMSF.startPosition.seconds );
						printf ( "frames\t\t\t= %d\n", 		trackDescriptorPtr->PMSF.startPosition.frames );
					
					}
					
					else if ( trackDescriptorPtr->address == 5 )
					{
						
						printf ( "This is a weird address number, must be a skip interval..." );
						printf ( "minutes = %d\n", 	trackDescriptorPtr->PMSF.startPosition.minutes );
						printf ( "seconds = %d\n", 	trackDescriptorPtr->PMSF.startPosition.seconds );
						printf ( "frames = %d\n", 	trackDescriptorPtr->PMSF.startPosition.frames );
					
					}
					
					break;
			
			}
						
			trackDescriptorPtr++;
			numberOfDescriptors--;
			
			printf ( "\n" );
			
		}
		
	}
			
}


//———————————————————————————————————————————————————————————————————————————
//	PrintUsage - Prints out usage
//———————————————————————————————————————————————————————————————————————————

void
PrintUsage ( void )
{
	
	printf ( "\n" );
	printf ( "Usage: CDTOC [-a] [-d label]\n" );
	printf ( "\t\t" );
	printf ( "-a Print the TOC Data from all available CDs\n" );
	printf ( "\t\t" );
	printf ( "-d Enter a specific bsd-style disk path to use" );
	printf ( "\t\t" );
	printf ( "e.g. /dev/disk0, /dev/rdisk2, disk3" );
	printf ( "\n" );
	
}
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

#include <mach/mach.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include <IOKit/scsi/IOSCSIMultimediaCommandsDevice.h>
#include <CoreFoundation/CoreFoundation.h>

//—————————————————————————————————————————————————————————————————————————————
//	Macros
//—————————————————————————————————————————————————————————————————————————————

#define DEBUG 0

#define DEBUG_ASSERT_COMPONENT_NAME_STRING "OpticalMessageListenerTool"

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

#define kIOCDBlockStorageDeviceString		"IOCDBlockStorageDevice"


//—————————————————————————————————————————————————————————————————————————————
//	Prototypes
//—————————————————————————————————————————————————————————————————————————————

static void
ServiceMatched ( void * refcon, io_iterator_t iterator );

static void
ServiceInterest ( void * refcon, io_service_t service, natural_t type, void * arg );


//—————————————————————————————————————————————————————————————————————————————
//	Globals
//—————————————————————————————————————————————————————————————————————————————

static IONotificationPortRef	gNotifyPort = NULL;


//—————————————————————————————————————————————————————————————————————————————
//	•	main - Our main entry point
//—————————————————————————————————————————————————————————————————————————————

int
main ( int argc, const char * argv[] )
{
	
	io_iterator_t	iterator	= MACH_PORT_NULL;
	
	// Create the notification port on which to receive notifications
	gNotifyPort = IONotificationPortCreate ( kIOMasterPortDefault );
	
	// Add it to the runloop
	CFRunLoopAddSource ( CFRunLoopGetCurrent ( ),
						 IONotificationPortGetRunLoopSource ( gNotifyPort ), 
						 kCFRunLoopCommonModes );
	
	// Add matching notifications for optical devices
	IOServiceAddMatchingNotification ( gNotifyPort,
									   kIOFirstMatchNotification,
									   IOServiceMatching ( kIOCDBlockStorageDeviceString ),
									   &ServiceMatched,
									   NULL,
									   &iterator );
	
	// The iterator might have objects here. Clear it out.
	ServiceMatched ( NULL, iterator );
	
	printf ( "OpticalMessageListener\n" );
	printf ( "Copyright 2002-2003, Apple Computer, Inc.\n\n" );
	fflush ( stdout );
	
	// Run the runloop. We should not return from this call until someone
	// quits the app.
	CFRunLoopRun ( );
	
	// Get rid of the iterator.
	if ( iterator != 0 )
	{
		
		IOObjectRelease ( iterator );
		iterator = 0;
		
	}
	
	// Remove the runloop source
	CFRunLoopRemoveSource ( CFRunLoopGetCurrent ( ),
							IONotificationPortGetRunLoopSource ( gNotifyPort ), 
							kCFRunLoopCommonModes );
	
	if ( gNotifyPort != 0 )
	{
		
		// Destroy the notification port
		IONotificationPortDestroy ( gNotifyPort );
		gNotifyPort = 0;
		
	}
	
	return 0;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	•	ServiceMatched - Callback for matching notifications
//—————————————————————————————————————————————————————————————————————————————

void
ServiceMatched ( void * refcon, io_iterator_t iterator )
{
	
	io_service_t	service = MACH_PORT_NULL;
	io_iterator_t	ignored = MACH_PORT_NULL;
		
	while ( ( service = IOIteratorNext ( iterator ) ) != MACH_PORT_NULL )
	{
		
		// Attach a general interest notification to this object
		IOServiceAddInterestNotification ( gNotifyPort,
										   service,
										   kIOGeneralInterest,
										   ServiceInterest,
										   NULL,
										   &ignored );
		
	}
	
}


//—————————————————————————————————————————————————————————————————————————————
//	•	ServiceInterest - Callback for general interest notifications
//—————————————————————————————————————————————————————————————————————————————

void
ServiceInterest ( void * refcon, io_service_t service, natural_t type, void * arg )
{
	
	printf ( "type = %08lX, arg = %08lX\n", ( UInt32 ) type, ( UInt32 ) arg );
	
	switch ( type )
	{
		
		case kIOMessageTrayStateChange:
		{	
			
			printf ( "Tray state change " );
			if ( ( UInt32 ) arg == kMessageTrayStateChangeRequestAccepted )
			{
				printf ( "accepted.\n" );
			}
			
			else if ( ( UInt32 ) arg == kMessageTrayStateChangeRequestRejected )
			{
				printf ( "rejected - exclusive user client has control.\n" );
			}
			
		}
		break;
		
		case kIOMessageMediaAccessChange:
		{	
			
			printf ( "Media access change " );
			if ( ( UInt32 ) arg == kMessageDeterminingMediaPresence )
			{
				printf ( "determining media presence.\n" );
			}
			
			else if ( ( UInt32 ) arg == kMessageFoundMedia )
			{
				printf ( "media found in drive.\n" );
			}
			
			else if ( ( UInt32 ) arg == kMessageFoundMedia )
			{
				printf ( "media type determined.\n" );
			}
			
		}
		break;
		
		case kIOMessageServiceIsTerminated:
		{	
			
			printf ( "Service terminated\n" );
			
		}
		break;

		case kIOMessageServiceIsSuspended:
		{	
			
			printf ( "Service suspended\n" );
			
		}
		break;

		case kIOMessageServiceIsResumed:
		{	
			
			printf ( "Service resumed\n" );
			
		}
		break;

		case kIOMessageServiceIsRequestingClose:
		{	
			
			printf ( "Requesting close\n" );
			
		}
		break;

		case kIOMessageServiceIsAttemptingOpen:
		{	
			
			printf ( "Attempting open\n" );
			
		}
		break;

		case kIOMessageServiceWasClosed:
		{	
			
			printf ( "Service closed\n" );
			
		}
		break;

		case kIOMessageServiceBusyStateChange:
		{	
			
			printf ( "Busy state change\n" );
			
		}
		break;
		
		default:
			break;
		
	}
	
	printf ( "\n" );
	fflush ( stdout );
	
}
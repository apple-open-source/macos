/*
 * © Copyright 2001-2003 Apple Computer, Inc. All rights reserved.
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

#import "MyDocument.h"
#import "AuthoringDeviceTester.h"
#import "AuthoringDevice.h"

#import <sys/mount.h>
#import <libkern/OSByteOrder.h>
#import <IOKit/IOBSD.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/storage/IOMedia.h>
#import <IOKit/scsi/SCSICommandOperationCodes.h>
#import <IOKit/scsi/SCSITask.h>
#import <Carbon/Carbon.h>


//—————————————————————————————————————————————————————————————————————————————
//	Constants
//—————————————————————————————————————————————————————————————————————————————

enum
{
	kUnmountFailed		= -2,
	kUnmountCancelled 	= -1,
	kStatusOK			= 0
};

#define	kNumBlocks					(1)
#define	kRequestSize				(2048 * kNumBlocks)
#define	kMyDocumentString			@"MyDocument"
#define kAlertString				@"Alert"
#define kOKString					@"OK"
#define kCancelString				@"Cancel"
#define IOBlockStorageDriverString	"IOBlockStorageDriver"


//—————————————————————————————————————————————————————————————————————————————
//	Macros
//—————————————————————————————————————————————————————————————————————————————

#define	DEBUG									0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING		"MyDocument"
#include <AssertMacros.h>


@implementation MyDocument


//—————————————————————————————————————————————————————————————————————————————
//	+myDocument - factory method
//—————————————————————————————————————————————————————————————————————————————

+ ( MyDocument * ) myDocument
{
	return [ [ [ self alloc ] init ] autorelease ];
}


//—————————————————————————————————————————————————————————————————————————————
// init
//—————————————————————————————————————————————————————————————————————————————

- ( id ) init
{
	
	[ super init ];
	[ NSBundle loadNibNamed : [ self windowNibName] owner : self ];

	return self;
	
}


//—————————————————————————————————————————————————————————————————————————————
// dealloc
//—————————————————————————————————————————————————————————————————————————————

- ( void ) dealloc
{
	
	[ self setTheAuthoringDeviceTester: nil ];
	[ super dealloc ];
	
}


#if 0
#pragma mark -
#pragma mark Accessor Methods
#pragma mark -
#endif


- ( NSWindow * ) theWindow { return theWindow; }
- ( AuthoringDeviceTester * ) theAuthoringDeviceTester { return theAuthoringDeviceTester; }
- ( void ) setTheAuthoringDeviceTester: ( AuthoringDeviceTester * ) value
{

	[ value retain ];
	[ theAuthoringDeviceTester release ];
	theAuthoringDeviceTester = value;

}

- ( AuthoringDevice * ) theAuthoringDevice { return theAuthoringDevice; }
- ( void ) setTheAuthoringDevice: ( AuthoringDevice * ) value
{
	
	NSMutableString *	titleString = [ NSMutableString stringWithCapacity: 0 ];
	
	[ value retain ];
	[ theAuthoringDevice release ];
	theAuthoringDevice = value;
	
	[ self setTheAuthoringDeviceTester: [ AuthoringDeviceTester authoringDeviceTesterForDevice: value withParentDoc: self ] ];

	[ testButton setTarget: [ self theAuthoringDeviceTester ] ];
	
	[ titleString setString: [ theAuthoringDevice theVendorName ] ];
	[ titleString appendString: @" " ];
	[ titleString appendString: [ theAuthoringDevice theProductName ] ];
	[ [ self theWindow ] setTitle: titleString ];
	
}


#if 0
#pragma mark -
#pragma mark Action Methods
#pragma mark -
#endif


//—————————————————————————————————————————————————————————————————————————————
//	switchTest:
//—————————————————————————————————————————————————————————————————————————————

- ( IBAction ) switchTest: ( id ) sender
{
	
	NSString	* testString = [ sender titleOfSelectedItem ];
	
	// Calls the function "testBlah" where Blah represents the
	// text of the popup menu (e.g. testModeSense10). The outlet
	// for the testButton is set to be the AuthoringDeviceTester 
	[ testButton setAction: NSSelectorFromString ( [ NSString stringWithFormat:@"%@%@", @"test", testString ] ) ];
	
}


//—————————————————————————————————————————————————————————————————————————————
//	isExclusiveAccessAvailable:
//—————————————————————————————————————————————————————————————————————————————

- ( IBAction ) isExclusiveAccessAvailable: ( id ) sender
{
	
	MMCDeviceInterface **		theInterface	= NULL;
	SCSITaskDeviceInterface **	devInterface	= NULL;
	Boolean						status			= false;
	
	theInterface = [ [ self theAuthoringDeviceTester ] interface ];
	require ( ( theInterface != NULL ), ErrorExit );
	
	devInterface = ( *theInterface )->GetSCSITaskDeviceInterface ( theInterface );
	require ( ( devInterface != NULL ), ErrorExit );
	
	// Ok. We have the SCSITaskDeviceInterface, let's see if
	// exclusive access is available.
	status = ( *devInterface )->IsExclusiveAccessAvailable ( devInterface );
	if ( status == true )
	{
		[ self appendLogText: @"Exclusive Access Is Available.\n\n" ];
	}
	
	else
	{
		[ self appendLogText: @"Exclusive Access Is Not Available.\n\n" ];
	}
	
	( *devInterface )->Release ( devInterface );
	
	
ErrorExit:
	
	
	return;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	getExclusiveAccess:
//—————————————————————————————————————————————————————————————————————————————

- ( IBAction ) getExclusiveAccess: ( id ) sender
{
	
	MMCDeviceInterface **	theInterface	= NULL;
	int						result			= 0;
	
	// Try to unmount the media if there is any.
	result = [ self unmountMedia ];
	
	// If the dialog was cancelled just return.
	if ( result == kUnmountCancelled )
	{
		return;
	}
	
	else if ( result == kUnmountFailed )
	{
		[ self appendLogText: @"*********** ERROR Unable to unmount media ***********\n\n" ];
	}
	
	theInterface = [ [ self theAuthoringDeviceTester ] interface ];
	
	// Else, we can proceed because no media was present. This allows us to just do our test.
	if ( theInterface != NULL )
	{
		
		SCSITaskDeviceInterface **	devInterface = NULL;
		
		devInterface = ( *theInterface )->GetSCSITaskDeviceInterface ( theInterface );
		if ( devInterface != NULL )
		{
			
			// Ok. We have the SCSITaskDeviceInterface, let's use it to get exclusive access
			// and run our test suite.
			[ self runExclusiveTestSuite: devInterface ];
			
			( *devInterface )->Release ( devInterface );
			
		}
		
	}
	
}


//—————————————————————————————————————————————————————————————————————————————
//	clearLog:
//—————————————————————————————————————————————————————————————————————————————

- ( IBAction ) clearLog: ( id ) sender
{
	
	if ( testOutputTextView )
	{
		
		NSRange	range = NSMakeRange ( 0, [ [ testOutputTextView string ] length ] );
		
		// Replace the range with an empty string
		[ testOutputTextView replaceCharactersInRange: range withString: @"" ];
		
	}
	
}


//—————————————————————————————————————————————————————————————————————————————
//	unmountMedia
//—————————————————————————————————————————————————————————————————————————————

- ( int ) unmountMedia
{
	
	io_service_t	service 	= MACH_PORT_NULL;
	int				result		= kStatusOK;
	
	// Get a handle to the io_service_t that represents our MMC Device
	service = [ [ self theAuthoringDeviceTester ] getServiceObject: [ self theAuthoringDevice ] ];
	require ( ( service != MACH_PORT_NULL ), ErrorExit );
	
	result = [ self findWholeMediaBSDName: service ];
	( void ) IOObjectRelease ( service );
	
	
ErrorExit:
	
	
	return result;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	findWholeMediaBSDName
//—————————————————————————————————————————————————————————————————————————————

- ( int ) findWholeMediaBSDName : ( io_service_t ) service
{
	
	CFStringRef				bsdName	= NULL;
	int						result	= kUnmountFailed;
	
	// Get the CF Properties for the IOMedia object
	bsdName = ( CFStringRef ) IORegistryEntrySearchCFProperty ( service,
																kIOServicePlane,
																CFSTR ( kIOBSDNameKey ),
																kCFAllocatorDefault,
																kIORegistryIterateRecursively );
	require ( ( bsdName != NULL ), ErrorExit );
	result = [ self unmountAllPartitions: bsdName ];
	
	CFRelease ( bsdName );
	
	
ErrorExit:
	
	
	return result;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	unmountAllPartitions
//—————————————————————————————————————————————————————————————————————————————

- ( int ) unmountAllPartitions: ( CFStringRef ) bsdName
{
	
	OSStatus			error		= noErr;
	struct statfs *		fsStats		= NULL;
	int					result		= kUnmountFailed;
	int					numMounts	= 0;
	unsigned int		index		= 0;
	FSRef				volFSRef	= { { 0 } };
	FSCatalogInfo		volumeInfo	= { 0 };
	
	// Get the mount info for all the mountpoints. We will traverse that data
	// and try and find any mountpoints which have the diskX identifier in them.
	numMounts = getmntinfo ( &fsStats, MNT_NOWAIT );
	require ( ( numMounts != 0 ), ErrorExit );
	
	for ( index = 0; index < numMounts; index++ )
	{
		
		// make a CFStringRef with the name of the BSD dev node/file descriptor
		CFStringRef cfStringMntOnName  = CFStringCreateWithCString (	NULL,
																		&fsStats[index].f_mntfromname[5] /* Starting at [5] gets rid of the /dev/ */,
																		CFStringGetSystemEncoding ( ) );
		
		// Compare the BSD dev node to the one we have. This is akin to doing a strncmp.
		if ( CFStringCompareWithOptions ( cfStringMntOnName,
										  bsdName,
										  CFRangeMake ( 0, CFStringGetLength ( bsdName ) ),
										  0 ) == kCFCompareEqualTo )
		{
			
			// Pop up a simple alert panel to warn the user that the disc will be unmounted
			result = NSRunAlertPanel ( kAlertString,
									   [ NSString stringWithFormat: @"Are you sure you want to unmount the disc: %s?", &fsStats[index].f_mntonname[9] ],
									   kOKString,
									   kCancelString,
									   nil );
			
			if ( result == NSAlertAlternateReturn )
			{
				
				// User cancelled unmount.
				result = kUnmountCancelled;
				CFRelease ( cfStringMntOnName );
				cfStringMntOnName = NULL;
				goto ErrorExit;
				
			}
			
			// Ask Carbon to unmount the partition.
			error = FSPathMakeRef ( fsStats[index].f_mntonname,
									&volFSRef,
									NULL );
			if ( error != noErr )
			{				
				
				result = kUnmountFailed;
				CFRelease ( cfStringMntOnName );
				cfStringMntOnName = NULL;
				goto ErrorExit;
				
			}
			
			error = FSGetCatalogInfo (	&volFSRef,
										kFSCatInfoVolume,
										&volumeInfo,
										NULL,
										NULL,
										NULL );
			if ( error != noErr )
			{
				
				result = kUnmountFailed;
				CFRelease ( cfStringMntOnName );
				cfStringMntOnName = NULL;
				goto ErrorExit;
				
			}
			
			error = FSUnmountVolumeSync ( volumeInfo.volume, 0, NULL );
			if ( error != noErr )
			{
				
				result = kUnmountFailed;
				CFRelease ( cfStringMntOnName );
				cfStringMntOnName = NULL;
				goto ErrorExit;
				
			}
			
		}
		
		CFRelease ( cfStringMntOnName );
		cfStringMntOnName = NULL;
		
	}
	
	result = kStatusOK;
	
	
ErrorExit:
	
	
	return result;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	runExclusiveTestSuite
//—————————————————————————————————————————————————————————————————————————————

- ( void ) runExclusiveTestSuite: ( SCSITaskDeviceInterface ** ) interface
{
	
	SCSIServiceResponse				serviceResponse	= kSCSIServiceResponse_Request_In_Process;
	SCSITaskStatus					taskStatus		= kSCSITaskStatus_No_Status;
	SCSI_Sense_Data					senseData		= { 0 };
	SCSICommandDescriptorBlock		cdb				= { 0 };
	SCSITaskInterface **			task			= NULL;
	IOReturn						err	 			= kIOReturnSuccess;
	UInt64							transferCount 	= 0;
	UInt32							transferCountHi = 0;
	UInt32							transferCountLo = 0;				
	UInt8 *							buffer			= NULL;
	IOVirtualRange					range;
	
	[ self appendLogText: @"runExclusiveTestSuite\n\n" ];
	
	// Get exclusive access to the device
	err = ( *interface )->ObtainExclusiveAccess ( interface );
	require ( ( err == kIOReturnSuccess ), ErrorExit );
	
	// Create a task now that we have exclusive access
	task = ( *interface )->CreateSCSITask ( interface );
	require ( ( task != NULL ), ReleaseExclusiveAccess );
	
	// Allocate a buffer the size of our request
	buffer = ( UInt8 * ) calloc ( 1, kRequestSize );
	require ( ( buffer != NULL ), ReleaseTask );
	
	// Set up the range.
	range.address = ( IOVirtualAddress ) buffer;
	range.length  = kRequestSize;
	
	// We're going to execute a READ_10 to the device as a
	// test of exclusive commands.
	cdb[0] = kSCSICmd_READ_10;
	
	// Set the block to be the block zero
	OSWriteBigInt32 ( &cdb[2], 0, 0 );
	
	// Tell the drive we only want this many blocks
	OSWriteBigInt16 ( &cdb[7], 0, kNumBlocks );
	
	// Set the actual cdb in the task
	err = ( *task )->SetCommandDescriptorBlock ( task, cdb, kSCSICDBSize_10Byte );
	require ( ( err == kIOReturnSuccess ), ReleaseBuffer );
	
	// Set the scatter-gather entry in the task
	err = ( *task )->SetScatterGatherEntries ( task,
											   &range,
											   1,
											   kRequestSize,
											   kSCSIDataTransfer_FromTargetToInitiator );
	require ( ( err == kIOReturnSuccess ), ReleaseBuffer );
	
	// Set the timeout in the task
	err = ( *task )->SetTimeoutDuration ( task, 15000 );
	require ( ( err == kIOReturnSuccess ), ReleaseBuffer );
	
	[ self appendLogText: @"Requesting read from block zero\n\n" ];
	
	// Send it!
	err = ( *task )->ExecuteTaskSync ( task, &senseData, &taskStatus, &transferCount );
	require ( ( err == kIOReturnSuccess ), ReleaseBuffer );
	
	// Get the SCSI service response
	err = ( *task )->GetSCSIServiceResponse ( task, &serviceResponse );
	require ( ( err == kIOReturnSuccess ), ReleaseBuffer );
	
	// Get the transfer counts
	transferCountHi = ( ( transferCount >> 32 ) & 0xFFFFFFFF );
	transferCountLo = ( transferCount & 0xFFFFFFFF );
	
	[ self appendLogText: [ NSString stringWithFormat:
		@"serviceResponse = %d (%@), taskStatus = %d (%@)\n",
		serviceResponse,
		[ [ self theAuthoringDeviceTester ] stringFromServiceResponse: serviceResponse ],
		taskStatus,
		[ [ self theAuthoringDeviceTester ] stringFromTaskStatus: taskStatus ] ] ];
	
	require ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ), ReleaseBuffer );
	
	if ( taskStatus == kSCSITaskStatus_GOOD )
	{
		
		int		index = 0;
		
		[ self appendLogText: [ NSString stringWithFormat:
			@"transferCountHi = 0x%08lx, transferCountLo = 0x%08lx\n\n", transferCountHi, transferCountLo ] ];
		[ self appendLogText: @"Data from read:\n\n" ];
		
		for ( index = 0; index < kRequestSize; index += 8 )
		{
			
			// Print hex values
			[ self appendLogText: [ NSString stringWithFormat: @"0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x \t\t\t",
									buffer[0+index], buffer[1+index], buffer[2+index], buffer[3+index],
									buffer[4+index], buffer[5+index], buffer[6+index], buffer[7+index] ] ];
			
			// Print ascii values
			[ self appendLogText: [ NSString stringWithFormat: @"%c%c%c%c%c%c%c%c\n",
									( ( ( buffer[0+index] > 0x7F ) || ( buffer[0+index] == 0x0a ) )  ? '.' : buffer[0+index] ),
									( ( ( buffer[1+index] > 0x7F ) || ( buffer[1+index] == 0x0a ) )  ? '.' : buffer[1+index] ),
									( ( ( buffer[2+index] > 0x7F ) || ( buffer[2+index] == 0x0a ) )  ? '.' : buffer[2+index] ),
									( ( ( buffer[3+index] > 0x7F ) || ( buffer[3+index] == 0x0a ) )  ? '.' : buffer[3+index] ),
									( ( ( buffer[4+index] > 0x7F ) || ( buffer[4+index] == 0x0a ) )  ? '.' : buffer[4+index] ),
									( ( ( buffer[5+index] > 0x7F ) || ( buffer[5+index] == 0x0a ) )  ? '.' : buffer[5+index] ),
									( ( ( buffer[6+index] > 0x7F ) || ( buffer[6+index] == 0x0a ) )  ? '.' : buffer[6+index] ),
									( ( ( buffer[7+index] > 0x7F ) || ( buffer[7+index] == 0x0a ) )  ? '.' : buffer[7+index] ) ] ];
			
		}
		
	}
	
	else if ( taskStatus == kSCSITaskStatus_CHECK_CONDITION )
	{
		[ [ self theAuthoringDeviceTester ] printSenseString: &senseData addRawValues: true ];
	}
	
	
ReleaseBuffer:
	
	
	require_quiet ( ( buffer != NULL ), ReleaseTask );
	free ( buffer );
	buffer = NULL;
	
	
ReleaseTask:
	
	
	require_quiet ( ( task != NULL ), ReleaseExclusiveAccess );
	( *task )->Release ( task );
	task = NULL;
	
	
ReleaseExclusiveAccess:
	
	
	err = ( *interface )->ReleaseExclusiveAccess ( interface );
	require ( ( err == kIOReturnSuccess ), ReleaseTask );
	
	
ErrorExit:
	
	
	[ self appendLogText: @"\n" ];
	
}


#if 0
#pragma mark -
#pragma mark Helper Methods
#pragma mark -
#endif


//—————————————————————————————————————————————————————————————————————————————
//	appendLogText:
//—————————————————————————————————————————————————————————————————————————————

- ( void ) appendLogText: ( NSString * ) logString;
{

	if ( testOutputTextView && logString )
	{

        NSScrollView *	logScrollView = [ testOutputTextView enclosingScrollView ];		
		NSRange      	range 	= NSMakeRange ( [ [ testOutputTextView string ] length ], 0 );
        NSRange      	newVisibleRange;

		// First, append the string
		[ testOutputTextView replaceCharactersInRange: range withString: logString ];
		
		// Now, scroll to the end if that's where we were before, or if there's no selection
		if ( logScrollView )
		{
			
			NSRect   visibleRect = [ logScrollView documentVisibleRect ];
			
			if ( floor ( NSMaxY ( [ testOutputTextView bounds] ) - NSMaxY ( visibleRect ) ) >= 0 )
			{
				
				newVisibleRange = NSMakeRange ( [ [ testOutputTextView string ] length ], 0 );
                [ testOutputTextView scrollRangeToVisible: newVisibleRange ];
			
			}
            
			else
			{
				newVisibleRange = [ testOutputTextView selectedRange ];
            }
        
		}
        
		else
		{
			newVisibleRange = [ testOutputTextView selectedRange ];            
		}
		
		if ( ( newVisibleRange.location == NSNotFound ) || ( newVisibleRange.length == 0 ) )
		{
			newVisibleRange = range;
		}
		
		[ testOutputTextView scrollRangeToVisible: newVisibleRange ];
		
	}
	
}


#if 0
#pragma mark -
#pragma mark Delegate Methods
#pragma mark -
#endif


- ( NSString * ) windowNibName
{
    // Override returning the nib file name of the document
    // If you need to use a subclass of NSWindowController or if your document supports multiple NSWindowControllers, you should remove this method and override -makeWindowControllers instead.
    return kMyDocumentString;
}


- ( void ) windowControllerDidLoadNib: ( NSWindowController * ) aController
{
    [ super windowControllerDidLoadNib:aController ];
    // Add any code here that need to be executed once the windowController has loaded the document's window.
}


- ( NSData * ) dataRepresentationOfType: ( NSString * ) aType
{
    // Insert code here to write your document from the given data.  You can also choose to override -fileWrapperRepresentationOfType: or -writeToFile:ofType: instead.
    return nil;
}


- ( BOOL ) loadDataRepresentation: ( NSData * ) data ofType: ( NSString * ) aType
{
    // Insert code here to read your document from the given data.  You can also choose to override -loadFileWrapperRepresentation:ofType: or -readFromFile:ofType: instead.
    return YES;
}


- ( void ) awakeFromNib
{
	
	NSString	* testString = [ testPopUpButton titleOfSelectedItem ];
	
	[ testButton setAction: NSSelectorFromString ( [ NSString stringWithFormat:@"%@%@", @"test", testString ] ) ];
	
}


@end
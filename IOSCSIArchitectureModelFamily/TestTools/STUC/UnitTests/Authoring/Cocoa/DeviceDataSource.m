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

#import "DeviceDataSource.h"
#import "AuthoringDeviceTester.h"
#import "ImageAndTextCell.h"

#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/IOKitKeys.h>
#import <IOKit/storage/IOStorageDeviceCharacteristics.h>
#import <IOKit/scsi/SCSITaskLib.h>
#import <IOKit/scsi/IOSCSIMultimediaCommandsDevice.h>


//—————————————————————————————————————————————————————————————————————————————
//	Macros
//—————————————————————————————————————————————————————————————————————————————

#define FILTER_DVD_DRIVES_ONLY			0	/* set to 1 to filter out non-DVD capable drives */
#define FILTER_DVD_BURNERS_ONLY			0	/* set to 1 to filter out non-DVD burners */
#define FILTER_ATAPI_DEVICES_ONLY		0	/* set to 1 to filter out non-ATAPI devices */


//—————————————————————————————————————————————————————————————————————————————
//	Prototypes
//—————————————————————————————————————————————————————————————————————————————

static void
AppearedNotificationHandler ( void * refCon, io_iterator_t iterator );

static void
DisappearedNotificationHandler ( void * refCon, io_iterator_t iterator );


//—————————————————————————————————————————————————————————————————————————————
//	Constants
//—————————————————————————————————————————————————————————————————————————————

#define kAppleVendorString								"Apple"


@interface DeviceDataSource(Private)
- ( void ) registerDeviceCallbackHandler;
@end

@implementation DeviceDataSource(Private)


//—————————————————————————————————————————————————————————————————————————————
//	registerDeviceCallbackHandler - Registers this object to get called back
//									when certain devices come online.
//—————————————————————————————————————————————————————————————————————————————

- ( void ) registerDeviceCallbackHandler
{
	
	IOReturn	 			kernErr;
	CFRunLoopSourceRef		runLoopSource;
	CFMutableDictionaryRef	matchingDict;
	CFMutableDictionaryRef	subDict;
	CFRunLoopRef			runLoop;
	
	// Create the dictionaries
	matchingDict = CFDictionaryCreateMutable ( kCFAllocatorDefault,
											   0,
											   &kCFTypeDictionaryKeyCallBacks,
											   &kCFTypeDictionaryValueCallBacks );
	
	subDict = CFDictionaryCreateMutable ( kCFAllocatorDefault,
										  0,
										  &kCFTypeDictionaryKeyCallBacks,
										  &kCFTypeDictionaryValueCallBacks );
	
	//
	//	Note: We are setting up a matching dictionary which looks like the following:
	//
	//	<dict>
	//		<key>IOPropertyMatch</key>
	//		<dict>
	//			<key>SCSITaskDeviceCategory</key>
	//			<string>SCSITaskAuthoringDevice</string>
	//		</dict>
	// </dict>
	//
	
	// Create the port on which we will receive notifications. We'll wrap it in a runLoopSource
	// which we then feed into the runLoop for async event notifications.
	deviceNotifyPort = IONotificationPortCreate ( kIOMasterPortDefault );
	if ( deviceNotifyPort == NULL )
	{
		return;		
	}
	
	// Get a runLoopSource for our mach port.
	runLoopSource = IONotificationPortGetRunLoopSource ( deviceNotifyPort );
	
	// Create a dictionary with the "SCSITaskDeviceCategory" key = "SCSITaskAuthoringDevice"
	CFDictionarySetValue ( 	subDict,
							CFSTR ( kIOPropertySCSITaskDeviceCategory ),
							CFSTR ( kIOPropertySCSITaskAuthoringDevice ) );
	
	// This shows you how to fine-tune the matching.
	
	#if FILTER_DVD_DRIVES_ONLY
	
	// Insert the "device-type" key = "DVD"
	CFDictionarySetValue ( 	subDict,
							CFSTR ( "device-type" ),
							CFSTR ( "DVD" ) );
	
	#endif /* FILTER_DVD_DRIVES_ONLY */
	
	// Add the dictionary to the main dictionary with the key "IOPropertyMatch" to
	// narrow the search to the above dictionary.
	CFDictionarySetValue ( 	matchingDict,
							CFSTR ( kIOPropertyMatchKey ),
							subDict );
	
	// Retain a reference since we arm both the appearance and disappearance notifications
	// and the call to IOServiceAddMatchingNotification() consumes a reference each time.
	matchingDict = ( CFMutableDictionaryRef ) CFRetain ( matchingDict );
	
	kernErr = IOServiceAddMatchingNotification ( deviceNotifyPort,
												 kIOFirstMatchNotification,
												 matchingDict,
												 AppearedNotificationHandler,
												 ( void * ) self,
												 &deviceAppearedIterator );
	
	// Since we got back an iterator already from this routine, we call the handler to immediately
	// dispatch any devices there already
	AppearedNotificationHandler ( ( void * ) self, deviceAppearedIterator );
	
	kernErr = IOServiceAddMatchingNotification ( deviceNotifyPort,
												 kIOTerminatedNotification,
												 matchingDict,
												 DisappearedNotificationHandler,
												 ( void * ) self,
												 &deviceDisappearedIterator );
	
	// Since we got back an iterator already from this routine, we call the handler to immediately
	// dispatch any devices removed already
	DisappearedNotificationHandler ( ( void * ) self, deviceDisappearedIterator );
	
	// Get our runLoop
	runLoop = [ [ NSRunLoop currentRunLoop ] getCFRunLoop ];
	
	// Add our runLoopSource to our runLoop
	CFRunLoopAddSource ( runLoop, runLoopSource, kCFRunLoopDefaultMode );
	
	// Release the dictionary
	CFRelease ( subDict );
	
}


@end


@implementation DeviceDataSource


//—————————————————————————————————————————————————————————————————————————————
//	init:
//—————————————————————————————————————————————————————————————————————————————

- ( id ) init
{

	[ super init ];
	
	deviceNotifyPort			= MACH_PORT_NULL;
	deviceAppearedIterator 		= 0;
	deviceDisappearedIterator 	= 0;
	
	// Create the mutable array which represents our list of
	// devices
	[ self setTheDeviceList: [ NSMutableArray arrayWithCapacity: 0 ] ];
	
	// Register ourself for callbacks when the devices we want
	// to know about come online/leave
	[ self registerDeviceCallbackHandler ];
		
	return self;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	dealloc:
//—————————————————————————————————————————————————————————————————————————————

- ( void ) dealloc
{
	
	// Free the array
	[ self setTheDeviceList: nil ];
	
	// Destroy the notify port if necessary
	if ( deviceNotifyPort != NULL )
	{
		IONotificationPortDestroy ( deviceNotifyPort );
		deviceNotifyPort = NULL;
	}
	
	// Destroy the iterator used for device appeared notifications
	if ( deviceAppearedIterator != 0 )
	{
		( void ) IOObjectRelease ( deviceAppearedIterator );
		deviceAppearedIterator = 0;
	}

	// Destroy the iterator used for device disappeared notifications
	if ( deviceDisappearedIterator != 0 )
	{
		( void ) IOObjectRelease ( deviceDisappearedIterator );
		deviceDisappearedIterator = 0;
	}
	
	// call our superclass
	[ super dealloc ];
	
}


#if 0
#pragma mark -
#pragma mark Accessor Methods
#pragma mark -
#endif


- ( NSTableView * ) theTableView { return theTableView; }
- ( NSMutableArray * ) theDeviceList { return theDeviceList; }
- ( void ) setTheDeviceList : ( NSMutableArray * ) value
{

	[ value retain ];
	[ theDeviceList release ];
	theDeviceList = value;

}
- ( int ) numberOfRowsInTableView: ( NSTableView * ) theTableView
{
	return [ theDeviceList count ];
}


#if 0
#pragma mark -
#pragma mark Delegate Methods
#pragma mark -
#endif


//—————————————————————————————————————————————————————————————————————————————
//	tableView:objectValueForTableColumn:row
//—————————————————————————————————————————————————————————————————————————————

- ( id ) 		tableView: ( NSTableView * ) theTableView
objectValueForTableColumn: ( NSTableColumn * ) theTableColumn
					  row: ( int ) rowIndex
{
	
	// This looks complicated but really isn't. It gets an element from our list of
	// devices and calls the method related to the identifier on that object. For instance,
	// say the column identifier is "theProductName" in the nib file, then that identifier
	// is passed and the object is asked for the value for that key. The method [ object theProductName ]
	// is invoked and the value returned is the member variable theProductName (an NSString *). This
	// is key-value coding and it is very cool. Learn it. Live it. Love it. :)
	return [ [ [ self theDeviceList ] objectAtIndex: rowIndex ] valueForKey: [ theTableColumn identifier ] ];
	
}


@end


#if 0
#pragma mark -
#pragma mark Device Matching Callback Methods
#pragma mark -
#endif


//—————————————————————————————————————————————————————————————————————————————
//	AppearedNotificationHandler
//—————————————————————————————————————————————————————————————————————————————

static void
AppearedNotificationHandler ( void * refCon, io_iterator_t iterator )
{
	
	io_service_t		theService	= MACH_PORT_NULL;
	DeviceDataSource *	dataSource	= ( DeviceDataSource * ) refCon;
	
	while ( theService = IOIteratorNext ( iterator ) )
	{
		
		CFMutableDictionaryRef	theDict 	= 0;
		AuthoringDevice * 		newDevice	= nil;
		IOReturn				theErr 		= 0;
		
		newDevice = [ AuthoringDevice device ];
		
		// Get the CF Properties for the io_service_t
		theErr = IORegistryEntryCreateCFProperties ( theService,
													 &theDict,
													 kCFAllocatorDefault,
													 0 );
		
		if ( theErr == kIOReturnSuccess )
		{
			
			// Check for the GUID key
			if ( CFDictionaryContainsKey ( theDict, CFSTR ( kIOPropertySCSITaskUserClientInstanceGUID ) ) )
			{
				
				NSData *	theData = nil;
				
				theData = ( NSData * ) CFDictionaryGetValue ( theDict, CFSTR ( kIOPropertySCSITaskUserClientInstanceGUID ) );
				[ newDevice setTheGUID: theData ];
				
			}
			
			// Check for the Protocol Characteristics key
			if ( CFDictionaryContainsKey ( theDict, CFSTR ( kIOPropertyProtocolCharacteristicsKey ) ) )
			{
				
				CFStringRef			physicalInterconnectString	= NULL;
				CFDictionaryRef		protocolDict				= NULL;
				
				protocolDict = ( CFDictionaryRef ) CFDictionaryGetValue ( theDict,
						CFSTR ( kIOPropertyProtocolCharacteristicsKey ) );
				
				physicalInterconnectString = ( CFStringRef ) CFDictionaryGetValue ( protocolDict,
						CFSTR ( kIOPropertyPhysicalInterconnectTypeKey ) );
				
				#if FILTER_ATAPI_DEVICES_ONLY
				
				if ( CFStringCompare ( physicalInterconnectString, CFSTR ( kIOPropertyPhysicalInterconnectTypeATAPI ), 0 ) != kCFCompareEqualTo )
				{
					
					// Not an ATAPI device. Clean up and bail. Do not call release on the AuthoringDevice as it was
					// given to you autoreleased.
					CFRelease ( theDict );
					theDict = 0;
					( void ) IOObjectRelease ( theService );
					continue;
					
				}
				
				#endif /* FILTER_ATAPI_DEVICES_ONLY */
				
				[ newDevice setThePhysicalInterconnect: ( NSString * ) physicalInterconnectString ];
				
			}
			
			// Check for the Device Characteristics key
			if ( CFDictionaryContainsKey ( theDict, CFSTR ( kIOPropertyDeviceCharacteristicsKey ) ) )
			{
				
				CFStringRef			string			= NULL;
				CFDictionaryRef		deviceDict		= NULL;
				CFNumberRef			features	 	= NULL;
				UInt32				value		 	= 0;
				
				#if FILTER_DVD_BURNERS_ONLY
								
				#endif /* FILTER_DVD_BURNERS_ONLY */
				
				deviceDict = ( CFDictionaryRef ) CFDictionaryGetValue ( theDict,
																		CFSTR ( kIOPropertyDeviceCharacteristicsKey ) );
				
				// Get the vendor, product, and firmware info for display
				string = ( CFStringRef ) CFDictionaryGetValue ( deviceDict, CFSTR ( kIOPropertyVendorNameKey ) );
				[ newDevice setTheVendorName: ( NSString * ) string ];
				
				string = ( CFStringRef ) CFDictionaryGetValue ( deviceDict, CFSTR ( kIOPropertyProductNameKey ) );
				[ newDevice setTheProductName: ( NSString * ) string ];
				
				string = ( CFStringRef ) CFDictionaryGetValue ( deviceDict, CFSTR ( kIOPropertyProductRevisionLevelKey ) );
				[ newDevice setTheProductRevisionLevel: ( NSString * ) string ];
				
				features = ( CFNumberRef ) CFDictionaryGetValue ( deviceDict, CFSTR ( kIOPropertySupportedCDFeatures ) );
				CFNumberGetValue ( features, kCFNumberLongType, &value );
				[ newDevice setCDFeatures: value ];

				features = ( CFNumberRef ) CFDictionaryGetValue ( deviceDict, CFSTR ( kIOPropertySupportedDVDFeatures ) );
				CFNumberGetValue ( features, kCFNumberLongType, &value );
				[ newDevice setDVDFeatures: value ];
				
				#if FILTER_DVD_BURNERS_ONLY
				
				if ( ( ( value & kDVDFeaturesWriteOnceMask ) == 0 ) &&
					 ( ( value & kDVDFeaturesReWriteableMask ) == 0 ) &&
					 ( ( value & kDVDFeaturesPlusRMask ) == 0 ) &&
					 ( ( value & kDVDFeaturesPlusRWMask ) == 0 ) )
				{
					
					// Not a burner. Clean up and bail. Do not call release on the AuthoringDevice as it was
					// given to you autoreleased.
					CFRelease ( theDict );
					theDict = 0;
					( void ) IOObjectRelease ( theService );
					continue;
					
				}
				
				#endif /* FILTER_DVD_BURNERS_ONLY */
				
			}
			
			// We're done with this dictionary, so release it.
			CFRelease ( theDict );
			theDict = 0;
			
			// Add the object to the data source so that it knows about this device
			[ [ dataSource theDeviceList ] addObject: newDevice ];
			
		}
		
		// Release the object.
		( void ) IOObjectRelease ( theService );
		
	}
	
	// Reload the data source once we've traversed the entire iterator
	[ [ dataSource theTableView ] reloadData ];
	
}


//—————————————————————————————————————————————————————————————————————————————
//	DisappearedNotificationHandler
//—————————————————————————————————————————————————————————————————————————————

static void
DisappearedNotificationHandler ( void * refCon, io_iterator_t iterator )
{
	
	io_service_t		theService	= MACH_PORT_NULL;
	DeviceDataSource *	dataSource	= ( DeviceDataSource * ) refCon;
	
	while ( theService = IOIteratorNext ( iterator ) )
	{
		
		CFMutableDictionaryRef	theDict		= NULL;
		AuthoringDevice * 		newDevice	= nil;
		IOReturn				theErr 		= kIOReturnSuccess;
		
		newDevice = [ AuthoringDevice device ];
		
		// Get the CF Properties for this node
		theErr = IORegistryEntryCreateCFProperties ( theService,
													 &theDict,
													 kCFAllocatorDefault,
													 0 );
		
		if ( theErr == kIOReturnSuccess )
		{
			
			// Get the GUID key
			if ( CFDictionaryContainsKey ( theDict, CFSTR ( kIOPropertySCSITaskUserClientInstanceGUID ) ) )
			{
				
				NSData *	theData = nil;
				
				theData = ( NSData * ) CFDictionaryGetValue ( theDict, CFSTR ( kIOPropertySCSITaskUserClientInstanceGUID ) );
				[ newDevice setTheGUID: theData ];
				
				// Since the GUID is used to distinguish between two objects (the object overrides isEqual),
				// we just create a new object and fill in the GUID key above, and then tell the data source
				// to remove it. If you decide to use more criteria than the GUID (like the physical interconnect)
				// be sure to add those fields to the object before calling removeObject on the theDeviceList
				[ [ dataSource theDeviceList ] removeObject: newDevice ];
				
			}
			
		}
		
		( void ) IOObjectRelease ( theService );
		
	}
	
	// Reload the data source once we've traversed the entire iterator
	[ [ dataSource theTableView ] reloadData ];
	
}
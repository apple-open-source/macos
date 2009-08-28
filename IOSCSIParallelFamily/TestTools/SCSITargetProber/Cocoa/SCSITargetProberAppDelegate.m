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


//—————————————————————————————————————————————————————————————————————————————
//	Imports
//—————————————————————————————————————————————————————————————————————————————

#import "SCSITargetProberAppDelegate.h"
#import "SCSITargetProberDocument.h"
#import "SCSITargetProberKeys.h"
#import "SCSIInitiator.h"
#import "AppPreferences.h"
#import "FakeSCSIInitiator.h"
#import <IOKit/IOKitLib.h>


//—————————————————————————————————————————————————————————————————————————————
//	Constants
//—————————————————————————————————————————————————————————————————————————————

#define kIOSCSIParallelInterfaceControllerClassString	"IOSCSIParallelInterfaceController"
#define kIOSCSITargetDeviceClassString					"IOSCSITargetDevice"


//—————————————————————————————————————————————————————————————————————————————
//	Prototypes
//—————————————————————————————————————————————————————————————————————————————

static void
ClearIterator ( io_iterator_t iterator );

static int
domainComparator ( id obj1, id obj2, void * context );


//—————————————————————————————————————————————————————————————————————————————
//	Implementation
//—————————————————————————————————————————————————————————————————————————————

@implementation SCSITargetProberAppDelegate


- ( id ) init
{
	
	self = [ super init ];
	
	if ( self != nil )
	{
		
		port					= NULL;
		appearedIterator		= MACH_PORT_NULL;
		disappearedIterator		= MACH_PORT_NULL;
		
		[ self setInitiators: [ [ [ NSMutableArray alloc ] initWithCapacity: 0 ] autorelease ] ];
		
	}
	
	return self;
	
}


- ( NSArray * ) initiators
{
	return initiators;
}


- ( void ) setInitiators: ( NSMutableArray * ) i
{
	
	[ i retain ];
	[ initiators release ];
	initiators = i;
	
}


- ( void ) applicationDidFinishLaunching: ( NSNotification * ) notification
{

#pragma unused ( notification )
	
	NSMutableDictionary *	dict = [ [ [ NSMutableDictionary alloc ] init ] autorelease ];
	
	// Set some initial defaults. If the user doesn't have a customized user
	// defaults database (they haven't modified the prefs window), we want to
	// have the basic defaults of "ID" and "Description" visible in the table
	// view.
	[ dict setValue: [ NSNumber numberWithBool: YES ] forKey: kShowTargetIDString ];
	[ dict setValue: [ NSNumber numberWithBool: YES ] forKey: kShowDescriptionString ];
	[ dict setValue: [ NSNumber numberWithBool: NO ] forKey: kShowRevisionString ];
	[ dict setValue: [ NSNumber numberWithBool: NO ] forKey: kShowFeaturesString ];
	[ dict setValue: [ NSNumber numberWithBool: NO ] forKey: kShowPDTString ];
	
	// Set the standard user defaults up.
	[ [ NSUserDefaultsController sharedUserDefaultsController ] setInitialValues: dict ];
	
	// Start our IONotificationPort stuff
	[ self startNotifications ];
	
	// If no controllers are found, tell the user.
	if ( [ initiators count ] == 0 )
	{
		
		// No cards found. Tell the user then quit.
		NSRunAlertPanel ( NSLocalizedString ( kNoControllersFoundTitle, "" ),
						  NSLocalizedString ( kNoControllersFoundText, "" ), 
						  nil, nil, nil );
		[ NSApp terminate: self ];
		
	}
	
	else
	{
		// Create a new document window
		[ self newDocument: self ];
	}
	
}


// Action called to instantiate a new document. This is called from
// -applicationDidFinishLaunching and by the Menu Item File->New
// and by the Cmd + 'N' key sequence.

- ( IBAction ) newDocument: ( id ) sender
{
#pragma unused ( sender )
	[ SCSITargetProberDocument newDocument: initiators ];
}


// Action called to load the preferences window. This is called from
// SCSITargetProber->Preferences and from Cmd + ',' key sequence.

- ( IBAction ) showPrefs: ( id ) sender
{
#pragma unused ( sender )
	[ AppPreferences showPrefs ];
}


// Called to start IOKit notifications. We get notifications when
// new IOSCSIParallelInterfaceController subclasses register
// and when IOSCSITargetDevice objects register (they don't do
// that yet but they will soon). Until IOSCSITargetDevice objects
// register, the UI will not update on a successful reprobe, but
// the plumbing is already done in the app to make the UI refresh
// when that changes in the kernel.

- ( void ) startNotifications
{
	
	IOReturn				result			= kIOReturnSuccess;
	CFRunLoopSourceRef		source			= NULL;
	CFRunLoopRef			runLoop			= NULL;
	CFMutableDictionaryRef	matchingDict	= NULL;
	
	// Create a notification port on which to receive messages.
	port = IONotificationPortCreate ( kIOMasterPortDefault );
	require ( ( port != NULL ), ErrorExit );
	
	// Get the runloop source so we can attach it to our runloop.
	source = IONotificationPortGetRunLoopSource ( port );
	require ( ( source != NULL ), ReleasePort );
	
	// Create the matching dictionary. Our matching dictionary uses
	// the "IOSCSIParallelInterfaceController" class matching - basically
	// any device which inherits from IOSCSIParallelInterfaceController.
	matchingDict = IOServiceMatching ( kIOSCSIParallelInterfaceControllerClassString );
	require ( ( matchingDict != NULL ), ReleaseSource );
	
	// Retain the dictionary as IOServiceAddMatchingNotification() retains one
	// reference.
	CFRetain ( matchingDict );
	
	// Set up notifications on first matches (plug in of CardBus SCSI cards).
	result = IOServiceAddMatchingNotification ( port,
												kIOFirstMatchNotification,
												matchingDict,
												AppearedNotificationHandler,
												( void * ) self,
												&appearedIterator );
	
	// Since we got back an iterator already from this routine, we call the handler to immediately
	// dispatch any devices there already
	AppearedNotificationHandler ( ( void * ) self, appearedIterator );
	
	// Set up notifications on termination (unplug of CardBus SCSI cards).
	result = IOServiceAddMatchingNotification ( port,
												kIOTerminatedNotification,
												matchingDict,
												DisappearedNotificationHandler,
												( void * ) self,
												&disappearedIterator );
	
	// Since we got back an iterator already from this routine, we call the handler to immediately
	// dispatch any devices there already
	DisappearedNotificationHandler ( ( void * ) self, disappearedIterator );
	
	// Create the matching dictionary. Our matching dictionary uses
	// the "IOSCSITargetDevice" class matching - basically
	// any device which inherits from IOSCSITargetDevice.
	matchingDict = IOServiceMatching ( kIOSCSITargetDeviceClassString );
	require ( ( matchingDict != NULL ), ReleaseSource );
	
	// Retain the dictionary as IOServiceAddMatchingNotification() retains one
	// reference.
	CFRetain ( matchingDict );

	// Set up notifications on first matches (new device arrives when we reprobe).	
	result = IOServiceAddMatchingNotification ( port,
												kIOFirstMatchNotification,
												matchingDict,
												AppearedNotificationHandler,
												( void * ) self,
												&appearedIterator );
	
	// Since we got back an iterator already, we want to empty it. We'll find target devices at
	// launch by iterating over the registry. The dynamic stuff is for when we reprobe...
	ClearIterator ( appearedIterator );
	
	// Set up notifications on termination (device goes away).	
	result = IOServiceAddMatchingNotification ( port,
												kIOTerminatedNotification,
												matchingDict,
												DisappearedNotificationHandler,
												( void * ) self,
												&disappearedIterator );
	
	// Since we got back an iterator already, we want to empty it. We'll find target devices at
	// launch by iterating over the registry. The dynamic stuff is for when we reprobe...
	ClearIterator ( disappearedIterator );

	// Get our run loop so we can add the notification's source to it.
	runLoop = [ [ NSRunLoop currentRunLoop ] getCFRunLoop ];
	
	// Add our runLoopSource to our runLoop
	CFRunLoopAddSource ( runLoop, source, kCFRunLoopDefaultMode );
		
	return;
	
	
ReleaseSource:
	
	
	CFRelease ( source );
	source = NULL;
	
	
ReleasePort:
	
	
	IONotificationPortDestroy ( port );
	port = NULL;
	
		
ErrorExit:
	
	
	return;
	
}


// Called to stop IOKit notifications.

- ( void ) stopNotifications
{
	
	// Destroy the notify port if necessary
	if ( port != NULL )
	{
		
		IONotificationPortDestroy ( port );
		port = NULL;
		
	}
	
	// Destroy the iterator used for device appeared notifications
	if ( appearedIterator != MACH_PORT_NULL )
	{
		
		( void ) IOObjectRelease ( appearedIterator );
		appearedIterator = MACH_PORT_NULL;
		
	}
	
	// Destroy the iterator used for device disappeared notifications
	if ( disappearedIterator != MACH_PORT_NULL )
	{
		
		( void ) IOObjectRelease ( disappearedIterator );
		disappearedIterator = MACH_PORT_NULL;
		
	}
	
}


// Called when devices we registered an interest in appear.

- ( void ) appearedNotification: ( io_iterator_t ) iterator
{
	
	io_service_t	service = MACH_PORT_NULL;
	
	service = IOIteratorNext ( iterator );
	while ( service != MACH_PORT_NULL )
	{
		
		// Is this an IOSCSIParallelInterfaceController?
		if ( IOObjectConformsTo ( service, kIOSCSIParallelInterfaceControllerClassString ) )
		{
			
			SCSIInitiator *		newInitiator = nil;
			
			// Yes, it is an IOSCSIParallelInterfaceController. Create a
			// SCSIInitiator object to represent it.
			newInitiator = [ [ SCSIInitiator alloc ] initWithService: service ];
			
			// Add the new object to the list.
			[ initiators addObject: newInitiator ];
			
			// Sort the list.
			[ initiators sortUsingFunction: domainComparator context: nil ];
			
			// Adding to the list retains the object, so we can safely release
			// our refcount on it. When the object is removed from the list,
			// it will be released.
			[ newInitiator release ];
			
		}
		
		// Is this an IOSCSITargetDevice?
		else if ( IOObjectConformsTo ( service, kIOSCSITargetDeviceClassString ) )
		{
			
			SCSIDevice *	newDevice	= nil;
			SCSIInitiator * initiator	= nil;
			io_service_t	parent		= MACH_PORT_NULL;
			int				domainID	= 0;
			int				count		= 0;
			int				index		= 0;
			
			// Get the IOSCSIParallelInterfaceDevice object.
			IORegistryEntryGetParentEntry ( service, kIOServicePlane, &parent );
			
			// Create the SCSIDevice object with the IOSCSIParallelInterfaceDevice.
			newDevice = [ [ SCSIDevice alloc ] initWithService: parent ];
			
			// Release the parent, we don't need it any more...
			IOObjectRelease ( parent );
			
			// Get the domainID for this new device.
			domainID = [ [ newDevice domainIdentifier ] intValue ];
			
			// Find out where to add the target device. Look at
			// each initiator to see if the domainID matches.
			count = [ initiators count ];
			for ( index = 0; index < count; index++ )
			{
				
				initiator = [ initiators objectAtIndex: index ];
				
				// Does domainID match?
				if ( domainID == [ initiator domainID ] )
				{
					
					// Yes, add the target device to this initiator.
					[ initiator addTargetDevice: newDevice ];
					
				}
				
			}
			
			// We can safely release this device now. Either it was added
			// to an initiator, or there wasn't an initator to associate it
			// with...
			[ newDevice release ];
			
		}
		
		// On to the next in the list...
		IOObjectRelease ( service );
		service = IOIteratorNext ( iterator );
		
	}
	
}


// Called when devices we registered an interest in disappear.

- ( void ) disappearedNotification: ( io_iterator_t ) iterator
{

	io_service_t	service = MACH_PORT_NULL;

	service = IOIteratorNext ( iterator );
	while ( service != MACH_PORT_NULL )
	{
		
		// Is this an IOSCSIParallelInterfaceController?
		if ( IOObjectConformsTo ( service, kIOSCSIParallelInterfaceControllerClassString ) )
		{
			
			int				domainID	= 0;
			NSEnumerator *	enumerator	= nil;
			SCSIInitiator * initiator	= nil;
			
			// Yes, get the domainID for this io_service_t.
			domainID = [ SCSIInitiator domainIDForService: service ];
			
			enumerator = [ initiators objectEnumerator ];
			initiator = [ enumerator nextObject ];
			
			// Find out which initiator to remove from the list.			
			while ( initiator != nil )
			{
				
				// Does the domainID match?
				if ( [ initiator domainID ] == domainID )
				{
					
					// Yes, remove this one.
					[ initiators removeObject: initiator ];
					break;
					
				}
				
				// On to the next object..
				initiator = [ enumerator nextObject ];
				
			}
			
		}
		
		// Is this an IOSCSITargetDevice?
		else if ( IOObjectConformsTo ( service, kIOSCSITargetDeviceClassString ) )
		{
			
			SCSIInitiator * initiator	= nil;
			int				domainID	= 0;
			int				count		= 0;
			int				index		= 0;
			int				targetID	= 0;
			
			// Yes, get the domainID and targetID.
			domainID = [ SCSIDevice domainIDForService: service ];
			targetID = [ SCSIDevice targetIDForService: service ];
			
			// Find which initiator has the same domainID.
			count = [ initiators count ];
			for ( index = 0; index < count; index++ )
			{
				
				initiator = [ initiators objectAtIndex: index ];
				
				// Does the domainID match?
				if ( domainID == [ initiator domainID ] )
				{
					
					// Yes, remove the device from this initiator's list.
					[ initiator removeTargetDevice: targetID ];
					break;
					
				}
				
			}
			
		}
		
		// On to the next in the list...
		IOObjectRelease ( service );
		service = IOIteratorNext ( iterator );
		
	}
	
}


- ( void ) dealloc
{
	
	[ self stopNotifications ];
	[ self setInitiators: nil ];
	
	// Call our superclass
	[ super dealloc ];
	
}


@end


#if 0
#pragma mark -
#pragma mark Static methods
#pragma mark -
#endif


//—————————————————————————————————————————————————————————————————————————————
//	domainComparator - Compares devices based on deviceIdentifier field
//—————————————————————————————————————————————————————————————————————————————

static int
domainComparator ( id obj1, id obj2, void * context )
{

#pragma unused ( context )
	
	int		result = NSOrderedSame;
	
	if ( [ obj1 domainID ] < [ obj2 domainID ] )
	{
		result = NSOrderedAscending;
	}
	
	if ( [ obj1 domainID ] > [ obj2 domainID ] )
	{
		result = NSOrderedDescending;
	}
	
	return result;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	ClearIterator - Clears devices from an iterator without performing
//					any action
//—————————————————————————————————————————————————————————————————————————————

static void
ClearIterator ( io_iterator_t iterator )
{
	
	io_service_t	service = MACH_PORT_NULL;
	
	service = IOIteratorNext ( iterator );
	while ( service != MACH_PORT_NULL )
	{
		
		IOObjectRelease ( service );
		service = IOIteratorNext ( iterator );
		
	}
	
}


#if 0
#pragma mark -
#pragma mark C->Obj-C glue
#pragma mark -
#endif


//—————————————————————————————————————————————————————————————————————————————
//	AppearedNotificationHandler - C->Obj-C glue
//—————————————————————————————————————————————————————————————————————————————

static void
AppearedNotificationHandler ( void * refCon, io_iterator_t iterator )
{
	
	SCSITargetProberAppDelegate *	app = ( SCSITargetProberAppDelegate * ) refCon;
	[ app appearedNotification: iterator ];
	
}


//—————————————————————————————————————————————————————————————————————————————
//	DisappearedNotificationHandler - C->Obj-C glue
//—————————————————————————————————————————————————————————————————————————————

static void
DisappearedNotificationHandler ( void * refCon, io_iterator_t iterator )
{
	
	SCSITargetProberAppDelegate *	app = ( SCSITargetProberAppDelegate * ) refCon;
	[ app disappearedNotification: iterator ];
	
}
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

#import <unistd.h>
#import "SCSIDevice.h"
#import "Probing.h"
#import "SCSITargetProberKeys.h"
#import <IOKit/IOKitLib.h>
#import <IOKit/IOKitKeys.h>
#import <IOKit/storage/IOStorageProtocolCharacteristics.h>
#import <IOKit/scsi/SCSICmds_INQUIRY_Definitions.h>


//—————————————————————————————————————————————————————————————————————————————
//	Constants
//—————————————————————————————————————————————————————————————————————————————

#define kIOSCSITargetDeviceClassString					"IOSCSITargetDevice"

// "SCSI I_T Nexus Features" is only defined in
// IOSCSIParallelInterfaceController.h, but it isn't exported to user space (yet).
// Until it is, we have to create a string here and also copy the enums from the
// header as well.
// Remove this when the header is exported.

#define kSCSI_I_T_NexusFeaturesString					@"SCSI I_T Nexus Features"

enum
{
	kSCSIParallelFeature_WideDataTransfer 					= 0,
	kSCSIParallelFeature_SynchronousDataTransfer 			= 1,
	kSCSIParallelFeature_QuickArbitrationAndSelection 		= 2,
	kSCSIParallelFeature_DoubleTransitionDataTransfers 		= 3,
	kSCSIParallelFeature_InformationUnitTransfers 			= 4
};

enum
{
	kSCSIParallelFeature_WideDataTransferMask 				= (1 << kSCSIParallelFeature_WideDataTransfer),
	kSCSIParallelFeature_SynchronousDataTransferMask 		= (1 << kSCSIParallelFeature_SynchronousDataTransfer),
	kSCSIParallelFeature_QuickArbitrationAndSelectionMask 	= (1 << kSCSIParallelFeature_QuickArbitrationAndSelection),
	kSCSIParallelFeature_DoubleTransitionDataTransfersMask 	= (1 << kSCSIParallelFeature_DoubleTransitionDataTransfers),
	kSCSIParallelFeature_InformationUnitTransfersMask 		= (1 << kSCSIParallelFeature_InformationUnitTransfers)
};


//—————————————————————————————————————————————————————————————————————————————
//	Implementation
//—————————————————————————————————————————————————————————————————————————————

@implementation SCSIDevice

- ( NSString * ) physicalInterconnect { return physicalInterconnect; }
- ( NSString * ) manufacturer { return manufacturer; }
- ( NSString * ) model { return model; }
- ( NSString * ) revision { return revision; }
- ( NSString * ) peripheralDeviceType { return peripheralDeviceType; }
- ( NSNumber * ) domainIdentifier { return domainIdentifier; }
- ( NSNumber * ) deviceIdentifier { return deviceIdentifier; }
- ( NSArray * ) features { return features; }
- ( NSImage * ) image { return image; }
- ( BOOL ) devicePresent { return devicePresent; }
- ( BOOL ) isInitiator { return isInitiator; }


// Called to initialize the object with an io_service_t.

- ( id ) initWithService: ( io_service_t ) service
{
	
	CFTypeRef		property 	= NULL;
	id				value		= NULL;
	IOReturn		result		= kIOReturnSuccess;
	io_iterator_t	iterator	= MACH_PORT_NULL;

	self = [ super init ];
	
	if ( self != nil )
	{
		
		// Init to known state
		[ self clearInformation ];
		
		// Get the protocol characteristics key.
		property = IORegistryEntrySearchCFProperty ( service,
												   kIOServicePlane,
												   CFSTR ( kIOPropertyProtocolCharacteristicsKey ),
												   kCFAllocatorDefault,
												   kIORegistryIterateRecursively );
		if ( property != NULL )
		{
			
			// Get the physical interconnect
			value = ( id ) CFDictionaryGetValue ( ( CFDictionaryRef ) property,
												  CFSTR ( kIOPropertyPhysicalInterconnectTypeKey ) );
			
			[ self setPhysicalInterconnect: ( NSString * ) value ];
			
			// Get the SCSI Domain ID
			value = ( id ) CFDictionaryGetValue ( ( CFDictionaryRef ) property,
												  CFSTR ( kIOPropertySCSIDomainIdentifierKey ) );
			
			[ self setDomainIdentifier: ( NSNumber * ) value ];
			
			// Get the SCSI Target ID
			value = ( id ) CFDictionaryGetValue ( ( CFDictionaryRef ) property,
												  CFSTR ( kIOPropertySCSITargetIdentifierKey ) );
			
			[ self setDeviceIdentifier: ( NSNumber * ) value ];
			
			CFRelease ( property );
			
		}
		
		// See if there is any more information we can fill in (i.e. is a target present?)		
		result = IORegistryEntryGetChildIterator ( service,
												   kIOServicePlane,
												   &iterator );
		if ( result == kIOReturnSuccess )
		{
			
			io_registry_entry_t		entry = MACH_PORT_NULL;
			
			entry = IOIteratorNext ( iterator );
			while ( entry != MACH_PORT_NULL )
			{
				
				// Is this an IOSCSITargetDevice?
				if ( IOObjectConformsTo ( entry, kIOSCSITargetDeviceClassString ) )
				{
					
					NSNumber *	number = nil;
					
					// We have a target device present at this location.
					// Get the information from it.
					[ self setDevicePresent: YES ];
					[ self setImage: [ NSImage imageNamed: kHardDiskImageString ] ];
					[ self setIsInitiator: NO ];
					
					// Get the PDT
					value = ( id ) IORegistryEntrySearchCFProperty (
									service,
									kIOServicePlane,
									CFSTR ( kIOPropertySCSIPeripheralDeviceType ),
									kCFAllocatorDefault,
									kIORegistryIterateRecursively );
					
					[ self setPeripheralDeviceType: [ NSString stringWithFormat: @"%02Xh", [ value intValue ] ] ];
					[ value release ];
					
					// Get the Vendor ID
					value = ( id ) IORegistryEntrySearchCFProperty (
									service,
									kIOServicePlane,
									CFSTR ( kIOPropertySCSIVendorIdentification ),
									kCFAllocatorDefault,
									kIORegistryIterateRecursively );
					
					[ self setManufacturer: value ];
					[ value release ];
					
					// Get the Product ID
					value = ( id ) IORegistryEntrySearchCFProperty (
									service,
									kIOServicePlane,
									CFSTR ( kIOPropertySCSIProductIdentification ),
									kCFAllocatorDefault,
									kIORegistryIterateRecursively );
					
					[ self setModel: value ];
					[ value release ];
					
					// Get the Product Revision Level
					value = ( id ) IORegistryEntrySearchCFProperty (
									service,
									kIOServicePlane,
									CFSTR ( kIOPropertySCSIProductRevisionLevel ),
									kCFAllocatorDefault,
									kIORegistryIterateRecursively );
					
					[ self setRevision: value ];
					[ value release ];
					
					// Get the I_T Nexus Features
					value = ( id ) IORegistryEntrySearchCFProperty (
									service,
									kIOServicePlane,
									CFSTR ( kIOPropertyProtocolCharacteristicsKey ),
									kCFAllocatorDefault,
									kIORegistryIterateRecursively );
					
					number = [ ( NSDictionary * ) value objectForKey: kSCSI_I_T_NexusFeaturesString ];
					
					[ self setFeatures: [ self buildFeatureList: number ] ];
					[ value release ];
					
				}
				
				IOObjectRelease ( entry );
				entry = IOIteratorNext ( iterator );
				
			}
			
			IOObjectRelease ( iterator );
			
		}
	
	}
	
	return self;
	
}


// Called to get the SCSI Domain ID for the io_service_t.

+ ( int ) domainIDForService: ( io_service_t ) service
{
	
	int			domain	= 0;
	id			value	= nil;
	
	// Get the protocol characteristics key.
	value = ( id ) IORegistryEntrySearchCFProperty ( service,
													 kIOServicePlane,
													 CFSTR ( kIOPropertyProtocolCharacteristicsKey ),
													 kCFAllocatorDefault,
													 0 );
	
	// Get the SCSI Domain ID.
	domain = [ [ value objectForKey: @kIOPropertySCSIDomainIdentifierKey ] intValue ];
	[ value release ];
	
	return domain;
	
}


// Called to get the SCSI Target ID for the io_service_t.

+ ( int ) targetIDForService: ( io_service_t ) service
{
	
	int			target	= 0;
	id			value	= nil;
	
	// Get the protocol characteristics key.
	value = ( id ) IORegistryEntrySearchCFProperty ( service,
													 kIOServicePlane,
													 CFSTR ( kIOPropertyProtocolCharacteristicsKey ),
													 kCFAllocatorDefault,
													 0 );
	
	// Get the SCSI Target ID.
	target = [ [ value objectForKey: @kIOPropertySCSITargetIdentifierKey ] intValue ];
	[ value release ];
	
	return target;
	
}


// Builds a feature list out of the bitmask.

- ( NSArray * ) buildFeatureList: ( NSNumber * ) number
{
	
	NSMutableArray *	featureList = nil;
	NSString *			string		= nil;
	int					value		= 0;
	
	// Create an array to which we'll add string descriptions.
	featureList = [ [ [ NSMutableArray alloc ] init ] autorelease ];
	
	// Get the actual value as an int.
	value = [ number intValue ];
	
	// Does this device support Sync transfer?
	if ( value & kSCSIParallelFeature_SynchronousDataTransferMask )
	{
		
		// Yes, get localized string.
		string = [ [ NSBundle mainBundle ] localizedStringForKey: kSCSIParallelFeatureSyncString
														   value: kSCSIParallelFeatureSyncString
														   table: nil ];
		// Add localized string to feature list.
		[ featureList addObject: string ];
		
	}
	
	// Does this device support Wide transfer?
	if ( value & kSCSIParallelFeature_WideDataTransferMask )
	{
		
		// Yes, get localized string.
		string = [ [ NSBundle mainBundle ] localizedStringForKey: kSCSIParallelFeatureWideString
														   value: kSCSIParallelFeatureWideString
														   table: nil ];
		// Add localized string to feature list.
		[ featureList addObject: string ];
		
	}
	
	// Does this device support QAS?
	if ( value & kSCSIParallelFeature_QuickArbitrationAndSelectionMask )
	{
		
		// Yes, get localized string.
		string = [ [ NSBundle mainBundle ] localizedStringForKey: kSCSIParallelFeatureQASString
														   value: kSCSIParallelFeatureQASString
														   table: nil ];
		// Add localized string to feature list.
		[ featureList addObject: string ];
		
	}
	
	// Does this device support DT?
	if ( value & kSCSIParallelFeature_DoubleTransitionDataTransfersMask )
	{
		
		// Yes, get localized string.
		string = [ [ NSBundle mainBundle ] localizedStringForKey: kSCSIParallelFeatureDTString
														   value: kSCSIParallelFeatureDTString
														   table: nil ];
		// Add localized string to feature list.
		[ featureList addObject: string ];
		
	}
	
	// Does this device support IU?
	if ( value & kSCSIParallelFeature_InformationUnitTransfersMask )
	{
		
		// Yes, get localized string.
		string = [ [ NSBundle mainBundle ] localizedStringForKey: kSCSIParallelFeatureIUString
														   value: kSCSIParallelFeatureIUString
														   table: nil ];
		// Add localized string to feature list.
		[ featureList addObject: string ];
		
	}
	
	return featureList;
	
}


- ( NSString * ) title
{
	
	NSString *	string = @"No Device Present";
	
	if ( devicePresent == YES )
	{
		string = [ NSString stringWithFormat: @"%@ %@", manufacturer, model ];
	}
	
	return string;
	
}


- ( void ) clearInformation
{
	
	[ self setPhysicalInterconnect: nil ];
	[ self setManufacturer: nil ];
	[ self setModel: nil ];
	[ self setRevision: nil ];
	[ self setPeripheralDeviceType: nil ];
	[ self setDomainIdentifier: nil ];
	[ self setDeviceIdentifier: nil ];
	[ self setFeatures: nil ];
	[ self setIsInitiator: NO ];
	[ self setDevicePresent: NO ];
	[ self setImage: [ NSImage imageNamed: kNothingImageString ] ];
	
}


- ( void ) setIsInitiator: ( BOOL ) value
{
	
	isInitiator = value;
	if ( isInitiator )
	{
		[ self setPeripheralDeviceType: @"N/A" ];
	}
	
}


- ( void ) setDevicePresent: ( BOOL ) value
{
	devicePresent = value;
}


- ( void ) setPhysicalInterconnect: ( NSString * ) p
{
	
	[ p retain ];
	[ physicalInterconnect release ];
	physicalInterconnect = p;
	
}


- ( void ) setManufacturer: ( NSString * ) m
{
	
	[ m retain ];
	[ manufacturer release ];
	manufacturer = m;
	
}


- ( void ) setModel: ( NSString * ) m
{
	
	[ m retain ];
	[ model release ];
	model = m;
	
}


- ( void ) setRevision: ( NSString * ) r
{
	
	[ r retain ];
	[ revision release ];
	revision = r;
	
}


- ( void ) setPeripheralDeviceType: ( NSString * ) p
{
	
	[ p retain ];
	[ peripheralDeviceType release ];
	peripheralDeviceType = p;
	
}


- ( void ) setDomainIdentifier: ( NSNumber * ) i
{
	
	[ i retain ];
	[ domainIdentifier release ];
	domainIdentifier = i;
	
}


- ( void ) setDeviceIdentifier: ( NSNumber * ) i
{
	
	[ i retain ];
	[ deviceIdentifier release ];
	deviceIdentifier = i;
	
}


- ( void ) setFeatures: ( NSArray * ) f
{
	
	[ f retain ];
	[ features release ];
	features = f;
	
}

- ( void ) setImage: ( NSImage * ) i
{

	[ i retain ];
	[ image release ];
	image = i;
	
}


// Action method called by clicking on the "Reprobe" button

- ( IBAction ) reprobe: ( id ) sender
{

#pragma unused ( sender )
	
	IOReturn	result = kIOReturnSuccess;
	
	// Call the ReprobeDomainTarget method and pass the domainID
	// and targetID for this object since it is the one we are
	// reprobing.
	result = ReprobeDomainTarget ( [ domainIdentifier intValue ],
								   [ deviceIdentifier intValue ] );
	
}


@end
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

#import "AuthoringDevice.h"
#import <IOKit/scsi/IOSCSIMultimediaCommandsDevice.h>


//—————————————————————————————————————————————————————————————————————————————
//	Constants
//—————————————————————————————————————————————————————————————————————————————

#define	kDefaultVendorName		@"No Vendor Name"
#define	kDefaultProductName		@"No Product Name"
#define kImageDVD_RW			@"DVD-RW.icns"
#define kImageDVD_R				@"DVD-R.icns"
#define kImageDVD_RAM			@"DVD-RAM.icns"
#define kImageCD_RW				@"CD-RW.icns"
#define kImageCD_R				@"CD-R.icns"


@implementation AuthoringDevice


//—————————————————————————————————————————————————————————————————————————————
//	+device - factory method
//—————————————————————————————————————————————————————————————————————————————

+ ( AuthoringDevice * ) device
{
	
	return [ [ [ self alloc ] init ] autorelease ];
	
}


//—————————————————————————————————————————————————————————————————————————————
//	isEqual - 	This allows us to tell if two AuthoringDevice objects
// 				are equal. We use the GUID to distinguish
//				objects.
//—————————————————————————————————————————————————————————————————————————————

- ( BOOL ) isEqual: ( AuthoringDevice * ) aDevice
{
	
	// Just compare the GUID ( an NSData * )
	if ( [ [ self theGUID ] isEqualToData: [ aDevice theGUID ] ] )
	{
	
		return YES;
	
	}
	
	return NO;

}


//—————————————————————————————————————————————————————————————————————————————
//	init
//—————————————————————————————————————————————————————————————————————————————

- ( id ) init
{

	[ super init ];

	[ self setTheVendorName: kDefaultVendorName ];
	[ self setTheProductName: kDefaultProductName ];
	
	return self;

}


//—————————————————————————————————————————————————————————————————————————————
//	dealloc
//—————————————————————————————————————————————————————————————————————————————

- ( void ) dealloc
{
	
	[ image release ];
	[ self setTheVendorName: nil ];
	[ self setTheProductName: nil ];
	
	[ super dealloc ];

}


#if 0
#pragma mark -
#pragma mark Accessor Methods
#pragma mark -
#endif


// This gets called for the first column to get the correct image to display.
- ( NSImage * ) theDeviceType
{
	
	if ( image == nil )
	{
		
		NSString *	imagePath 		= nil;
		NSImage *	finalImage		= nil;
		NSImage *	badge			= nil;
		NSImage *	firstImage		= nil;
		
		if ( dvdFeatures & kDVDFeaturesReWriteableMask )
		{
			imagePath = kImageDVD_RW;
		}
		
		else if ( dvdFeatures & kDVDFeaturesRandomWriteableMask )
		{
			imagePath = kImageDVD_RAM;
		}
		
		else if ( dvdFeatures & kDVDFeaturesWriteOnceMask )
		{
			imagePath = kImageDVD_R;
		}
		
		else if ( cdFeatures & kCDFeaturesReWriteableMask )
		{
			imagePath = kImageCD_RW;
		}
		
		else if ( cdFeatures & kCDFeaturesWriteOnceMask )
		{
			imagePath = kImageCD_R;
		}
		
		firstImage = [ NSImage imageNamed: imagePath ];
		[ firstImage setScalesWhenResized: YES ];
		[ firstImage setSize: NSMakeSize ( 128, 128 ) ];
		
		badge = [ self physicalInterconnectBadge ];
		
		finalImage = [ [ NSImage alloc ] initWithSize: NSMakeSize ( 128, 128 ) ];
		[ finalImage lockFocus ];
		
		[ firstImage compositeToPoint: NSZeroPoint operation: NSCompositeSourceOver ];
		[ badge compositeToPoint: NSMakePoint ( 80, 0 ) operation: NSCompositeSourceOver ];
		
		[ finalImage unlockFocus ];
		image = [ finalImage autorelease ];
		
	}
	
	return image;
	
}

// This gets called to get/set the vendor name.
- ( NSString * ) theVendorName { return theVendorName; }
- ( void ) setTheVendorName : ( NSString * ) value
{

	[ value retain ];
	[ theVendorName release ];
	theVendorName = value;

}

// This gets called to get/set the product name.
- ( NSString * ) theProductName { return theProductName; }
- ( void ) setTheProductName : ( NSString * ) value
{

	[ value retain ];
	[ theProductName release ];
	theProductName = value;

}

// This gets called to get/set the product revision level.
- ( NSString * ) theProductRevisionLevel { return theProductRevisionLevel; }
- ( void ) setTheProductRevisionLevel : ( NSString * ) value
{

	[ value retain ];
	[ theProductRevisionLevel release ];
	theProductRevisionLevel = value;

}

// This gets called to get/set the interface.
- ( NSString * ) thePhysicalInterconnect { return thePhysicalInterconnect; }
- ( void ) setThePhysicalInterconnect : ( NSString * ) value
{
	
	[ value retain ];
	[ thePhysicalInterconnect release ];
	thePhysicalInterconnect = value;
	
}

- ( NSImage * ) physicalInterconnectBadge
{
	
	NSSize		size	= { 48.0, 48.0 };
	NSImage *	badge	= nil;
	
	badge = [ NSImage imageNamed: thePhysicalInterconnect ];
	
	if ( badge != nil )
	{
		[ badge setScalesWhenResized: YES ];
		[ badge setSize: size ];
	}
	
	return badge;
	
}


// This gets called to get/set the GUID.
- ( NSData * ) theGUID { return theGUID; }
- ( void ) setTheGUID : ( NSData * ) value
{
	
	[ value retain ];
	[ theGUID release ];
	theGUID = value;
	
}

- ( void ) setCDFeatures: ( UInt32 ) value
{
	cdFeatures = value;
}

- ( void ) setDVDFeatures: ( UInt32 ) value
{
	dvdFeatures = value;
}



@end
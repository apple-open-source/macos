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

#import "InterfaceController.h"
#import "MyDocument.h"
#import "DeviceDataSource.h"
#import "AuthoringDevice.h"
#import "VerticallyCenteredTextFieldCell.h"
#import "ImageAndTextCell.h"


//—————————————————————————————————————————————————————————————————————————————
//	Constants
//—————————————————————————————————————————————————————————————————————————————

#define kCDImagePath	@"CD-R"


@implementation InterfaceController


//—————————————————————————————————————————————————————————————————————————————
//	init
//—————————————————————————————————————————————————————————————————————————————

- ( id ) init
{
			
	[ super init ];
	
	// Create the document array. We keep an array of documents so that in case
	// a user selects a drive and then tries to select one again, it just brings
	// the window forward and out of hiding.
	[ self setTheDocumentArray: [ NSMutableArray arrayWithCapacity : 0 ] ];
	
	return self;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	dealloc
//—————————————————————————————————————————————————————————————————————————————

- ( void ) dealloc
{
	
	[ self setTheDocumentArray: nil ];
	[ super dealloc ];
	
}


//—————————————————————————————————————————————————————————————————————————————
//	awakeFromNib
//—————————————————————————————————————————————————————————————————————————————

- ( void ) awakeFromNib
{
	
	// This is a dirty trick we do to set the far left column up so
	// that we can put images in it. We create an NSImageCell and tell
	// it we want an NSImage inside it. Later on, when the selector for
	// that column is called in the data source, the correct image name
	// is returned and that will put the correct image in the column.
	NSImageCell *						theImageCell 	= nil;
	NSImage *							theImage		= nil;
	VerticallyCenteredTextFieldCell *	textCell 		= nil;
	
	theImageCell = [ [ NSImageCell alloc ] init ];
	textCell = [ [ VerticallyCenteredTextFieldCell alloc ] init ];
	theImage = [ NSImage imageNamed: kCDImagePath ];
	
	[ theImageCell setObjectValue: theImage ];
	[ iconColumn setDataCell: theImageCell ];
	[ theImageCell release ];
	
	[ vendorColumn setDataCell: textCell ];
	[ productColumn setDataCell: textCell ];
	[ revisionLevelColumn setDataCell: textCell ];
	[ physicalInterconnectColumn setDataCell: textCell ];
	
	[ textCell release ];
	
	[ [ self theTableView ] setDoubleAction: @selector ( selectDrive: ) ];
	
}


//—————————————————————————————————————————————————————————————————————————————
//	selectDrive:
//—————————————————————————————————————————————————————————————————————————————

- ( void ) selectDrive: ( id ) sender
{
	
	NSNumber * 			selectedIndex	= nil;
	NSEnumerator * 		rowEnumerator	= nil;
	DeviceDataSource *	dataSource		= nil;
	
	rowEnumerator = [ [ self theTableView ] selectedRowEnumerator ];
	
	selectedIndex = [ rowEnumerator nextObject ];
	while ( selectedIndex != nil )
	{
		
		MyDocument * 		theDocument			= nil;
		NSEnumerator * 		documentEnumerator	= nil;
		AuthoringDevice *	theSelectedDevice	= nil;
		id					object				= nil;
		
		documentEnumerator = [ [ self theDocumentArray ] objectEnumerator ];
		
		dataSource = ( DeviceDataSource * ) ( [ [ self theTableView ] dataSource ] );
		theSelectedDevice = [ [ dataSource theDeviceList ] objectAtIndex: [ selectedIndex intValue ] ];
		
		object = [ documentEnumerator nextObject ];
		while ( object != nil )
		{
			
			if ( [ [ object theAuthoringDevice ] isEqual : theSelectedDevice ] )
			{
				
				NSWindow *	theWindow = nil;
				
				theDocument = ( MyDocument * ) object;
				theWindow = [ theDocument theWindow ];
				
				// They selected a window for which we have a document created. Just
				// bring it to the front.
				[ [ theDocument theWindow ] makeKeyAndOrderFront: nil ];
				return;
				
			}
			
			object = [ documentEnumerator nextObject ];
			
		}
		
		// Create a new document
		theDocument = [ MyDocument myDocument ];
		
		[ theDocumentArray addObject: theDocument ];
		
		[ theDocument setTheAuthoringDevice:
			[ [ dataSource theDeviceList ] objectAtIndex: [ selectedIndex intValue ] ] ];
		
		selectedIndex = [ rowEnumerator nextObject ];
		
	}
	
}


#if 0
#pragma mark -
#pragma mark Accessor Methods
#pragma mark -
#endif


- ( NSTableView * ) theTableView { return theTableView; }
- ( NSMutableArray * ) theDocumentArray { return theDocumentArray; }
- ( void ) setTheDocumentArray: ( NSMutableArray * ) value
{
	
	[ value retain ];
	[ theDocumentArray release ];
	theDocumentArray = value;
	
}


@end
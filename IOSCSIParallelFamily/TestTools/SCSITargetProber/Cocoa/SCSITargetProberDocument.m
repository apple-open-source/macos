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

#import "SCSITargetProberDocument.h"
#import "SCSITargetProberKeys.h"
#import <Foundation/NSKeyValueObserving.h>


//—————————————————————————————————————————————————————————————————————————————
//	Constants
//—————————————————————————————————————————————————————————————————————————————

static NSString * SCSIDocToolbarIdentifier	= @"com.apple.dts.SCSIDocToolbarIdentifier";
static NSString * SCSIDocInfoIdentifier		= @"com.apple.dts.SCSIDocInfoIdentifier";
static NSString * SCSIDocPrefsIdentifier	= @"com.apple.dts.SCSIDocPrefsIdentifier";
static NSString * SCSIDocNibName			= @"SCSITargetProberDocument";

#define kNumTableColumns	5


//—————————————————————————————————————————————————————————————————————————————
//	Implementation
//—————————————————————————————————————————————————————————————————————————————

@implementation SCSITargetProberDocument


// Factory method to create a new SCSITargetProberDocument.

+ ( id ) newDocument: ( NSArray * ) i
{
	
	id	doc = [ [ SCSITargetProberDocument alloc ] initWithInitiators: i ];
	[ NSBundle loadNibNamed: SCSIDocNibName owner: doc ];
	[ doc setupToolbar ];
	
	return doc;
	
}


- ( id ) initWithInitiators: ( NSArray * ) i
{
	
	self = [ super init ];
	if ( self != nil )
	{
		
		[ self setInvisibleTableColumns: [ [ [ NSMutableDictionary alloc ] initWithCapacity: kNumTableColumns ] autorelease ] ];
		[ self setInitiators: i ];
		[ controller setSelectionIndex: 0 ];
		
	}
	
	return self;
	
}


- ( void ) dealloc
{
	
	[ self setInvisibleTableColumns: nil ];
	[ self setInitiators: nil ];
	[ super dealloc ];
	
}


- ( NSString *	) windowNibName
{
	return SCSIDocNibName;
}


// Called when the SCSITargetProberDocument nib loads.

- ( void ) awakeFromNib
{
	
	id		values = nil;
	id		column = nil;
	
	// We use a clever caching scheme for the table columns. We save the
	// NSTableColumn in an NSDictionary and use the column identifier as the
	// dictionary key. This allows us to 1) retain the NSTableColumn
	// and 2) allows us to easily look up the table column when it comes time to
	// add it back into the NSTableView. Retaining the NSTableColumn is
	// beneficial because it preserves the Cocoa Bindings we set up in
	// InterfaceBuilder and it means we don't have to create an NSTableColumn
	// on-the-fly AND establish bindings for it (which is a very tedious process).
	//
	// NB: This isn't always a win (it requires more memory), but it is a win in this
	// case since the number of table columns maxes out at 5.
	
	// Pull the initial user defaults to determine which columns to show. By
	// default, we built the nib to have all 5 possible columns.
	// We remove whichever ones aren't required, based on saved/default preferences.
	values = [ [ NSUserDefaultsController sharedUserDefaultsController ] values ];
	
	// Does the user want the "ID" field visible?
	if ( [ [ values valueForKey: kShowTargetIDString ] boolValue ] == NO )
	{
		
		// No, remove the table column.
		column = [ table tableColumnWithIdentifier: kTableColumnIDString ];
		[ invisibleTableColumns setObject: column forKey: kTableColumnIDString ];
		[ table removeTableColumn: column ];
		
	}
	
	// Does the user want the "Description" field visible?
	if ( [ [ values valueForKey: kShowDescriptionString ] boolValue ] == NO )
	{
		
		// No, remove the table column.
		column = [ table tableColumnWithIdentifier: kTableColumnDescriptionString ];
		[ invisibleTableColumns setObject: column forKey: kTableColumnDescriptionString ];
		[ table removeTableColumn: column ];
		
	}
	
	// Does the user want the "Revision" field visible?
	if ( [ [ values valueForKey: kShowRevisionString ] boolValue ] == NO )
	{
		
		// No, remove the table column.
		column = [ table tableColumnWithIdentifier: kTableColumnRevisionString ];
		[ invisibleTableColumns setObject: column forKey: kTableColumnRevisionString ];
		[ table removeTableColumn: column ];
		
	}
	
	// Does the user want the "Features" field visible?
	if ( [ [ values valueForKey: kShowFeaturesString ] boolValue ] == NO )
	{
		
		// No, remove the table column.
		column = [ table tableColumnWithIdentifier: kTableColumnFeaturesString ];
		[ invisibleTableColumns setObject: column forKey: kTableColumnFeaturesString ];
		[ table removeTableColumn: column ];
		
	}
	
	// Does the user want the "PDT" field visible?
	if ( [ [ values valueForKey: kShowPDTString ] boolValue ] == NO )
	{
		
		// No, remove the table column.
		column = [ table tableColumnWithIdentifier: kTableColumnPDTString ];
		[ invisibleTableColumns setObject: column forKey: kTableColumnPDTString ];
		[ table removeTableColumn: column ];
		
	}
	
	// Observe any value changes to NSUserDefaultsController. This registers us
	// for notifications when the prefs window checkboxes change.
	[ [ NSUserDefaultsController sharedUserDefaultsController ] addObserver: self
		forKeyPath: kShowTargetIDKeyPath options: NSKeyValueObservingOptionNew context: nil ];

	[ [ NSUserDefaultsController sharedUserDefaultsController ] addObserver: self
		forKeyPath: kShowDescriptionKeyPath options: NSKeyValueObservingOptionNew context: nil ];

	[ [ NSUserDefaultsController sharedUserDefaultsController ] addObserver: self
		forKeyPath: kShowRevisionKeyPath options: NSKeyValueObservingOptionNew context: nil ];

	[ [ NSUserDefaultsController sharedUserDefaultsController ] addObserver: self
		forKeyPath: kShowFeaturesKeyPath options: NSKeyValueObservingOptionNew context: nil ];

	[ [ NSUserDefaultsController sharedUserDefaultsController ] addObserver: self
		forKeyPath: kShowPDTKeyPath options: NSKeyValueObservingOptionNew context: nil ];
	
}


// Called when values change for NSUserDefaultsController.
// See NSKeyValueObserving.h for more information.

- ( void ) observeValueForKeyPath: ( NSString * ) keyPath
						 ofObject: ( id ) object
						   change: ( NSDictionary * ) change
						  context: ( void * ) context
{

#pragma unused ( change )
#pragma unused ( context )
	
	id			column 			= nil;
	BOOL		showTargetID	= NO;
	BOOL		showDescription	= NO;
	BOOL		showRevision	= NO;
	BOOL		showFeatures	= NO;
	BOOL		showPDT			= NO;
	int			numColumns 		= 0;
	
	// Get the new defaults.
	showTargetID 	= [ [ [ object values ] valueForKey: kShowTargetIDString ] boolValue ];
	showDescription = [ [ [ object values ] valueForKey: kShowDescriptionString ] boolValue ];
	showRevision 	= [ [ [ object values ] valueForKey: kShowRevisionString ] boolValue ];
	showFeatures 	= [ [ [ object values ] valueForKey: kShowFeaturesString ] boolValue ];
	showPDT 		= [ [ [ object values ] valueForKey: kShowPDTString ] boolValue ];
	numColumns		= [ table numberOfColumns ];
	
	// Is the key path which changed "ID"?
	if ( [ keyPath isEqual: kShowTargetIDKeyPath ] )
	{
		
		// Yes, it changed. What is the value?
		if ( showTargetID == NO )
		{
			
			// "ID" column should not be visible. Remove it from the table column.
			column = [ table tableColumnWithIdentifier: kTableColumnIDString ];
			[ invisibleTableColumns setObject: column forKey: kTableColumnIDString ];
			[ table removeTableColumn: column ];
			
		}
		
		else
		{
			
			// "ID" column should be visible. Add the table column back into the table.
			column = [ invisibleTableColumns objectForKey: kTableColumnIDString ];
			[ table addTableColumn: column ];
			
			// Since addTableColumn adds the column at the end, we have to
			// readjust the table columns. Wish there were an
			// addTableColumn: atIndex: method...
			if ( numColumns > 0 )
			{
				[ table moveColumn: numColumns toColumn: 0 ];
			}
			[ invisibleTableColumns removeObjectForKey: kTableColumnIDString ];
			
		}
		
	}
	
	// Is the key path which changed "Description"?
	if ( [ keyPath isEqual: kShowDescriptionKeyPath ] )
	{
		
		// Yes, it changed. What is the value?
		if ( showDescription == NO )
		{
			
			// "Description" column should not be visible. Remove it from the table column.
			column = [ table tableColumnWithIdentifier: kTableColumnDescriptionString ];
			[ invisibleTableColumns setObject: column forKey: kTableColumnDescriptionString ];
			[ table removeTableColumn: column ];
			
		}
		
		else
		{
			
			int columnNumber = 0;
			
			// "Description" column should be visible. Add the table column back into the table.
			column = [ invisibleTableColumns objectForKey: kTableColumnDescriptionString ];
			[ table addTableColumn: column ];

			// Since addTableColumn adds the column at the end, we have to
			// readjust the table columns.
			if ( showTargetID == YES )
			{
				columnNumber++;
			}
			
			if ( numColumns != columnNumber )
			{
				[ table moveColumn: numColumns toColumn: columnNumber ];
			}
			
			[ invisibleTableColumns removeObjectForKey: kTableColumnDescriptionString ];
			
		}
		
	}

	// Is the key path which changed "Revision"?
	if ( [ keyPath isEqual: kShowRevisionKeyPath ] )
	{
		
		// Yes, it changed. What is the value?
		if ( showRevision == NO )
		{
			
			// "Revision" column should not be visible. Remove it from the table column.
			column = [ table tableColumnWithIdentifier: kTableColumnRevisionString ];
			[ invisibleTableColumns setObject: column forKey: kTableColumnRevisionString ];
			[ table removeTableColumn: column ];
			
		}
		
		else
		{
			
			int columnNumber = 0;
			
			// "Revision" column should be visible. Add the table column back into the table.
			column = [ invisibleTableColumns objectForKey: kTableColumnRevisionString ];
			[ table addTableColumn: column ];
			
			// Since addTableColumn adds the column at the end, we have to
			// readjust the table columns.
			if ( showTargetID == YES )
			{
				columnNumber++;
			}
			
			if ( showDescription == YES )
			{
				columnNumber++;
			}
			
			if ( numColumns != columnNumber )
			{
				[ table moveColumn: numColumns toColumn: columnNumber ];
			}
			
			[ invisibleTableColumns removeObjectForKey: kTableColumnRevisionString ];
			
		}
		
	}
	
	// Is the key path which changed "Features"?
	if ( [ keyPath isEqual: kShowFeaturesKeyPath ] )
	{
	
		// Yes, it changed. What is the value?
		if ( showFeatures == NO )
		{
			
			// "Features" column should not be visible. Remove it from the table column.
			column = [ table tableColumnWithIdentifier: kTableColumnFeaturesString ];
			[ invisibleTableColumns setObject: column forKey: kTableColumnFeaturesString ];
			[ table removeTableColumn: column ];
			
		}
		
		else
		{
			
			int columnNumber = 0;
			
			// "Revision" column should be visible. Add the table column back into the table.
			column = [ invisibleTableColumns objectForKey: kTableColumnFeaturesString ];
			[ table addTableColumn: column ];
			
			// Since addTableColumn adds the column at the end, we have to
			// readjust the table columns.
			if ( showTargetID == YES )
			{
				columnNumber++;
			}
			
			if ( showDescription == YES )
			{
				columnNumber++;
			}
			
			if ( showRevision == YES )
			{
				columnNumber++;
			}
			
			if ( numColumns != columnNumber )
			{
				[ table moveColumn: numColumns toColumn: columnNumber ];
			}
			
			[ invisibleTableColumns removeObjectForKey: kTableColumnFeaturesString ];
			
		}
		
	}
	
	// Is the key path which changed "PDT"?
	if ( [ keyPath isEqual: kShowPDTKeyPath ] )
	{
		
		// Yes, it changed. What is the value?
		if ( showPDT == NO )
		{
			
			// "PDT" column should not be visible. Remove it from the table column.
			column = [ table tableColumnWithIdentifier: kTableColumnPDTString ];
			[ invisibleTableColumns setObject: column forKey: kTableColumnPDTString ];
			[ table removeTableColumn: column ];
			
		}
		
		else
		{
			
			// int columnNumber = 0;
			
			// "PDT" column should be visible. Add the table column back into the table.
			column = [ invisibleTableColumns objectForKey: kTableColumnPDTString ];
			[ table addTableColumn: column ];
			
			// Since PDT is always the last column (for now), it will
			// automatically be added as the last column by addTableColumn:
			// so we don't need to do any moving around of the column...
			
			/*
			if ( showTargetID == YES )
			{
				columnNumber++;
			}
			
			if ( showDescription == YES )
			{
				columnNumber++;
			}
			
			if ( showRevision == YES )
			{
				columnNumber++;
			}
			
			if ( showFeatures == YES )
			{
				columnNumber++;
			}
			
			if ( numColumns != columnNumber )
			{
				[ table moveColumn: numColumns toColumn: columnNumber ];
			}
			*/
			
			[ invisibleTableColumns removeObjectForKey: kTableColumnPDTString ];
			
		}
		
	}
	
}


- ( void ) setInvisibleTableColumns: ( NSMutableDictionary * ) d
{
	
	[ d retain ];
	[ invisibleTableColumns release ];
	invisibleTableColumns = d;
	
}


- ( void ) setInitiators: ( NSArray * ) i
{
	
	[ i retain ];
	[ initiators release ];
	initiators = i;
	
}


- ( NSArray * ) initiators { return initiators; }


- ( BOOL ) drawerVisible { return drawerVisible; }


- ( void ) setDrawerVisible: ( BOOL ) visible
{
	drawerVisible = visible;
}


// Action called to change the drawer state. This is hooked
// up to the Get Info / Hide Info button in the Toolbar.

- ( IBAction ) toggleDrawer: ( id ) sender
{

#pragma unused ( sender )
	
	BOOL			visible = NO;
	NSToolbar *		toolbar = nil;
	
	visible = ![ self drawerVisible ];
	
	toolbar = [ proberWindow toolbar ];
	if ( toolbar != nil )
	{
		
		NSArray *		items	= nil;
		
		items = [ toolbar items ];
		if ( items != nil )
		{
			
			int				count	= [ items count ];
			int				index	= 0;
			NSToolbarItem * item	= nil;
			
			for ( index = 0; index < count; index++ )
			{
				
				item = [ items objectAtIndex: index ];
				if ( [ [ item itemIdentifier ] isEqual: SCSIDocInfoIdentifier ] )
				{
					
					NSString *	localizedString = nil;
					
					if ( visible )
					{
						
						localizedString = [ [ NSBundle mainBundle ] localizedStringForKey: kHideInfoToolbarItemString
																	value: @"Hide Info (Not Localized)"
																	table: nil ];
						
					}
					
					else
					{
						
						localizedString = [ [ NSBundle mainBundle ] localizedStringForKey: kGetInfoToolbarItemString
																	value: @"Get Info (Not Localized)"
																	table: nil ];
						
					}
					
					// Set the text label to be displayed in the toolbar and customization palette 
					[ item setLabel: localizedString ];
					[ item setPaletteLabel: localizedString ];
					
					if ( visible )
					{
						
						localizedString = [ [ NSBundle mainBundle ] localizedStringForKey: kHideInfoToolbarItemToolTipString
																	value: @"Hide Info (Not Localized)"
																	table: nil ];
						
					}
					
					else
					{
						
						localizedString = [ [ NSBundle mainBundle ] localizedStringForKey: kGetInfoToolbarItemToolTipString
																	value: @"Get Info (Not Localized)"
																	table: nil ];
						
					}
					
					[ item setToolTip: localizedString ];
					
				}
				
			}
			
		}
	
	}
	
	// NB: Must use accessor method to change the drawer's visiblity since
	// the drawer's visiblity binding is bound to the "visible" ivar for this class.
	// Bindings require that you use accessors. See the Cocoa Bindings
	// documentation for more information.
	[ self setDrawerVisible: visible ];
	
}


#if 0
#pragma mark -
#pragma mark Toolbar Support
#pragma mark -
#endif


- ( void ) setupToolbar
{
	
	// Create a new toolbar instance, and attach it to our document window 
	NSToolbar * toolbar = [ [ [ NSToolbar alloc ] initWithIdentifier: SCSIDocToolbarIdentifier ] autorelease ];
	
	// Set up toolbar properties: Allow customization, give a default display mode,
	// and remember state in user defaults.
	[ toolbar setAllowsUserCustomization: YES ];
	[ toolbar setAutosavesConfiguration: YES ];
	[ toolbar setDisplayMode: NSToolbarDisplayModeIconAndLabel ];
	
	// We are the delegate.
	[ toolbar setDelegate: self ];
	
	// Attach the toolbar to the document window.
	[ proberWindow setToolbar: toolbar ];
	[ proberWindow makeKeyAndOrderFront: self ];
	
}


- ( NSToolbarItem * ) toolbar: ( NSToolbar * ) toolbar
		itemForItemIdentifier: ( NSString * ) itemIdent
	willBeInsertedIntoToolbar: ( BOOL ) willBeInserted
{

#pragma unused ( toolbar )
#pragma unused ( willBeInserted )
	
	// Required delegate method:  Given an item identifier, this method returns an item.
	// The toolbar will use this method to obtain toolbar items that can be displayed
	// in the customization sheet, or in the toolbar itself.
	NSToolbarItem * toolbarItem = [ [ [ NSToolbarItem alloc ] initWithItemIdentifier: itemIdent ] autorelease ];
	
	if ( [ itemIdent isEqual: SCSIDocInfoIdentifier ] )
	{
		
		NSString * localizedString = nil;

		localizedString = [ [ NSBundle mainBundle ] localizedStringForKey: kGetInfoToolbarItemString
													value: @"Get Info (Not Localized)"
													table: nil ];
		
		// Set the text label to be displayed in the toolbar and customization palette.
		[ toolbarItem setLabel: localizedString ];
		[ toolbarItem setPaletteLabel: localizedString ];

		localizedString = [ [ NSBundle mainBundle ] localizedStringForKey: kGetInfoToolbarItemToolTipString
													value: @"Get Info (Not Localized)"
													table: nil ];
		
		// Set up a reasonable tooltip and image.
		[ toolbarItem setToolTip: localizedString ];
		[ toolbarItem setImage: [ NSImage imageNamed: kInfoImageString ] ];
		
		// Tell the item what message to send when it is clicked.
		[ toolbarItem setTarget: self ];
		[ toolbarItem setAction: @selector ( toggleDrawer: ) ];
		
	}
	
	else if ( [ itemIdent isEqual: SCSIDocPrefsIdentifier ] )
	{
		
		NSString * localizedString = nil;
		
		localizedString = [ [ NSBundle mainBundle ] localizedStringForKey: kShowPrefsToolbarItemString
													value: @"Show Preferences (Not Localized)"
													table: nil ];
		
		// Set the text label to be displayed in the toolbar and customization palette.
		[ toolbarItem setLabel: localizedString ];
		[ toolbarItem setPaletteLabel: localizedString ];

		localizedString = [ [ NSBundle mainBundle ] localizedStringForKey: kShowPrefsToolbarItemToolTipString
													value: @"Show Preferences (Not Localized)"
													table: nil ];
		
		// Set up a reasonable tooltip and image.
		[ toolbarItem setToolTip: localizedString ];
		[ toolbarItem setImage: [ NSImage imageNamed: kPrefsImageString ] ];
		
		// Tell the item what message to send when it is clicked.
		[ toolbarItem setTarget: [ [ NSApplication sharedApplication ] delegate ] ];
		[ toolbarItem setAction: @selector ( showPrefs: ) ];
		
	}
	
	else
	{
		
		// itemIdent refered to a toolbar item that is not provided or supported by us or Cocoa.
		// Returning nil will inform the toolbar this kind of item is not supported.
		toolbarItem = nil;
		
	}
	
	return toolbarItem;
	
}


- ( void ) toolbarWillAddItem: ( NSNotification * ) notif
{
	
	// Optional delegate method:  Before an new item is added to the toolbar, this notification is posted.
	// This is the best place to notice a new item is going into the toolbar.  For instance, if you need to 
	// cache a reference to the toolbar item or need to set up some initial state, this is the best place 
	// to do it.  The notification object is the toolbar to which the item is being added.	The item being 
	// added is found by referencing the @"item" key in the userInfo.
	
	NSToolbarItem *	addedItem = [ [ notif userInfo ] objectForKey: @"item" ];
	if ( [ [ addedItem itemIdentifier ] isEqual: NSToolbarPrintItemIdentifier ] )
	{
		
		// Make sure we set ourselves as the target for the "Print" item.
		// This causes the button to be enabled (rather than disabled), and
		// to send the -printDocument: action to use when the button is clicked.
		[ addedItem setTarget: self ];
		
	}
	
}  


- ( NSArray * ) toolbarDefaultItemIdentifiers: ( NSToolbar * ) toolbar
{

#pragma unused ( toolbar )
	
	// Required delegate method:  Returns the ordered list of items to be shown in the toolbar by default	 
	// If during the toolbar's initialization, no overriding values are found in the user defaults, or if the
	// user chooses to revert to the default items this set will be used. Make sure the array is terminated
	// with a nil element!
	return [ NSArray arrayWithObjects:	SCSIDocInfoIdentifier,
										SCSIDocPrefsIdentifier,
										NSToolbarFlexibleSpaceItemIdentifier,
										NSToolbarPrintItemIdentifier,
										NSToolbarCustomizeToolbarItemIdentifier,
										nil ];
	
}


- ( NSArray * ) toolbarAllowedItemIdentifiers: ( NSToolbar * ) toolbar
{

#pragma unused ( toolbar )
	
	// Required delegate method:  Returns the list of all allowed items by identifier.	By default, the toolbar 
	// does not assume any items are allowed, even the separator.  So, every allowed item must be explicitly listed	  
	// The set of allowed items is used to construct the customization palette. Make sure the array is terminated
	// with a nil element!
	return [ NSArray arrayWithObjects:	SCSIDocInfoIdentifier,
										SCSIDocPrefsIdentifier,
										NSToolbarPrintItemIdentifier,
										NSToolbarCustomizeToolbarItemIdentifier,
										NSToolbarFlexibleSpaceItemIdentifier,
										NSToolbarSpaceItemIdentifier,
										NSToolbarSeparatorItemIdentifier,
										nil ];
	
}


- ( BOOL ) validateToolbarItem: ( NSToolbarItem * ) toolbarItem
{

#pragma unused ( toolbarItem )
	
	return YES;
	
}


#if 0
#pragma mark -
#pragma mark Printing Support
#pragma mark -
#endif


// Action called by the "Print" Toolbar button.

- ( IBAction ) printDocument: ( id ) sender
{

#pragma unused ( sender )
	
	// This message is sent by the print toolbar item.
	NSPrintOperation *	printOperation = nil;
	
	printOperation = [ NSPrintOperation printOperationWithView: [ proberWindow contentView ] ];
	[ printOperation runOperationModalForWindow: proberWindow delegate: nil didRunSelector: NULL contextInfo: NULL ];
	
}


@end
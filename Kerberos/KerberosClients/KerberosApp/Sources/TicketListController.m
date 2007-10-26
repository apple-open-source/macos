/*
 * TicketListController.m
 *
 * $Header$
 *
 * Copyright 2004 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#import "TicketListController.h"
#import "TicketInfoController.h"
#import "KerberosCacheCollection.h"
#import "KerberosCache.h"
#import "Utilities.h"
#import "KerberosErrorAlert.h"
#import "KerberosPreferences.h"

NSString *ActiveUserMenuToolbarItemIdentifier = @"ActiveUserMenu";
NSString *GetTicketsToolbarItemIdentifier     = @"GetTickets";
NSString *RenewTicketsToolbarItemIdentifier   = @"RenewTickets";
NSString *DestroyTicketsToolbarItemIdentifier = @"DestroyTickets";
NSString *ChangePasswordToolbarItemIdentifier = @"ChangePassword";
NSString *TicketInfoToolbarItemIdentifier     = @"TicketInfo";

NSString *TicketListFrameAutosaveName         = @"KATicketListWindowPosition";

@interface ActiveUserToolbarItem : NSToolbarItem
{
}

- (id) initWithItemIdentifier: (NSString *) itemIdentifier;
- (void) validate;
- (void) menuNeedsUpdate: (NSMenu *) menu;

@end

@implementation ActiveUserToolbarItem

// ---------------------------------------------------------------------------

- (id) initWithItemIdentifier: (NSString *) itemIdentifier
{
    if ((self = [super initWithItemIdentifier: itemIdentifier])) {
        NSRect frameRect = { { 0, 0 }, { 150, 22 } };
        
        NSPopUpButton *button = [[[NSPopUpButton alloc] initWithFrame: frameRect pullsDown: NO] autorelease];
        if (!button) {
            [self release];
            return NULL;
        }
        
        [button setAutoenablesItems: NO];  // item state set by menuNeedsUpdate:
        [[button menu] setDelegate: self];
        [[button cell] setControlSize: NSSmallControlSize];

        NSSize minimumSize = [button bounds].size;
        NSSize maximumSize = [button bounds].size;
        maximumSize.width += 200;
        
        [self setView: button];
        [self setMinSize: minimumSize];
        [self setMaxSize: maximumSize];
    }
    return self;
}

// ---------------------------------------------------------------------------

- (void) validate
{
    NSMenu *menu = [[self view] menu];
    if (menu) {
        [self menuNeedsUpdate: menu];
    }
}

// ---------------------------------------------------------------------------

- (void) menuNeedsUpdate: (NSMenu *) menu
{
    // Assume all menus are the active user popup menu
    // If we get a second menu type, we'll have to figure out something else
    int defaultCacheIndex = 0;
    
    [Utilities synchronizeCacheMenu: menu 
                              popup: YES
             staticPrefixItemsCount: 0
                         headerItem: NO
                  checkDefaultCache: YES
                  defaultCacheIndex: &defaultCacheIndex
                           selector: @selector (changeActiveUser:)
                             sender: [self target]];
    
    NSPopUpButton *popUpButton = (NSPopUpButton *) [self view];
    if ((popUpButton != NULL) && [popUpButton isKindOfClass: [NSPopUpButton class]]) {
        [popUpButton selectItemAtIndex: defaultCacheIndex];
    }
}

@end

#pragma mark -

@implementation TicketListController

// ---------------------------------------------------------------------------

- (id) init
{
    if ((self = [super initWithWindowNibName: @"TicketList"])) {
        cacheCollection = NULL;
        toolbar = NULL;
        minuteTimer = NULL;
        cacheNameString = NULL;
        cascadePoint = NSZeroPoint;
        
        cacheCollection = [[KerberosCacheCollection sharedCacheCollection] retain];
        if (!cacheCollection) {
            [self release];
            return NULL;
        }
        
        toolbar = [[NSToolbar alloc] initWithIdentifier: @"TicketListToolbar"];
        if (!toolbar) {
            [self release];
            return NULL;
        }
        [toolbar setAutosavesConfiguration: YES];
    }
    
    return self;
}

// ---------------------------------------------------------------------------

- (void) dealloc
{
    dprintf ("Ticket List window %lx releasing...", (long) self);
    [[NSNotificationCenter defaultCenter] removeObserver: self];

    if (minuteTimer) { 
        // invalidate a TargetOwnedTimer before releasing it
        [minuteTimer invalidate]; 
        [minuteTimer release]; 
    }

    if (cacheCollection) { [cacheCollection release]; }
    if (cacheNameString) { [cacheNameString release]; }
    if (toolbar        ) { [toolbar release]; }
    
    [super dealloc];
}

// ---------------------------------------------------------------------------

- (void) windowDidLoad
{
    [super windowDidLoad];
    
    dprintf ("TicketListController %lx entering windowDidLoad:...", (long) self);
    
    minuteTimer = [[TargetOwnedTimer scheduledTimerWithTimeInterval: 30 // controls the minute counters in the window
                                                             target: self 
                                                           selector: @selector (minuteTimer:) 
                                                           userInfo: NULL
                                                            repeats: YES] retain];
    
    // toolbar
    [toolbar setDelegate: self];
    [[self window] setToolbar: toolbar];
    
    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector (cacheCollectionDidChange:)
                                                 name: CacheCollectionDidChangeNotification
                                               object: nil];
    
    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector (preferencesDidChange:)
                                                 name: PreferencesDidChangeNotification
                                               object: nil];
    
    // load the first time manually.  After that the notifications will do the work
    [self cacheCollectionDidChange: NULL]; 
}

// ---------------------------------------------------------------------------

- (IBAction) showWindow: (id) sender
{    
    // Check to see if the window was closed before. ([self window] will load the window)
    if (![[self window] isVisible]) {
        dprintf ("TicketListController %lx displaying window...", (long) self);
        
        // If the user doesn't want to save the window preferences or 
        // if we failed to read them, center the window on screen
        if ([[KerberosPreferences sharedPreferences] ticketWindowDefaultPosition] ||
            ![[self window] setFrameUsingName: TicketListFrameAutosaveName]) {
            [[self window] center];
        }
        
        [self preferencesDidChange: NULL];
        [self synchronizeCascadePoint];
    }
    
    [super showWindow: sender];
}

#pragma mark --- Actions --

// ---------------------------------------------------------------------------

- (IBAction) getTickets: (id) sender
{
    KLStatus err = [KerberosPrincipal getTickets];
    if (!err) {
        [cacheCollection update];
    } else if (err != klUserCanceledErr) {
        [KerberosErrorAlert alertForError: err
                                   action: KerberosGetTicketsAction
                   modalForWindow: [self window]];
    }        
}

// ---------------------------------------------------------------------------

- (IBAction) changePasswordForSelectedCache: (id) sender
{
    KerberosCache *cache = [self selectedCache];
    if (cache) {
        KLStatus err = [[cache principal] changePassword];
        if (!err) {
            [cacheCollection update];
        } else if (err != klUserCanceledErr) {
            [KerberosErrorAlert alertForError: err
                                       action: KerberosChangePasswordAction
                       modalForWindow: [self window]];
        }        
    }
}

// ---------------------------------------------------------------------------

- (IBAction) destroyTicketsForSelectedCache: (id) sender
{
    KerberosCache *cache = [self selectedCache];
    if (cache) {
        KLStatus err = [[cache principal] destroyTickets];
        if (!err) {
            [cacheCollection update];
        } else if (err != klUserCanceledErr) {
            [KerberosErrorAlert alertForError: err
                                       action: KerberosDestroyTicketsAction
                       modalForWindow: [self window]];
        }        
    }
}

// ---------------------------------------------------------------------------

- (IBAction) renewTicketsForSelectedCache: (id) sender
{
    KerberosCache *cache = [self selectedCache];
    if (cache) {
        KLStatus err = [[cache principal] renewTickets];
        if (!err) {
            [cacheCollection update];
        } else if (err != klUserCanceledErr) {
            [KerberosErrorAlert alertForError: err
                                       action: KerberosRenewTicketsAction
                       modalForWindow: [self window]];
        }        
    }
}

// ---------------------------------------------------------------------------

- (IBAction) validateTicketsForSelectedCache: (id) sender
{
    KerberosCache *cache = [self selectedCache];
    if (cache) {
        KLStatus err = [[cache principal] validateTickets];
        if (!err) {
            [cacheCollection update];
        } else if (err != klUserCanceledErr) {
            [KerberosErrorAlert alertForError: err
                                       action: KerberosValidateTicketsAction
                       modalForWindow: [self window]];
        }        
    }
}

// ---------------------------------------------------------------------------

- (IBAction) changeActiveUser: (id) sender
{
    if ([sender isMemberOfClass: [NSMenuItem class]]) {
        NSMenuItem *item = sender;
        NSMenu *menu = [item menu];
        
        if ([cacheCollection numberOfCaches] > 0) {
            KerberosCache *cache = [cacheCollection cacheAtIndex: [menu indexOfItem: item]];
            if (![cache isDefault]) {
                KLStatus err = [[cache principal] setDefault];
                if (!err) {
                    [cacheCollection update];
                } else if (err != klUserCanceledErr) {
                    [KerberosErrorAlert alertForError: err
                                               action: KerberosChangeActiveUserAction
                               modalForWindow: [self window]];
                }        
            }
        }
    }
}

// ---------------------------------------------------------------------------

- (IBAction) showTicketInfo: (id) sender
{
    KerberosCredential *credential = [self selectedCredential];
    if (credential) {
        NSWindowController *infoWindowController = [credential infoWindowController];
        if (!infoWindowController) {
            infoWindowController = [[[TicketInfoController alloc] initWithCredential: credential] autorelease];
            cascadePoint = [[infoWindowController window] cascadeTopLeftFromPoint: cascadePoint];
            [credential setInfoWindowController: infoWindowController];
        }
        [infoWindowController showWindow: credential];
    }
}

#pragma mark --- Data Source Methods --

// ---------------------------------------------------------------------------

- (int) numberOfRowsInTableView: (NSTableView *) tableView
{
    if (tableView == cacheCollectionTableView) {
        return [cacheCollection numberOfCaches];
    }
    
    if (tableView == ticketsTableView) {
        KerberosCache *cache = [self selectedCache];
        if (cache) { return [cache numberOfCredentials]; }
    }
    
    return 0;
}

// ---------------------------------------------------------------------------

- (id) tableView: (NSTableView *) tableView objectValueForTableColumn: (NSTableColumn *) tableColumn row: (int) rowIndex
{
    NSAttributedString *string = NULL;
    
    if (tableView == cacheCollectionTableView) {
        KerberosCache *cache = [cacheCollection cacheAtIndex: rowIndex];
        
        if (cache) {
            int state = [cache state];
            cc_time_t timeRemaining = [cache timeRemaining];

            if (tableColumn == cacheCollectionTableColumn) {
                string = [Utilities attributedStringForControlType: kUtilitiesTableCellControlType
                                                            string: [cache principalString]
                                                         alignment: kUtilitiesLeftStringAlignment
                                                              bold: [cache isDefault]
                                                            italic: (state != CredentialValid)
                                                               red: NO];
                
            } else if (tableColumn == cacheCollectionTimeRemainingTableColumn) {
                string = [Utilities attributedStringForControlType: kUtilitiesTableCellControlType
                                                            string: [cache shortTimeRemainingString]
                                                         alignment: kUtilitiesRightStringAlignment
                                                              bold: [cache isDefault]
                                                            italic: (state != CredentialValid)
                                                               red: (state != CredentialValid || 
                                                                     timeRemaining <= kFiveMinutes)];
            }
        } 
    }
    
    if ((tableView == ticketsTableView) && [self hasSelectedCache]) {
        KerberosCredential *credential = [[self selectedCache] credentialAtIndex: rowIndex];

        if (credential) { 
            int state = [credential state];
            cc_time_t timeRemaining = [credential timeRemaining];

            if (tableColumn == ticketsTableColumn) { 
                string = [Utilities attributedStringForControlType: kUtilitiesTableCellControlType
                                                            string: [credential servicePrincipalString]
                                                         alignment: kUtilitiesLeftStringAlignment
                                                              bold: NO
                                                            italic: (state != CredentialValid)
                                                               red: NO];

            } else if (tableColumn == ticketsTimeRemainingTableColumn) {
                string = [Utilities attributedStringForControlType: kUtilitiesTableCellControlType
                                                            string: [credential shortTimeRemainingString]
                                                         alignment: kUtilitiesRightStringAlignment
                                                              bold: NO
                                                            italic: (state != CredentialValid)
                                                               red: (state != CredentialValid || 
                                                                     timeRemaining <= kFiveMinutes)];
            }
        }
    }
    
    return string ? string : @"";
}

#pragma mark --- Delegate Methods --

// ---------------------------------------------------------------------------

- (BOOL) splitView: (NSSplitView *) sender canCollapseSubview: (NSView *) subview
{
    return !([[sender subviews] objectAtIndex: 0] == subview);
}

// ---------------------------------------------------------------------------

- (float) splitView: (NSSplitView *) sender constrainMinCoordinate: (float) proposedMin ofSubviewAt: (int) offset
{
    return (proposedMin + ([[cacheCollectionTableView headerView] frame].size.height + 
                           ([cacheCollectionTableView rowHeight] * 2) + 
                           ([cacheCollectionTableView intercellSpacing].height * 3)));
}

// ---------------------------------------------------------------------------

- (float) splitView: (NSSplitView *) sender constrainMaxCoordinate: (float) proposedMax ofSubviewAt: (int) offset
{
    float m = (proposedMax - ([[ticketsTableView headerView] frame].size.height +
                              ([ticketsTableView rowHeight] * 2) + 
                              ([ticketsTableView intercellSpacing].height * 3)));
    return m;
}

// ---------------------------------------------------------------------------

- (NSToolbarItem *) toolbar: (NSToolbar *) aToolbar 
      itemForItemIdentifier: (NSString *) itemIdentifier 
  willBeInsertedIntoToolbar: (BOOL) flag
{
    NSToolbarItem *toolbarItem = NULL;
    
    if ([itemIdentifier isEqual: ActiveUserMenuToolbarItemIdentifier]) {
        toolbarItem = [[[ActiveUserToolbarItem alloc] initWithItemIdentifier: itemIdentifier] autorelease];    
        
        [toolbarItem setLabel:        NSLocalizedString (@"ActiveUserMenuToolbarItemLabel", NULL)];
        [toolbarItem setPaletteLabel: NSLocalizedString (@"ActiveUserMenuToolbarItemLabel", NULL)];
        [toolbarItem setToolTip:      NSLocalizedString (@"ActiveUserMenuToolbarItemToolTip", NULL)];
        [toolbarItem setTarget:       self];
        
    } else {
        toolbarItem = [[[NSToolbarItem alloc] initWithItemIdentifier: itemIdentifier] autorelease];
        [toolbarItem setTarget: self];
        
        if ([itemIdentifier isEqual: GetTicketsToolbarItemIdentifier]) {
            [toolbarItem setLabel:        NSLocalizedString (@"GetTicketsToolbarItemLabel", NULL)];
            [toolbarItem setPaletteLabel: NSLocalizedString (@"GetTicketsToolbarItemLabel", NULL)];
            [toolbarItem setToolTip:      NSLocalizedString (@"GetTicketsToolbarItemToolTip", NULL)];
            [toolbarItem setImage:        [NSImage imageNamed: @"GetTickets"]];
            [toolbarItem setAction:       @selector (getTickets:)]; 
            
        } else if ([itemIdentifier isEqual: RenewTicketsToolbarItemIdentifier]) {
            [toolbarItem setLabel:        NSLocalizedString (@"RenewTicketsToolbarItemLabel", NULL)];
            [toolbarItem setPaletteLabel: NSLocalizedString (@"RenewTicketsToolbarItemLabel", NULL)];
            [toolbarItem setToolTip:      NSLocalizedString (@"RenewTicketsToolbarItemToolTip", NULL)];
            [toolbarItem setImage:        [NSImage imageNamed: @"RenewTickets"]];
            [toolbarItem setAction:       @selector (renewTicketsForSelectedCache:)]; 
            
        } else if ([itemIdentifier isEqual: DestroyTicketsToolbarItemIdentifier]) {
            [toolbarItem setLabel:        NSLocalizedString (@"DestroyTicketsToolbarItemLabel", NULL)];
            [toolbarItem setPaletteLabel: NSLocalizedString (@"DestroyTicketsToolbarItemLabel", NULL)];
            [toolbarItem setToolTip:      NSLocalizedString (@"DestroyTicketsToolbarItemToolTip", NULL)];
            [toolbarItem setImage:        [NSImage imageNamed: @"DestroyTickets"]];
            [toolbarItem setAction:       @selector (destroyTicketsForSelectedCache:)]; 
            
        } else if ([itemIdentifier isEqual: ChangePasswordToolbarItemIdentifier]) {
            [toolbarItem setLabel:        NSLocalizedString (@"ChangePasswordToolbarItemLabel", NULL)];
            [toolbarItem setPaletteLabel: NSLocalizedString (@"ChangePasswordToolbarItemLabel", NULL)];
            [toolbarItem setToolTip:      NSLocalizedString (@"ChangePasswordToolbarItemToolTip", NULL)];
            [toolbarItem setImage:        [NSImage imageNamed: @"ChangePassword"]];
            [toolbarItem setAction:       @selector (changePasswordForSelectedCache:)]; 
            
        } else if ([itemIdentifier isEqual: TicketInfoToolbarItemIdentifier]) {
            [toolbarItem setLabel:        NSLocalizedString (@"TicketInfoToolbarItemLabel", NULL)];
            [toolbarItem setPaletteLabel: NSLocalizedString (@"TicketInfoToolbarItemLabel", NULL)];
            [toolbarItem setToolTip:      NSLocalizedString (@"TicketInfoToolbarItemToolTip", NULL)];
            [toolbarItem setImage:        [NSImage imageNamed: @"TicketInfo"]];
            [toolbarItem setAction:       @selector (showTicketInfo:)]; 
        } else {
            // Unsupported item type
            toolbarItem = NULL;
        }
    }
    
    return toolbarItem;
}

// ---------------------------------------------------------------------------

- (BOOL) validateToolbarItem: (NSToolbarItem *) theItem
{
    NSString *itemIdentifier = [theItem itemIdentifier];
    
    if ([itemIdentifier isEqual: ActiveUserMenuToolbarItemIdentifier]) {
        return YES;
        
    } else if ([itemIdentifier isEqual: GetTicketsToolbarItemIdentifier]) {
        return YES;
        
    } else if ([itemIdentifier isEqual: RenewTicketsToolbarItemIdentifier]) {
        return [self hasSelectedCache];
        
    } else if ([itemIdentifier isEqual: DestroyTicketsToolbarItemIdentifier]) {
        return [self hasSelectedCache];
        
    } else if ([itemIdentifier isEqual: ChangePasswordToolbarItemIdentifier]) {
        return [self hasSelectedCache];
        
    } else if ([itemIdentifier isEqual: TicketInfoToolbarItemIdentifier]) {
        return [self hasSelectedCredential];
        
    }
    
    // Unsupported item type or error
    return NO;
}

// ---------------------------------------------------------------------------

- (NSArray *) toolbarAllowedItemIdentifiers: (NSToolbar *) aToolbar
{
    return [NSArray arrayWithObjects: 
        ActiveUserMenuToolbarItemIdentifier, 
        NSToolbarFlexibleSpaceItemIdentifier, 
        GetTicketsToolbarItemIdentifier, 
        NSToolbarSeparatorItemIdentifier, 
        RenewTicketsToolbarItemIdentifier, 
        DestroyTicketsToolbarItemIdentifier, 
        ChangePasswordToolbarItemIdentifier, 
        TicketInfoToolbarItemIdentifier, NULL];
}

// ---------------------------------------------------------------------------

- (NSArray *) toolbarDefaultItemIdentifiers: (NSToolbar *) aToolbar
{
    return [NSArray arrayWithObjects: 
        ActiveUserMenuToolbarItemIdentifier, 
        NSToolbarFlexibleSpaceItemIdentifier, 
        GetTicketsToolbarItemIdentifier, 
        NSToolbarSeparatorItemIdentifier, 
        RenewTicketsToolbarItemIdentifier, 
        DestroyTicketsToolbarItemIdentifier, 
        ChangePasswordToolbarItemIdentifier, 
        NSToolbarSeparatorItemIdentifier, 
        TicketInfoToolbarItemIdentifier, NULL];
}

#pragma mark -- Callbacks --

// ---------------------------------------------------------------------------

- (void) minuteTimer: (TargetOwnedTimer *) timer
{
    // Update things with time values in them:
    [cacheCollectionTableView reloadData];
    [ticketsTableView reloadData];
    [self synchronizeWindowTitle];
}

#pragma mark --- Notifications --

// ---------------------------------------------------------------------------

- (void) cacheCollectionDidChange: (NSNotification *) notification
{
    dprintf ("Ticket List window %lx got CacheCollectionDidChangeNotification", (long) self);
    [cacheCollectionTableView reloadData];
    [ticketsTableView reloadData];

    // Attempt to select the cache that was previously selected
    // Make sure we don't select anything while we are still looking at
    // cacheNameString because selecting entries invalidates it
    unsigned int newSelectedIndex = NSNotFound;
    if (cacheNameString) {
        KerberosCache *cache = [cacheCollection findCacheForName: cacheNameString];
        if (cache) {
            newSelectedIndex = [cacheCollection indexOfCache: cache];
        }
    }
    if (newSelectedIndex != NSNotFound) {
        [cacheCollectionTableView selectRowIndexes: [NSIndexSet indexSetWithIndex: newSelectedIndex]
                              byExtendingSelection: NO];
    } else if ([cacheCollection numberOfCaches] > 0) {
        [cacheCollectionTableView selectRowIndexes: [NSIndexSet indexSetWithIndex: 0]
                              byExtendingSelection: NO];  // select first one
    }
    
    [self tableViewSelectionDidChange: NULL];
    
    // Now that the ticket list is up to date, let other objects know about it
    [[NSNotificationCenter defaultCenter] postNotificationName: TicketListDidChangeNotification 
                                                        object: self];
    
    dprintf ("Ticket List window %lx done with CacheCollectionDidChangeNotification", (long) self);
}

// ---------------------------------------------------------------------------

- (void) preferencesDidChange: (NSNotification *) notification
{
    if ([[self window] isVisible]) {
        [[self window] saveFrameUsingName: TicketListFrameAutosaveName];
    }
    
    if ([[KerberosPreferences sharedPreferences] ticketWindowDefaultPosition]) {
        [self setWindowFrameAutosaveName: @""]; // Don't save frame position
    } else {
        [self setWindowFrameAutosaveName: TicketListFrameAutosaveName];
    }
}

// ---------------------------------------------------------------------------

- (void) tableViewSelectionDidChange: (NSNotification *) notification
{    
    NSTableView *tableView = [notification object];
    
    if (tableView == cacheCollectionTableView) {
        [ticketsTableView reloadData];
        
        KerberosCache *cache = [self selectedCache];
        if (cache) {
            // Used when cacheCollection changes out from under us 
            // so the user's selection follows the cache
            if (cacheNameString) { [cacheNameString release]; }
            cacheNameString = [[cache ccacheName] retain];
        } else {
            if (cacheNameString) { [cacheNameString release]; }
            cacheNameString = NULL;
        }
        
        [self synchronizeToolbar];
        [self synchronizeWindowTitle];
        [[NSNotificationCenter defaultCenter] postNotificationName: CacheSelectionDidChangeNotification object: self];
    }
    
    if (tableView == ticketsTableView) {
        [self synchronizeToolbar];
        [self synchronizeWindowTitle];
        [[NSNotificationCenter defaultCenter] postNotificationName: TicketSelectionDidChangeNotification object: self];
    }
}


// ---------------------------------------------------------------------------

- (void) windowDidMove: (NSNotification *) notification
{
    [self synchronizeCascadePoint];
}

#pragma mark -- Utility Functions --

// ---------------------------------------------------------------------------

- (void) synchronizeWindowTitle
{
    KerberosCache *defaultCache = [cacheCollection defaultCache];
    if (defaultCache) {
        [[self window] setTitle: [NSString stringWithFormat: 
            NSLocalizedString (@"KAppStringWindowTitleFormat", NULL), 
            [defaultCache principalString], 
            [defaultCache longTimeRemainingString]]];
    } else {
        [[self window] setTitle: NSLocalizedString (@"KAppStringNoTicketsAvailable", NULL)];
    }
}

// ---------------------------------------------------------------------------

- (void) synchronizeCascadePoint
{
    NSRect frameRect = [[self window] frame];
    NSPoint frameUpperLeft = frameRect.origin;
    frameUpperLeft.y += frameRect.size.height;
    
    NSRect contentRect = [[[self window] contentView] frame];
    NSPoint contentUpperLeft = [[self window] convertBaseToScreen: contentRect.origin];
    contentUpperLeft.y += contentRect.size.height;
    
    float titleBarHeight = frameUpperLeft.y - contentUpperLeft.y;
    
    cascadePoint.x = frameUpperLeft.x + titleBarHeight; 
    cascadePoint.y = frameUpperLeft.y - titleBarHeight;
    
    //NSLog (@"framePoint is (%f, %f)\n", frameUpperLeft.x, frameUpperLeft.y);
    //NSLog (@"cascadePoint is (%f, %f)\n", cascadePoint.x, cascadePoint.y);
}

// ---------------------------------------------------------------------------

- (void) synchronizeToolbar
{
    // Update the toolbar items
    [toolbar validateVisibleItems];
}

// ---------------------------------------------------------------------------

- (BOOL) hasSelectedCache
{
    return ([cacheCollectionTableView selectedRow] >= 0);
}

// ---------------------------------------------------------------------------

- (KerberosCache *) selectedCache
{
    if ([self hasSelectedCache]) {
        return [cacheCollection cacheAtIndex: [cacheCollectionTableView selectedRow]]; 
    } else {
        return NULL;
    }
}

// ---------------------------------------------------------------------------

- (BOOL) selectedCacheNeedsValidation
{
    KerberosCache *selectedCache = [self selectedCache];
    return ((selectedCache != NULL) && [selectedCache needsValidation]);
}

// ---------------------------------------------------------------------------

- (BOOL) hasSelectedCredential
{
    return ([ticketsTableView selectedRow] >= 0);
}

// ---------------------------------------------------------------------------

- (KerberosCredential *) selectedCredential
{
    if ([self hasSelectedCache] && [self hasSelectedCredential]) {
        return [[self selectedCache] credentialAtIndex: [ticketsTableView selectedRow]];
    } else {
        return NULL;
    }
}

@end

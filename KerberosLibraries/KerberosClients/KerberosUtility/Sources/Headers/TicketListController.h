/*
 * TicketListController.h
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

#import "KerberosCacheCollection.h"
#import "KerberosCache.h"

@interface ActiveUserToolbarItem : NSToolbarItem
{
}

- (id) initWithItemIdentifier: (NSString *) itemIdentifier;
- (void) validate;
- (void) menuNeedsUpdate: (NSMenu *) menu;

@end

@interface TicketListController : NSWindowController
{
    IBOutlet NSTableView *cacheCollectionTableView;
    IBOutlet NSTableColumn *cacheCollectionTableColumn;
    IBOutlet NSTableColumn *cacheCollectionTimeRemainingTableColumn;
    
    IBOutlet NSTableView   *ticketsTableView;
    IBOutlet NSTableColumn *ticketsTableColumn;
    IBOutlet NSTableColumn *ticketsTimeRemainingTableColumn;
    
    IBOutlet NSToolbar *toolbar;
    IBOutlet NSToolbarItem *activeUserToolbarItem;
    IBOutlet NSToolbarItem *newToolbarItem;
    IBOutlet NSToolbarItem *renewToolbarItem;
    IBOutlet NSToolbarItem *destroyToolbarItem;
    IBOutlet NSToolbarItem *passwordToolbarItem;
    IBOutlet NSToolbarItem *infoToolbarItem;

    
    TargetOwnedTimer *minuteTimer;
    KerberosCacheCollection *cacheCollection;
    NSString *cacheNameString;
    NSPoint cascadePoint;
}

- (id) init;
- (void) dealloc;

- (void) windowDidLoad;

- (IBAction) showWindow: (id) sender;
- (IBAction) getTickets: (id) sender;
- (IBAction) changePasswordForSelectedCache: (id) sender;
- (IBAction) destroyTicketsForSelectedCache: (id) sender;
- (IBAction) renewTicketsForSelectedCache: (id) sender;
- (IBAction) validateTicketsForSelectedCache: (id) sender;
- (IBAction) changeActiveUser: (id) sender;
- (IBAction) showTicketInfo: (id) sender;

- (int) numberOfRowsInTableView: (NSTableView *) tableView;
- (id) tableView: (NSTableView *) tableView objectValueForTableColumn: (NSTableColumn *) tableColumn row: (int) rowIndex;

- (BOOL) splitView: (NSSplitView *) sender canCollapseSubview: (NSView *) subview;
- (float) splitView: (NSSplitView *) sender constrainMinCoordinate: (float) proposedMin ofSubviewAt: (int) offset;
- (float) splitView: (NSSplitView *) sender constrainMaxCoordinate: (float) proposedMax ofSubviewAt: (int) offset;

- (void) minuteTimer: (TargetOwnedTimer *) timer;

- (void) cacheCollectionDidChange: (NSNotification *) notification;
- (void) preferencesDidChange: (NSNotification *) notification;
- (void) tableViewSelectionDidChange: (NSNotification *) notification;
- (void) windowDidMove: (NSNotification *) notification;

- (void) synchronizeWindowTitle;
- (void) synchronizeCascadePoint;
- (void) synchronizeToolbar;

- (BOOL) hasSelectedCache;
- (KerberosCache *) selectedCache;
- (BOOL) selectedCacheNeedsValidation;
- (BOOL) hasSelectedCredential;
- (KerberosCredential *) selectedCredential;

@end

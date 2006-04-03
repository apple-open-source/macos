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

#import "CacheCollection.h"
#import "Cache.h"

@interface TicketListController : NSWindowController
{
    IBOutlet NSTableView *cacheCollectionTableView;
    IBOutlet NSTableColumn *cacheCollectionTableColumn;
    IBOutlet NSTableColumn *cacheCollectionTimeRemainingTableColumn;
    
    IBOutlet NSOutlineView *ticketsOutlineView;
    IBOutlet NSTableColumn *ticketsTableColumn;
    IBOutlet NSTableColumn *ticketsTimeRemainingTableColumn;
    
    NSToolbar *toolbar;
    
    TargetOwnedTimer *minuteTimer;
    CacheCollection *cacheCollection;
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

- (id)   outlineView: (NSOutlineView *) outlineView child: (int) rowIndex ofItem: (id) item;
- (BOOL) outlineView: (NSOutlineView *) outlineView isItemExpandable: (id) item;
- (int)  outlineView: (NSOutlineView *) outlineView numberOfChildrenOfItem: (id) item;
- (id)   outlineView: (NSOutlineView *) outlineView objectValueForTableColumn: (NSTableColumn *) tableColumn byItem: (id) item;

- (BOOL) outlineView: (NSOutlineView *) outlineView shouldEditTableColumn: (NSTableColumn *) tableColumn item: (id) item;

- (BOOL) splitView: (NSSplitView *) sender canCollapseSubview: (NSView *) subview;
- (float) splitView: (NSSplitView *) sender constrainMinCoordinate: (float) proposedMin ofSubviewAt: (int) offset;
- (float) splitView: (NSSplitView *) sender constrainMaxCoordinate: (float) proposedMax ofSubviewAt: (int) offset;

- (NSToolbarItem *) toolbar: (NSToolbar *) aToolbar itemForItemIdentifier: (NSString *) itemIdentifier willBeInsertedIntoToolbar: (BOOL) flag;
- (NSArray *) toolbarAllowedItemIdentifiers: (NSToolbar *) aToolbar;
- (NSArray *) toolbarDefaultItemIdentifiers: (NSToolbar *) aToolbar;

- (void) minuteTimer: (TargetOwnedTimer *) timer;

- (void) cacheCollectionDidChange: (NSNotification *) notification;
- (void) preferencesDidChange: (NSNotification *) notification;
- (void) tableViewSelectionDidChange: (NSNotification *) notification;
- (void) outlineViewSelectionDidChange: (NSNotification *) notification;
- (void) windowDidMove: (NSNotification *) notification;

- (void) synchronizeWindowTitle;
- (void) synchronizeCascadePoint;
- (void) synchronizeToolbar;

- (BOOL) haveSelectedCache;
- (Cache *) selectedCache;
- (BOOL) selectedCacheNeedsValidation;
- (BOOL) haveSelectedCredential;
- (Credential *) selectedCredential;

@end

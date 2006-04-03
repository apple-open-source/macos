/*
 * KerberosController.h
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
#import "TicketInfoController.h"
#import "TicketListController.h"
#import "PreferencesController.h"
#import "RealmsEditorController.h"


@interface KerberosController : NSObject
{    
    IBOutlet NSMenu *ticketsMenu;
    IBOutlet NSMenuItem *newTicketsMenuItem;
    IBOutlet NSMenuItem *renewTicketsMenuItem;
    IBOutlet NSMenuItem *validateTicketsMenuItem;
    IBOutlet NSMenuItem *destroyTicketsMenuItem;
    IBOutlet NSMenuItem *changePasswordMenuItem;
    IBOutlet NSMenuItem *showTicketInfoMenuItem;
    
    IBOutlet NSMenu *activeUserMenu;
    IBOutlet NSMenuItem *activeUserMenuItem;
    
    IBOutlet NSWindow *aboutWindow;
    IBOutlet NSTextField *aboutVersionTextField;
    IBOutlet NSTextField *aboutCopyrightTextField;
    
    IBOutlet NSMenu *dockMenu;
    IBOutlet NSMenuItem *dockNewTicketsMenuItem;
    IBOutlet NSMenuItem *dockRenewTicketsMenuItem;
    IBOutlet NSMenuItem *dockValidateTicketsMenuItem;
    IBOutlet NSMenuItem *dockDestroyTicketsMenuItem;
    IBOutlet NSMenuItem *dockChangePasswordMenuItem;
    IBOutlet NSMenuItem *dockSeparatorItem;
    
    CacheCollection *cacheCollection;
    
    NSTimer *minuteTimer;

    TicketListController *ticketListController;
    PreferencesController *preferencesController;
    RealmsEditorController *realmsEditorController;
    
    NSImage *ticketsKerberosIconImage;
    NSImage *warningKerberosIconImage;
    NSImage *noTicketsKerberosIconImage;
    NSImage *kerberosAppIconImage;
}

- (id) init;
- (void) dealloc;

- (IBAction) getTickets: (id) sender;
- (IBAction) changePasswordForSelectedCache: (id) sender;
- (IBAction) changePasswordForActiveUser: (id) sender;
- (IBAction) destroyTicketsForSelectedCache: (id) sender;
- (IBAction) destroyTicketsForActiveUser: (id) sender;
- (IBAction) renewTicketsForSelectedCache: (id) sender;
- (IBAction) renewTicketsForActiveUser: (id) sender;
- (IBAction) validateTicketsForSelectedCache: (id) sender;
- (IBAction) validateTicketsForActiveUser: (id) sender;
- (IBAction) changeActiveUser: (id) sender;
- (IBAction) showTicketInfo: (id) sender;
- (IBAction) showPreferences: (id) sender;
- (IBAction) showAboutBox: (id) sender;
- (IBAction) showTicketList: (id) sender;
- (IBAction) editRealms: (id) sender;

- (void) awakeFromNib;
- (NSMenu *) applicationDockMenu: (NSApplication *) sender;
- (void) menuNeedsUpdate: (NSMenu *) menu;

- (void) applicationDidBecomeActive: (NSNotification *) notification;
- (void) applicationWillTerminate: (NSNotification *) notification;
- (void) preferencesDidChange: (NSNotification *) notification;
- (void) cacheCollectionDidChange: (NSNotification *) notification;
- (void) ticketListDidChange: (NSNotification *) notification;
- (void) listSelectionDidChange: (NSNotification *) notification;
- (void) windowWillClose: (NSNotification *) notification;

- (void) minuteTimer: (NSTimer *) timer;

- (BOOL) haveTicketListWindow;
- (BOOL) ticketListWindowHasSelectedCache;
- (BOOL) ticketListWindowSelectedCacheNeedsValidation;
- (BOOL) ticketListWindowHasSelectedCredential;
- (BOOL) haveDefaultCache;
- (BOOL) defaultCacheNeedsValidation;

- (void) synchronizeDockIcon;

- (NSImage *) ticketsKerberosIconImage;
- (NSImage *) warningKerberosIconImage;
- (NSImage *) noTicketsKerberosIconImage;
- (NSImage *) kerberosAppIconImage;
- (NSImage *) dockIconImageForCache: (Cache *) cache;

@end

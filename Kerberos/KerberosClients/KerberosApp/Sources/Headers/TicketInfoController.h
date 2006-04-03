/*
 * TicketInfoController.h
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


#import "Credential.h"
#import "TargetOwnedTimer.h"

@interface TicketInfoController : NSWindowController
{
    IBOutlet NSTextField *clientPrincipalTextField;
    IBOutlet NSTextField *servicePrincipalTextField;
    IBOutlet NSTextField *versionTextField;
    IBOutlet NSTextField *statusTextField;

    IBOutlet NSTabView *ticketTabView;
    IBOutlet NSTabViewItem *flagsTabViewItem;
    IBOutlet NSTabViewItem *v4EncryptionTabViewItem;
    IBOutlet NSTabViewItem *v5EncryptionTabViewItem;
    
    IBOutlet NSTextField *issuedTimeTextField;
    IBOutlet NSTextField *startTimeTextField;
    IBOutlet NSTextField *expirationTimeTextField;
    IBOutlet NSTextField *renewUntilTimeHeaderTextField;
    IBOutlet NSTextField *renewUntilTimeTextField;

    IBOutlet NSTextField *forwardableTextField;
    IBOutlet NSTextField *forwardedTextField;
    IBOutlet NSTextField *proxiableTextField;
    IBOutlet NSTextField *proxiedTextField;
    IBOutlet NSTextField *postdatableTextField;
    IBOutlet NSTextField *postdatedTextField;
    IBOutlet NSTextField *invalidTextField;
    IBOutlet NSTextField *renewableTextField;
    IBOutlet NSTextField *initialTextField;
    IBOutlet NSTextField *preauthenticatedTextField;
    IBOutlet NSTextField *hardwareAuthenticatedTextField;
    IBOutlet NSTextField *sKeyTextField;

    IBOutlet NSTableView *ipAddressesTableView;
    
    IBOutlet NSTextField *stringToKeyTypeTextField;
    IBOutlet NSTextField *sessionKeyEnctypeTextField;
    IBOutlet NSTextField *servicePrincipalEnctypeTextField;
    
    Credential *credential;
    NSDateFormatter *timeFormatter;
    
    TargetOwnedTimer *stateTimer;
}

- (id) initWithCredential: (Credential *) aCredential;
- (void) dealloc;

- (void) windowDidLoad;
- (void) stateTimer: (TargetOwnedTimer *) timer;

- (int) numberOfRowsInTableView: (NSTableView *) tableView;
- (id) tableView: (NSTableView *) tableView objectValueForTableColumn: (NSTableColumn *) tableColumn row: (int) index;

@end

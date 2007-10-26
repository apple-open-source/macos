/*
 * TicketInfoController.m
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

#import "TicketInfoController.h"

@implementation TicketInfoController

// ---------------------------------------------------------------------------

- (id) initWithCredential: (KerberosCredential *) aCredential
{
    dprintf ("Entering initWithCredential:...");
    if ((self = [super initWithWindowNibName: @"TicketInfo"])) {
        // aCredential retains this object (by adding it to an array) 
        // so don't retain the credential or we will leak an object pair
        credential = aCredential;  
        
        timeFormatter = [[NSDateFormatter alloc] initWithDateFormat: @"%c" allowNaturalLanguage: NO];
        if (!timeFormatter) {
            [self release];
            return NULL;
        }
        
        stateTimer = NULL;
    }
    return self;
}

// ---------------------------------------------------------------------------

- (void) dealloc 
{
    if (timeFormatter) { [timeFormatter release]; }
    
    if (stateTimer) {
        // invalidate a TargetOwnedTimer before releasing it
        [stateTimer invalidate];
        [stateTimer release];
    }
    
    [super dealloc];
}

// ---------------------------------------------------------------------------

- (void) windowDidLoad
{
    dprintf ("Entering ticket info window windowDidLoad...");
    [super windowDidLoad];
    
    NSString *format;
    NSString *clientPrincipal = [credential clientPrincipalString];
    NSString *servicePrincipal = [credential servicePrincipalString];
    NSString *version = [credential versionString];
    NSString *yes = NSLocalizedString (@"KAppStringYes", NULL);
    NSString *no = NSLocalizedString (@"KAppStringNo", NULL);
    
    // Window Title
    format = NSLocalizedString (@"KAppStringTicketInfoWindowTitleFormat", NULL);
    [[self window] setTitle: [NSString stringWithFormat: format, servicePrincipal, version]];
    
    // Header Text
    [clientPrincipalTextField setObjectValue: clientPrincipal];
    [servicePrincipalTextField setObjectValue: servicePrincipal];
    format = NSLocalizedString (@"KAppStringTicketInfoVersionFormat", NULL);
    [self updateStatusTextField];
    
    // Times Tab
    [issuedTimeTextField setFormatter: timeFormatter];
    [issuedTimeTextField setObjectValue: [NSDate dateWithTimeIntervalSince1970: [credential issueTime]]];

    [startTimeTextField setFormatter: timeFormatter];
    [startTimeTextField setObjectValue: [NSDate dateWithTimeIntervalSince1970: [credential startTime]]];
    
    [expirationTimeTextField setFormatter: timeFormatter];
    [expirationTimeTextField setObjectValue: [NSDate dateWithTimeIntervalSince1970: [credential expirationTime]]];
    
    if ([credential version] == cc_credentials_v4) {
        [renewUntilTimeHeaderTextField setHidden: YES];
        [renewUntilTimeTextField       setHidden: YES];
    } else {
        if ([credential renewable]) {
            [renewUntilTimeTextField setFormatter: timeFormatter];
            [renewUntilTimeTextField setObjectValue: [NSDate dateWithTimeIntervalSince1970: [credential renewUntilTime]]];
        } else {
            [renewUntilTimeTextField setObjectValue: NSLocalizedString (@"KAppStringNotRenewable", NULL)];
        }
    }
    
    // Flags Tab
    if ([credential version] == cc_credentials_v4) {
        [ticketTabView removeTabViewItem: flagsTabViewItem];
    } else {
        [forwardableTextField           setObjectValue: [credential forwardable]           ? yes : no];
        [forwardedTextField             setObjectValue: [credential forwarded]             ? yes : no];
        [proxiableTextField             setObjectValue: [credential proxiable]             ? yes : no];
        [proxiedTextField               setObjectValue: [credential proxied]               ? yes : no];
        [postdatableTextField           setObjectValue: [credential postdatable]           ? yes : no];
        [postdatedTextField             setObjectValue: [credential postdated]             ? yes : no];
        [invalidTextField               setObjectValue: [credential invalid]               ? yes : no];
        [renewableTextField             setObjectValue: [credential renewable]             ? yes : no];
        [initialTextField               setObjectValue: [credential initial]               ? yes : no];
        [preauthenticatedTextField      setObjectValue: [credential preauthenticated]      ? yes : no];
        [hardwareAuthenticatedTextField setObjectValue: [credential hardwareAuthenticated] ? yes : no];
        [sKeyTextField                  setObjectValue: [credential isSKey]                ? yes : no];
    }
    
    // Encryption Tab
    [sessionKeyEnctypeTextField       setObjectValue: [credential sessionKeyEnctypeString]];
    [servicePrincipalEnctypeTextField setObjectValue: [credential servicePrincipalKeyEnctypeString]];
    
    [ticketTabView selectFirstTabViewItem: self];

    // Initialize timer
    stateTimer = [[TargetOwnedTimer scheduledTimerWithTimeInterval: 5 // checks for ticket state changes
                                                            target: self 
                                                          selector: @selector(stateTimer:) 
                                                          userInfo: NULL
                                                           repeats: YES] retain];
    
}

// ---------------------------------------------------------------------------

- (void) stateTimer: (TargetOwnedTimer *) timer 
{
    [self updateStatusTextField];
}

// ---------------------------------------------------------------------------

- (void) updateStatusTextField
{
    int state = [credential state];
    NSString *stateString = [credential longStateString];
    
    if (state == CredentialValid) {
        [statusTextField setObjectValue: stateString];
    } else {
        NSDictionary *attributes = [NSDictionary dictionaryWithObject: [NSColor redColor]
                                                               forKey: NSForegroundColorAttributeName];
        [statusTextField setObjectValue: [[[NSAttributedString alloc] initWithString: stateString
                                                                 attributes: attributes] autorelease]];
    }
}

#pragma mark --- Data Source Methods --

// ---------------------------------------------------------------------------

- (int) numberOfRowsInTableView: (NSTableView *) tableView
{
    int count = [[credential addresses] count];
    return (count > 0) ? count: 1;
}

// ---------------------------------------------------------------------------

- (id) tableView: (NSTableView *) tableView objectValueForTableColumn: (NSTableColumn *) tableColumn row: (int) rowIndex
{
    if ([[credential addresses] count] > 0) {
        return [[[credential addresses] objectAtIndex: rowIndex] stringValue];
    } else {
        return NSLocalizedString (@"KAppStringNone", NULL);
    }
}

@end

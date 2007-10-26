/*
 * RealmsEditorController.m
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

#import "RealmsEditorController.h"
#import "Utilities.h"
#import "KerberosErrorAlert.h"


@implementation RealmsEditorController

// ---------------------------------------------------------------------------

- (id) init
{
    if ((self = [super initWithWindowNibName: @"EditRealms"])) {
        errorMessageString = NULL;
        errorDescriptionString = NULL;
        realmsConfiguration = NULL;
        
        realmsConfiguration = [[RealmsConfiguration alloc] init];
        if (!realmsConfiguration) {
            [self release];
            return NULL;
        }
    }
    
    return self;
}

// ---------------------------------------------------------------------------

- (void) dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver: self];

    if (errorMessageString    ) { [errorMessageString release]; }
    if (errorDescriptionString) { [errorDescriptionString release]; }
    if (realmsConfiguration   ) { [realmsConfiguration release]; }

    [super dealloc];
}

// ---------------------------------------------------------------------------

- (void) windowDidLoad
{
    dprintf ("RealmsEditorController %lx entering windowDidLoad:...", (long) self);

    [super windowDidLoad];

    
    [[NSNotificationCenter defaultCenter] addObserver: self
                                             selector: @selector (realmConfigurationErrorNeedsDisplay:)
                                                 name: RealmConfigurationErrorNotification
                                               object: nil];
        

    [self setWindowFrameAutosaveName: @"KARealmsEditorWindowPosition"];
}

// ---------------------------------------------------------------------------

- (IBAction) showWindow: (id) sender
{
    // Check to see if the window was closed before. ([self window] will load the window)
    if (![[self window] isVisible]) {
        dprintf ("RealmsEditorController %lx displaying window...", (long) self);
        
        [realmsConfiguration load]; // re-read the profile
        
        // Set up the realms table
        // If there is at least one realm, select the first one.
        // Otherwise forge a selection notification so the realm data gets reset    
        [realmsTableView reloadData];
        if ([realmsConfiguration numberOfRealms] > 0) {
            [self selectRealm: [realmsConfiguration realmAtIndex: 0] edit: NO];
        } else {
            [self tableViewSelectionDidChange: [NSNotification notificationWithName: NSTableViewSelectionDidChangeNotification 
                                                                             object: realmsTableView]];
        }
    
        [realmTabView selectTabViewItem: settingsTabViewItem];
        [configureRealmsAutomaticallyCheckbox setState: [realmsConfiguration useDNS] ? NSOnState : NSOffState];
        
        [[self window] setFrameUsingName: [self windowFrameAutosaveName]];
    }
    
    [super showWindow: sender];
}

#pragma mark --- Actions --

// ---------------------------------------------------------------------------

- (IBAction) addRealm: (id) sender
{
    KerberosRealm *newRealm = [KerberosRealm emptyRealm];
    if ((newRealm != NULL) && [self stopEditing]) {
        [realmsConfiguration addRealm: newRealm];
        [realmsTableView reloadData];
        [realmTabView selectTabViewItem: settingsTabViewItem];
        [self selectRealm: newRealm edit: YES];
    }
}

// ---------------------------------------------------------------------------

- (IBAction) removeRealm: (id) sender
{
    KerberosRealm *selectedRealm = [self selectedRealm];
    if (selectedRealm) {
        NSString *format = NSLocalizedString (@"KAppStringAskRemoveRealm", NULL);
        if (([[self currentRealmString] length] == 0) ||
            [KerberosErrorAlert alertForYNQuestion: [NSString stringWithFormat: format, [self currentRealmString]]
                                    modalForWindow: [self window]]) {
            [realmsConfiguration removeRealm: selectedRealm];
        }
    }
    [realmsTableView reloadData];
}

// ---------------------------------------------------------------------------

- (IBAction) makeDefaultRealm: (id) sender
{
    KerberosRealm *selectedRealm = [self selectedRealm];
    if (selectedRealm) {
        [realmsConfiguration setDefaultRealm: [selectedRealm name]];
    }
    [realmsTableView reloadData];
}

// ---------------------------------------------------------------------------

- (IBAction) displayRealmInDialogPopupCheckboxWasHit: (id) sender
{
    KerberosRealm *selectedRealm = [self selectedRealm];
    if (selectedRealm) {
        [selectedRealm setDisplayInDialogPopup: ([displayInDialogPopupCheckbox state] == NSOnState)];
    }
}

// ---------------------------------------------------------------------------

- (IBAction) configureRealmsWithDNSCheckboxWasHit: (id) sender
{
    [realmsConfiguration setUseDNS: ([configureRealmsAutomaticallyCheckbox state] == NSOnState)];
}

// ---------------------------------------------------------------------------

- (IBAction) addServer: (id) sender
{
    KerberosRealm *selectedRealm = [self selectedRealm];
    if (selectedRealm) {
        KerberosServer *newServer = [KerberosServer emptyServer];
        if ((newServer != NULL) && [self stopEditing]) {
            [selectedRealm addServer: newServer];
            [serversTableView reloadData];
            [self selectServer: newServer edit: YES];
        } 
    }
}

// ---------------------------------------------------------------------------

- (IBAction) removeServer: (id) sender
{
    KerberosRealm *selectedRealm = [self selectedRealm];
    KerberosServer *selectedServer = [self selectedServer];
    if ((selectedRealm != NULL) && (selectedServer != NULL)) {
        NSString *format = NSLocalizedString (@"KAppStringAskRemoveServer", NULL);
        if (([[self currentServerString] length] == 0) ||
            [KerberosErrorAlert alertForYNQuestion: [NSString stringWithFormat: format, [self currentServerString]]
                                    modalForWindow: [self window]]) {
            [selectedRealm removeServer: selectedServer];
        }
    }
    [serversTableView reloadData];
}

// ---------------------------------------------------------------------------

- (IBAction) addDomain: (id) sender
{
    KerberosRealm *selectedRealm = [self selectedRealm];
    if (selectedRealm) {
        KerberosDomain *newDomain = [KerberosDomain emptyDomain];
        if (newDomain != NULL && [self stopEditing]) {
            [selectedRealm addDomain: newDomain];
            [domainsTableView reloadData];
            [self selectDomain: newDomain edit: YES];
        }
    }
}

// ---------------------------------------------------------------------------

- (IBAction) removeDomain: (id) sender
{
    KerberosRealm *selectedRealm = [self selectedRealm];
    KerberosDomain *selectedDomain = [self selectedDomain];
    if ((selectedRealm != NULL) && (selectedDomain != NULL)) {
        NSString *format = NSLocalizedString (@"KAppStringAskRemoveDomain", NULL);
        if (([[self currentDomainString] length] == 0) ||
            [KerberosErrorAlert alertForYNQuestion: [NSString stringWithFormat: format, [self currentDomainString]]
                                    modalForWindow: [self window]]) {
            [selectedRealm removeDomain: selectedDomain];
        }
    }
    [domainsTableView reloadData];
}

// ---------------------------------------------------------------------------

- (IBAction) makeDefaultDomain: (id) sender
{
    KerberosRealm *selectedRealm = [self selectedRealm];
    KerberosDomain *selectedDomain = [self selectedDomain];
    if ((selectedRealm != NULL) && (selectedDomain != NULL)) {
        [selectedRealm setDefaultDomain: [selectedDomain name]];
    }
    [domainsTableView reloadData];
}

// ---------------------------------------------------------------------------

- (IBAction) apply: (id) sender
{
    [self saveChanges];
}

// ---------------------------------------------------------------------------

- (IBAction) ok: (id) sender
{
    if ([self saveChanges]) {
        [self close];
    }
}

// ---------------------------------------------------------------------------

- (IBAction) cancel: (id) sender
{
    [realmsConfiguration abandon];
    [self close];
}

#pragma mark --- Data Source Methods --

// ---------------------------------------------------------------------------

- (int) numberOfRowsInTableView: (NSTableView *) tableView
{
    if (tableView == realmsTableView) {
        return [realmsConfiguration numberOfRealms];
    } else {
        int selectedRowIndex = [realmsTableView selectedRow];
        if (selectedRowIndex >= 0) {
            if (tableView == serversTableView) {
                return [[realmsConfiguration realmAtIndex: selectedRowIndex] numberOfServers];
            } else if (tableView == domainsTableView) {
                return [[realmsConfiguration realmAtIndex: selectedRowIndex] numberOfDomains];
            }
        }
    }
    
    return 0;
}

// ---------------------------------------------------------------------------

- (id) tableView: (NSTableView *) tableView objectValueForTableColumn: (NSTableColumn *) tableColumn row: (int) rowIndex
{
    if (tableView == realmsTableView) {
        KerberosRealm *realm = [realmsConfiguration realmAtIndex: rowIndex];
        if (realm) {
            BOOL isDefault = ([[realm name] compare: [realmsConfiguration defaultRealm]] == NSOrderedSame);
            return [Utilities attributedStringForControlType: kUtilitiesTableCellControlType
                                                      string: [realm name]
                                                   alignment: kUtilitiesLeftStringAlignment
                                                        bold: isDefault
                                                      italic: NO
                                                         red: NO];
        }
    } else {
        KerberosRealm *realm = [self selectedRealm];
        if (realm) {
            if (tableView == serversTableView) {
                KerberosServer *server = [realm serverAtIndex: rowIndex];
                if (server) {
                    if (tableColumn == serverTypeColumn) {
                        return [NSNumber numberWithInt: [server typeMenuIndex]];
                    } else if (tableColumn == serverColumn) {
                        return [server host];
                    } else if (tableColumn == serverPortColumn) {
                        return [server port];
                    } 
                }
            } else if (tableView == domainsTableView) {
                KerberosDomain *domain = [realm domainAtIndex: rowIndex];
                if (domain) {
                    BOOL isDefault = ([[domain name] compare: [realm defaultDomain]] == NSOrderedSame);
                    return [Utilities attributedStringForControlType: kUtilitiesSmallTableCellControlType
                                                              string: [domain name]
                                                           alignment: kUtilitiesLeftStringAlignment
                                                                bold: isDefault
                                                              italic: NO
                                                                 red: NO];
                }
            }
        }
    }

    return @"";
}

// ---------------------------------------------------------------------------

- (void) tableView: (NSTableView *) tableView setObjectValue: value forTableColumn: (NSTableColumn *) tableColumn row: (int) rowIndex
{
    if (tableView == realmsTableView) {
        [[realmsConfiguration realmAtIndex: rowIndex] setName: value];
    } else {
        KerberosRealm *realm = [self selectedRealm];
        if (realm) {
            if (tableView == serversTableView) {
                KerberosServer *server = [realm serverAtIndex: rowIndex];
                if (server) {
                    if (tableColumn == serverTypeColumn) {
                        [server setTypeMenuIndex: [value intValue]];
                    } else if (tableColumn == serverColumn) {
                        [server setHost: value];
                    } else if (tableColumn == serverPortColumn) {
                        [server setPort: value];
                    } 
                }
            } else if (tableView == domainsTableView) {
                KerberosDomain *domain = [realm domainAtIndex: rowIndex];
                if (domain) {
                    if (tableColumn == domainColumn) {
                        [domain setName: value];
                    }
                }
            }
        }            
    }
}

#pragma mark --- Delegate Methods --

// ---------------------------------------------------------------------------

- (BOOL) selectionShouldChangeInTableView: (NSTableView *) tableView
{
    BOOL allow = YES;
    if ((tableView == realmsTableView) || (tableView == serversTableView) || (tableView == domainsTableView)) {
        allow = [self stopEditing];
    }

    return allow;
}

// ---------------------------------------------------------------------------

- (BOOL) tabView: (NSTabView *) tabView shouldSelectTabViewItem: (NSTabViewItem *) tabViewItem
{
    BOOL allow = YES;
    if (tabView == realmTabView) {
        allow = [self stopEditing];
    }
    
    return allow;
}

// ---------------------------------------------------------------------------

- (void) sheetDidEnd: (NSWindow *) sheet returnCode: (int) returnCode contextInfo: (void *) contextInfo
{
    if (sheet == [self window]) {
        [NSApp stopModalWithCode: returnCode];
    }
}

#pragma mark --- Notifications --

// ---------------------------------------------------------------------------

- (void) controlTextDidChange: (NSNotification *) notification
{
    if ([notification object] == realmTextField) {
        KerberosRealm *realm = [self selectedRealm];
        if (realm) {
            if ([[realmTextField stringValue] length] > 0) {
                [realm setName: [realmTextField stringValue]];
            } else {
                [realm setName: NULL];
            }
            [realmsTableView reloadData];
        }
    }
}

// ---------------------------------------------------------------------------

- (void) realmConfigurationErrorNeedsDisplay: (NSNotification *) notification
{
    [KerberosErrorAlert alertForMessage: [self errorMessage]
                            description: [self errorDescription]
                         modalForWindow: [self window]];
}

// ---------------------------------------------------------------------------

- (void) tableViewSelectionDidChange: (NSNotification *) notification
{   
    BOOL realmIsSelected  = ([realmsTableView selectedRow]  >= 0);
    BOOL serverIsSelected = ([serversTableView selectedRow] >= 0);
    BOOL domainIsSelected = ([domainsTableView selectedRow] >= 0);
    
    // Set control enabledness:
    [displayInDialogPopupCheckbox setEnabled: realmIsSelected];
    [realmTextField               setEnabled: realmIsSelected];
    [removeRealmButton            setEnabled: realmIsSelected];
    
    [serversTableView   setEnabled: realmIsSelected];
    [addServerButton    setEnabled: realmIsSelected];
    [removeServerButton setEnabled: serverIsSelected];
    
    [domainsTableView        setEnabled: realmIsSelected];
    [addDomainButton         setEnabled: realmIsSelected];
    [removeDomainButton      setEnabled: domainIsSelected];
    [makeDefaultDomainButton setEnabled: domainIsSelected];       
    
    // Gray out text if necessary:
    NSColor *color = realmIsSelected ? [NSColor blackColor] : [NSColor grayColor];
    [realmHeaderTextField            setTextColor: color];
    [mappingDescriptionTextField     setTextColor: color];
    
    if ([notification object] == realmsTableView) {
        // If we changed realms, load the realm configuration
        KerberosRealm *realm = [self selectedRealm];        
        if (realm) {
            [displayInDialogPopupCheckbox setState: [realm displayInDialogPopup] ? NSOnState : NSOffState];
            [realmTextField setObjectValue: [realm name]];        
        }
        
        [serversTableView reloadData];
        [domainsTableView reloadData];
    }
}

#pragma mark -- Editing --

// ---------------------------------------------------------------------------

// Code to verify that the currently entered realm/server/domain is valid
// so that we can deselect it.  We'd like to prevent the user from leaving
// these fields blank, since empty strings will result in a corrupt 
// configuration file.
//
// Problem #1:  When you create a new realm/server/domain, it gets
// added to the array and manually set up for editing so there is an empty 
// text field and a blinking carat.  The problem is that unless the user 
// types text into the text field view, it *doesn't* get set up for editing 
// from the standpoint of delegates and formatters.  
//
// In other words, if you add an item to the table data source, call 
// reloadData on the NSTableView and then call 
// editColumn:indexOfObject:row:withEvent:select:, if the user clicks away 
// without typing anything, neither control:textShouldEndEditing: nor 
// control:didFailToFormatString:errorDescription: get called.   This means
// that unless the user types something into the text field and then deletes 
// it, he can merrily click the add button to add another one and none of our
// delegates get called.
//
// Problem #2: While a text field view in a table is being edited, the string 
// in the field editor is not written out to the data source.  This means that
// if the user is editing something and then clicks "Apply", the old value gets
// written to disk because that's what's in the data source.  So we also 
// manually stop any editing which is currently going on.

// ---------------------------------------------------------------------------

- (NSString *) currentRealmString
{
    KerberosRealm *realm = [self selectedRealm];
    NSText *editor = [realmTextField currentEditor];
    NSString *string = NULL;
    
    if (!string && editor) { string = [editor string]; }
    if (!string && realm ) { string = [realm name]; }
    
    return string;
}

// ---------------------------------------------------------------------------

- (NSString *) currentServerString
{
    KerberosServer *server = [self selectedServer];
    NSText *editor = [serversTableView currentEditor];
    NSString *string = NULL;
    
    if (!string && editor) { string = [editor string]; }
    if (!string && server) { string = [server host]; }
    
    return string;
}

// ---------------------------------------------------------------------------

- (NSString *) currentDomainString
{
    KerberosDomain *domain = [self selectedDomain];
    NSText *editor = [domainsTableView currentEditor];
    NSString *string = NULL;
    
    if (!string && editor) { string = [editor string]; }
    if (!string && domain) { string = [domain name]; }
    
    return string;
}

// ---------------------------------------------------------------------------

- (BOOL) stopEditing
{
    BOOL allow = YES;
    
    if (allow) {
        NSString *string = [self currentRealmString];
        NSText *editor = [realmTextField currentEditor];
        
        if (string) {
            //NSLog (@"Realm text field editor exists and contains '%@'", string);
            if ([string length] <= 0) {
                [self displayErrorMessage: NSLocalizedString (@"KAppStringRealmsConfigurationError", NULL)
                              description: NSLocalizedString (@"KAppStringEmptyRealm", NULL)];                
                allow = NO;
            } else {
                if (editor) { [[realmTextField selectedCell] endEditing: editor]; }
            }
        }
        
        if (!allow && !editor) { 
            [[self window] makeFirstResponder: realmTextField];  // edit again
        }
    }
    
    if (allow) {
        NSString *string = [self currentServerString];
        NSText *editor = [serversTableView currentEditor];
        
        if (string) {
            //NSLog (@"Server table ditor exists and contains '%@'", string);
            if ([string length] <= 0) {
                [self displayErrorMessage: NSLocalizedString (@"KAppStringRealmsConfigurationError", NULL)
                              description: NSLocalizedString (@"KAppStringEmptyServer", NULL)];                
                allow = NO;
                
            } else {
                if (editor) { [[serversTableView selectedCell] endEditing: editor]; }
            }

            if (!allow && !editor) { 
                [serversTableView editColumn: [[serversTableView tableColumns] indexOfObject: serverColumn]
                                         row: [serversTableView selectedRow]
                                   withEvent: NULL
                                      select: YES];  // edit again
            }
        }
    }

    if (allow) {
        NSString *string = [self currentDomainString];
        NSText *editor = [domainsTableView currentEditor];
        
        if (string) {
            KerberosRealm *selectedRealm = [self selectedRealm];
            KerberosRealm *mappedRealm = NULL;
            
            //NSLog (@"Domain table editor exists and contains '%@'", string);
            if ([string length] <= 0) {
                [self displayErrorMessage: NSLocalizedString (@"KAppStringRealmsConfigurationError", NULL)
                              description: NSLocalizedString (@"KAppStringEmptyDomain", NULL)];
                allow = NO;
                
            } else if ((selectedRealm != NULL) && ![realmsConfiguration allowDomainString: string 
                                                                         mappedToNewRealm: selectedRealm 
                                                                             currentRealm: &mappedRealm]) {
                [self displayErrorMessage: NSLocalizedString (@"KAppStringRealmsConfigurationError", NULL)
                              description: [NSString stringWithFormat: NSLocalizedString (@"KAppStringDuplicateDomainFormat", NULL),
                                  string, [mappedRealm name]]];                
                allow = NO;
                
            } else {
                if (editor) { [[domainsTableView selectedCell] endEditing: editor]; }
            }            

            if (!allow && !editor) { 
                [domainsTableView editColumn: [[domainsTableView tableColumns] indexOfObject: domainColumn]
                                         row: [domainsTableView selectedRow]
                                   withEvent: NULL
                                      select: YES];  // edit again
            }
        }
    }
    
    return allow;
}

#pragma mark -- Miscellaneous --

// ---------------------------------------------------------------------------

- (BOOL) saveChanges
{
    BOOL success = NO;
    
    if ([self stopEditing]) {
        krb5_error_code err = [realmsConfiguration flush];
        if (!err) {
            success = YES;
            [realmsTableView reloadData];
            [self tableViewSelectionDidChange: [NSNotification notificationWithName: NSTableViewSelectionDidChangeNotification 
                                                                             object: realmsTableView]];
        } else if ((err != errAuthorizationCanceled) && (err != userCanceledErr)) {
            // Report non-user-initiated error conditions
            [self displayErrorMessage: NSLocalizedString (@"KAppStringRealmsConfigurationWriteError", NULL)
                          description: [NSString stringWithUTF8String: error_message (err)]];
        }
    }    
    
    return success;
}

// ---------------------------------------------------------------------------

- (KerberosRealm *) selectedRealm
{
    int selectedRowIndex = [realmsTableView selectedRow];
    if (selectedRowIndex >= 0) {
        return [realmsConfiguration realmAtIndex: selectedRowIndex];
    }
    return NULL;
}

// ---------------------------------------------------------------------------

- (KerberosServer *) selectedServer
{
    int selectedRowIndex = [serversTableView selectedRow];
    KerberosRealm *selectedRealm = [self selectedRealm];
    if ((selectedRealm != NULL) && (selectedRowIndex >= 0)) {
        return [selectedRealm serverAtIndex: selectedRowIndex];
    }
    return NULL;
}

// ---------------------------------------------------------------------------

- (KerberosDomain *) selectedDomain
{
    int selectedRowIndex = [domainsTableView selectedRow];
    KerberosRealm *selectedRealm = [self selectedRealm];
    if ((selectedRealm != NULL) && (selectedRowIndex >= 0)) {
        return [selectedRealm domainAtIndex: selectedRowIndex];
    }
    return NULL;
}

// ---------------------------------------------------------------------------

- (void) selectRealm: (KerberosRealm *) realm edit: (BOOL) edit
{
    unsigned int realmIndex = [realmsConfiguration indexOfRealm: realm];
    if (realmIndex != NSNotFound) {
        NSIndexSet *indexes = [NSIndexSet indexSetWithIndex: realmIndex];
        [realmsTableView selectRowIndexes: indexes byExtendingSelection: NO];
        [realmsTableView scrollRowToVisible: realmIndex];
        if (edit) {
            [realmTabView selectTabViewItem: settingsTabViewItem]; 
            [[self window] makeFirstResponder: realmTextField];
        }
    }
}

// ---------------------------------------------------------------------------

- (void) selectServer: (KerberosServer *) server edit: (BOOL) edit
{
    KerberosRealm *selectedRealm = [self selectedRealm];
    if (selectedRealm) {
        unsigned int serverIndex = [selectedRealm indexOfServer: server];
        if (serverIndex != NSNotFound) {
            NSIndexSet *indexes = [NSIndexSet indexSetWithIndex: serverIndex];
            [serversTableView selectRowIndexes: indexes byExtendingSelection: NO];
            [serversTableView scrollRowToVisible: serverIndex];
            if (edit) {
                [serversTableView editColumn: [[serversTableView tableColumns] indexOfObject: serverColumn] 
                                         row: serverIndex withEvent: NULL select: YES];
            }
        }
    }
}

// ---------------------------------------------------------------------------

- (void) selectDomain: (KerberosDomain *) domain edit: (BOOL) edit
{
    KerberosRealm *selectedRealm = [self selectedRealm];
    if (selectedRealm) {
        unsigned int domainIndex = [selectedRealm indexOfDomain: domain];
        if (domainIndex != NSNotFound) {
            NSIndexSet *indexes = [NSIndexSet indexSetWithIndex: domainIndex];
            [domainsTableView selectRowIndexes: indexes byExtendingSelection: NO];
            [domainsTableView scrollRowToVisible: domainIndex];
            if (edit) {
                [domainsTableView editColumn: [[domainsTableView tableColumns] indexOfObject: domainColumn] 
                                         row: domainIndex withEvent: NULL select: YES];
            }
        }
    }
}

// ---------------------------------------------------------------------------

- (NSString *) errorMessage
{
    return (errorMessageString != NULL) ? errorMessageString : @"Realms Editor Error";
}

// ---------------------------------------------------------------------------

- (NSString *) errorDescription
{
    return (errorDescriptionString != NULL) ? errorDescriptionString : @"Unknown error";
}

// ---------------------------------------------------------------------------

- (void) setErrorMessage: (NSString *) newErrorMessage description: (NSString *) newErrorDescription
{
    if (errorMessageString) { [errorMessageString release]; }
    errorMessageString = [newErrorMessage retain];
    
    if (errorDescriptionString) { [errorDescriptionString release]; }
    errorDescriptionString = [newErrorDescription retain];
}

// ---------------------------------------------------------------------------

// Post error messages as a notification with NSNotificationCoalescingOnName
// because may of the error conditions get triggered multiple times by a single
// user action (such as a click)

- (void) displayErrorMessage: (NSString *) errorMessage description: (NSString *) errorDescription
{
    NSNotification *notification = [NSNotification notificationWithName: RealmConfigurationErrorNotification 
                                                                 object: self];
    
    [self setErrorMessage: errorMessage description: errorDescription];
    [[NSNotificationQueue defaultQueue] enqueueNotification: notification
                                               postingStyle: NSPostWhenIdle
                                               coalesceMask: NSNotificationCoalescingOnName 
                                                   forModes: NULL];
}


@end

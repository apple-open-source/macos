/*
 * RealmsEditorController.h
 *
 * $Header: /cvs/kfm/KerberosClients/KerberosApp/Sources/Headers/RealmsEditorController.h,v 1.11 2005/05/25 20:36:06 lxs Exp $
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

#import "RealmsConfiguration.h"

@interface RealmsEditorController : NSWindowController
{
    IBOutlet NSTableView *realmsTableView;
    IBOutlet NSTableColumn *realmColumn;
    IBOutlet NSButton *addRealmButton;
    IBOutlet NSButton *removeRealmButton;
    IBOutlet NSButton *makeDefaultRealmButton;
    
    IBOutlet NSTabView *realmTabView;
    
    IBOutlet NSTabViewItem *settingsTabViewItem;
    IBOutlet NSTextField *realmHeaderTextField;
    IBOutlet NSTextField *realmTextField;
    IBOutlet NSTextField *alternateV4RealmHeaderTextField;
    IBOutlet NSTextField *alternateV4RealmTextField;
    IBOutlet NSTextField *notCommonTextField;
    IBOutlet NSButton *displayInDialogPopupCheckbox;
    
    IBOutlet NSTabViewItem *serversTabViewItem;
    IBOutlet NSTableView *serversTableView;
    IBOutlet NSTableColumn *serverVersionColumn;
    IBOutlet NSMenu *serverVersionMenu;
    IBOutlet NSTableColumn *serverTypeColumn;
    IBOutlet NSMenu *serverTypeMenu;
    IBOutlet NSTableColumn *serverColumn;
    IBOutlet NSTableColumn *serverPortColumn;
    IBOutlet NSButton *addServerButton;
    IBOutlet NSButton *removeServerButton;
    
    IBOutlet NSTabViewItem *domainsTabViewItem;
    IBOutlet NSTableView *domainsTableView;
    IBOutlet NSTableColumn *domainColumn;
    IBOutlet NSButton *addDomainButton;
    IBOutlet NSButton *removeDomainButton;
    IBOutlet NSButton *makeDefaultDomainButton;
    IBOutlet NSTextField *mappingDescriptionTextField;
   
    IBOutlet NSButton *configureRealmsAutomaticallyCheckbox;
    IBOutlet NSButton *applyButton;
    IBOutlet NSButton *cancelButton;
    IBOutlet NSButton *okButton;
    
    
    IBOutlet NSWindow *addServerWindow;

    IBOutlet NSMatrix *versionMatrix;
    IBOutlet NSButtonCell *v4RadioButtonCell;
    IBOutlet NSButtonCell *v5RadioButtonCell;
    
    IBOutlet NSMatrix *typeMatrix;
    IBOutlet NSButtonCell *kdcRadioButtonCell;
    IBOutlet NSButtonCell *adminRadioButtonCell;
    IBOutlet NSButtonCell *krb524RadioButtonCell;
    IBOutlet NSButtonCell *kpasswdRadioButtonCell;
    
    IBOutlet NSTextField *hostTextField;
    
    IBOutlet NSMatrix *portMatrix;
    IBOutlet NSButtonCell *defaultPortRadioButtonCell;
    IBOutlet NSButtonCell *customPortRadioButtonCell;
    IBOutlet NSTextField *customPortTextField;
  
    IBOutlet NSButton *addServerOKButton;
    IBOutlet NSButton *addServerCancelButton;
    
    
    RealmsConfiguration *realmsConfiguration;
    NSString *errorMessageString;
    NSString *errorDescriptionString;
}

- (id) init;
- (void) dealloc;

- (void) windowDidLoad;
- (IBAction) showWindow: (id) sender;

- (IBAction) addRealm: (id) sender;
- (IBAction) removeRealm: (id) sender;
- (IBAction) makeDefaultRealm: (id) sender;
- (IBAction) displayRealmInDialogPopupCheckboxWasHit: (id) sender;
- (IBAction) configureRealmsWithDNSCheckboxWasHit: (id) sender;
- (IBAction) addServer: (id) sender;
- (IBAction) removeServer: (id) sender;
- (IBAction) addDomain: (id) sender;
- (IBAction) removeDomain: (id) sender;
- (IBAction) makeDefaultDomain: (id) sender;
- (IBAction) apply: (id) sender;
- (IBAction) cancel: (id) sender;
- (IBAction) ok: (id) sender;

- (IBAction) versionRadioButtonWasHit: (id) sender;
- (IBAction) typeRadioButtonWasHit: (id) sender;
- (IBAction) portRadioButtonWasHit: (id) sender;
- (IBAction) addServerCancel: (id) sender;
- (IBAction) addServerOK: (id) sender;

- (int) numberOfRowsInTableView: (NSTableView *) tableView;
- (id) tableView: (NSTableView *) tableView objectValueForTableColumn: (NSTableColumn *) tableColumn row: (int) rowIndex;
- (void) tableView: (NSTableView *) tableView setObjectValue: value forTableColumn: (NSTableColumn *) tableColumn row: (int) rowIndex;

- (BOOL) selectionShouldChangeInTableView: (NSTableView *) tableView;
- (BOOL) tabView: (NSTabView *) tabView shouldSelectTabViewItem: (NSTabViewItem *) tabViewItem;
- (void) sheetDidEnd: (NSWindow *) sheet returnCode: (int) returnCode contextInfo: (void *) contextInfo;

- (void) realmConfigurationErrorNeedsDisplay: (NSNotification *) notification;
- (void) tableViewSelectionDidChange: (NSNotification *) notification;

- (NSString *) currentRealmString;
- (NSString *) currentServerString;
- (NSString *) currentDomainString;
- (BOOL) stopEditing;

- (BOOL) saveChanges;

- (KerberosRealm *) selectedRealm;
- (KerberosServer *) selectedServer;
- (KerberosDomain *) selectedDomain;

- (void) selectRealm: (KerberosRealm *) realm edit: (BOOL) edit;
- (void) selectServer: (KerberosServer *) server edit: (BOOL) edit;
- (void) selectDomain: (KerberosDomain *) domain edit: (BOOL) edit;

- (NSString *) errorMessage;
- (NSString *) errorDescription;
- (void) setErrorMessage: (NSString *) newErrorMessage description: (NSString *) newErrorDescription;
- (void) displayErrorMessage: (NSString *) errorMessage description: (NSString *) errorDescription;

@end

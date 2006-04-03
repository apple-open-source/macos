/*
 * ChangePasswordController.m
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

#import "KerberosAgentController.h"
#import "ChangePasswordController.h"
#import "ErrorAlert.h"

#define ChangePasswordControllerString(key) NSLocalizedStringFromTable (key, @"ChangePasswordController", NULL)

@implementation ChangePasswordController

// ---------------------------------------------------------------------------

- (id) initWithPrincipal: (Principal *) inPrincipal
{
    if ((self = [super initWithWindowNibName: @"ChangePasswordController"])) {
        dprintf ("ChangePasswordController initializing");
        
        result = klNoErr;
        principal = (inPrincipal != NULL) ? [inPrincipal retain] : NULL;
        applicationNameString = NULL;
        applicationIcon = NULL;
        isSheet = NO;
        newPassword = NULL;
    }
    
    return self;
}

// ---------------------------------------------------------------------------

- (void) dealloc
{
    dprintf ("ChangePasswordController deallocating");
    if (principal             != NULL) { [principal release]; }
    if (applicationNameString != NULL) { [applicationNameString release]; }
    if (applicationIcon       != NULL) { [applicationIcon release]; }
    if (newPassword           != NULL) { [newPassword release]; }
    [super dealloc];
}

#pragma mark -- Loading --

// ---------------------------------------------------------------------------

- (void) windowDidLoad
{
    dprintf ("Entering ChangePasswordController windowDidLoad");
    
    // Set up the dialog icon:
    NSImage *applicationIconImage = [NSImage imageNamed: @"NSApplicationIcon"];
    [kerberosIconImageView setImage: applicationIconImage];
    if (applicationIcon != NULL) {
        [kerberosIconImageView setBadgeImage: applicationIcon];
    }

    // Text Fields
    NSString *principalString = [principal displayStringForKLVersion: kerberosVersion_V5];
    if (applicationNameString != NULL) {
        [headerTextField setStringValue: 
            [NSString stringWithFormat: ChangePasswordControllerString (@"ChangePasswordControllerApplicationRequest"), 
                applicationNameString, principalString]];
    } else {
        [headerTextField setStringValue: 
            [NSString stringWithFormat: ChangePasswordControllerString (@"ChangePasswordControllerRequest"), principalString]];
    }
    
    [oldPasswordSecureTextField setObjectValue: @""];
    [newPasswordSecureTextField setObjectValue: @""];
    [verifyPasswordSecureTextField setObjectValue: @""];
}    

// ---------------------------------------------------------------------------

- (void) controlTextDidChange: (NSNotification *) aNotification
{
    BOOL havePasswords = (([[oldPasswordSecureTextField    stringValue] length] > 0) && 
                          ([[newPasswordSecureTextField    stringValue] length] > 0) &&
                          ([[verifyPasswordSecureTextField stringValue] length] > 0));
    
    [okButton setEnabled: havePasswords];
}

#pragma mark -- Actions --

// ---------------------------------------------------------------------------

- (IBAction) ok: (id) sender
{
    KLStatus err = klNoErr;
    BOOL rejected = NO;
    NSMutableString *rejectionError = NULL;
    NSMutableString *rejectionDescription = NULL;
    
    if (![[newPasswordSecureTextField stringValue] isEqualToString: [verifyPasswordSecureTextField stringValue]]) {
        err = klPasswordMismatchErr;
    }        

    
    if (err == klNoErr) {
        rejectionError = [[NSMutableString alloc] init];
        rejectionDescription = [[NSMutableString alloc] init];
        if ((rejectionError == NULL) || (rejectionDescription == NULL)) {
            err = klMemFullErr;
        }
    }
    
    if (err == klNoErr) {
        err = [principal changePasswordWithOldPassword: [oldPasswordSecureTextField stringValue]
                                           newPassword: [newPasswordSecureTextField stringValue]
                                              rejected: &rejected
                                        rejectionError: rejectionError
                                  rejectionDescription: rejectionDescription];
    }
    
    if (err == klNoErr) {
        if (rejected) {
            [ErrorAlert alertForMessage: rejectionError
                            description: rejectionDescription
                         modalForWindow: [self window]];
        } else {
            // Remember the new password in case the caller wants to get new tickets with it later
            if (newPassword != NULL) { [newPassword release]; }
            newPassword = [[newPasswordSecureTextField stringValue] retain];
            
            [self stopWithCode: err];
        }
    } else {
        [ErrorAlert alertForError: err
                           action: KerberosChangePasswordAction
                   modalForWindow: [self window]];        
    }
    
    if (rejectionError       != NULL) { [rejectionError release]; }
    if (rejectionDescription != NULL) { [rejectionDescription release]; }
}

// ---------------------------------------------------------------------------

- (IBAction) cancel: (id) sender
{
    [self stopWithCode: klUserCanceledErr];
}

#pragma mark -- Miscellaneous --

// ---------------------------------------------------------------------------

- (NSString *) newPassword
{
    return newPassword;
}

// ---------------------------------------------------------------------------

- (void) setApplicationNameString: (NSString *) string
{
    if (applicationNameString != NULL) { [applicationNameString release]; }
    applicationNameString = string;
    if (applicationNameString != NULL) { [applicationNameString retain]; }
}

// ---------------------------------------------------------------------------

- (void) setApplicationIcon: (NSImage *) icon
{
    if (applicationIcon != NULL) { [applicationIcon release]; }
    applicationIcon = icon;
    if (applicationIcon != NULL) { [applicationIcon retain]; }
}

// ---------------------------------------------------------------------------

- (int) runWindow
{
    isSheet = NO;
    
    [[NSApp delegate] addActiveWindow: [self window]];
    [[self window] center];
    [self showWindow: self];
    [NSApp run];
    [self close];
    [[NSApp delegate] removeActiveWindow: [self window]];
    
    return result;
}

// ---------------------------------------------------------------------------

- (int) runSheetModalForWindow: (NSWindow *) parentWindow
{
    isSheet = YES;
    
    [NSApp beginSheet: [self window]
       modalForWindow: parentWindow 
        modalDelegate: self 
       didEndSelector: NULL
          contextInfo: NULL];
    
    result = [NSApp runModalForWindow: [self window]];
    
    [self close];
    
    return result;
}

// ---------------------------------------------------------------------------

- (void) stopWithCode: (int) returnCode
{
    if (isSheet) {
        [NSApp endSheet: [self window] returnCode: returnCode];
        [NSApp stopModalWithCode: returnCode];
    } else {
        result = returnCode;
        [NSApp stop: self];
    }
}

// ---------------------------------------------------------------------------

- (BOOL) worksWhenModal
{
    return YES;
}

@end


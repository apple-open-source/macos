/*
 * ChangePasswordController.h
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLoginServer/Headers/ChangePasswordController.h,v 1.4 2004/12/01 18:57:30 lxs Exp $
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

#include <Kerberos/Kerberos.h>
#import "Principal.h"
#import "BadgedImageView.h"

@interface ChangePasswordController : NSWindowController
{
    IBOutlet BadgedImageView *kerberosIconImageView;
    IBOutlet NSTextField *headerTextField;
    IBOutlet NSSecureTextField *oldPasswordSecureTextField;
    IBOutlet NSSecureTextField *newPasswordSecureTextField;
    IBOutlet NSSecureTextField *verifyPasswordSecureTextField;
    IBOutlet NSButton *okButton;
    IBOutlet NSButton *cancelButton;

    Principal *principal;
    NSString *applicationNameString;
    NSImage *applicationIcon;
    BOOL isSheet;
    NSString *newPassword;
    
    KLStatus result;
}

- (id) initWithPrincipal: (Principal *) inPrincipal;
- (void) dealloc;

- (void) windowDidLoad;

- (void) controlTextDidChange: (NSNotification *) notification;

- (IBAction) ok: (id) sender;
- (IBAction) cancel: (id) sender;

- (NSString *) newPassword;
    
- (void) setApplicationNameString: (NSString *) string;
- (void) setApplicationIcon: (NSImage *) icon;

- (int) runWindow;
- (int) runSheetModalForWindow: (NSWindow *) parentWindow;

- (void) stopWithCode: (int) returnCode;

@end
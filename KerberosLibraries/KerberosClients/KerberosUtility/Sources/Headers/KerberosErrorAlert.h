/*
 * KerberosErrorAlert.h
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


typedef enum _KerberosAction {
    KerberosNoAction = 0,
    KerberosGetTicketsAction,
    KerberosRenewTicketsAction,
    KerberosValidateTicketsAction,
    KerberosDestroyTicketsAction,
    KerberosChangePasswordAction,
    KerberosChangeActiveUserAction
} KerberosAction;

@interface KerberosErrorAlert : NSAlert
{
}

// warning: setFloatAtLevel not thread-safe. call once before spawning threads
+ (void) setFloatAtLevel: (NSInteger) level;

+ (void) alertForError: (int) error
                action: (KerberosAction) action;

+ (void) alertForError: (int) error
                action: (KerberosAction) action
        modalForWindow: (NSWindow *) parentWindow;

//+ (void) alertForError: (int) error 
//                action: (KerberosAction) action 
//                sender: (id) sender;

+ (BOOL) alertForYNQuestion: (NSString *) question
                     action: (KerberosAction) action;

+ (BOOL) alertForYNQuestion: (NSString *) question
                     action: (KerberosAction) action 
             modalForWindow: (NSWindow *) parentWindow;

//+ (BOOL) alertForYNQuestion: (NSString *) question
//                     action: (KerberosAction) action 
//                     sender: (id) sender;

+ (void) alertForMessage: (NSString *) message
             description: (NSString *) description;

+ (void) alertForMessage: (NSString *) message
             description: (NSString *) description 
          modalForWindow: (NSWindow *) parentWindow;

//+ (void) alertForMessage: (NSString *) message
//             description: (NSString *) description 
//                  sender: (id) sender;

+ (BOOL) alertForYNQuestion: (NSString *) question;

+ (BOOL) alertForYNQuestion: (NSString *) question
             modalForWindow: (NSWindow *) parentWindow;

//+ (BOOL) alertForYNQuestion: (NSString *) question
//                     sender: (id) sender;

- (NSInteger) runModal;

- (void) beginSheetModalForWindow: (NSWindow *) window 
                    modalDelegate: (id) modalDelegate 
                   didEndSelector: (SEL) alertDidEndSelector 
                      contextInfo: (void *) contextInfo;

- (NSString *) messageTextForAction: (KerberosAction) action;

- (NSString *) informativeTextForErrorCode: (int) error;

- (void) errorSheetDidEnd: (NSAlert *) alert returnCode: (int) returnCode contextInfo: (void *) contextInfo;

@end

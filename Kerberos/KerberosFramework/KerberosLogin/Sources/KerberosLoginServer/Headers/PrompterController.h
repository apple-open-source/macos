/*
 * PrompterController.h
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLoginServer/Headers/PrompterController.h,v 1.3 2004/12/01 18:57:39 lxs Exp $
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
#import "BadgedImageView.h"

krb5_error_code GraphicalKerberosPrompter (krb5_context  context,
                                           void         *data,
                                           const char   *name,
                                           const char   *banner,
                                           int           num_prompts,
                                           krb5_prompt   prompts[]);

@interface Prompt : NSObject
{
    NSString *promptString;
    BOOL secure;
    NSMutableString *responseString;
}

- (id) initWithPrompt: (krb5_prompt *) prompt;
- (void) dealloc;

- (NSString *) prompt;
- (BOOL) secure;
- (void) setResponse: (NSString *) response;

- (void) saveResponseInPrompt: (krb5_prompt *) prompt;

@end

@interface PrompterController : NSWindowController
{
    IBOutlet BadgedImageView *kerberosIconImageView;
    IBOutlet NSTextField *bannerTextField;
    IBOutlet NSMatrix *promptsMatrix;
    IBOutlet NSBox *responsesBox;
    IBOutlet NSButton *okButton;
    IBOutlet NSButton *cancelButton;

    NSMutableArray *responseTextFieldArray;
    
    NSString *titleString;
    NSString *bannerString;
    NSArray *promptsArray;
    NSString *applicationNameString;
    NSString *applicationIconPathString;
    BOOL isSheet;
    KLStatus result;
}

- (id) initWithTitle: (NSString *) title
              banner: (NSString *) banner
             prompts: (NSArray *) prompts;

- (void) dealloc;

- (void) windowDidLoad;

- (IBAction) ok: (id) sender;
- (IBAction) cancel: (id) sender;

- (int) runWindow;
- (int) runSheetModalForWindow: (NSWindow *) parentWindow;

- (void) stopWithCode: (int) returnCode;

@end


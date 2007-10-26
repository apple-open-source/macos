/*
 * KerberosAgentMachIPCShim.m
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

#import "ChangePasswordController.h"
#import "PrompterController.h"
#import "SFKerberosCredentialSelector.h"
#import "KerberosErrorAlert.h"
#import "KerberosAgentIPCServer.h"
#import "KerberosLoginPrivate.h"


// ---------------------------------------------------------------------------

static boolean_t CallingApplicationIsKerberosApp (NSString *inApplicationPath)
{
    NSBundle *bundle = [NSBundle bundleWithPath: inApplicationPath];
    if (bundle != NULL) {
        NSString *identifier = [bundle bundleIdentifier];
        if (identifier != NULL) {
            return ([identifier compare: @"edu.mit.Kerberos.KerberosApp"] == NSOrderedSame);
        }
    }
    
    return false;
}

// ---------------------------------------------------------------------------

static boolean_t CallingApplicationIsKerberosMenu (NSString *inApplicationPath)
{
    NSBundle *bundle = [NSBundle bundleWithPath: inApplicationPath];
    if (bundle != NULL) {
        NSString *identifier = [bundle bundleIdentifier];
        if (identifier != NULL) {
            return ([identifier compare: @"edu.mit.Kerberos.KerberosMenu"] == NSOrderedSame ||
                    [identifier compare: @"com.apple.systemuiserver"] == NSOrderedSame);
        }
    }
    
    return false;
}

// ---------------------------------------------------------------------------

static NSImage *CallingApplicationIcon (NSString *inApplicationPath)
{
    if (CallingApplicationIsKerberosApp (inApplicationPath) ||
        CallingApplicationIsKerberosMenu (inApplicationPath)) {
        return NULL;
    } else {
        return [[NSWorkspace sharedWorkspace] iconForFile: inApplicationPath];
    }
}

// ---------------------------------------------------------------------------

static NSString *CallingApplicationName (task_t inTask, NSString *inApplicationPath)
{
    KLStatus err = klNoErr;
    pid_t taskPID;
    ProcessSerialNumber taskPSN;
    CFStringRef taskNameStringRef = NULL;
    NSString *taskName = NULL;
    
    if (CallingApplicationIsKerberosApp (inApplicationPath) ||
        CallingApplicationIsKerberosMenu (inApplicationPath)) {
        return NULL;
    }
    
    if (!err) {
        err = pid_for_task (inTask, &taskPID);
    }
    
    if (!err) {
        err = GetProcessForPID (taskPID, &taskPSN);
    }
    
    if (!err) {
        err = CopyProcessName (&taskPSN, &taskNameStringRef);
    }
    
    if (!err) {
        taskName = [NSString stringWithString: (NSString *) taskNameStringRef];
    }
    
    if (taskNameStringRef != NULL) { CFRelease (taskNameStringRef); }
    
    return taskName;
}

// ---------------------------------------------------------------------------

static boolean_t CallingApplicationIsFrontProcess (task_t inTask, NSString *inApplicationPath)
{
    KLStatus err = klNoErr;
    Boolean taskIsFrontProcess;
    pid_t taskPID;
    ProcessSerialNumber taskPSN, frontPSN;
    
    if (CallingApplicationIsKerberosMenu (inApplicationPath)) {
        return true;
    }

    if (!err) {
        err = pid_for_task (inTask, &taskPID);
    }
    
    if (err == klNoErr) {
        err = GetProcessForPID (taskPID, &taskPSN);
    }
    
    if (err == klNoErr) {
        err = GetFrontProcess (&frontPSN);
    }
    
    if (err == klNoErr) {
        err = SameProcess (&taskPSN, &frontPSN, &taskIsFrontProcess);
    }
    
    return (err == klNoErr) ? taskIsFrontProcess : false;
}

#pragma mark -

// ---------------------------------------------------------------------------

kern_return_t KAIPCGetServerPID (mach_port_t  inServerPort,
                                 KLIPCPid    *outServerPID)
{
    return pid_for_task (mach_task_self (), outServerPID);
}

// ---------------------------------------------------------------------------

kern_return_t KAIPCAcquireNewInitialTickets (mach_port_t        inServerPort,
                                             task_t             inApplicationTask,
                                             KLIPCInString      inApplicationPath, mach_msg_type_number_t inApplicationPathCnt,
                                             KLIPCBoolean       inAllowHomeDirectoryAccess,
                                             KLIPCInPrincipal   inPrincipal,       mach_msg_type_number_t inPrincipalCnt,
                                             KLIPCFlags         inFlags,
                                             KLIPCTime          inLifetime,
                                             KLIPCTime          inRenewableLifetime,
                                             KLIPCTime          inStartTime,
                                             KLIPCBoolean       inForwardable,
                                             KLIPCBoolean       inProxiable,
                                             KLIPCBoolean       inAddressless,
                                             KLIPCInString      inServiceName,     mach_msg_type_number_t inServiceNameCnt,
                                             KLIPCOutPrincipal *outPrincipal,      mach_msg_type_number_t *outPrincipalCnt,
                                             KLIPCOutString    *outCacheName,      mach_msg_type_number_t *outCacheNameCnt,
                                             KLIPCStatus       *outResult)
{
    kern_return_t   err = KERN_SUCCESS;
    KLStatus        result = klNoErr;
    
    KerberosPrincipal *principal = NULL;
    NSString *serviceNameString = NULL;
    NSString *applicationPath = NULL;
    
    const char*     newPrincipalString = NULL;
    const char*     newCacheNameString = NULL;
    char*           newPrincipal = NULL;
    char*           newCacheName = NULL;
    size_t          newPrincipalLength = 0;
    size_t          newCacheNameLength = 0;
    
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        result = __KLSetHomeDirectoryAccess (inAllowHomeDirectoryAccess);
    }
    
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        if (inApplicationPathCnt > 0) {
            applicationPath = [[NSString alloc] initWithUTF8String: inApplicationPath];
            if (applicationPath == NULL) { result = klMemFullErr; }
        }
    }
    
    if (inPrincipalCnt > 0) {
        NSString *principalString = NULL;
        
        if ((result == klNoErr) && (err == KERN_SUCCESS)) {
			principalString = [NSString stringWithUTF8String: inPrincipal];
            if (principalString == NULL) { result = klMemFullErr; }
        }
        
        if ((result == klNoErr) && (err == KERN_SUCCESS)) {
            principal = [[KerberosPrincipal alloc] initWithString: principalString];
            if (principal == NULL) { result = klMemFullErr; }
        }
    }
    
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        if (inServiceNameCnt > 0) {
            serviceNameString = [[NSString alloc] initWithUTF8String: inServiceName];
            if (serviceNameString == NULL) { result = klMemFullErr; }
        }
    }
    
      //
      // Select a credential to use for authenticating.
      // This can be a:
      //              - identity (using PKINIT)
      //              - .Mac identity (using PKINIT)
      //              - Internet Password in the user's keychain
      //              - password taken from the user in the Kerberos authentication panel
      //
    SFKerberosCredentialSelector *credentialSelector = NULL;
      if ((result == klNoErr) && (err == KERN_SUCCESS)) {
              credentialSelector = [[SFKerberosCredentialSelector alloc] init];
              if ( credentialSelector != NULL )
                      result = [credentialSelector selectCredentialWithPrincipal:principal
                                                                              serviceName:serviceNameString
                                                                              applicationTask:inApplicationTask 
                                                                              applicationPath:applicationPath 
                                                                              inLifetime:inLifetime
                                                                              inRenewableLifetime:inRenewableLifetime
                                                                              inFlags:inFlags
                                                                              inStartTime:inStartTime
                                                                              inForwardable:inForwardable
                                                                              inProxiable:inProxiable
                                                                              inAddressless:inAddressless
                                                                              isAutoPopup:!CallingApplicationIsFrontProcess(inApplicationTask, applicationPath)
                                                                              inApplicationName:CallingApplicationName(inApplicationTask, applicationPath)
                                                                              inApplicationIcon:CallingApplicationIcon(applicationPath)];
              else
                      result = klMemFullErr;
    }

      if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        newPrincipalString = [[[credentialSelector acquiredPrincipal] displayString] UTF8String];
        newCacheNameString = [[credentialSelector acquiredCacheName] UTF8String];
          newPrincipalLength = (newPrincipalString != NULL) ? strlen (newPrincipalString) + 1 : 0;
          newCacheNameLength = (newCacheNameString != NULL) ? strlen (newCacheNameString) + 1 : 0;
      }

    if (credentialSelector != NULL) { [credentialSelector release]; }
    
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        if (newPrincipalString != NULL) {
            err = vm_allocate (mach_task_self (), (vm_address_t *) &newPrincipal, newPrincipalLength, true);
        }
    }
    
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        if (newCacheNameString != NULL) {
            err = vm_allocate (mach_task_self (), (vm_address_t *) &newCacheName, newCacheNameLength, true);
        }
    }
    
    if (err == KERN_SUCCESS) {
        if (result == klNoErr) {
            memmove (newPrincipal, newPrincipalString, newPrincipalLength);
            memmove (newCacheName, newCacheNameString, newCacheNameLength);
            *outPrincipal = newPrincipal;
            *outCacheName = newCacheName;
            *outPrincipalCnt = newPrincipalLength;
            *outCacheNameCnt = newCacheNameLength;
            newPrincipal = NULL;
            newCacheName = NULL;
        }
        *outResult = result;
    }
    
    // newPrincipalString and newCacheNameString are autoreleased
    
    if (newPrincipal      != NULL) { vm_deallocate (mach_task_self (), (vm_address_t) newPrincipal, newPrincipalLength); }
    if (newCacheName      != NULL) { vm_deallocate (mach_task_self (), (vm_address_t) newCacheName, newCacheNameLength); }
    if (serviceNameString != NULL) { [serviceNameString release]; }
    if (applicationPath   != NULL) { [applicationPath release]; }
    if (principal         != NULL) { [principal release]; }
    
    kipc_server_quit ();
    return err;
}

// ---------------------------------------------------------------------------

kern_return_t KAIPCChangePassword (mach_port_t       inServerPort,
                                   task_t            inApplicationTask,
                                   KLIPCInString     inApplicationPath, mach_msg_type_number_t inApplicationPathCnt,
                                   KLIPCBoolean      inAllowHomeDirectoryAccess,
                                   KLIPCInPrincipal  inPrincipal,       mach_msg_type_number_t inPrincipalCnt,
                                   KLIPCStatus      *outResult)
{
    kern_return_t   err = KERN_SUCCESS;
    KLStatus        result = klNoErr;

    ChangePasswordController *controller = NULL;
    KerberosPrincipal                *principal = NULL;
    NSString                 *applicationPath = NULL;

    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        result = __KLSetHomeDirectoryAccess (inAllowHomeDirectoryAccess);
    }

    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        if (inApplicationPathCnt > 0) {
            applicationPath = [[NSString alloc] initWithUTF8String: inApplicationPath];
            if (applicationPath == NULL) { result = klMemFullErr; }
        }
    }
    
    if (inPrincipalCnt > 0) {
        NSString *principalString = NULL;
        
        if ((result == klNoErr) && (err == KERN_SUCCESS)) {
            principalString = [NSString stringWithUTF8String: inPrincipal];
            if (principalString == NULL) { result = klMemFullErr; }
        }
        
        if ((result == klNoErr) && (err == KERN_SUCCESS)) {
            principal = [[KerberosPrincipal alloc] initWithString: principalString];
            if (principal == NULL) { result = klMemFullErr; }
        }
    }

    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        controller = [[ChangePasswordController alloc] initWithPrincipal: principal];
        if (controller == NULL) { result = klFatalDialogErr; }
    }
    
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        [controller setApplicationNameString: CallingApplicationName (inApplicationTask, applicationPath)];
        [controller setApplicationIcon: CallingApplicationIcon (applicationPath)];
        
        [NSApp activateIgnoringOtherApps: YES];
        result = [controller runWindow];
    }

    if (err == KERN_SUCCESS) {
        *outResult = result;
    }

    if (applicationPath != NULL) { [applicationPath release]; }
    if (principal       != NULL) { [principal release]; }
    if (controller      != NULL) { [controller release]; }

    kipc_server_quit ();
    return err;
}

// ---------------------------------------------------------------------------

kern_return_t KAIPCPrompter (mach_port_t      inServerPort,
                             task_t           inApplicationTask,
                             KLIPCInString    inApplicationPath, mach_msg_type_number_t inApplicationPathCnt,
                             KLIPCBoolean     inAllowHomeDirectoryAccess,
                             KLIPCInString    inName,            mach_msg_type_number_t  inNameCnt,
                             KLIPCInString    inBanner,          mach_msg_type_number_t  inBannerCnt,
                             KLIPCIndex       inNumPrompts,
                             KLIPCInString    inPrompts,         mach_msg_type_number_t  inPromptsCnt,
                             KLIPCInString    inHiddens,         mach_msg_type_number_t  inHiddensCnt,
                             KLIPCInIntArray  inReplySizes,      mach_msg_type_number_t  inReplySizesCnt,
                             KLIPCOutString  *outReplies,        mach_msg_type_number_t *outRepliesCnt,
                             KLIPCStatus     *outResult)
{
    kern_return_t   err = KERN_SUCCESS;
    KLStatus        result = klNoErr;
    
    PrompterController *controller = NULL;
    NSMutableArray     *promptsArray = NULL;
    NSString           *applicationPath = NULL;

    krb5_prompt        *prompts = NULL;
    krb5_data          *promptReplies = NULL;
    char               *promptReplyData = NULL;
    int                 promptReplyDataSize = 0;
    KLIPCIndex          i;
    
    char           *replies = NULL;

    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        result = __KLSetHomeDirectoryAccess (inAllowHomeDirectoryAccess);
    }
        
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        if (inApplicationPathCnt > 0) {
            applicationPath = [[NSString alloc] initWithUTF8String: inApplicationPath];
            if (applicationPath == NULL) { result = klMemFullErr; }
        }
    }
    
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        promptsArray = [[NSMutableArray alloc] init];
        if (promptsArray == NULL) { result = klMemFullErr; }
    }

    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        prompts = (krb5_prompt *) malloc (inNumPrompts * sizeof (krb5_prompt));
        if (prompts == NULL) { result = klMemFullErr; }
    }
    
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        promptReplies = (krb5_data *) malloc (inNumPrompts * sizeof (krb5_data));
        if (promptReplies == NULL) { result = klMemFullErr; }
    }
    
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        for (i = 0; i < inNumPrompts; i++)
            promptReplyDataSize += inReplySizes[i];
        
        promptReplyData = (char *) malloc (promptReplyDataSize);
        if (promptReplyData == NULL) { result = klMemFullErr; }
    }
    
    /* Build the array of prompts */
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        char	*currentPromptString = (char *)inPrompts;
        char	*currentPromptReplyData = promptReplyData;
        
        for (i = 0; i < inNumPrompts; i++) {
            prompts[i].prompt = currentPromptString;
            prompts[i].hidden = inHiddens[i] == '1' ? true : false;
            
            prompts[i].reply = &promptReplies[i];
            prompts[i].reply->length = inReplySizes[i];
            prompts[i].reply->data = currentPromptReplyData;
            
            Prompt *prompt = [[Prompt alloc] initWithPrompt: &prompts[i]];
            if (prompt == NULL) { result = klMemFullErr; break; }
            [promptsArray addObject: prompt];
            [prompt release];
            
            currentPromptString += strlen (currentPromptString) + 1;
            currentPromptReplyData += inReplySizes[i];
        }
    }
    
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        NSString *titleString  = (inNameCnt   > 0) ? [NSString stringWithUTF8String: inName]   : NULL;
        NSString *bannerString = (inBannerCnt > 0) ? [NSString stringWithUTF8String: inBanner] : NULL;
        
        controller = [[PrompterController alloc] initWithTitle: titleString
                                                        banner: bannerString
                                                       prompts: promptsArray];
        if (controller == NULL) { result = klFatalDialogErr; }
    }
    
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        [NSApp activateIgnoringOtherApps: YES];
        result = [controller runWindow];
    }
    
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        err = vm_allocate (mach_task_self (), (vm_address_t *) &replies, promptReplyDataSize, true);
    }
    
    if (err == KERN_SUCCESS) {
        if (result == klNoErr) {
            for (i = 0; i < inNumPrompts; i++) {
                Prompt *prompt = [promptsArray objectAtIndex: i];
                [prompt saveResponseInPrompt: &prompts[i]];
            }        

            memmove (replies, promptReplyData, promptReplyDataSize);
            *outReplies = replies;
            *outRepliesCnt = promptReplyDataSize;
            replies = NULL;
        }
        *outResult = result;
    }
    
    if (replies         != NULL) { vm_deallocate (mach_task_self (), (vm_address_t) replies, promptReplyDataSize); }
    if (prompts         != NULL) { free (prompts); }
    if (promptReplies   != NULL) { free (promptReplies); }
    if (promptReplyData != NULL) { free (promptReplyData); }
    if (applicationPath != NULL) { [applicationPath release]; }
    if (controller      != NULL) { [controller release]; }
    
    kipc_server_quit ();
    return err;
}

// ---------------------------------------------------------------------------

kern_return_t KAIPCHandleError (mach_port_t            inServerPort,
                                task_t                 inApplicationTask,
                                KLIPCInString          inApplicationPath, mach_msg_type_number_t inApplicationPathCnt,
                                KLIPCBoolean           inAllowHomeDirectoryAccess,
                                KLIPCStatus            inError,
                                KLIPCDialogIdentifier  inDialogIdentifier,
                                KLIPCBoolean           inShowAlert,
                                KLIPCStatus           *outResult)
{
    kern_return_t err = KERN_SUCCESS;
    KLStatus      result = klNoErr;
    
    if (inShowAlert) {
        if ((result == klNoErr) && (err == KERN_SUCCESS)) {
            result = __KLSetHomeDirectoryAccess (inAllowHomeDirectoryAccess);
        }

        if ((result == klNoErr) && (err == KERN_SUCCESS)) {
            KerberosAction action = KerberosNoAction;

            switch (inDialogIdentifier) {
                case loginLibrary_LoginDialog:
                    action = KerberosGetTicketsAction;
                    
                case loginLibrary_ChangePasswordDialog:
                    action = KerberosChangePasswordAction;
                    break;
            }
            
            [NSApp activateIgnoringOtherApps: YES];
            [KerberosErrorAlert alertForError: inError action: action];
        }
    }

    if (err == KERN_SUCCESS) {
        *outResult = result;
    }

    kipc_server_quit ();
    return err;
}

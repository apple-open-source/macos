/*
 * KLSMachIPCShim.m
 *
 * $Header: /cvs/kfm/KerberosFramework/KerberosLogin/Sources/KerberosLoginServer/KLSMachIPCShim.m,v 1.9 2003/07/03 19:52:50 lxs Exp $
 *
 * Copyright 2003 Massachusetts Institute of Technology.
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

#import "KLSController.h"
#import "KerberosLoginIPCServer.h"
#import "KerberosLoginPrivate.h"

// ---------------------------------------------------------------------------

kern_return_t KLSIPCGetServerPID (
    mach_port_t 			inServerPort,
    KLIPCPid*				outServerPID)
{
    return pid_for_task (mach_task_self (), outServerPID);
}

// ---------------------------------------------------------------------------

kern_return_t KLSIPCAcquireNewInitialTickets (mach_port_t        inServerPort,
                                              KLIPCInString      inApplicationName, mach_msg_type_number_t inApplicationNameCnt,
                                              KLIPCInString      inApplicationIconPath, mach_msg_type_number_t inApplicationIconPathCnt,
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

    KLSController  *controller = NULL;
    char*           klOutPrincipalString = NULL;
    char*           klOutCacheName = NULL;
    char*           newPrincipal = NULL;
    char*           newCacheName = NULL;
    
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        controller = [NSApp delegate];
        if (controller == NULL) { result = klFatalDialogErr; }
    }

    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        result = __KLSetHomeDirectoryAccess (inAllowHomeDirectoryAccess);
    }
    
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        result = [controller getTicketsForPrincipal: (inPrincipalCnt > 0) ? inPrincipal : NULL
                                              flags: inFlags
                                           lifetime: inLifetime
                                  renewableLifetime: inRenewableLifetime
                                          startTime: inStartTime
                                        forwardable: inForwardable
                                          proxiable: inProxiable
                                        addressless: inAddressless
                                        serviceName: (inServiceNameCnt > 0) ? inServiceName : NULL
                                    applicationName: inApplicationName
                                applicationIconPath: inApplicationIconPath];
    }
    
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        klOutPrincipalString = [controller loginAcquiredPrincipal];
        klOutCacheName = [controller loginAcquiredCacheName];
    }    
    
    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        err = vm_allocate (mach_task_self (), (vm_address_t*)&newPrincipal, strlen (klOutPrincipalString) + 1, true);
    }

    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        err = vm_allocate (mach_task_self (), (vm_address_t*)&newCacheName, strlen (klOutCacheName) + 1, true);
    }

    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        memmove (newPrincipal, klOutPrincipalString, (strlen (klOutPrincipalString) + 1)  * sizeof (char));
        memmove (newCacheName, klOutCacheName, (strlen (klOutCacheName) + 1) * sizeof (char));
        *outPrincipal = newPrincipal;
        *outPrincipalCnt = strlen (klOutPrincipalString) + 1;
        *outCacheName = newCacheName;
        *outCacheNameCnt = strlen (klOutCacheName) + 1;
    }

    if (err == KERN_SUCCESS) {
        *outResult = result;
    } else {
        if (newPrincipal != NULL) {
            vm_deallocate (mach_task_self (), (vm_address_t)newPrincipal, strlen (klOutPrincipalString) + 1);
        }
        if (newCacheName != NULL) {
            vm_deallocate (mach_task_self (), (vm_address_t)newCacheName, strlen (klOutCacheName) + 1);
        }
    }
        
    mach_server_quit_self ();
    return err;
}

// ---------------------------------------------------------------------------

kern_return_t KLSIPCChangePassword (mach_port_t       inServerPort,
                                    KLIPCInString     inApplicationName,     mach_msg_type_number_t inApplicationNameCnt,
                                    KLIPCInString     inApplicationIconPath, mach_msg_type_number_t inApplicationIconPathCnt,
                                    KLIPCBoolean      inAllowHomeDirectoryAccess,
                                    KLIPCInPrincipal  inPrincipal,           mach_msg_type_number_t inPrincipalCnt,
                                    KLIPCStatus      *outResult)
{
    kern_return_t   err = KERN_SUCCESS;
    KLStatus        result = klNoErr;

    KLSController *controller = NULL;

    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        result = __KLSetHomeDirectoryAccess (inAllowHomeDirectoryAccess);
    }

    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        controller = [NSApp delegate];
        if (controller == NULL) { result = klFatalDialogErr; }
    }

    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        result = [controller changePasswordForPrincipal: (inPrincipalCnt > 0) ? inPrincipal : NULL];
    }

    if (err == KERN_SUCCESS) {
        *outResult = result;
    }

    mach_server_quit_self ();
    return err;
}

// ---------------------------------------------------------------------------

kern_return_t KLSIPCPrompter (mach_port_t      inServerPort,
                              KLIPCInString    inApplicationName,     mach_msg_type_number_t inApplicationNameCnt,
                              KLIPCInString    inApplicationIconPath, mach_msg_type_number_t inApplicationIconPathCnt,
                              KLIPCBoolean     inAllowHomeDirectoryAccess,
                              KLIPCInString    inName,                mach_msg_type_number_t  inNameCnt,
                              KLIPCInString    inBanner,              mach_msg_type_number_t  inBannerCnt,
                              KLIPCIndex       inNumPrompts,
                              KLIPCInString    inPrompts,             mach_msg_type_number_t  inPromptsCnt,
                              KLIPCInString    inHiddens,             mach_msg_type_number_t  inHiddensCnt,
                              KLIPCInIntArray  inReplySizes,          mach_msg_type_number_t  inReplySizesCnt,
                              KLIPCOutString  *outReplies,            mach_msg_type_number_t *outRepliesCnt,
                              KLIPCStatus     *outResult)
{
    kern_return_t   err = KERN_SUCCESS;
    KLStatus        result = klNoErr;
    
    KLSController  *controller = NULL;
    krb5_prompt    *prompts = NULL;
    krb5_data      *promptReplies = NULL;
    char           *promptReplyData = NULL;
    int             promptReplyDataSize = 0;
    KLIPCIndex      i;
    
    char           *replies = NULL;
    
    /* Allocate memory for the array of prompts */
    if ((err == KERN_SUCCESS) && (result == noErr)) {
        prompts = (krb5_prompt *) malloc (inNumPrompts * sizeof (krb5_prompt));
        if (prompts == NULL) { result = klMemFullErr; }
    }
    
    if ((err == KERN_SUCCESS) && (result == noErr)) {
        promptReplies = (krb5_data *) malloc (inNumPrompts * sizeof (krb5_data));
        if (promptReplies == NULL) { result = klMemFullErr; }
    }

    if ((err == KERN_SUCCESS) && (result == noErr)) {
        for (i = 0; i < inNumPrompts; i++)
            promptReplyDataSize += inReplySizes[i];

        promptReplyData = (char *) malloc (promptReplyDataSize);
        if (promptReplyData == NULL) { result = klMemFullErr; }
    }

    /* Build the array of prompts */
    if ((err == KERN_SUCCESS) && (result == klNoErr)) {
        char	*currentPromptString = (char *)inPrompts;
        char	*currentPromptReplyData = promptReplyData;

        for (i = 0; i < inNumPrompts; i++) {
            prompts[i].prompt = currentPromptString;
            prompts[i].hidden = inHiddens[i] == '1' ? true : false;

            prompts[i].reply = &promptReplies[i];
            prompts[i].reply->length = inReplySizes[i];
            prompts[i].reply->data = currentPromptReplyData;

            currentPromptString += strlen (currentPromptString) + 1;
            currentPromptReplyData += inReplySizes[i];
        }
    }

    if ((err == KERN_SUCCESS) && (result == klNoErr)) {
        controller = [NSApp delegate];
        if (controller == NULL) { result = klFatalDialogErr; }
    }

    if ((result == klNoErr) && (err == KERN_SUCCESS)) {
        result = __KLSetHomeDirectoryAccess (inAllowHomeDirectoryAccess);
    }

    if ((err == KERN_SUCCESS) && (result == klNoErr)) {
        result = [controller promptWithTitle: inName
                                      banner: inBanner
                                     prompts: prompts
                                 promptCount: inNumPrompts];
    }
    
    if ((err == KERN_SUCCESS) && (result == klNoErr)) {
        // vm_allocate the replies:
        err = vm_allocate (mach_task_self (), (vm_address_t*)&replies, promptReplyDataSize, true);
    }
    
    if ((err == KERN_SUCCESS) && (result == klNoErr)) {
        memmove (replies, promptReplyData, promptReplyDataSize);
        *outReplies = replies;
        *outRepliesCnt = promptReplyDataSize;
    }

    if (err == KERN_SUCCESS) {
        *outResult = result;
    } else {
        if (replies != NULL) {
            vm_deallocate (mach_task_self (), (vm_address_t)replies, promptReplyDataSize);
        }
    }

    if (prompts         != NULL) { free (prompts); }
    if (promptReplies   != NULL) { free (promptReplies); }
    if (promptReplyData != NULL) { free (promptReplyData); }
    
    mach_server_quit_self ();
    return err;
}

// ---------------------------------------------------------------------------

kern_return_t KLSIPCHandleError (mach_port_t            inServerPort,
                                 KLIPCInString          inApplicationName,     mach_msg_type_number_t inApplicationNameCnt,
                                 KLIPCInString          inApplicationIconPath, mach_msg_type_number_t inApplicationIconPathCnt,
                                 KLIPCBoolean           inAllowHomeDirectoryAccess,
                                 KLIPCStatus            inError,
                                 KLIPCDialogIdentifier  inDialogIdentifier,
                                 KLIPCBoolean           inShowAlert,
                                 KLIPCStatus           *outResult)
{
    kern_return_t   err = KERN_SUCCESS;
    KLStatus        result = klNoErr;
    
    KLSController *controller = NULL;

    if (inShowAlert) {
        if ((result == klNoErr) && (err == KERN_SUCCESS)) {
            result = __KLSetHomeDirectoryAccess (inAllowHomeDirectoryAccess);
        }

        if ((result == klNoErr) && (err == KERN_SUCCESS)) {
            controller = [NSApp delegate];
            if (controller == NULL) { result = klFatalDialogErr; }
        }

        if ((result == klNoErr) && (err == KERN_SUCCESS)) {
            [controller displayKLError: inError windowIdentifier: inDialogIdentifier];
        }

        if (err == KERN_SUCCESS) {
            *outResult = result;
        }
    }

    mach_server_quit_self ();
    return err;
}
